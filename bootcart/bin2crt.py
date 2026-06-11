#!/usr/bin/env python3
"""Wrap an 8K C64 ROM binary in a VICE CRT V2.0 cartridge image."""

import struct
import sys


def crt_header(name: str, exrom: int = 0, game: int = 0) -> bytes:
    title = b"C64 CARTRIDGE" + b"\x00" * 3
    version = struct.pack(">I", 2)  # CRT V2.0
    hwtype = struct.pack(">H", 0)   # Generic cartridge
    cart_name = name.encode("ascii", errors="replace")[:31]
    cart_name = cart_name.ljust(32, b"\x00")
    return title + version + hwtype + bytes([exrom, game]) + b"\x00\x00" + b"\x00" * 4 + cart_name


def chip_packet(rom: bytes, load_addr: int = 0x8000, bank: int = 0) -> bytes:
    chip_name = b"ROM" + b"\x00" * 29
    header = b"CHIP"
    header += struct.pack(">I", 16 + 32 + len(rom))
    header += struct.pack(">H", 0)          # ROM chip
    header += struct.pack(">H", bank)
    header += struct.pack(">H", load_addr)
    header += struct.pack(">H", len(rom))
    header += chip_name
    return header + rom


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <rom.bin> <output.crt>", file=sys.stderr)
        return 2

    rom = open(sys.argv[1], "rb").read()
    if len(rom) != 0x2000:
        print(f"error: expected 8192-byte ROM, got {len(rom)} bytes", file=sys.stderr)
        return 1

    crt = crt_header("MAGBBS BOOT") + chip_packet(rom)
    with open(sys.argv[2], "wb") as out:
        out.write(crt)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
