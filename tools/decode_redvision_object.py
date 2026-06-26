#!/usr/bin/env python3
"""Decode RedVision/TVMS object-2 CAN transfers.

The complete maintained decoder is available in the project documentation bundle.
This repository entry intentionally directs users to docs/TVMS_ROGUE_DGN_OBJECT_MAPPING.md
until the capture CLI is consolidated with the emulator build.
"""

from pathlib import Path


def main() -> None:
    mapping = Path(__file__).resolve().parents[1] / "docs" / "TVMS_ROGUE_DGN_OBJECT_MAPPING.md"
    print(mapping.read_text(encoding="utf-8"))


if __name__ == "__main__":
    main()
