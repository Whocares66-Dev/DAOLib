"""Read a probe session: what the game did, and which functions it ran when.

    python tools/probe_report.py timeline
    python tools/probe_report.py pauses
    python tools/probe_report.py calls FROM-TO [--top N]
    python tools/probe_report.py compare FROM-TO FROM-TO [--top N] [--update]

Times are seconds since the probe loaded, as the timeline prints them.

- timeline: every ten seconds, the pause state and game mode, how many
  script commands and message handlers ran, the busiest of each, and the
  thread samples in the exe.
- pauses: every SetPaused call from probe.log, with the function its
  caller is in.
- calls: the script commands and handlers that ran in a stretch, most first.
- compare: the functions on the game's stacks in the first stretch and not
  in the second, by how much more often: run with fighting first and
  exploring second, and the top of the list is the combat code.

It reads the session from the probe's folder (dao/Log.h:
Documents\\BioWare\\Dragon Age\\daolib, or --logs) and the function list
Ghidra exported to game/ghidra/functions.tsv (dev/PROBE.md says how to
make it).
"""

import argparse
import bisect
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FUNCTIONS = os.path.join(HERE, "..", "game", "ghidra", "functions.tsv")


def default_logs():
    home = os.path.expanduser("~")
    for docs in (
        os.path.join(home, "OneDrive", "Documents"),
        os.path.join(home, "Documents"),
    ):
        path = os.path.join(docs, "BioWare", "Dragon Age", "daolib")
        if os.path.isdir(path):
            return path
    return os.path.join(home, "Documents", "BioWare", "Dragon Age", "daolib")


class Functions:
    """Address -> the Ghidra function containing it."""

    def __init__(self, path=FUNCTIONS):
        self.entries, self.ends, self.names = [], [], []
        if not os.path.exists(path):
            return
        rows = []
        with open(path, encoding="utf-8") as f:
            for line in f:
                if line.startswith("#"):
                    continue
                entry, end, name = line.rstrip("\n").split("\t", 2)
                rows.append((int(entry, 16), int(end, 16), name))
        rows.sort()
        for entry, end, name in rows:
            self.entries.append(entry)
            self.ends.append(end)
            self.names.append(name)

    def containing(self, address):
        i = bisect.bisect_right(self.entries, address) - 1
        if i >= 0 and address <= self.ends[i]:
            return self.entries[i], self.names[i]
        return None, f"?{address:#010x}"


def parse_range(text):
    lo, hi = text.split("-")
    return float(lo) * 1000, float(hi) * 1000


def parse_ranges(text):
    """ "1930-1940,3460-3480" -> [(ms, ms), ...]: several stretches as one."""
    return [parse_range(part) for part in text.split(",")]


def in_ranges(t0, t1, ranges):
    """Whether a sample window belongs to the stretches: its middle is in one.
    Windows start a few milliseconds late each time, so a window never quite
    fits a stretch typed in whole seconds."""
    middle = (t0 + t1) / 2
    return any(lo <= middle <= hi for lo, hi in ranges)


