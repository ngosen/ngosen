#!/usr/bin/env python3
"""Compare the per-key output of the Go and Rust runners.

Usage: compare.py CASES GO_OUT RUST_OUT [--diffs FILE]
"""

import argparse
import collections

COLUMNS = ["op", "shown", "committed", "text", "raw", "lower", "valid", "valid_full"]
# What the user sees: the preedit after every key, and what a space commits at the end.
SHOWN, COMMITTED = 1, 2


def load(path):
    rows = collections.defaultdict(list)
    with open(path, encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            rows[parts[0]].append(parts[2:])
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cases")
    ap.add_argument("go")
    ap.add_argument("rust")
    ap.add_argument("--diffs", help="write every differing case here")
    ap.add_argument("--samples", type=int, default=3)
    args = ap.parse_args()

    cases = {}
    with open(args.cases, encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            cases[parts[0]] = parts[1:]
    go, rust = load(args.go), load(args.rust)

    total = collections.Counter()
    differ = collections.Counter()
    by_column = collections.Counter()
    samples = collections.defaultdict(list)
    diff_out = open(args.diffs, "w", encoding="utf-8") if args.diffs else None

    for cid, (im, modern, free, w2u, keys) in cases.items():
        kind = cid.rsplit("-", 1)[0]
        total[kind] += 1
        g, r = go.get(cid, []), rust.get(cid, [])
        first = None
        for step, (gr, rr) in enumerate(zip(g, r), 1):
            if gr != rr:
                col = next(c for c, a, b in zip(COLUMNS, gr, rr) if a != b)
                first = (step, col, gr, rr)
                break
        if first is None and len(g) != len(r):
            first = (min(len(g), len(r)) + 1, "rows", [], [])
        if first is None:
            continue
        if len(g) != len(r) or g[-1][COMMITTED] != r[-1][COMMITTED]:
            level = "final"
        elif any(a[SHOWN] != b[SHOWN] for a, b in zip(g, r)):
            level = "midword"
        else:
            level = "internal"
        differ[(kind, level)] += 1
        step, col, gr, rr = first
        by_column[col] += 1
        line = f"{cid}\t{im} modern={modern} free={free}\t{keys}\tstep {step} {col}\tgo={gr}\trust={rr}"
        if len(samples[(kind, level, col)]) < args.samples:
            samples[(kind, level, col)].append(line)
        if diff_out:
            diff_out.write(f"{level}\t{line}\n")

    levels = ("final", "midword", "internal")
    sums = {lv: sum(n for (_, level), n in differ.items() if level == lv) for lv in levels}
    print(f"cases: {sum(total.values())}, " + ", ".join(f"{lv}: {n}" for lv, n in sums.items()))
    print()
    print("final = the committed word differs; midword = only the preedit while typing differs;")
    print("internal = only engine state the user does not see differs.")
    print()
    print("| kind | cases | final | midword | internal |")
    print("| --- | --- | --- | --- | --- |")
    for kind in sorted(total):
        counts = " | ".join(str(differ[(kind, lv)]) for lv in levels)
        print(f"| {kind} | {total[kind]} | {counts} |")
    print()
    print("First differing column:", dict(by_column))
    for (kind, level, col), lines in sorted(samples.items()):
        print(f"\n{kind} / {level} / {col}:")
        for line in lines:
            print("  " + line)


if __name__ == "__main__":
    main()
