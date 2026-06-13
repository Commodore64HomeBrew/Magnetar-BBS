#!/usr/bin/env python3
"""Post-link RAM checks for Magnetar BBS C64 core and bank overlays."""

import re
import sys

CORE_HIMEM = 0xB000
CORE_STACK = 0x0400
CORE_MIN_GAP = 256
BBS_API_BASE = 0xA210
BBS_SHARED_BASE = 0xA280
BBS_SHARED_SIZE = 0x0347
BBS_SHARED_STRUCT_MIN = 0x0347  # sizeof(bbs_shared_t); keep BBS_SHARED_SIZE >= this
BBS_API_STUB_SIZE = 3
BBS_API_STUB_COUNT = 22
BBS_API_SIZE = BBS_API_STUB_COUNT * BBS_API_STUB_SIZE
BBS_ABI_VERSION = 0x0100

BBS_BANK_BASE = 0xB000
BANK_TOP = 0xD000
BANK_STACK = 0x0200
BANK_RAM_SIZE = BANK_TOP - BANK_STACK - BBS_BANK_BASE
BANK_MIN_STACK_GAP = 48
BANK_HDR_SIZE = 0x15

BBS_BANK_INIT_OFF = 0x06
BBS_BANK_DEINIT_OFF = 0x09
BBS_BANK_SET_OP_OFF = 0x0C
BBS_BANK_FEED_OFF = 0x0F
BBS_BANK_RUN_STATS_OFF = 0x12

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


def jmp_target(buf, off):
    if buf[off] != 0x4C:
        return None
    return buf[off + 1] | (buf[off + 2] << 8)


def check_bank_header(bin_path, bank_id):
    try:
        with open(bin_path, "rb") as f:
            hdr = f.read(BANK_HDR_SIZE)
    except OSError as e:
        raise SystemExit(f"{bin_path}: {e}") from e
    if len(hdr) < BANK_HDR_SIZE:
        raise SystemExit(f"{bin_path}: header {len(hdr)} B < {BANK_HDR_SIZE} B")
    expect = 0x30 + bank_id
    if hdr[0:4] != bytes([0x42, 0x42, 0x4B, expect]):
        raise SystemExit(
            f"{bin_path}: sig want BBK{chr(expect)}, got "
            f"{hdr[0]:02x}{hdr[1]:02x}{hdr[2]:02x}{hdr[3]:02x}"
        )
    abi = hdr[4] | (hdr[5] << 8)
    if abi != BBS_ABI_VERSION:
        raise SystemExit(
            f"{bin_path}: ABI ${abi:04X}, expected ${BBS_ABI_VERSION:04X}"
        )
    code_lo = BBS_BANK_BASE + BANK_HDR_SIZE
    for off, name in (
        (BBS_BANK_INIT_OFF, "init"),
        (BBS_BANK_DEINIT_OFF, "deinit"),
    ):
        tgt = jmp_target(hdr, off)
        if tgt is None:
            raise SystemExit(
                f"{bin_path}: ${BBS_BANK_BASE + off:04X} ({name}) not JMP "
                f"(got ${hdr[off]:02X}) — stale bank .bin?"
            )
        if tgt < code_lo or tgt >= BANK_TOP:
            raise SystemExit(
                f"{bin_path}: {name} JMP ${tgt:04X} outside "
                f"${code_lo:04X}-${BANK_TOP - 1:04X}"
            )
    if bank_id == 3:
        tgt = jmp_target(hdr, BBS_BANK_RUN_STATS_OFF)
        if tgt is None:
            raise SystemExit(
                f"{bin_path}: run_stats at "
                f"${BBS_BANK_BASE + BBS_BANK_RUN_STATS_OFF:04X} not JMP"
            )
        if tgt < code_lo or tgt >= BANK_TOP:
            raise SystemExit(
                f"{bin_path}: run_stats JMP ${tgt:04X} outside bank code"
            )


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
    if resapi[2] != BBS_API_SIZE:
        raise SystemExit(
            f"RESAPI size {resapi[2]} B, expected {BBS_API_SIZE} B "
            f"({BBS_API_STUB_COUNT} JMP stubs)"
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


def check_bank(map_path, bin_path, bank_id):
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
    check_bank_header(bin_path, bank_id)
    print(
        f"bank OK: linked to ${hi - 1:04X}, stack gap {stack_gap} B, "
        f"bin {bin_sz} B ({BANK_RAM_SIZE - bin_sz} B under RAM cap)"
    )


def main():
    if len(sys.argv) != 5:
        raise SystemExit(f"usage: {sys.argv[0]} core|bank MAP BIN BANK_ID")
    kind, map_path, bin_path, bank_id_s = sys.argv[1:5]
    if kind == "core":
        check_core(map_path, bin_path)
    elif kind == "bank":
        check_bank(map_path, bin_path, int(bank_id_s))
    else:
        raise SystemExit(f"unknown kind {kind!r}")


if __name__ == "__main__":
    main()
