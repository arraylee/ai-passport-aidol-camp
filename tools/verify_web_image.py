#!/usr/bin/env python3
"""Verify the fixed offsets and protected boundary of an AI PASSPORT web BIN."""

import argparse
import hashlib
import json
from pathlib import Path


BOOT_MAGIC = 0xE9
PARTITION_MAGIC = bytes.fromhex("aa50")
PARTITION_OFFSET = 0x8000
APP_OFFSET = 0x10000
DEFAULT_IDENTITY_START = 0x356000


class VerificationError(RuntimeError):
    pass


def integer(value: str) -> int:
    return int(value, 0)


def verify(
    path: Path,
    identity_start: int = DEFAULT_IDENTITY_START,
    resource_offset: int | None = None,
    resource_magic: bytes = b"WBPK",
) -> dict:
    data = path.read_bytes()
    if not data:
        raise VerificationError("image is empty")
    if len(data) > identity_start:
        raise VerificationError(
            f"image ends at 0x{len(data):x}, beyond identity start 0x{identity_start:x}"
        )
    if data[0] != BOOT_MAGIC:
        raise VerificationError("missing ESP bootloader magic at 0x0")
    if data[PARTITION_OFFSET : PARTITION_OFFSET + 2] != PARTITION_MAGIC:
        raise VerificationError("missing partition-table magic at 0x8000")
    if len(data) <= APP_OFFSET or data[APP_OFFSET] != BOOT_MAGIC:
        raise VerificationError("missing ESP application magic at 0x10000")
    if resource_offset is not None:
        end = resource_offset + len(resource_magic)
        if data[resource_offset:end] != resource_magic:
            raise VerificationError(
                f"missing resource magic at 0x{resource_offset:x}"
            )
    return {
        "file": str(path),
        "size": len(data),
        "end": f"0x{len(data):x}",
        "identity_start": f"0x{identity_start:x}",
        "identity_gap": identity_start - len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument(
        "--identity-start", type=integer, default=DEFAULT_IDENTITY_START
    )
    parser.add_argument("--resource-offset", type=integer)
    parser.add_argument("--resource-magic", default="WBPK")
    args = parser.parse_args()
    try:
        report = verify(
            args.image,
            identity_start=args.identity_start,
            resource_offset=args.resource_offset,
            resource_magic=args.resource_magic.encode("ascii"),
        )
    except (OSError, UnicodeEncodeError, VerificationError) as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
