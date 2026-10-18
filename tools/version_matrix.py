"""Which executables of the game DAOLib's loader finds an address file for, tried in play.

Each variant's exe is staged into the install it belongs to, the game is started with the
build's loader, and Eluvian (the test bench plugin, DAOEluvianMCP) says whether the plugins
found their addresses: `health` has frames running, and a save is loaded into the world. The
loader's lines about the address library are read from loader.log. The install is put back as it
was, exe and loader, when the run ends, or fails.

    python tools/version_matrix.py                    every variant
    python tools/version_matrix.py steam-nexus-laa    the named ones
    python tools/version_matrix.py --list

THIS STARTS THE GAME, a copy of each variant in turn, and ends it after each. Say so before
running it, and have no game open.

The exes are kept in C:\\Modding\\DAOVersions\\<store>\\DAOrigins-<variant>-<version>.exe
(MANIFEST.sha256 beside them), the stock ones too, from which an install's exe is put back.
It needs the plugins Eluvian and turnbased in each install's daolib\\plugins (copied from the
GOG install's), and a debug build of this tree. EA's exe is not here: its folder is not
writable, and the EA app starts it (tools/ea_play.py).
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
VERSIONS = Path(r"C:\Modding\DAOVersions")
VERSION = "1.05.13263.0"
LOGS = Path.home() / "OneDrive" / "Documents" / "BioWare" / "Dragon Age" / "daolib"
PORT = 8930

INSTALLS = {
    "gog": {
        "bin": Path(
            r"C:\Program Files (x86)\GOG Galaxy\Games\Dragon Age Origins\bin_ship"
        ),
        "env": {},
        "stock": VERSIONS / "GOG" / f"DAOrigins-gog-{VERSION}.exe",
    },
    "steam": {
        "bin": Path(
            r"E:\SteamLibrary\steamapps\common\Dragon Age Ultimate Edition\bin_ship"
        ),
        # What the Steam stub wants of a game started by hand (dev/BUILDS.md).
        "env": {"SteamAppId": "47810", "SteamGameId": "47810"},
        "stock": VERSIONS / "Steam" / f"DAOrigins-steam-stock-{VERSION}.exe",
    },
}

# name: (install, exe, the file the loader should read, whether by the exe's code)
VARIANTS = {
    "gog": (
        "gog",
        VERSIONS / "GOG" / f"DAOrigins-gog-{VERSION}.exe",
        "daoaddresses-4eb19d53-009ec0a2-004e3024-00400000.bin",
        False,
    ),
    "steam-stock": (
        "steam",
        VERSIONS / "Steam" / f"DAOrigins-steam-stock-{VERSION}.exe",
        "daoaddresses-4eb19d53-00a49000-009ed2ee-00400000.bin",
        False,
    ),
    "steam-steamless": (
        "steam",
        VERSIONS / "Steam" / f"DAOrigins-steam-steamless-unpacked-{VERSION}.exe",
        "daoaddresses-4eb19d53-009ec0a2-004e3024-00400000.bin",
        True,
    ),
    "steam-nexus": (
        "steam",
        VERSIONS / "Steam_Decrypted" / f"DAOrigins-steam-nexus-decrypted-{VERSION}.exe",
        "daoaddresses-4eb19d53-009ec0a2-004e3024-00400000.bin",
        True,
    ),
    "steam-nexus-laa": (
        "steam",
        VERSIONS
        / "Steam_Decrypted"
        / f"DAOrigins-steam-nexus-decrypted-laa-{VERSION}.exe",
        "daoaddresses-4eb19d53-009ec0a2-004e3024-00400000.bin",
        True,
    ),
}


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def say(text):
    print(text, flush=True)


def kill_game():
    subprocess.run(
        ["taskkill", "/F", "/IM", "DAOrigins.exe"], capture_output=True, check=False
    )
    for _ in range(20):
        found = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq DAOrigins.exe"],
            capture_output=True,
            text=True,
            check=False,
        )
        if "DAOrigins.exe" not in found.stdout:
            return
        time.sleep(0.5)
    raise RuntimeError("DAOrigins.exe will not end")


def game_running():
    found = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq DAOrigins.exe"],
        capture_output=True,
        text=True,
        check=False,
    )
    return "DAOrigins.exe" in found.stdout


def request(method, path, body=None, timeout=20):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(
        f"http://127.0.0.1:{PORT}{path}",
        data=data,
        method=method,
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as answer:
            return json.loads(answer.read() or b"{}")
    except (urllib.error.URLError, OSError, ValueError):
        return None


class Install:
    """One install, with its exe and its loader staged, and put back."""

    def __init__(self, name, build):
        self.name = name
        spec = INSTALLS[name]
        self.bin = spec["bin"]
        self.env = spec["env"]
        self.stock = spec["stock"]
        self.build = build
        self.exe = self.bin / "DAOrigins.exe"
        self.saved = REPO / "build" / "version_matrix" / name
        self.stock_hash = sha256(self.stock)

    def save(self):
        """What is there, to be put back: the loader and the address files. The exe is put
        back from the stock copy kept in DAOVersions."""
        self.saved.mkdir(parents=True, exist_ok=True)
        loader = self.bin / "dinput8.dll"
        self.had_loader = loader.exists()
        if self.had_loader:
            shutil.copy2(loader, self.saved / "dinput8.dll")
        folder = self.bin / "daolib" / "addresses"
        self.had_addresses = folder.exists()
        if self.had_addresses:
            shutil.copytree(folder, self.saved / "addresses", dirs_exist_ok=True)

    def stage_loader(self):
        """This tree's loader and its address files, and nothing else of the address folder:
        what the loader makes of its own is made afresh."""
        shutil.copy2(self.build / "dinput8.dll", self.bin / "dinput8.dll")
        folder = self.bin / "daolib" / "addresses"
        shutil.rmtree(folder, ignore_errors=True)
        folder.mkdir(parents=True)
        for file in (self.build / "daolib" / "addresses").iterdir():
            shutil.copy2(file, folder / file.name)

    def stage_exe(self, exe):
        shutil.copy2(exe, self.exe)
        if sha256(self.exe) != sha256(exe):
            raise RuntimeError(f"{self.exe} is not {exe}")

    def restore(self):
        """The exe from the stock copy, checked; the loader and address files as they were."""
        shutil.copy2(self.stock, self.exe)
        if sha256(self.exe) != self.stock_hash:
            raise RuntimeError(f"{self.exe} was not put back")
        loader = self.bin / "dinput8.dll"
        if self.had_loader:
            shutil.copy2(self.saved / "dinput8.dll", loader)
        else:
            loader.unlink(missing_ok=True)
        folder = self.bin / "daolib" / "addresses"
        shutil.rmtree(folder, ignore_errors=True)
        if self.had_addresses:
            shutil.copytree(self.saved / "addresses", folder)


def lines_of(path):
    """A log's lines. The loader and the plugins start a log afresh for each session (the one
    before is kept beside it as <name>.previous.log), so the whole file is this run's."""
    if not path.exists():
        return []
    return path.read_bytes().decode("utf-8", "replace").splitlines()


