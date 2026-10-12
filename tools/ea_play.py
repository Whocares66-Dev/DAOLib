"""Start the EA build of the game with the loader from the build tree, with
nothing copied into the game: for a check in play before, or without,
tools/deploy.ps1 -Install ea (the EA app's folder gives the user no write
access until it is granted, dev/BUILDS.md).

    python tools/ea_play.py                  build\\debug's loader and plugins; the game is left running
    python tools/ea_play.py --for 45         the same, and the game is ended after 45 seconds
    python tools/ea_play.py --preset release
    python tools/ea_play.py --for 45 --when-free 20   wait first until no game has run for 20 s
    python tools/ea_play.py --examples resume,saves   with examples\\resume and examples\\saves too
    python tools/ea_play.py --for 120 --examples resume --force-load
                                             into the last save unattended, its missing content forced past

A copy started this way runs one at a time (dev/BUILDS.md), so this refuses
while any DAOrigins.exe is running, the GOG install's included, unless told
to wait.

What goes in is what tools/deploy.ps1 would deploy, staged in
build\\<preset>\\play: the loader, the plugins that are not examples (the
probe and example.dll), their PDBs, the address library's files, and
Ghidra's function names.

THIS STARTS THE GAME, through the EA app, which it opens if it is not open.
Say so before running it.

The exe is started as the EA app expects (it hands off to the EA app, which
starts it again, dev/BUILDS.md), and build\\<preset>\\dinput8.dll is loaded
into the run the EA app starts, as soon as that process has a kernel32 to
load it with: LoadLibraryW on a thread made in the game, the path written
into the game's memory first. That is more than a second before the wrapper
has decrypted the game's code, which this checks, since a loader that came
after would miss the game's start. From there everything is as deployed. The
loader looks for daolib\\plugins beside itself, so the build's plugins load,
and being named dinput8.dll it is the dinput8.dll the game's own imports
then bind to.

It does not show that the wrapper finds a dinput8.dll put beside the exe.
Only a deployed run does.
"""

import ctypes
import os
import shutil
import struct
import subprocess
import sys
import time
from ctypes import wintypes

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
# The address library's tools know the EA build's wrapper: how to find the
# run the EA app starts, and whether its code is still encrypted.
sys.path.insert(1, os.path.join(HERE, "..", "extern", "DAOAddressLibrary", "tools"))
import daoexe  # noqa: E402
import ea_dump  # noqa: E402

k32 = ea_dump.k32
psapi = ctypes.WinDLL("psapi", use_last_error=True)

# terminate, create thread, vm operation, vm read, vm write, query, wait
ACCESS = 0x0001 | 0x0002 | 0x0008 | 0x0010 | 0x0020 | 0x0400 | 0x00100000
LIST_MODULES_32BIT = 0x1
MEM_COMMIT_RESERVE = 0x3000
PAGE_READWRITE = 0x4
WAIT_SECONDS = 240
WAIT_FOR_FREE_SECONDS = 1500

psapi.EnumProcessModulesEx.argtypes = [
    wintypes.HANDLE,
    ctypes.POINTER(wintypes.HMODULE),
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    wintypes.DWORD,
]
psapi.GetModuleBaseNameW.argtypes = [
    wintypes.HANDLE,
    wintypes.HMODULE,
    wintypes.LPWSTR,
    wintypes.DWORD,
]
k32.VirtualAllocEx.restype = ctypes.c_void_p
k32.VirtualAllocEx.argtypes = [
    wintypes.HANDLE,
    ctypes.c_void_p,
    ctypes.c_size_t,
    wintypes.DWORD,
    wintypes.DWORD,
]
k32.WriteProcessMemory.argtypes = [
    wintypes.HANDLE,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
]
k32.CreateRemoteThread.restype = wintypes.HANDLE
k32.CreateRemoteThread.argtypes = [
    wintypes.HANDLE,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.c_void_p,
    ctypes.c_void_p,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
]
k32.GetExitCodeThread.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
psapi.GetMappedFileNameW.argtypes = [
    wintypes.HANDLE,
    ctypes.c_void_p,
    wintypes.LPWSTR,
    wintypes.DWORD,
]


