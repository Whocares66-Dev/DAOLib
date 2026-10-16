"""Builds DAOLib's documentation site (DAOLib #45, #49) into build/docs-site/DAOLib.

The site is Jekyll with Just the Docs (docs/_config.yml, as the Eluvian and
SkyrimFollowerTactics sites are). Its pages come from three places:

- the API reference (api/): Doxygen reads the public headers (docs/Doxyfile,
  through tools/doxygen_filter.py) and writes XML, which tools/doxygen_xml.py
  turns into a page per namespace and class;
- the guide (guide.html): dev/PLUGINS.md;
- the front page, the loader's page and the styling: docs/.

The working tree's docs are the site's root ("Current"); each release tag
vX.Y.Z is built from that tag's own headers and guide, with today's styling,
converter and filter, into X.Y.Z/. Every version is listed in the
version selector, which keeps the page you are on where that version has it.

Needs Doxygen 1.18 (on PATH, or the Windows zip unpacked in build/tools/doxygen)
and Ruby with Bundler (`bundle install` in docs/ once).

    python tools/build_docs.py            # build
    python tools/build_docs.py --serve    # build, then serve on http://127.0.0.1:8045/DAOLib/
"""

import argparse
import functools
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
DOCS = ROOT / "docs"
BUILD = ROOT / "build"
SITE = BUILD / "docs-site"
REPOSITORY = "https://github.com/Whocares66-Dev/DAOLib"
# What a release's docs are built from: its own headers and guide; the rest
# (the styling, the front page, the Doxyfile, the filter, the converter) is
# today's.
FROM_THE_TAG = ("src/dao", "dev/PLUGINS.md")
FROM_TODAY = ("docs", "tools/doxygen_filter.py")
# What a local `jekyll serve` or `bundle install` leaves in docs/.
LOCAL_OUTPUT = shutil.ignore_patterns(
    "_site", ".jekyll-cache", ".jekyll-metadata", ".bundle", "vendor"
)

sys.path.insert(0, str(ROOT / "tools"))
import doxygen_xml  # noqa: E402


def doxygen():
    found = shutil.which("doxygen")
    if found:
        return found
    local = BUILD / "tools" / "doxygen" / "doxygen.exe"
    if local.exists():
        return str(local)
    sys.exit(
        "Doxygen was not found: put it on PATH, or unzip its Windows release into build/tools/doxygen"
    )


def setting(name):
    """A top-level value of docs/_config.yml."""
    match = re.search(
        rf"^{name}:\s*(.+)$", (DOCS / "_config.yml").read_text(encoding="utf-8"), re.M
    )
    return match.group(1).strip() if match else None


def api_pages(source, xml, ref):
    """Doxygen's XML of `source`'s headers, as API pages in its docs/."""
    config = (source / "docs" / "Doxyfile").read_text(encoding="utf-8")
    config += f'\nOUTPUT_DIRECTORY = "{xml.as_posix()}"\nWARN_LOGFILE = "{(xml / "warnings.txt").as_posix()}"\n'
    xml.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [doxygen(), "-"], cwd=source, input=config.encode("utf-8"), check=False
    )
    if result.returncode != 0:
        sys.exit(f"Doxygen failed ({result.returncode}) in {source}")
    count = doxygen_xml.Site(xml / "xml", ref, REPOSITORY).write(source / "docs")
    warnings = (
        (xml / "warnings.txt").read_text(encoding="utf-8", errors="replace").strip()
    )
    return count, warnings


def guide_page(source):
    """dev/PLUGINS.md as the site's guide page."""
    body = (source / "dev" / "PLUGINS.md").read_text(encoding="utf-8")
    front = (
        "---\ntitle: Writing a plugin\nnav_order: 4\npermalink: /guide.html\n---\n\n"
    )
    # Kept from Liquid, as the API pages are: its examples have C++ braces.
    (source / "docs" / "guide.md").write_text(
        front + "{% raw %}\n" + body.rstrip() + "\n{% endraw %}\n", encoding="utf-8"
    )


def pages(site_source):
    """Every page's URL, from its permalink, for the version selector."""
    urls = []
    for path in site_source.rglob("*.md"):
        match = re.search(
            r'^permalink:\s*"?([^"\n]+)"?\s*$', path.read_text(encoding="utf-8"), re.M
        )
        if match:
            urls.append(match.group(1))
    return sorted(urls)


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