def wait_for(predicate, seconds, step=2.0):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return True
        if not game_running():
            return False
        time.sleep(step)
    return False


def try_variant(install, name, load_save):
    _, exe, want_file, by_code = VARIANTS[name]
    result = {"variant": name, "exe": exe.name, "problems": []}
    problem = result["problems"].append
    kill_game()
    install.stage_loader()
    install.stage_exe(exe)
    loader_log = LOGS / "loader.log"
    tb_log = LOGS / "turnbased.log"
    env = {**os.environ, **install.env}
    # Shown but not activated: the window in front stays in front (DAOLib #198).
    behind = subprocess.STARTUPINFO()
    behind.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    behind.wShowWindow = 4  # SW_SHOWNOACTIVATE
    subprocess.Popen(
        [str(install.exe)],
        cwd=str(install.bin),
        env=env,
        startupinfo=behind,
        creationflags=0x00000008,  # DETACHED_PROCESS
    )

    def serving():
        health = request("GET", "/api/health")
        return bool(health) and health.get("frames", 0) > 0

    # The wrapped builds wait for the stub first; the menu is up within a minute.
    result["serving"] = wait_for(serving, 120)
    result["health"] = request("GET", "/api/health")
    if not result["serving"]:
        problem(
            "Eluvian never reported frames: the plugins found no addresses, or the game did not start"
        )
    log = lines_of(loader_log)
    result["loader"] = [line[line.find("] ") + 2 :] for line in log if "] [" in line][
        :14
    ]
    text = "\n".join(log)
    if "address library:" not in text:
        problem("the loader read no address file")
    elif want_file not in text:
        problem(f"the loader did not read {want_file}")
    if by_code != ("DAOLib #110): that build's file is read" in text):
        problem(f"matched by code: expected {by_code}")
    if "plugins are not loaded" in text:
        problem("plugins are not loaded")
    for line in log:
        if "[error]" in line or "[critical]" in line:
            problem("loader.log: " + line.split("] ", 2)[-1])
    tb = "\n".join(lines_of(tb_log))
    result["turnbased_ready"] = "ready; turn-based fights begin when combat does" in tb
    if not result["turnbased_ready"]:
        problem("turnbased did not report ready")
    for line in tb.splitlines():
        if "[error]" in line:
            problem("turnbased.log: " + line.split("] ", 2)[-1])
    if result["serving"] and load_save:
        asked = request(
            "POST",
            "/api/tool/saves",
            {"action": "load", "character": "Lyna", "kind": "auto", "number": 3},
            timeout=60,
        )
        result["load_asked"] = asked
        time.sleep(30)
        health = request("GET", "/api/health")
        result["health_after_load"] = health
        if not health or health.get("mode") in (None, "none"):
            problem(
                f"after the load the game's mode is {health and health.get('mode')}"
            )
        hostile = request(
            "POST", "/api/tool/creatures", {"side": "hostile"}, timeout=30
        )
        result["hostile"] = hostile if hostile is None else str(hostile)[:200]
    result["ok"] = not result["problems"]
    kill_game()
    time.sleep(2)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("variants", nargs="*")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--no-load", action="store_true", help="do not load a save")
    parser.add_argument("--preset", default="debug")
    options = parser.parse_args()
    if options.list:
        for name, (install, exe, want, by_code) in VARIANTS.items():
            print(f"{name}: {install}, {exe.name}, reads {want}, by code: {by_code}")
        return 0
    names = options.variants or list(VARIANTS)
    for name in names:
        if name not in VARIANTS:
            sys.exit(f"no variant {name!r}: --list")
    if game_running():
        sys.exit("a DAOrigins.exe is running: end it first")
    build = REPO / "build" / options.preset
    results = []
    installs = {}
    try:
        for name in names:
            which = VARIANTS[name][0]
            if which not in installs:
                installs[which] = Install(which, build)
                installs[which].save()
            say(f"== {name}")
            result = try_variant(installs[which], name, not options.no_load)
            results.append(result)
            say(json.dumps(result, indent=1))
    finally:
        kill_game()
        for install in installs.values():
            install.restore()
            say(f"{install.name}: exe, loader and address files put back")
    summary = {r["variant"]: ("ok" if r["ok"] else r["problems"]) for r in results}
    say(json.dumps(summary, indent=1))
    out = REPO / "build" / "version_matrix" / "results.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(results, indent=1))
    return 0 if all(r["ok"] for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
