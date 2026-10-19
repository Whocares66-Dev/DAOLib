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
version selector, which keeps the page you are on where that version has it;
the selector reads the list from versions.json at the site's root.

A release's built site is kept in build/docs-cache under its tag and a hash
of everything that makes it (the tag's commit, docs/, the filter, the
converter, this script and Doxygen's version), and only built again when one
of those changes (DAOLib #201). A push that changes the headers alone builds
Current alone.

Needs Doxygen 1.18 (on PATH, or the Windows zip unpacked in build/tools/doxygen)
and Ruby with Bundler (`bundle install` in docs/ once).

    python tools/build_docs.py            # build
    python tools/build_docs.py --serve    # build, then serve on http://127.0.0.1:8045/DAOLib/
    python tools/build_docs.py --current  # the working tree's docs alone, no release's
"""

import argparse
import functools
import hashlib
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
CACHE = BUILD / "docs-cache"
# Today's files a release's built site depends on, beside docs/ and its tag.
BUILT_BY = ("tools/build_docs.py", "tools/doxygen_xml.py", "tools/doxygen_filter.py")
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


def release_key(tag):
    """A hash of what a release's built site is made from."""
    digest = hashlib.sha256()
    for command in (
        ["git", "rev-parse", f"{tag}^{{commit}}"],
        [doxygen(), "--version"],
    ):
        out = subprocess.run(
            command, cwd=ROOT, capture_output=True, text=True, check=True
        )
        digest.update(out.stdout.strip().encode() + b"\0")
    files = [ROOT / part for part in BUILT_BY]
    skipped = {"_site", ".jekyll-cache", ".jekyll-metadata", ".bundle", "vendor"}
    files += sorted(
        path
        for path in DOCS.rglob("*")
        if path.is_file() and not skipped & set(path.relative_to(DOCS).parts)
    )
    for path in files:
        digest.update(path.relative_to(ROOT).as_posix().encode() + b"\0")
        digest.update(path.read_bytes() + b"\0")
    return digest.hexdigest()[:16]


def release_site(tag, staging, url):
    """A release's built site and its pages' URLs: from the cache, or built
    into it. None when the tag has no docs to build."""
    kept = CACHE / f"{tag}-{release_key(tag)}"
    if not (kept / "pages.json").exists():
        source = stage_release(tag, staging)
        if source is None:
            return None
        count, _ = api_pages(source, staging / f"xml-{tag}", tag)
        guide_page(source)
        print(f"{tag[1:]}: {count} API pages", flush=True)
        for stale in CACHE.glob(f"{tag}-*"):
            shutil.rmtree(stale)
        jekyll(source / "docs", kept / "site", overlay(staging, tag[1:], url))
        (kept / "pages.json").write_text(
            json.dumps(pages(source / "docs")), encoding="utf-8"
        )
    else:
        print(f"{tag[1:]}: kept from {kept.relative_to(ROOT)}", flush=True)
    return kept / "site", json.loads((kept / "pages.json").read_text(encoding="utf-8"))


def overlay(staging, label, url):
    """The config Jekyll merges after the site's own for one version: its
    baseurl and label, and where the site's root is (versions.json)."""
    path = staging / f"config-{label}.yml"
    # JSON is valid YAML.
    path.write_text(
        json.dumps(
            dict(
                baseurl=url,
                docs_version=label,
                docs_root=setting("baseurl").rstrip("/"),
            )
        ),
        encoding="utf-8",
    )
    return path


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


def build(releases_too=True):
    baseurl = setting("baseurl").rstrip("/")
    if SITE.exists():
        shutil.rmtree(SITE)
    destination = SITE / baseurl.lstrip("/")
    destination.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix="docs-sources-", dir=BUILD) as temporary:
        staging = pathlib.Path(temporary)
        current = stage_current(staging)
        count, warnings = api_pages(current, staging / "xml-Current", "master")
        guide_page(current)
        print(f"Current: {count} API pages", flush=True)
        jekyll(current / "docs", destination, overlay(staging, "Current", baseurl))
        versions = [
            dict(label="Current", baseurl=baseurl, pages=pages(current / "docs"))
        ]
        for tag in releases() if releases_too else []:
            url = f"{baseurl}/{tag[1:]}"
            built = release_site(tag, staging, url)
            if built is not None:
                shutil.copytree(built[0], destination / tag[1:])
                versions.append(dict(label=tag[1:], baseurl=url, pages=built[1]))
    (destination / "versions.json").write_text(json.dumps(versions), encoding="utf-8")
    (destination / ".nojekyll").touch()
    (SITE / "warnings.txt").write_text(warnings, encoding="utf-8")
    print(
        f"site: {destination}; {len(versions)} version(s); "
        f"{len(warnings.splitlines())} Doxygen warning line(s) in {SITE / 'warnings.txt'}"
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
    parser.add_argument(
        "--current",
        action="store_true",
        help="build the working tree's docs alone, for a quick look",
    )
    parser.add_argument("--port", type=int, default=8045)
    args = parser.parse_args()
    baseurl = build(releases_too=not args.current)
    if args.serve:
        serve(args.port, baseurl)


if __name__ == "__main__":
    main()