def export_rva(dll, name):
    """The RVA of a function a DLL exports by name, read from the file."""
    img = daoexe.Image(dll)
    d = img.data
    o = img.offset(img.dirs[0][0])
    _, _, _, _, _, _, _, count, functions, names, ordinals = struct.unpack_from(
        "<IIHHIIIIIII", d, o
    )
    for i in range(count):
        if (
            img.cstring(struct.unpack_from("<I", d, img.offset(names) + 4 * i)[0])
            == name
        ):
            ordinal = struct.unpack_from("<H", d, img.offset(ordinals) + 2 * i)[0]
            return struct.unpack_from("<I", d, img.offset(functions) + 4 * ordinal)[0]
    raise KeyError(name)


def module_base(process, name):
    """Where a 32-bit module of the process is loaded, or None while the
    process has no module list yet: one just created has only ntdll."""
    modules = (wintypes.HMODULE * 512)()
    needed = wintypes.DWORD()
    if not psapi.EnumProcessModulesEx(
        process,
        modules,
        ctypes.sizeof(modules),
        ctypes.byref(needed),
        LIST_MODULES_32BIT,
    ):
        return None
    text = ctypes.create_unicode_buffer(260)
    for module in modules[: needed.value // ctypes.sizeof(wintypes.HMODULE)]:
        if (
            psapi.GetModuleBaseNameW(process, module, text, 260)
            and text.value.lower() == name
        ):
            return module
    return None


def load_library(process, dll):
    """LoadLibraryW(dll) on a thread of the process's own. The module's
    handle there, or 0 if the load failed."""
    system = os.path.join(os.environ["SystemRoot"], "SysWOW64", "kernel32.dll")
    rva = export_rva(system, "LoadLibraryW")
    deadline = time.monotonic() + 30
    while True:
        base = module_base(process, "kernel32.dll")
        if base:
            break
        if time.monotonic() > deadline:
            raise RuntimeError("the game has no kernel32 after 30 s")
    path = (dll + "\0").encode("utf-16-le")
    there = k32.VirtualAllocEx(
        process, None, len(path), MEM_COMMIT_RESERVE, PAGE_READWRITE
    )
    if not there or not k32.WriteProcessMemory(process, there, path, len(path), None):
        raise ctypes.WinError(ctypes.get_last_error())
    thread = k32.CreateRemoteThread(process, None, 0, base + rva, there, 0, None)
    if not thread:
        raise ctypes.WinError(ctypes.get_last_error())
    k32.WaitForSingleObject(thread, 30000)
    code = wintypes.DWORD()
    k32.GetExitCodeThread(thread, ctypes.byref(code))
    k32.CloseHandle(thread)
    return code.value


def stage(preset, examples=()):
    """build\\<preset>\\play: what tools/deploy.ps1 would put in the game, the
    loader, the plugins that are not examples and the examples named, their
    PDBs and Ghidra's function names. The path of the loader there, or None
    with no build."""
    build = os.path.normpath(os.path.join(HERE, "..", "build", preset))
    if not os.path.exists(os.path.join(build, "dinput8.dll")):
        return None
    play = os.path.join(build, "play")
    shutil.rmtree(play, ignore_errors=True)
    os.makedirs(os.path.join(play, "daolib", "plugins"))
    for name in ("dinput8.dll", "dinput8.pdb"):
        if os.path.exists(os.path.join(build, name)):
            shutil.copy2(os.path.join(build, name), play)
    plugins = os.path.join(build, "daolib", "plugins")
    for name in sorted(os.listdir(plugins)):
        stem = os.path.splitext(name)[0]
        wanted = not stem.startswith("example_") or stem[len("example_") :] in examples
        if name.endswith((".dll", ".pdb")) and wanted:
            shutil.copy2(
                os.path.join(plugins, name), os.path.join(play, "daolib", "plugins")
            )
    functions = os.path.join(HERE, "..", "game", "ghidra", "functions.tsv")
    if os.path.exists(functions):
        shutil.copy2(functions, os.path.join(play, "daolib"))
    # The address library's files, one per build of the game: without the
    # one for the running build the loader loads nothing.
    shutil.copytree(
        os.path.join(build, "daolib", "addresses"),
        os.path.join(play, "daolib", "addresses"),
    )
    return os.path.join(play, "dinput8.dll")


def end_children(parent, name):
    """End the processes of that name the parent started: the wrapper's
    ActivationUI.exe outlives a game that is ended (seen 2026-10-01)."""
    snapshot = k32.CreateToolhelp32Snapshot(ea_dump.TH32CS_SNAPPROCESS, 0)
    entry = ea_dump.PROCESSENTRY32W()
    entry.dwSize = ctypes.sizeof(entry)
    more = k32.Process32FirstW(snapshot, ctypes.byref(entry))
    while more:
        if entry.th32ParentProcessID == parent and entry.szExeFile.lower() == name:
            child = k32.OpenProcess(0x0001, False, entry.th32ProcessID)
            if child:
                k32.TerminateProcess(child, 0)
                k32.CloseHandle(child)
        more = k32.Process32NextW(snapshot, ctypes.byref(entry))
    k32.CloseHandle(snapshot)


def games_running():
    """The process ids of every DAOrigins.exe, of any install."""
    snapshot = k32.CreateToolhelp32Snapshot(ea_dump.TH32CS_SNAPPROCESS, 0)
    entry = ea_dump.PROCESSENTRY32W()
    entry.dwSize = ctypes.sizeof(entry)
    out = []
    more = k32.Process32FirstW(snapshot, ctypes.byref(entry))
    while more:
        if entry.szExeFile.lower() == "daorigins.exe":
            out.append(entry.th32ProcessID)
        more = k32.Process32NextW(snapshot, ctypes.byref(entry))
    k32.CloseHandle(snapshot)
    return out


def main(argv):
    preset = argv[argv.index("--preset") + 1] if "--preset" in argv else "debug"
    seconds = float(argv[argv.index("--for") + 1]) if "--for" in argv else None
    # The game runs one at a time: a second DAOrigins.exe, of either install,
    # exits within seconds of starting (0xffffffff, no crash; seen 2026-10-01
    # with the GOG game up), and the one already running is someone's.
    already = games_running()
    if already and "--when-free" in argv:
        # Another session's tests start and end the game every few minutes;
        # a gap this long is not the moment between its deploy and its start.
        quiet = float(argv[argv.index("--when-free") + 1])
        print(f"waiting for no game to have run for {quiet:g} s", flush=True)
        give_up = time.monotonic() + WAIT_FOR_FREE_SECONDS
        free_since = None
        while free_since is None or time.monotonic() - free_since < quiet:
            if time.monotonic() > give_up:
                print(
                    f"a game was running for {WAIT_FOR_FREE_SECONDS} s; not started",
                    file=sys.stderr,
                )
                return 2
            if games_running():
                free_since = None
            elif free_since is None:
                free_since = time.monotonic()
            time.sleep(1)
    elif already:
        print(
            f"DAOrigins.exe is already running (pid {already[0]}); the game runs one at a time",
            file=sys.stderr,
        )
        return 2
    examples = (
        argv[argv.index("--examples") + 1].split(",") if "--examples" in argv else ()
    )
    loader = stage(preset, examples)
    if not loader:
        print(
            f"no build\\{preset}\\dinput8.dll; run tools\\build.ps1 -Preset {preset} first",
            file=sys.stderr,
        )
        return 2
    exe = os.path.join(ea_dump.INSTALL, "daorigins.exe")
    watch = ea_dump.probes(daoexe.Image(exe))
    started = time.monotonic()
    marker = os.path.join(log_directory(), "example_resume.force")
    if "--force-load" in argv:
        # examples\resume answers the game's "requires the following content"
        # with Force Load only where this file is (its header says why).
        os.makedirs(os.path.dirname(marker), exist_ok=True)
        with open(marker, "w"):
            pass
    try:
        return play(exe, loader, watch, seconds, started)
    finally:
        if seconds is not None and os.path.exists(marker):
            os.remove(marker)


def log_directory():
    """Documents\\BioWare\\Dragon Age\\daolib, wherever Documents is (under
    OneDrive on this machine): dao::LogDirectory's answer."""
    documents = ctypes.create_unicode_buffer(260)
    ctypes.windll.shell32.SHGetFolderPathW(None, 5, None, 0, documents)
    return os.path.join(documents.value, "BioWare", "Dragon Age", "daolib")


def play(exe, loader, watch, seconds, started):
    first = subprocess.Popen([exe], cwd=ea_dump.INSTALL)
    print(f"started {exe}, pid {first.pid}; it hands off to the EA app")
    deadline = time.monotonic() + WAIT_SECONDS
    while True:
        found = ea_dump.running(exe, ACCESS, apart_from=(first.pid,))
        if found:
            break
        if time.monotonic() > deadline:
            print(
                f"the EA app did not start the game in {WAIT_SECONDS} s: is it signed in?",
                file=sys.stderr,
            )
            return 1
    process, pid = found
    print(f"the EA app started pid {pid}")
    handle = load_library(process, loader)
    # Read after the load: still the file's bytes means the loader was there
    # before the wrapper decrypted, with the game's start still to come.
    encrypted = [
        ea_dump.read(process, va, len(on_disk)) == on_disk for va, on_disk in watch
    ]
    if not handle:
        print("LoadLibraryW failed in the game", file=sys.stderr)
        k32.TerminateProcess(process, 1)
        return 1
    print(
        f"loader in at {handle:#x}; the game's code was {'still encrypted' if all(encrypted) else 'ALREADY DECRYPTED'}"
    )
    if not all(encrypted):
        print("too late for the game's start; ending the game", file=sys.stderr)
        k32.TerminateProcess(process, 1)
        return 1
    if seconds is None:
        print("the game is left running")
        return 0
    if k32.WaitForSingleObject(process, int(seconds * 1000)) == 0:
        code = wintypes.DWORD()
        k32.GetExitCodeProcess(process, ctypes.byref(code))
        print(
            f"the game exited by itself with {code.value:#x}, "
            f"{time.monotonic() - started:.1f} s after this started"
        )
        return 1
    served = loader_import(process, exe)
    if served:
        print(
            f"the game's own DINPUT8.dll imports go to {served[0]:#x}, in {served[1]}"
        )
    k32.TerminateProcess(process, 0)
    end_children(pid, "activationui.exe")
    print(f"ended the game after {seconds:g} s")
    return 0


def loader_import(process, exe):
    """(address, the file it is in) of the first function the game imports
    from DINPUT8.dll, as the wrapper resolved it. In the loader's file, the
    wrapper found DINPUT8.dll by its name, as the system's loader does, and
    took the one already there; that is the lookup which finds a dinput8.dll
    beside the exe first when none is loaded. None if it cannot be read."""
    img = daoexe.Image(exe)
    o = img.offset(ea_dump.original_imports(img)[0])
    while True:
        _, _, _, name, iat = struct.unpack_from("<IIIII", img.data, o)
        if name == 0:
            return None
        if img.cstring(name).lower() == "dinput8.dll":
            break
        o += 20
    slot = ea_dump.read(process, img.image_base + iat, 4)
    if not slot or len(slot) != 4:
        return None
    target = struct.unpack("<I", slot)[0]
    path = ctypes.create_unicode_buffer(1024)
    psapi.GetMappedFileNameW(process, target, path, 1024)
    return target, path.value


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
