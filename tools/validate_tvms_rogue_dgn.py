#!/usr/bin/env python3
"""Reference entrypoint for TVMS Rogue DGN/object validation.

Confirmed mappings and unresolved controlled-test requirements are maintained in
``docs/TVMS_ROGUE_DGN_OBJECT_MAPPING.md``.
"""

from pathlib import Path


def main() -> None:
    mapping = Path(__file__).resolve().parents[1] / "docs" / "TVMS_ROGUE_DGN_OBJECT_MAPPING.md"
    print(mapping.read_text(encoding="utf-8"))


if __name__ == "__main__":
    main()
