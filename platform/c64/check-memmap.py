#!/usr/bin/env python3
"""Post-link RAM checks for Magnetar BBS C64 core and bank overlays."""

import re
import sys

CORE_HIMEM = 0xB000
CORE_STACK = 0x0400
CORE_MIN_GAP = 256
BBS_SHARED_BASE = 0xA280

BANK_BASE = 0xB000
BANK_TOP = 0xD000
BANK_STACK = 0x0200
BANK_MIN_STACK_GAP = 48
BANK_MAX_BIN = 0x2000


def parse_segments(path, names):
    segs = {}
    pat = re.compile(
        r"^(" + "|".join(names) + r")\s+([0-9A-Fa-f]{6})\s+([0-9A-Fa-f]{6})\s+([0-9A-Fa-f]{6})"
    )
    with open(path, encoding="ascii", errors="replace") as f:
        for line in f:
            m = pat.match(line.strip())
            if not m:
                continue
            name = m.group(1)
            start = int(m.group(2), 16)
            end_ex = int(m.group(3), 16) + 1
            size = int(m.group(4), 16)
            segs[name] = (start, end_ex, size)
    return segs


def parse_symbol_addr(map_path, symbol):
    pat = re.compile(rf"^{re.escape(symbol)}\s+([0-9A-Fa-f]{{6}})\s+RL")
    with open(map_path, encoding="ascii", errors="replace") as f:
        for line in f:
            m = pat.match(line.strip())
            if m:
                return int(m.group(1), 16)
    return None


def check_core(map_path, bin_path):
    segs = parse_segments(map_path, ["BSS", "SHARED", "LOWBSS", "CODE", "DATA", "INIT"])
    bss = segs.get("BSS")
    if bss is None:
        raise SystemExit(f"{map_path}: missing BSS segment")
    bss_end = bss[1]
    stack_lo = CORE_HIMEM - CORE_STACK
    gap = stack_lo - bss_end
    if bss_end > stack_lo:
        raise SystemExit(
            f"core BSS ends ${bss_end - 1:04X}, overlaps stack at ${stack_lo:04X}"
        )
    if gap < CORE_MIN_GAP:
        raise SystemExit(
            f"core BSS/stack gap {gap} B < minimum {CORE_MIN_GAP} B "
            f"(BSS ends ${bss_end - 1:04X}, stack ${stack_lo:04X})"
        )
    if bss_end > CORE_HIMEM:
        raise SystemExit(f"core BSS ends ${bss_end - 1:04X}, above bank base ${CORE_HIMEM:04X}")

    shared = segs.get("SHARED")
    lowbss = parse_segments(map_path, ["LOWBSS"]).get("LOWBSS")
    if lowbss is not None and lowbss[1] > BBS_SHARED_BASE:
        raise SystemExit(
            f"LOWBSS ends ${lowbss[1] - 1:04X}, intrudes SHARED at ${BBS_SHARED_BASE:04X}"
        )
    if shared is None:
        raise SystemExit(f"{map_path}: missing SHARED segment")
    if shared[0] != BBS_SHARED_BASE:
        raise SystemExit(
            f"SHARED at ${shared[0]:04X}, expected fixed ABI ${BBS_SHARED_BASE:04X}"
        )
    if bss[0] < shared[1] and bss_end > shared[0]:
        raise SystemExit(
            f"BSS ${bss[0]:04X}-${bss_end - 1:04X} overlaps SHARED "
            f"${shared[0]:04X}-${shared[1] - 1:04X}"
        )

    main_hi = 0
    for name in ("CODE", "RODATA", "DATA", "INIT", "ONCE"):
        seg = segs.get(name)
        if seg is not None and seg[1] > main_hi:
            main_hi = seg[1]
    if main_hi > shared[0]:
        raise SystemExit(
            f"MAIN ends ${main_hi - 1:04X}, intrudes SHARED at ${shared[0]:04X}"
        )

    print(
        f"core OK: SHARED ${shared[0]:04X}-${shared[1] - 1:04X}, "
        f"BSS ${bss[0]:04X}-${bss_end - 1:04X}, "
        f"stack gap {gap} B, bank gap {CORE_HIMEM - bss_end} B"
    )


def check_bank(map_path, bin_path):
    segs = parse_segments(map_path, ["BSS", "CODE", "RODATA", "DATA", "BANKHDR"])
    hi = max(s[1] for s in segs.values()) if segs else BANK_BASE
    stack_lo = BANK_TOP - BANK_STACK
    if hi > stack_lo:
        raise SystemExit(
            f"{map_path}: linked image ends ${hi - 1:04X}, "
            f"intrudes stack region ${stack_lo:04X}-${BANK_TOP - 1:04X}"
        )
    stack_gap = stack_lo - hi
    if segs.get("BSS") and stack_gap < BANK_MIN_STACK_GAP:
        raise SystemExit(
            f"{map_path}: only {stack_gap} B between BSS and stack "
            f"(need {BANK_MIN_STACK_GAP} B)"
        )
    try:
        with open(bin_path, "rb") as f:
            bin_sz = len(f.read())
    except OSError as e:
        raise SystemExit(f"{bin_path}: {e}") from e
    if bin_sz > BANK_MAX_BIN:
        raise SystemExit(f"{bin_path}: {bin_sz} B exceeds bank load cap {BANK_MAX_BIN} B")
    print(
        f"bank OK: linked to ${hi - 1:04X}, stack gap {stack_gap} B, "
        f"bin {bin_sz} B ({BANK_MAX_BIN - bin_sz} B load headroom)"
    )


def main():
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} core|bank MAP BIN")
    kind, map_path, bin_path = sys.argv[1:4]
    if kind == "core":
        check_core(map_path, bin_path)
    elif kind == "bank":
        check_bank(map_path, bin_path)
    else:
        raise SystemExit(f"unknown kind {kind!r}")


if __name__ == "__main__":
    main()
