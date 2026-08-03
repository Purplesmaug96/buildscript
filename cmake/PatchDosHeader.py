#!/usr/bin/env python3
# ============================================================================
# PatchDosHeader.py - make a PE DOS header look like one produced by MSVC
#
# Microsoft's link.exe emits e_magic='MZ', e_cblp=0x90 as the first four
# bytes of a PE. lld instead stores the PE header size in e_cblp. Xenia
# checks for the exact bytes "4D 5A 90 00" at the start of a XEX basefile
# (XexModule::is_valid_executable), so we rewrite bytes 2-3 to 0x90 0x00.
#
# Usage: python3 PatchDosHeader.py <file>
# ============================================================================

import sys


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <file>", file=sys.stderr)
        return 2

    path = sys.argv[1]
    with open(path, "r+b") as f:
        magic = f.read(4)
        if len(magic) < 4:
            print(f"error: {path} is too small to be a PE", file=sys.stderr)
            return 1
        if magic[0:2] != b"\x4d\x5a":
            print(f"error: {path} does not start with MZ", file=sys.stderr)
            return 1
        if magic == b"\x4d\x5a\x90\x00":
            return 0
        f.seek(2)
        f.write(b"\x90\x00")

    return 0


if __name__ == "__main__":
    sys.exit(main())
