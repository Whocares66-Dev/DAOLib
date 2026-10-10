"""Builds DAOLib's documentation site (DAOLib #45) into build/docs-site/html.

Runs Doxygen with docs/Doxyfile: the public headers' comments (through
tools/doxygen_filter.py), the guide pages, and the theme in docs/theme. The
working tree's docs are the site's root ("Current"); each release tag vX.Y.Z
is built from that tag's own headers and guide, with today's Doxyfile,
filter and theme, into X.Y.Z/; versions.json at the root lists them for the
version selector in the title bar (docs/theme/dao-site.js). A member whose
name is unique on its page is linked as #Name everywhere, not by Doxygen's
hash of its signature (readable_anchors). A header's page
links to its source on GitHub at the same version (master, or the tag)
rather than carrying a copy. Doxygen is found on PATH, or under
build/tools/doxygen (the portable zip from
github.com/doxygen/doxygen/releases). Its warnings go to
build/docs-site/warnings.txt (the current docs') and are counted here.

    python tools/build_docs.py            # build
    python tools/build_docs.py --serve    # build, then serve on http://127.0.0.1:8045
"""

import argparse
import functools
import html
import http.server
import io
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
SITE = ROOT / "build" / "docs-site"
HTML = SITE / "html"
# What a release's docs are built from: its own headers and guide; the rest
# (Doxyfile, theme, front page, filter) is the working tree's.
FROM_THE_TAG = ("src/dao", "dev/PLUGINS.md")
FROM_TODAY = ("docs", "tools/doxygen_filter.py")


def doxygen():
    found = shutil.which("doxygen")
    if found:
        return found
    local = ROOT / "build" / "tools" / "doxygen" / "doxygen.exe"
    if local.exists():
        return str(local)
    sys.exit(
        "Doxygen was not found: put it on PATH, or unzip its Windows release into build/tools/doxygen"
    )


# A member's title as Doxygen 1.18 writes it: its hash anchor, then the title
# with a diamond permalink to that anchor before the name.
MEMBER_TITLE = re.compile(
    r'<a id="(a[0-9a-f]{32})" name="\1"></a>\s*'
    r'<h2 class="memtitle"><span class="permalink"><a href="#\1">&#9670;&#160;</a></span>(.*?)</h2>',
    re.S,
)
# A link to a member: a page's name and the member's hash anchor.
MEMBER_LINK = re.compile(r"([\w.-]+\.html)#(a[0-9a-f]{32})")
SAME_PAGE_LINK = re.compile(r'href="#(a[0-9a-f]{32})"')


def anchor_name(title):
    """A member's readable anchor, from its title: "Advance() [1/2]" is Advance."""
    text = re.sub(r"<[^>]*>", "", title)
    text = re.sub(r"\[\d+/\d+\]", "", text)
    text = re.sub(r"[()\s]", "", text)
    text = html.unescape(text)
    return re.sub(r"[^\w~]", "_", text)


def readable_anchors(output):
    """Gives each member whose name is unique on its page the anchor #Name and
    points every link to it there, across the site; the name is its own link.

    Doxygen's anchors hash the whole signature, so a link breaks when a
    parameter changes; #Name holds until the name does. An overloaded name
    keeps Doxygen's anchor. Doxygen's anchors stay beside the new ones, so a
    link made before still works."""
    pages = sorted(output.glob("*.html"))
    names = {}  # (page, hash) -> readable name
    for page in pages:
        text = page.read_text(encoding="utf-8")
        titles = MEMBER_TITLE.findall(text)
        counts = {}
        for _, title in titles:
            counts[anchor_name(title)] = counts.get(anchor_name(title), 0) + 1
        taken = set(re.findall(r'\bid="([^"]+)"', text))
        for anchor, title in titles:
            name = anchor_name(title)
            if counts[name] == 1 and name not in taken:
                names[(page.name, anchor)] = name

        def title_link(match, page=page.name):
            anchor, title = match.group(1), match.group(2)
            name = names.get((page, anchor))
            target = name or anchor
            heading = f' id="{name}"' if name else ""
            return (
                f'<a id="{anchor}" name="{anchor}"></a>\n'
                f'<h2 class="memtitle"{heading}><a href="#{target}">{title}</a></h2>'
            )

        page.write_text(MEMBER_TITLE.sub(title_link, text), encoding="utf-8")

    def link(match):
        name = names.get((match.group(1), match.group(2)))
        return f"{match.group(1)}#{name}" if name else match.group(0)

    for path in [*pages, *output.glob("*.js"), *output.glob("search/*.js")]:
        text = path.read_text(encoding="utf-8")
        rewritten = MEMBER_LINK.sub(link, text)
        if path.suffix == ".html":
            rewritten = SAME_PAGE_LINK.sub(
                lambda m, page=path.name: (
                    f'href="#{names.get((page, m.group(1)), m.group(1))}"'
                ),
                rewritten,
            )
        if rewritten != text:
            path.write_text(rewritten, encoding="utf-8")
    return len(names)


