#!/usr/bin/env python3
"""Post-link RAM checks for Magnetar BBS C64 core and bank overlays."""

import re
import sys

CORE_HIMEM = 0xB000
CORE_STACK = 0x0400
CORE_MIN_GAP = 256
BBS_API_BASE = 0xA210
BBS_SHARED_BASE = 0xA280
BBS_SHARED_SIZE = 0x0350
BBS_SHARED_STRUCT_MIN = 0x0347  # sizeof(bbs_shared_t); keep BBS_SHARED_SIZE >= this

BBS_BANK_BASE = 0xB000
BANK_TOP = 0xD000
BANK_STACK = 0x0200
BANK_RAM_SIZE = BANK_TOP - BANK_STACK - BBS_BANK_BASE
BANK_MIN_STACK_GAP = 48
BANK_HDR_SIZE = 0x15

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

    segs = parse_segments(
        map_path, ["BSS", "SHARED", "LOWBSS", "CODE", "DATA", "INIT", "ONCE", "RESAPI"]
    )
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

    if bss[0] != BBS_SHARED_BASE + BBS_SHARED_SIZE:
        raise SystemExit(
            f"BSSHI starts ${bss[0]:04X}, expected ${BBS_SHARED_BASE + BBS_SHARED_SIZE:04X}"
        )

    shared = segs.get("SHARED")
    lowbss = segs.get("LOWBSS")
    resapi = segs.get("RESAPI")
    if resapi is None:
        raise SystemExit(f"{map_path}: missing RESAPI segment")
    if resapi[0] != BBS_API_BASE:
        raise SystemExit(
            f"RESAPI at ${resapi[0]:04X}, expected ${BBS_API_BASE:04X}"
        )
    if resapi[1] > BBS_SHARED_BASE:
        raise SystemExit(
            f"RESAPI ends ${resapi[1] - 1:04X}, intrudes SHARED at ${BBS_SHARED_BASE:04X}"
        )
    if lowbss is not None and lowbss[1] > BBS_API_BASE:
        raise SystemExit(
            f"LOWBSS ends ${lowbss[1] - 1:04X}, intrudes RESAPI at ${BBS_API_BASE:04X}"
        )
    if shared is None:
        raise SystemExit(f"{map_path}: missing SHARED segment")
    if shared[0] != BBS_SHARED_BASE:
        raise SystemExit(
            f"SHARED at ${shared[0]:04X}, expected fixed ABI ${BBS_SHARED_BASE:04X}"
        )
    if shared[2] != BBS_SHARED_SIZE:
        raise SystemExit(
            f"SHARED size {shared[2]} B, expected {BBS_SHARED_SIZE} B"
        )
    if BBS_SHARED_SIZE < BBS_SHARED_STRUCT_MIN:
        raise SystemExit(
            f"BBS_SHARED_SIZE {BBS_SHARED_SIZE} B < bbs_shared_t minimum "
            f"{BBS_SHARED_STRUCT_MIN} B"
        )

    main_hi = 0
    for name in ("CODE", "RODATA", "DATA", "INIT", "ONCE"):
        seg = segs.get(name)
        if seg is not None and seg[1] > main_hi:
            main_hi = seg[1]
    if main_hi > BBS_API_BASE:
        raise SystemExit(
            f"MAIN ends ${main_hi - 1:04X}, intrudes RESAPI at ${BBS_API_BASE:04X}"
        )

    print(
        f"core OK: RESAPI ${resapi[0]:04X}-${resapi[1] - 1:04X}, "
        f"SHARED ${shared[0]:04X}-${shared[1] - 1:04X}, "
        f"BSS ${bss[0]:04X}-${bss_end - 1:04X}, stack gap {gap} B"
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
    if bankhdr is not None:
        if bankhdr[0] != BBS_BANK_BASE:
            raise SystemExit(
                f"{map_path}: BANKHDR at ${bankhdr[0]:04X}, expected ${BBS_BANK_BASE:04X}"
            )
        if bankhdr[2] < BANK_HDR_SIZE:
            raise SystemExit(
                f"{map_path}: BANKHDR size {bankhdr[2]} B < {BANK_HDR_SIZE} B"
            )
    try:
        with open(bin_path, "rb") as f:
            bin_sz = len(f.read())
    except OSError as e:
        raise SystemExit(f"{bin_path}: {e}") from e
    if bin_sz > BANK_RAM_SIZE:
        raise SystemExit(
            f"{bin_path}: {bin_sz} B exceeds bank RAM {BANK_RAM_SIZE} B"
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