def stage_release(tag, staging):
    """A release's sources: its own headers and guide, today's styling."""
    source = staging / tag
    source.mkdir()
    archive = subprocess.run(
        ["git", "archive", "--format=tar", tag, *FROM_THE_TAG],
        cwd=ROOT,
        capture_output=True,
        check=False,
    )
    if archive.returncode != 0:
        print(f"{tag}: not built ({archive.stderr.decode(errors='replace').strip()})")
        return None
    with tarfile.open(fileobj=io.BytesIO(archive.stdout)) as tar:
        tar.extractall(source, filter="data")
    for part in FROM_TODAY:
        target = source / part
        target.parent.mkdir(parents=True, exist_ok=True)
        if (ROOT / part).is_dir():
            shutil.copytree(ROOT / part, target, ignore=LOCAL_OUTPUT)
        else:
            shutil.copy2(ROOT / part, target)
    return source


def stage_current(staging):
    """The working tree's sources."""
    source = staging / "current"
    source.mkdir()
    shutil.copytree(DOCS, source / "docs", ignore=LOCAL_OUTPUT)
    for part in ("src/dao", "dev/PLUGINS.md", "tools/doxygen_filter.py"):
        target = source / part
        target.parent.mkdir(parents=True, exist_ok=True)
        if (ROOT / part).is_dir():
            shutil.copytree(ROOT / part, target)
        else:
            shutil.copy2(ROOT / part, target)
    return source


def jekyll(site_source, destination, overlay):
    bundle = shutil.which("bundle")
    if not bundle:
        sys.exit("Install Ruby and Bundler, then run `bundle install` in docs/")
    env = {**os.environ, "BUNDLE_GEMFILE": str(DOCS / "Gemfile")}
    subprocess.run(
        [
            bundle,
            "exec",
            "jekyll",
            "build",
            "--source",
            str(site_source),
            "--destination",
            str(destination),
            "--config",
            f"{site_source / '_config.yml'},{overlay}",
        ],
        cwd=DOCS,
        env=env,
        check=True,
    )


def build():
    baseurl = setting("baseurl").rstrip("/")
    if SITE.exists():
        shutil.rmtree(SITE)
    destination = SITE / baseurl.lstrip("/")
    destination.mkdir(parents=True)
    warnings = []
    with tempfile.TemporaryDirectory(prefix="docs-sources-", dir=BUILD) as temporary:
        staging = pathlib.Path(temporary)
        sources = [("Current", stage_current(staging), baseurl, "master")]
        for tag in releases():
            source = stage_release(tag, staging)
            if source is not None:
                sources.append((tag[1:], source, f"{baseurl}/{tag[1:]}", tag))
        for label, source, _, ref in sources:
            count, warned = api_pages(source, staging / f"xml-{label}", ref)
            guide_page(source)
            print(f"{label}: {count} API pages")
            if warned and label == "Current":
                warnings.append(warned)
        versions = [
            dict(label=label, baseurl=url, pages=pages(source / "docs"))
            for label, source, url, _ in sources
        ]
        for index, (label, source, url, _) in enumerate(sources):
            # JSON is valid YAML; Jekyll merges this after the site's own config.
            overlay = staging / f"config-{index}.yml"
            overlay.write_text(
                json.dumps(
                    dict(baseurl=url, docs_version=label, docs_versions=versions)
                ),
                encoding="utf-8",
            )
            jekyll(
                source / "docs",
                destination if index == 0 else destination / label,
                overlay,
            )
    (destination / ".nojekyll").touch()
    (SITE / "warnings.txt").write_text("\n".join(warnings), encoding="utf-8")
    print(
        f"site: {destination}; {len(versions)} version(s); "
        f"{len(warnings[0].splitlines()) if warnings else 0} Doxygen warning line(s) in {SITE / 'warnings.txt'}"
    )
    return baseurl


def serve(port, baseurl):
    handler = functools.partial(
        http.server.SimpleHTTPRequestHandler, directory=str(SITE)
    )
    with http.server.ThreadingHTTPServer(("0.0.0.0", port), handler) as server:
        print(
            f"serving http://127.0.0.1:{port}{baseurl}/ (and on this machine's network addresses; Ctrl+C ends it)"
        )
        server.serve_forever()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--serve", action="store_true", help="serve the site after building it"
    )
    parser.add_argument("--port", type=int, default=8045)
    args = parser.parse_args()
    baseurl = build()
    if args.serve:
        serve(args.port, baseurl)


if __name__ == "__main__":
    main()