def fix_outline_escape(output):
    """Doxygen 1.18's navtree.js escapes "=" as "&#x3D;" for the page outline,
    then shows it as text: "operator&#x3D;". An "=" needs no escaping."""
    navtree = output / "navtree.js"
    text = navtree.read_text(encoding="utf-8")
    navtree.write_text(text.replace("'=': '&#x3D;'", "'=': '='"), encoding="utf-8")


def run_doxygen(cwd, output, warnings, ref):
    """Doxygen over docs/Doxyfile in `cwd`, its HTML into `output`, its source
    links to GitHub at `ref`."""
    config = (cwd / "docs" / "Doxyfile").read_text(encoding="utf-8")
    config += f'\nOUTPUT_DIRECTORY = "{output.parent.as_posix()}"\nHTML_OUTPUT = "{output.name}"\n'
    config += f'WARN_LOGFILE = "{warnings.as_posix()}"\n'
    env = dict(os.environ, DAOLIB_DOCS_REF=ref)
    result = subprocess.run(
        [doxygen(), "-"], cwd=cwd, input=config.encode("utf-8"), env=env, check=False
    )
    if result.returncode != 0:
        sys.exit(f"Doxygen failed ({result.returncode}) in {cwd}")
    readable_anchors(output)
    fix_outline_escape(output)


def releases():
    """The release tags, newest first."""
    tags = subprocess.run(
        ["git", "tag", "--list"], cwd=ROOT, capture_output=True, text=True, check=True
    )
    found = []
    for tag in tags.stdout.split():
        match = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)", tag)
        if match:
            found.append((tuple(map(int, match.groups())), tag))
    return [tag for _, tag in sorted(found, reverse=True)]


def build_release(tag, scratch):
    """A release's docs, from its own headers and guide, into HTML/X.Y.Z."""
    stage = scratch / tag
    stage.mkdir()
    archive = subprocess.run(
        ["git", "archive", "--format=tar", tag, *FROM_THE_TAG],
        cwd=ROOT,
        capture_output=True,
        check=False,
    )
    if archive.returncode != 0:
        print(f"{tag}: not built ({archive.stderr.decode(errors='replace').strip()})")
        return False
    with tarfile.open(fileobj=io.BytesIO(archive.stdout)) as tar:
        tar.extractall(stage, filter="data")
    for part in FROM_TODAY:
        source = ROOT / part
        target = stage / part
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, target)
        else:
            shutil.copy2(source, target)
    run_doxygen(stage, HTML / tag[1:], scratch / f"{tag}-warnings.txt", tag)
    return True


def build():
    if SITE.exists():
        shutil.rmtree(SITE)
    SITE.mkdir(parents=True)
    warnings = SITE / "warnings.txt"
    run_doxygen(ROOT, HTML, warnings, "master")
    versions = [{"label": "Current", "path": ""}]
    with tempfile.TemporaryDirectory() as temporary:
        built = [
            tag for tag in releases() if build_release(tag, pathlib.Path(temporary))
        ]
    versions.extend({"label": tag, "path": tag[1:] + "/"} for tag in built)
    (HTML / "versions.json").write_text(
        json.dumps(versions, indent=1), encoding="utf-8"
    )
    count = (
        len(warnings.read_text(encoding="utf-8", errors="replace").splitlines())
        if warnings.exists()
        else 0
    )
    print(
        f"site: {HTML / 'index.html'}; {len(versions)} version(s); {count} warning line(s) in {warnings}"
    )


def serve(port):
    handler = functools.partial(
        http.server.SimpleHTTPRequestHandler, directory=str(HTML)
    )
    with http.server.ThreadingHTTPServer(("0.0.0.0", port), handler) as server:
        print(
            f"serving http://127.0.0.1:{port}/ (and on this machine's network addresses; Ctrl+C ends it)"
        )
        server.serve_forever()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--serve", action="store_true", help="serve the site after building it"
    )
    parser.add_argument("--port", type=int, default=8045)
    args = parser.parse_args()
    build()
    if args.serve:
        serve(args.port)


if __name__ == "__main__":
    main()
