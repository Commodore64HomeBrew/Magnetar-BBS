#!/usr/bin/env python3
"""Post-link RAM checks for Magnetar BBS C64 core and bank overlays."""

import re
import sys

# Core (c64-bbs-core.cfg, bbs-bank.h, bbs-defs.h)
CORE_HIMEM = 0xB000
CORE_STACK = 0x0400
CORE_MIN_GAP = 256
BBS_SHARED_BASE = 0xA280
BBS_SHARED_SIZE = 0x368

# Banks (c64-bbs-bank.cfg)
BBS_BANK_BASE = 0xB000
BANK_TOP = 0xD000
BANK_STACK = 0x0200
BANK_RAM_SIZE = BANK_TOP - BANK_STACK - BBS_BANK_BASE
BANK_MIN_STACK_GAP = 48

# Screen RAM (bbs-defs.h) — runtime only, not linker segments
SCR_BASE = 0x0400
SCR_LAST = 0x07E7
XMODEM_RBUF_SIZE = 132
XFER_RX_SIZE = 128


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


def check_screen_layout():
    rbuf_base = SCR_LAST + 1 - XMODEM_RBUF_SIZE
    tx_base = SCR_BASE + XFER_RX_SIZE
    tx_size = rbuf_base - tx_base
    if rbuf_base + XMODEM_RBUF_SIZE - 1 != SCR_LAST:
        raise SystemExit("screen xmodem rbuf does not end at SCR_LAST")
    if tx_base != SCR_BASE + XFER_RX_SIZE:
        raise SystemExit("xfer TX base mismatch")
    if tx_size <= 0:
        raise SystemExit("xfer TX size non-positive")


def check_core(map_path, bin_path):
    check_screen_layout()

    segs = parse_segments(map_path, ["BSS", "SHARED", "LOWBSS", "CODE", "DATA", "INIT", "ONCE"])
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

    if bss[0] != BBS_SHARED_BASE + BBS_SHARED_SIZE:
        raise SystemExit(
            f"BSSHI starts ${bss[0]:04X}, expected ${BBS_SHARED_BASE + BBS_SHARED_SIZE:04X}"
        )

    shared = segs.get("SHARED")
    lowbss = segs.get("LOWBSS")
    if lowbss is not None:
        if lowbss[1] > BBS_SHARED_BASE:
            raise SystemExit(
                f"LOWBSS ends ${lowbss[1] - 1:04X}, intrudes SHARED at ${BBS_SHARED_BASE:04X}"
            )
        once = segs.get("ONCE")
        if once is not None and lowbss[0] < once[1]:
            raise SystemExit(
                f"LOWBSS ${lowbss[0]:04X} overlaps ONCE ending ${once[1] - 1:04X}"
            )
    if shared is None:
        raise SystemExit(f"{map_path}: missing SHARED segment")
    if shared[0] != BBS_SHARED_BASE:
        raise SystemExit(
            f"SHARED at ${shared[0]:04X}, expected fixed ABI ${BBS_SHARED_BASE:04X}"
        )
    if shared[2] != BBS_SHARED_SIZE:
        raise SystemExit(
            f"SHARED size {shared[2]} B, expected {BBS_SHARED_SIZE} B (bbs-shared-reserve.S)"
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

    once = segs.get("ONCE")
    once_end = once[1] if once else 0
    lowbss_note = ""
    if lowbss is not None:
        lowbss_note = f", LOWBSS ${lowbss[0]:04X}-${lowbss[1] - 1:04X}"
    elif once_end > 0 and once_end < BBS_SHARED_BASE:
        lowbss_note = f", gap ONCE..SHARED {BBS_SHARED_BASE - once_end} B"

    print(
        f"core OK: SHARED ${shared[0]:04X}-${shared[1] - 1:04X}, "
        f"BSS ${bss[0]:04X}-${bss_end - 1:04X}, "
        f"stack gap {gap} B, bank base ${CORE_HIMEM:04X}{lowbss_note}"
    )


def check_bank(map_path, bin_path):
    if BBS_BANK_BASE != CORE_HIMEM:
        raise SystemExit("BBS_BANK_BASE must equal core __HIMEM__")

    segs = parse_segments(map_path, ["BSS", "CODE", "RODATA", "DATA", "BANKHDR"])
    hi = max(s[1] for s in segs.values()) if segs else BBS_BANK_BASE
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
    bankhdr = segs.get("BANKHDR")
    if bankhdr is not None and bankhdr[0] != BBS_BANK_BASE:
        raise SystemExit(
            f"{map_path}: BANKHDR at ${bankhdr[0]:04X}, expected ${BBS_BANK_BASE:04X}"
        )
    try:
        with open(bin_path, "rb") as f:
            bin_sz = len(f.read())
    except OSError as e:
        raise SystemExit(f"{bin_path}: {e}") from e
    if bin_sz > BANK_RAM_SIZE:
        raise SystemExit(
            f"{bin_path}: {bin_sz} B exceeds bank RAM {BANK_RAM_SIZE} B "
            f"(${BBS_BANK_BASE:04X}-${stack_lo - 1:04X})"
        )
    print(
        f"bank OK: linked to ${hi - 1:04X}, stack gap {stack_gap} B, "
        f"bin {bin_sz} B ({BANK_RAM_SIZE - bin_sz} B under RAM cap)"
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