def read_calls(logs):
    """([(ms, paused, mode, second)], [(ms, address, delta, label)])."""
    status, calls = [], []
    with open(os.path.join(logs, "probe-calls.tsv"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            parts = line.rstrip("\n").split("\t")
            if parts[1] == "status":
                status.append(
                    (int(parts[0]), int(parts[2]), int(parts[3]), int(parts[4]))
                )
            elif parts[1] == "call":
                calls.append(
                    (int(parts[0]), int(parts[2], 16), int(parts[3]), parts[4])
                )
    return status, calls


def read_samples(logs):
    """{(from, to): {"samples": n, "thread": {tid: n}, "eip": {a: n}, "stack": {a: n}}}"""
    windows = collections.OrderedDict()
    with open(os.path.join(logs, "probe-samples.tsv"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            t0, t1, kind, key, n = line.rstrip("\n").split("\t")
            w = windows.setdefault(
                (int(t0), int(t1)),
                {
                    "samples": 0,
                    "usamples": 0,
                    "thread": {},
                    "eip": {},
                    "stack": {},
                    "ueip": {},
                    "ustack": {},
                },
            )
            if kind in ("samples", "usamples"):
                w[kind] = int(n)
            elif kind == "thread":
                w["thread"][int(key)] = int(n)
            else:
                w[kind][int(key, 16)] = int(n)
    return windows


def kind_of(label):
    return "cmd" if label.startswith("COMMAND_") else "msg"


def short(label):
    first = label.split(" | ")[0]
    return first.removeprefix("COMMAND_")


def timeline(logs, top):
    status, calls = read_calls(logs)
    windows = (
        read_samples(logs)
        if os.path.exists(os.path.join(logs, "probe-samples.tsv"))
        else {}
    )
    by_window = collections.defaultdict(collections.Counter)
    for ms, _, delta, label in calls:
        by_window[ms // 10000][label] += delta
    state = {}
    for ms, paused, mode, second in status:
        state.setdefault(ms // 10000, (paused, mode, second))
    samples = {t0 // 10000: w for (t0, _), w in windows.items()}
    print(
        f"{'from s':>7} {'paused':>6} {'mode':>4}  {'cmds':>6} {'msgs':>5} {'in exe':>6}  busiest"
    )
    for w in sorted(set(by_window) | set(state)):
        counter = by_window[w]
        cmds = sum(n for label, n in counter.items() if kind_of(label) == "cmd")
        msgs = sum(n for label, n in counter.items() if kind_of(label) == "msg")
        paused, mode, _ = state.get(w, (-1, -1, -1))
        in_exe = sum(samples[w]["thread"].values()) if w in samples else 0
        busiest = ", ".join(
            f"{short(label)} {n}" for label, n in counter.most_common(top)
        )
        print(
            f"{w * 10:>7} {paused:>6} {mode:>4}  {cmds:>6} {msgs:>5} {in_exe:>6}  {busiest}"
        )


def pauses(logs, functions):
    pattern = re.compile(
        r"^\[(?P<time>[\d:.]+)\].*SetPaused\((?P<paused>\w+), (?P<second>\w+)\) from (?P<caller>0x[0-9a-f]+):"
        r" paused (?P<before>\w+) -> (?P<after>\w+)"
    )
    keys = re.compile(r"^\[(?P<time>[\d:.]+)\].*pause key")
    with open(os.path.join(logs, "probe.log"), encoding="utf-8") as f:
        for line in f:
            m = pattern.match(line)
            if m:
                entry, name = functions.containing(int(m["caller"], 16))
                where = f"{name} @ {entry:#010x}" if entry else name
                print(
                    f"{m['time']}  SetPaused({m['paused']}, {m['second']})  {m['before']} -> {m['after']}"
                    f"  from {m['caller']} in {where}"
                )
            elif keys.match(line):
                print(f"{keys.match(line)['time']}  pause key")


def calls_in(logs, span, top):
    _, calls = read_calls(logs)
    lo, hi = span
    counter = collections.Counter()
    labels = {}
    for ms, address, delta, label in calls:
        if lo <= ms <= hi:
            counter[address] += delta
            labels[address] = label
    for address, n in counter.most_common(top):
        print(f"{n:>8}  {address:#010x}  {labels[address]}")


def compare(logs, functions, first, second, top, update=False):
    """With update, only the samples taken with the game's update on the
    stack (the probe's u-rows): the game's logic, not every thread's idling."""
    windows = read_samples(logs)
    prefix = "u" if update else ""

    def presence(ranges):
        per_function = collections.Counter()
        total = 0
        for (t0, t1), w in windows.items():
            if in_ranges(t0, t1, ranges):
                total += w[prefix + "samples"]
                seen = collections.Counter()
                for address, n in list(w[prefix + "stack"].items()) + list(
                    w[prefix + "eip"].items()
                ):
                    entry, _ = functions.containing(address)
                    if entry is not None:
                        seen[entry] = max(seen[entry], n)
                per_function.update(seen)
        return per_function, total

    a, total_a = presence(first)
    b, total_b = presence(second)
    if not total_a or not total_b:
        print(
            "no ten-second window has its middle inside one of the stretches; widen them",
            file=sys.stderr,
        )
        return 1
    rows = []
    for entry, n in a.items():
        share_a = n / total_a
        share_b = b.get(entry, 0) / total_b
        rows.append((share_a - share_b, share_a, share_b, entry))
    rows.sort(reverse=True)
    print(f"{total_a} samples in the first stretch, {total_b} in the second")
    print(f"{'first':>7} {'second':>7}  function")
    for _, share_a, share_b, entry in rows[:top]:
        _, name = functions.containing(entry)
        print(f"{share_a:>7.2%} {share_b:>7.2%}  {entry:#010x}  {name}")
    return 0


def new_step():
    return {
        "samples": 0,
        "outside": 0,
        "leaf": collections.Counter(),
        "inner": collections.Counter(),
        "stack": collections.Counter(),
        "module": collections.Counter(),
        "via": collections.Counter(),
        "chain": collections.Counter(),
    }


def read_loading(logs):
    """probe-loading.tsv as a list of loading screens, each a dict: its
    number, span in ms and samples, the recorder's health over it, the
    processor time by thread with the thread's name, the loading thread's
    samples by (state, step, inside) under "steps", and each watched
    thread's under "threads" by its name. src/plugins/probe/Recorder.h has
    the rows."""
    screens = {}
    path = os.path.join(logs, "probe-loading.tsv")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            n, kind, *rest = line.rstrip("\n").split("\t")
            screen = screens.setdefault(
                int(n),
                {
                    "n": int(n),
                    "from": 0,
                    "to": 0,
                    "samples": 0,
                    "health": None,
                    "cpu": {},
                    "steps": {},
                    "threads": {},
                },
            )
            if kind == "screen":
                screen["from"], screen["to"], screen["samples"] = (int(x) for x in rest)
            elif kind == "health":
                screen["health"] = tuple(int(x) for x in rest)
            elif kind == "cpu":
                screen["cpu"][int(rest[0])] = (
                    int(rest[1]),
                    rest[2] == "1",
                    rest[3] if len(rest) > 3 else "?",
                )
            else:
                # A step's rows: the loading thread's or, with a thread's name and a dot
                # before the kind, a watched thread's.
                name, _, kind = kind.rpartition(".")
                steps = (
                    screen["threads"].setdefault(name, {}) if name else screen["steps"]
                )
                key = (int(rest[0]), int(rest[1]), rest[2] == "1")
                step = steps.setdefault(key, new_step())
                if kind == "step":
                    step["samples"], step["outside"] = int(rest[3]), int(rest[4])
                elif kind in ("module", "via"):
                    step[kind][rest[3]] += int(rest[4])
                elif kind == "chain":
                    step["chain"][tuple(int(a, 16) for a in rest[3].split(","))] += int(
                        rest[4]
                    )
                else:
                    step[kind][int(rest[3], 16)] += int(rest[4])
    return [screens[n] for n in sorted(screens)]


def by_function(functions, counts, once=False):
    """Counts by address as counts by the function holding each. With
    `once`, a function's count is its busiest address's, not their sum: a
    sample's stack can hold two returns into one function, and a function
    is under a sample once."""
    out = collections.Counter()
    for address, count in counts.items():
        name = functions.containing(address)[1]
        out[name] = max(out[name], count) if once else out[name] + count
    return out


# The game's functions that only wait, by their entries in GOG's build: a
# sample that left the exe from one is a thread with nothing to do.
# WaitForSingleObject's two wrappers and Sleep's (dao/Addresses.h, "The
# engine's threads").
WAITS = {0x004E6120, 0x004E6500, 0x004E6170}


def gather(steps_of_screens):
    """The steps of several screens as one, by (state, step): the samples
    inside the update and between two, and the counts summed."""
    steps = {}
    for screen_steps in steps_of_screens:
        for key, step in screen_steps.items():
            mine = steps.setdefault(
                key[:2],
                {
                    "inside": 0,
                    "between": 0,
                    "self": collections.Counter(),
                    **new_step(),
                },
            )
            mine["inside" if key[2] else "between"] += step["samples"]
            mine["outside"] += step["outside"]
            for kind in ("inner", "stack", "module", "via", "chain"):
                mine[kind].update(step[kind])
            mine["self"].update(step["leaf"])
            mine["self"].update(step["inner"])
    return steps


def waiting(functions, mine):
    """The samples of a step taken with the thread in one of the game's waits."""
    return sum(
        count
        for address, count in mine["inner"].items()
        if functions.containing(address)[0] in WAITS
    )


def print_step(functions, mine, samples, top):
    """A step's functions: in, the modules outside, and the paths seen most."""
    print("  in (the leaf, or the game's function that called out):")
    for name, count in by_function(functions, mine["self"]).most_common(top):
        print(f"    {100 * count / samples:5.1f}%  {name}")
    if mine["module"]:
        where = ", ".join(
            f"{name} {100 * count / samples:.0f}%"
            for name, count in mine["module"].most_common(8)
        )
        print(f"  outside the game's code, in: {where}")
    if mine["via"]:
        print("  outside the game's code, called through (from the top of the stack):")
        for path, count in mine["via"].most_common(6):
            # The game's own callers are addresses; the system's are names already.
            places = (
                functions.containing(int(p, 16))[1] if p.startswith("0x") else p
                for p in path.split(" < ")
            )
            print(f"    {100 * count / samples:5.1f}%  {' < '.join(places)}")
    if mine["chain"]:
        print("  paths seen most (innermost caller first):")
        for chain, count in mine["chain"].most_common(4):
            path = " < ".join(functions.containing(a)[1] for a in chain if a)
            print(
                f"    {100 * count / samples:5.1f}%  {path or '(no caller in the game)'}"
            )


def print_screen(s):
    """A screen's line: its time, the processor time by thread name, the recorder's health."""
    by_name = collections.Counter()
    for ms, loading_thread, name in s["cpu"].values():
        by_name["its thread" if loading_thread else name] += ms
    used = ", ".join(f"{name} {ms}" for name, ms in by_name.most_common(6))
    print(f"  screen {s['n']}: {s['to'] - s['from']} ms; processor time, ms: {used}")
    if s["health"]:
        turns, gap, not_suspended, no_context, not_found = s["health"][:5]
        print(
            f"    the recorder: {turns} turns, the longest gap {gap} ms; suspend failed {not_suspended}, "
            f"no context {no_context}, thread not known {not_found}"
        )


def loading(logs, functions, top, only=None):
    """Where the loading thread was, step by step: the functions it was in
    (its leaf, or the game's function that had called out of the exe), the
    modules it was in outside the game's code, the paths of callers seen
    most, and the functions it was under; then, for each of the engine's
    threads watched with it, how much of each step it spent waiting and
    where it was. Over every loading screen, or the ones asked for."""
    screens = [s for s in read_loading(logs) if not only or s["n"] in only]
    if not screens:
        print("no loading screens in probe-loading.tsv")
        return 1
    total_ms = sum(s["to"] - s["from"] for s in screens)
    total = sum(s["samples"] for s in screens) or 1
    per_sample = total_ms / total
    print(
        f"{len(screens)} loading screen(s), {total_ms} ms, {total} samples of the loading thread "
        f"({per_sample:.2f} ms a sample)"
    )
    for s in screens:
        print_screen(s)
    steps = gather(s["steps"] for s in screens)
    ranked = sorted(steps.items(), key=lambda kv: -(kv[1]["inside"] + kv[1]["between"]))
    for (state, step), mine in ranked:
        samples = mine["inside"] + mine["between"]
        if samples * 100 < total:
            continue
        print(
            f"\nstate {state} step {step}: {samples} samples, about {samples * per_sample:.0f} ms "
            f"({100 * samples / total:.0f}% of all); {mine['inside']} inside the update, {mine['between']} between; "
            f"{mine['outside']} outside the game's code, {waiting(functions, mine)} in a wait"
        )
        print_step(functions, mine, samples, top)
        print("  under (on the stack; a stale word counts too):")
        for name, count in by_function(functions, mine["stack"], once=True).most_common(
            top
        ):
            print(f"    {100 * count / samples:5.1f}%  {name}")
    for name in sorted({name for s in screens for name in s["threads"]}):
        theirs = gather(s["threads"].get(name, {}) for s in screens)
        print(
            f"\n{name}, sampled with the loading thread, by the loading thread's step:"
        )
        for (state, step), _ in ranked:
            mine = theirs.get((state, step))
            samples = mine["inside"] + mine["between"] if mine else 0
            if samples * 100 < total:
                continue
            idle = waiting(functions, mine)
            print(
                f"\n  while state {state} step {step}: {samples} samples, {100 * idle / samples:.0f}% in a wait, "
                f"{mine['outside']} outside the game's code"
            )
            print_step(functions, mine, samples, top)
    return 0


def main(argv):
    # --logs and --top go after the command, where they are naturally typed.
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--logs", default=default_logs())
    common.add_argument("--top", type=int, default=5)
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("timeline", parents=[common])
    sub.add_parser("pauses", parents=[common])
    p = sub.add_parser("calls", parents=[common])
    p.add_argument("span")
    p = sub.add_parser("loading", parents=[common])
    p.add_argument(
        "--screen",
        type=int,
        action="append",
        help="only this loading screen; may be given again",
    )
    p.add_argument(
        "--functions",
        default=FUNCTIONS,
        help="Ghidra's function list, if not this checkout's",
    )
    p = sub.add_parser("compare", parents=[common])
    p.add_argument("first")
    p.add_argument("second")
    p.add_argument(
        "--update",
        action="store_true",
        help="only samples with the game's update on the stack (probes from 2026-09-30 on)",
    )
    args = parser.parse_args(argv)

    if args.command == "timeline":
        timeline(args.logs, args.top)
        return 0
    functions = Functions(getattr(args, "functions", FUNCTIONS))
    if not functions.entries:
        print(
            f"no {os.path.normpath(FUNCTIONS)}: export it from Ghidra first (dev/PROBE.md)",
            file=sys.stderr,
        )
    if args.command == "pauses":
        pauses(args.logs, functions)
        return 0
    if args.command == "loading":
        return loading(args.logs, functions, max(args.top, 12), args.screen)
    if args.command == "calls":
        calls_in(args.logs, parse_range(args.span), max(args.top, 20))
        return 0
    return compare(
        args.logs,
        functions,
        parse_ranges(args.first),
        parse_ranges(args.second),
        max(args.top, 40),
        args.update,
    )


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
