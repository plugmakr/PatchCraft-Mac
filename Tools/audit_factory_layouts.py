#!/usr/bin/env python3
"""Quick audit of factory demo layout.json files for common UI issues."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "FactoryDemos"
CONTROL_TYPES = {
    "knob", "slider", "button", "dropdown", "padGrid", "drumGrid",
    "label", "valueDisplay", "toggle", "keyboard", "waveform",
}


def rect(e: dict) -> tuple[int, int, int, int]:
    return e.get("x", 0), e.get("y", 0), e.get("width", 0), e.get("height", 0)


def overlaps(a: tuple[int, int, int, int], b: tuple[int, int, int, int]) -> bool:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def main() -> None:
    for layout_path in sorted(ROOT.glob("**/*.patchcraft/layout.json")):
        data = json.loads(layout_path.read_text(encoding="utf-8"))
        els = data.get("elements", [])
        controls = [
            e for e in els
            if e.get("type") in CONTROL_TYPES and not e.get("locked")
        ]
        # Elements assigned to different tab pages are never visible together,
        # so only flag overlaps between elements that can co-exist on screen.
        tab_groups: set[str] = set()
        for e in els:
            if e.get("type") == "tabPanel":
                tab_groups |= {t.lower().replace(" ", "_") for t in e.get("tabs", [])}

        def covisible(a: dict, b: dict) -> bool:
            ga, gb = a.get("groupId", ""), b.get("groupId", "")
            if ga == gb or ga not in tab_groups or gb not in tab_groups:
                return True
            return False

        issues: list[tuple] = []
        for i, a in enumerate(controls):
            ra = rect(a)
            if ra[2] <= 0 or ra[3] <= 0:
                continue
            for b in controls[i + 1 :]:
                rb = rect(b)
                if rb[2] <= 0 or rb[3] <= 0:
                    continue
                if overlaps(ra, rb) and covisible(a, b):
                    issues.append((a.get("id"), a.get("type"), b.get("id"), b.get("type")))

        bad_labels = [
            e for e in els
            if e.get("type") == "knob" and e.get("labelPosition") not in (None, "bottom", "hidden")
        ]
        small_pads = [
            e for e in els
            # 320px yields at least 74px square cells after standard 4x4 gaps,
            # which is the minimum authored performance-pad target.
            if e.get("type") == "padGrid" and (e.get("width", 0) < 320 or e.get("height", 0) < 320)
        ]
        # Tab panels are a supported flagship feature; only flag broken ones.
        tab_panels = [
            e for e in els
            if e.get("type") == "tabPanel" and len(e.get("tabs", [])) < 2
        ]
        unbound = [
            e for e in controls
            if e.get("type") not in ("label", "keyboard")
            and e.get("id") not in ("presets",)
            and not e.get("action")
            and not e.get("parameterId")
        ]

        if not (issues or bad_labels or small_pads or tab_panels or unbound):
            continue

        print(f"\n=== {layout_path.parent.name} ===")
        if tab_panels:
            print(f"  tabPanel: {[e.get('id') for e in tab_panels]}")
        if small_pads:
            print(f"  small pads: {[(e.get('id'), e.get('width'), e.get('height')) for e in small_pads]}")
        if bad_labels:
            print(f"  bad knob labels: {[(e.get('id'), e.get('labelPosition')) for e in bad_labels]}")
        if unbound:
            print(f"  unbound controls: {[(e.get('id'), e.get('type')) for e in unbound[:6]]}")
        for iss in issues[:10]:
            print(f"  overlap: {iss}")
        if len(issues) > 10:
            print(f"  ... +{len(issues) - 10} more overlaps")


if __name__ == "__main__":
    main()
