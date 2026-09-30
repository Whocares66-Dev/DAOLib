"""Read a probe session: what the game did, and which functions it ran when.

    python tools/probe_report.py timeline
    python tools/probe_report.py pauses
    python tools/probe_report.py calls FROM-TO [--top N]
    python tools/probe_report.py compare FROM-TO FROM-TO [--top N]

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
                (int(t0), int(t1)), {"samples": 0, "thread": {}, "eip": {}, "stack": {}}
            )
            if kind == "samples":
                w["samples"] = int(n)
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


def compare(logs, functions, first, second, top):
    windows = read_samples(logs)

    def presence(ranges):
        per_function = collections.Counter()
        total = 0
        for (t0, t1), w in windows.items():
            if in_ranges(t0, t1, ranges):
                total += w["samples"]
                seen = collections.Counter()
                for address, n in list(w["stack"].items()) + list(w["eip"].items()):
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
    p = sub.add_parser("compare", parents=[common])
    p.add_argument("first")
    p.add_argument("second")
    args = parser.parse_args(argv)

    if args.command == "timeline":
        timeline(args.logs, args.top)
        return 0
    functions = Functions()
    if not functions.entries:
        print(
            f"no {os.path.normpath(FUNCTIONS)}: export it from Ghidra first (dev/PROBE.md)",
            file=sys.stderr,
        )
    if args.command == "pauses":
        pauses(args.logs, functions)
        return 0
    if args.command == "calls":
        calls_in(args.logs, parse_range(args.span), max(args.top, 20))
        return 0
    return compare(
        args.logs,
        functions,
        parse_ranges(args.first),
        parse_ranges(args.second),
        max(args.top, 40),
    )


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
