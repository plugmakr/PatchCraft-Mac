#!/usr/bin/env python3
"""Normalize shipped FactoryDemos layout files for Player-safe presentation.

The Player already owns the title, preset browser, and top transport chrome.
Factory demo layouts should therefore focus on the instrument body: artwork,
tabs, playable controls, meters, keyboard, and performance elements.  This
script removes duplicate title/tagline chrome and normalizes control labels so
the demos load with predictable alignment in Studio, Brand Lab, Player, and
exported VST builds.
"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEMO_ROOT = ROOT / "FactoryDemos"

TOP_LABEL_IDS = {
    "title",
    "subtitle",
    "tagline",
    "logo",
    "presets",
    "preset",
    "preset_browser",
}

EMPTY_TOP_CHROME_IDS = {
    "top_chrome",
}


def _center_clamp(element: dict, max_width: int, max_height: int) -> None:
    width = int(element.get("width", 0) or 0)
    height = int(element.get("height", 0) or 0)
    if width > max_width:
        element["x"] = int(element.get("x", 0)) + round((width - max_width) / 2)
        element["width"] = max_width
    if height > max_height:
        element["y"] = int(element.get("y", 0)) + round((height - max_height) / 2)
        element["height"] = max_height


def _is_duplicate_top_element(element: dict) -> bool:
    element_id = str(element.get("id", "")).lower()
    element_type = str(element.get("type", "")).lower()
    y = int(element.get("y", 9999) or 9999)

    if element_id in EMPTY_TOP_CHROME_IDS:
        return True

    if element_id in TOP_LABEL_IDS and y < 110:
        return True

    # A few generated demos used unlabeled title bars. Keep actual controls,
    # but remove pure text/logo chrome from the first 100px.
    if element_type in {"label", "image"} and y < 92 and element_id in TOP_LABEL_IDS:
        return True

    return False


def _normalize_knob(element: dict) -> None:
    _center_clamp(element, 82, 90)
    element.setdefault("style", "Vintage Gold")
    element["labelPosition"] = "bottom"
    element["labelSpacing"] = 2
    element["labelOffsetX"] = 0
    element["labelOffsetY"] = 0

    width = int(element.get("width", 82) or 82)
    if width <= 72:
        element["labelSize"] = min(float(element.get("labelSize", 10) or 10), 10)
    else:
        element["labelSize"] = min(float(element.get("labelSize", 11) or 11), 11)


def _normalize_slider(element: dict) -> None:
    element["labelPosition"] = element.get("labelPosition", "bottom")
    element["labelSpacing"] = min(float(element.get("labelSpacing", 2) or 2), 4)
    element["labelSize"] = min(float(element.get("labelSize", 10) or 10), 10)


def rework_layout(path: Path) -> bool:
    data = json.loads(path.read_text(encoding="utf-8"))
    elements = data.get("elements", [])
    if not isinstance(elements, list):
        return False

    original = json.dumps(data, sort_keys=True)

    cleaned: list[dict] = []
    for element in elements:
        if _is_duplicate_top_element(element):
            continue

        element_type = str(element.get("type", "")).lower()
        if element_type == "knob":
            _normalize_knob(element)
        elif element_type == "slider":
            _normalize_slider(element)
        elif element_type in {"button", "toggle"}:
            element["labelSize"] = min(float(element.get("labelSize", 11) or 11), 11)
        elif element_type == "tabpanel":
            element["height"] = max(32, min(int(element.get("height", 34) or 34), 38))
            element["labelSize"] = min(float(element.get("labelSize", 11) or 11), 11)
        elif element_type == "keyboard":
            element["x"] = max(36, int(element.get("x", 40) or 40))
            element["width"] = min(1208, int(element.get("width", 1200) or 1200))
            element["height"] = min(68, max(56, int(element.get("height", 62) or 62)))

        cleaned.append(element)

    data["elements"] = cleaned
    normalized = json.dumps(data, sort_keys=True)
    if normalized == original:
        return False

    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return True


def main() -> None:
    changed = []
    for layout in sorted(DEMO_ROOT.glob("*.patchcraft/layout.json")):
        if rework_layout(layout):
            changed.append(layout.parent.name)

    if changed:
        print("Reworked demo layouts:")
        for name in changed:
            print(f" - {name}")
    else:
        print("Factory demo layouts already normalized.")


if __name__ == "__main__":
    main()
