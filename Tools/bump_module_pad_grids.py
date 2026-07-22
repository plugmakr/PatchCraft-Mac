#!/usr/bin/env python3
"""Set all 4x4 module PadGrid surfaces to standardPadGridExtent(4, 4)."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "Source" / "Studio" / "CanvasEditor_Actions.cpp"

CHORD_STRIP = (
    'ElementType::PadGrid, "Suggested Chords", "mpActiveBank", 18, 142, 190, 98'
)


def main() -> None:
    text = PATH.read_text(encoding="utf-8")
    pattern = re.compile(
        r"(addChildSurface\s*\(\s*layout,\s*panelId,\s*ElementType::PadGrid,\s*"
        r"[^,]+,\s*[^,]+,\s*\d+,\s*\d+,\s*)\d+(\s*,\s*)\d+(\s*\))"
    )

    def repl(match: re.Match[str]) -> str:
        return (
            match.group(1)
            + "standardPadGridExtent (4, 4)"
            + match.group(2)
            + "standardPadGridExtent (4, 4)"
            + match.group(3)
        )

    new_text, count = pattern.subn(repl, text)
    if CHORD_STRIP not in new_text:
        new_text = new_text.replace(
            'ElementType::PadGrid, "Suggested Chords", "mpActiveBank", 18, 142, '
            "standardPadGridExtent (4, 4), standardPadGridExtent (4, 4)",
            CHORD_STRIP,
        )
    PATH.write_text(new_text, encoding="utf-8")
    print(f"updated {count} pad grids in CanvasEditor_Actions.cpp")


if __name__ == "__main__":
    main()
