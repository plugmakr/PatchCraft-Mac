#!/usr/bin/env python3
"""Rebuild PatchCraft factory demos as distinct ship-facing products.

This script intentionally overwrites generated demo JSON and generated PNG
assets inside FactoryDemos/*.patchcraft. It preserves existing sample folders.
"""

from __future__ import annotations

import json
import math
import random
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FACTORY = ROOT / "FactoryDemos"
W, H = 1280, 800


def rgba(hex_rgb: str, alpha: int = 255) -> tuple[int, int, int, int]:
    hex_rgb = hex_rgb.strip().lstrip("#")
    return (int(hex_rgb[0:2], 16), int(hex_rgb[2:4], 16), int(hex_rgb[4:6], 16), alpha)


def colour_string(hex_rgb: str, alpha: int = 255) -> str:
    r, g, b, a = rgba(hex_rgb, alpha)
    return f"{a:02x}{r:02x}{g:02x}{b:02x}"


def font(size: int, bold: bool = False) -> ImageFont.ImageFont:
    candidates = [
        "C:/Windows/Fonts/seguisb.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf",
    ]
    for candidate in candidates:
        try:
            return ImageFont.truetype(candidate, size)
        except Exception:
            pass
    return ImageFont.load_default()


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def shape(eid: str, x: int, y: int, w: int, h: int, *, bg: str, border: str,
          radius: int = 10, stroke: float = 1.2, group: str = "", glow: float = 0.0) -> dict[str, Any]:
    return {
        "id": eid,
        "type": "shape",
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "shapeKind": "roundedRect",
        "backgroundColour": bg,
        "borderColour": border,
        "strokeWidth": stroke,
        "cornerRadius": radius,
        "glowAmount": glow,
        "groupId": group,
    }


def label(eid: str, text: str, x: int, y: int, w: int, h: int, *,
          colour: str, size: float = 14.0, group: str = "") -> dict[str, Any]:
    return {
        "id": eid,
        "type": "label",
        "label": text,
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "textColour": colour,
        "labelSize": size,
        "labelPosition": "hidden",
        "groupId": group,
    }


def knob(eid: str, text: str, param: str, x: int, y: int, *, accent: str,
         size: int = 78, group: str = "", style: str = "Modern Dark") -> dict[str, Any]:
    return {
        "id": eid,
        "type": "knob",
        "x": x,
        "y": y,
        "width": size,
        "height": size + 22,
        "label": text,
        "parameterId": param,
        "knobStyle": style,
        "accentColour": accent,
        "backgroundColour": "d0080b10",
        "borderColour": "88435260",
        "labelPosition": "bottom",
        "labelSize": 10.5,
        "contentPadding": 0.0,
        "groupId": group,
    }


def slider(eid: str, text: str, param: str, x: int, y: int, w: int, h: int, *,
           accent: str, group: str = "") -> dict[str, Any]:
    return {
        "id": eid,
        "type": "slider",
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "label": text,
        "parameterId": param,
        "accentColour": accent,
        "backgroundColour": "cc080b10",
        "borderColour": "88435260",
        "labelPosition": "bottom",
        "labelSize": 10.0,
        "contentPadding": 0.0,
        "groupId": group,
    }


def value_display(eid: str, text: str, param: str, x: int, y: int, w: int, h: int, *,
                  accent: str, group: str = "") -> dict[str, Any]:
    return {
        "id": eid,
        "type": "valueDisplay",
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "label": text,
        "parameterId": param,
        "accentColour": accent,
        "backgroundColour": "d006080d",
        "borderColour": "77435260",
        "labelPosition": "hidden",
        "labelSize": 11.0,
        "groupId": group,
    }


def dropdown(eid: str, text: str, param: str, x: int, y: int, w: int, h: int, *,
             accent: str, group: str = "") -> dict[str, Any]:
    return {
        "id": eid,
        "type": "dropdown",
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "label": text,
        "parameterId": param,
        "accentColour": accent,
        "backgroundColour": "dd080b10",
        "borderColour": "88435260",
        "labelPosition": "hidden",
        "labelSize": 11.0,
        "groupId": group,
    }


def button(eid: str, text: str, param: str, x: int, y: int, w: int, h: int, *,
           accent: str, group: str = "") -> dict[str, Any]:
    return {
        "id": eid,
        "type": "button",
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "label": text,
        "parameterId": param,
        "accentColour": accent,
        "backgroundColour": "dd080b10",
        "borderColour": accent,
        "labelPosition": "hidden",
        "labelSize": 11.0,
        "groupId": group,
    }


def param(pid: str, name: str, mn: float, mx: float, default: float, unit: str = "",
          section: str = "main", mode: str = "continuous", step: float = 0.0) -> dict[str, Any]:
    return {
        "id": pid,
        "name": name,
        "type": "float",
        "min": mn,
        "max": mx,
        "default": default,
        "unit": unit,
        "section": section,
        "displayMode": mode,
        "step": step,
        "hostAutomatable": True,
        "midiLearnable": True,
        "modulatable": mode != "toggle",
        "visible": True,
    }


def base_params(engine: str) -> list[dict[str, Any]]:
    params = [
        param("attack", "Attack", 0.001, 5.0, 0.01, "s", "amp"),
        param("decay", "Decay", 0.001, 5.0, 0.22, "s", "amp"),
        param("sustain", "Sustain", 0.0, 1.0, 0.42, "", "amp"),
        param("release", "Release", 0.01, 10.0, 0.45, "s", "amp"),
        param("filterCutoff", "Cutoff", 20.0, 20000.0, 5200.0, "Hz", "filter"),
        param("filterResonance", "Resonance", 0.0, 1.0, 0.18, "", "filter"),
        param("delayTime", "Delay Time", 0.02, 2.0, 0.25, "s", "fx"),
        param("delayFeedback", "Feedback", 0.0, 0.95, 0.25, "", "fx"),
        param("delayMix", "Delay Mix", 0.0, 1.0, 0.12, "%", "fx"),
        param("reverbMix", "Reverb", 0.0, 1.0, 0.18, "%", "fx"),
        param("volume", "Volume", 0.0, 1.5, 0.72, "", "output"),
        param("pan", "Pan", -1.0, 1.0, 0.0, "", "output"),
        param("stereoWidth", "Width", 0.0, 2.0, 1.05, "", "output"),
        param("outputGainDb", "Output", -24.0, 12.0, -3.0, "dB", "output"),
        param("outputLimiter", "Limiter", 0.0, 1.0, 1.0, "", "output", "toggle", 1.0),
        param("outputCeilingDb", "Ceiling", -12.0, 0.0, -1.0, "dB", "output"),
        param("modWheel", "Mod Wheel", 0.0, 1.0, 0.0, "", "perform"),
        param("expression", "Expression", 0.0, 1.0, 1.0, "", "perform"),
        param("projectBpm", "BPM", 40.0, 240.0, 124.0, "bpm", "perform", "continuous", 1.0),
        param("bpmSync", "Sync", 0.0, 1.0, 1.0, "", "perform", "toggle", 1.0),
        param("retrigger", "Retrigger", 0.0, 1.0, 1.0, "", "perform", "toggle", 1.0),
        param("macro_motion", "Motion", 0.0, 1.0, 0.45, "", "macro"),
        param("macro_tone", "Tone", 0.0, 1.0, 0.55, "", "macro"),
        param("macro_character", "Character", 0.0, 1.0, 0.50, "", "macro"),
        param("macro_space", "Space", 0.0, 1.0, 0.35, "", "macro"),
    ]
    if engine == "synth":
        params += [
            param("oscType", "Wave", 0.0, 3.0, 1.0, "", "source", "stepped", 1.0),
            param("osc2Type", "Wave 2", 0.0, 3.0, 0.0, "", "source", "stepped", 1.0),
            param("oscBlend", "Blend", 0.0, 1.0, 0.08, "", "source"),
            param("octave", "Octave", -2.0, 2.0, 0.0, "", "source", "stepped", 1.0),
            param("detune", "Detune", -50.0, 50.0, 4.0, "ct", "source"),
            param("osc2Detune", "Detune 2", -50.0, 50.0, -3.0, "ct", "source"),
            param("subBlend", "Sub", 0.0, 1.0, 0.08, "", "source"),
            param("noiseBlend", "Noise", 0.0, 1.0, 0.0, "", "source"),
            param("lfoRate", "LFO Rate", 0.05, 20.0, 1.0, "Hz", "motion"),
            param("lfoAmount", "LFO Amt", 0.0, 1.0, 0.05, "", "motion"),
            param("vibratoDepth", "Vibrato", 0.0, 1.0, 0.0, "", "motion"),
            param("vibratoRate", "Vib Rate", 0.1, 12.0, 5.2, "Hz", "motion"),
            param("wtEnabled", "WT", 0.0, 1.0, 0.0, "", "source", "toggle", 1.0),
            param("wtPosition", "WT Pos", 0.0, 1.0, 0.25, "", "source"),
            param("wtMorph", "WT Morph", 0.0, 1.0, 0.20, "", "source"),
            param("wtLevel", "WT Level", 0.0, 1.5, 0.0, "", "source"),
        ]
    if engine in {"sample", "synth"}:
        params += [
            param("velocitySensitivity", "Vel Sens", 0.0, 1.0, 0.5, "", "sample"),
            param("samplePitch", "Sample Tune", -24.0, 24.0, 0.0, "st", "sample"),
            param("sampleStart", "Start", 0.0, 1.0, 0.0, "", "sample"),
            param("sampleLength", "Length", 0.01, 1.0, 1.0, "", "sample"),
            param("sampleSlice", "Slice", 0.0, 1.0, 0.0, "", "sample"),
            param("sampleSliceCount", "Slices", 1.0, 16.0, 8.0, "", "sample", "stepped", 1.0),
            param("sampleGlitch", "Glitch", 0.0, 1.0, 0.0, "", "sample"),
            param("sampleGlitchGrid", "Glitch Grid", 0.0, 1.0, 0.25, "", "sample"),
            param("granularOn", "Grain", 0.0, 1.0, 0.0, "", "sample", "toggle", 1.0),
            param("granularDensity", "Density", 0.0, 1.0, 0.35, "", "sample"),
            param("granularSizeMs", "Grain Size", 8.0, 240.0, 80.0, "ms", "sample"),
            param("granularSpread", "Spread", 0.0, 1.0, 0.35, "", "sample"),
            param("granularScan", "Scan", -3.0, 3.0, 0.0, "", "sample"),
            param("granularPitchSpread", "Pitch Spread", 0.0, 36.0, 3.0, "st", "sample"),
            param("granularPanSpread", "Pan Spread", 0.0, 1.0, 0.35, "", "sample"),
        ]
    if engine == "sample":
        for i in range(1, 17):
            params += [
                param(f"pad{i}Volume", f"Pad {i} Level", 0.0, 2.0, 1.0, "", "pads"),
                param(f"pad{i}Pitch", f"Pad {i} Tune", -24.0, 24.0, 0.0, "st", "pads"),
                param(f"pad{i}Pan", f"Pad {i} Pan", -1.0, 1.0, 0.0, "", "pads"),
            ]
    if engine == "fx":
        params += [
            param("inputTrimDb", "Input", -24.0, 24.0, 0.0, "dB", "input"),
            param("drive", "Drive", 0.0, 1.0, 0.12, "", "fx"),
            param("mix", "Mix", 0.0, 1.0, 0.55, "", "fx"),
        ]
    params += advanced_fx_params()
    return dedupe_params(params)


def advanced_fx_params() -> list[dict[str, Any]]:
    specs = [
        ("dynThresholdDb", "Threshold", -80, 12, -18, "dB", "dynamics"),
        ("dynRatio", "Ratio", 1, 40, 2.5, "", "dynamics"),
        ("dynAttackMs", "Attack", 0.1, 200, 18, "ms", "dynamics"),
        ("dynReleaseMs", "Release", 5, 1200, 180, "ms", "dynamics"),
        ("dynMakeupDb", "Makeup", -24, 24, 0, "dB", "dynamics"),
        ("dynMix", "Comp Mix", 0, 1, 0.0, "", "dynamics"),
        ("chorusRate", "Chor Rate", 0.01, 8, 0.35, "Hz", "mod"),
        ("chorusDepth", "Chor Depth", 0, 1, 0.0, "", "mod"),
        ("chorusFeedback", "Chor FB", 0, 0.95, 0.0, "", "mod"),
        ("chorusMix", "Chorus", 0, 1, 0.0, "", "mod"),
        ("phaserRate", "Phaser Rate", 0.01, 8, 0.25, "Hz", "mod"),
        ("phaserDepth", "Phaser Depth", 0, 1, 0.0, "", "mod"),
        ("phaserFeedback", "Phaser FB", 0, 0.95, 0.0, "", "mod"),
        ("phaserMix", "Phaser", 0, 1, 0.0, "", "mod"),
        ("combFreq", "Comb Hz", 40, 5000, 220, "Hz", "creative"),
        ("combFeedback", "Comb FB", 0, 0.95, 0.0, "", "creative"),
        ("combMix", "Comb", 0, 1, 0.0, "", "creative"),
        ("resonatorFreq", "Res Hz", 40, 6000, 440, "Hz", "creative"),
        ("resonatorQ", "Res Q", 0.1, 10, 1.0, "", "creative"),
        ("resonatorMix", "Resonator", 0, 1, 0.0, "", "creative"),
        ("spectralTilt", "Tilt", -1, 1, 0.0, "", "tone"),
        ("spectralMix", "Tilt Mix", 0, 1, 0.0, "", "tone"),
        ("tapeDrive", "Tape Drive", 0, 1, 0.0, "", "tone"),
        ("tapeTone", "Tape Tone", 0, 1, 0.5, "", "tone"),
        ("tapeFlutter", "Flutter", 0, 1, 0.0, "", "tone"),
        ("tapeMix", "Tape", 0, 1, 0.0, "", "tone"),
        ("vinylAge", "Vinyl Age", 0, 1, 0.0, "", "destruct"),
        ("vinylDust", "Dust", 0, 1, 0.0, "", "destruct"),
        ("vinylWarp", "Warp", 0, 1, 0.0, "", "destruct"),
        ("vinylMix", "Vinyl", 0, 1, 0.0, "", "destruct"),
        ("lofiBits", "Bits", 4, 16, 16, "", "destruct"),
        ("lofiRate", "Rate", 1000, 44100, 44100, "Hz", "destruct"),
        ("lofiMix", "LoFi", 0, 1, 0.0, "", "destruct"),
        ("vocalFormant", "Formant", -1, 1, 0.0, "", "creative"),
        ("vocalBody", "Body", 0, 1, 0.5, "", "creative"),
        ("vocalMix", "Vocal", 0, 1, 0.0, "", "creative"),
        ("multiTapTime", "Tap Time", 0.02, 2, 0.375, "s", "space"),
        ("multiTapFeedback", "Tap FB", 0, 0.95, 0.0, "", "space"),
        ("multiTapSpread", "Tap Spread", 0, 1, 0.5, "", "space"),
        ("multiTapMix", "Multi Tap", 0, 1, 0.0, "", "space"),
        ("convolutionSize", "Space Size", 0, 1, 0.4, "", "space"),
        ("convolutionMix", "Space", 0, 1, 0.0, "", "space"),
    ]
    return [param(*spec) for spec in specs]


def dedupe_params(params: list[dict[str, Any]]) -> list[dict[str, Any]]:
    seen: dict[str, dict[str, Any]] = {}
    for p in params:
        seen[p["id"]] = p
    return list(seen.values())


def controls_from(params: list[tuple[str, str]], x0: int, y0: int, *, cols: int,
                  accent: str, group: str, size: int = 74, xgap: int = 116, ygap: int = 112) -> list[dict[str, Any]]:
    out = []
    for i, (pid, name) in enumerate(params):
        x = x0 + (i % cols) * xgap
        y = y0 + (i // cols) * ygap
        out.append(knob(f"{group}_{pid}", name, pid, x, y, accent=accent, size=size, group=group))
    return out


def knob_grid(params: list[tuple[str, str]], frame: tuple[int, int, int, int], *,
              cols: int, accent: str, group: str, top_pad: int = 46,
              side_pad: int = 18, bottom_pad: int = 16, max_size: int = 84) -> list[dict[str, Any]]:
    """Lay knobs out on an even grid that is guaranteed to fit, evenly spaced and
    centred, inside ``frame`` (x, y, w, h). The first ``top_pad`` pixels are left
    for the panel title. Knob size is derived from the available cell so columns
    and rows never overflow the panel — this is what keeps every demo aligned."""
    fx, fy, fw, fh = frame
    count = max(1, len(params))
    rows = math.ceil(count / cols)
    cell_w = (fw - 2 * side_pad) / cols
    cell_h = (fh - top_pad - bottom_pad) / rows
    # Reserve ~24px under each knob for its label.
    size = int(min(cell_w - 14, cell_h - 26, max_size))
    size = max(30, size)
    knob_h = size + 22
    out = []
    for i, (pid, name) in enumerate(params):
        c = i % cols
        r = i // cols
        x = int(fx + side_pad + c * cell_w + (cell_w - size) / 2)
        y = int(fy + top_pad + r * cell_h + (cell_h - knob_h) / 2)
        out.append(knob(f"{group}_{pid}", name, pid, x, y, accent=accent, size=size, group=group))
    return out


def demo_hint_elements(pack: dict[str, Any]) -> list[dict[str, Any]]:
    # Factory demos ship clean: no instructional badges or explanatory text on
    # the instrument face. Discoverability lives in the product page, not the UI.
    return []


def base_layout(pack: dict[str, Any]) -> list[dict[str, Any]]:
    accent = pack["accent"]
    els = [
        {"id": "background", "type": "image", "x": 0, "y": 0, "width": W, "height": H,
         "asset": "assets/background-clean.png", "locked": True},
        # The Player chrome (logo + name + tagline) already brands the product, so
        # the instrument face stays clean — no duplicated/oversized in-canvas title.
        dropdown("presets", "Preset", "", 974, 38, 248, 34, accent=accent, group="brand"),
        slider("pitchwheel", "Pitch", "modWheel", 52, 612, 34, 118, accent=accent, group="global"),
        slider("modwheel", "Mod", "expression", 96, 612, 34, 118, accent=accent, group="global"),
        knob("global_volume", "Volume", "volume", 1126, 596, accent=accent, size=72, group="global"),
        knob("global_pan", "Pan", "pan", 1126, 690, accent=pack["accent2"], size=72, group="global"),
        value_display("output_value", "Out", "outputGainDb", 1028, 704, 82, 34, accent=accent, group="global"),
    ]
    els += demo_hint_elements(pack)
    return els


def synth_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    step_arp = bool(pack.get("step_arp"))
    els = base_layout(pack)
    source_frame = (54, 112, 360, 250)
    visual_frame = (430, 112, 752, 250)
    deck_frame = (54, 384, 1000, 244)
    spectrum_w = 430 if step_arp else 700
    els += [
        shape("source_frame", *source_frame, bg="bb090d14", border=accent, radius=16, glow=0.12, group="source"),
        shape("visual_frame", *visual_frame, bg="aa0a1018", border=accent2, radius=16, group="motion"),
        shape("deck_frame", *deck_frame, bg="aa0a1018", border=accent, radius=16, group="deck"),
        label("source_title", "SOURCE", source_frame[0] + 24, source_frame[1] + 18, 200, 22, colour="ffeef6ff", size=14, group="source"),
        label("visual_title", "MOTION", visual_frame[0] + 24, visual_frame[1] + 18, 200, 22, colour="ffeef6ff", size=14, group="motion"),
        label("deck_title", "TONE / FX", deck_frame[0] + 24, deck_frame[1] + 16, 200, 22, colour="ffeef6ff", size=14, group="deck"),
        dropdown("wave_select", "Wave", "oscType", source_frame[0] + 24, source_frame[1] + 52, 144, 30, accent=accent, group="source"),
        dropdown("wave2_select", "Wave 2", "osc2Type", source_frame[0] + 180, source_frame[1] + 52, 144, 30, accent=accent2, group="source"),
        {"id": "spectrum", "type": "spectrumAnalyzer", "x": visual_frame[0] + 24, "y": visual_frame[1] + 52,
         "width": spectrum_w, "height": 170, "parameterId": "macro_motion", "accentColour": accent2,
         "backgroundColour": "cc06090f", "borderColour": "66505b72", "label": "Motion View",
         "labelPosition": "hidden", "groupId": "motion"},
        {"id": "keyboard", "type": "keyboard", "x": 180, "y": 642, "width": 838, "height": 80,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
    ]
    els += knob_grid([("oscBlend", "Blend"), ("octave", "Oct"), ("detune", "Detune"), ("subBlend", "Sub")],
                     (source_frame[0], source_frame[1] + 96, source_frame[2], source_frame[3] - 96),
                     cols=4, accent=accent, group="src_knobs", top_pad=10)
    deck = [
        ("filterCutoff", "Cutoff"), ("filterResonance", "Res"), ("attack", "Atk"), ("release", "Rel"),
        ("lfoRate", "LFO Hz"), ("lfoAmount", "LFO Amt"), ("delayMix", "Delay"), ("reverbMix", "Reverb"),
        ("macro_motion", "Motion"), ("macro_tone", "Tone"), ("macro_character", "Color"), ("macro_space", "Space"),
    ]
    cols = 6
    if step_arp:
        deck += [("arpLaneRate", "Rate"), ("arpLaneGate", "Gate")]
        cols = 7
    els += knob_grid(deck, deck_frame, cols=cols, accent=accent, group="deck_knobs")
    if step_arp:
        els.append({
            "id": "mini_step_orbit", "type": "arpLane", "x": visual_frame[0] + 476, "y": visual_frame[1] + 50,
            "width": 250, "height": 184, "label": "Steps", "parameterId": "arpLaneRate", "accentColour": accent2,
            "backgroundColour": "cc05080d", "borderColour": accent2, "labelPosition": "hidden", "arpLaneIndex": 0,
            "arpLaneSteps": 16, "arpLaneMode": "multiRing", "arpLaneTarget": "notes", "groupId": "motion",
        })
    return {"canvas": {"width": W, "height": H}, "elements": els}


def drums_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    pattern = [
        [1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0],
        [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
        [0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0],
        [0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0],
        [0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1],
        [0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0],
        [0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0],
        [0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
    ]
    pad_frame = (52, 112, 452, 430)
    grid_frame = (524, 112, 658, 210)
    mix_frame = (524, 360, 658, 268)
    els += [
        shape("pad_frame", *pad_frame, bg="bb080b10", border=accent, radius=18, group="pads", glow=0.14),
        shape("grid_frame", *grid_frame, bg="bb080b10", border=accent2, radius=16, group="pattern"),
        shape("mix_frame", *mix_frame, bg="aa0a1018", border=accent, radius=16, group="mix"),
        label("pad_title", "DRUM PADS", pad_frame[0] + 26, pad_frame[1] + 18, 200, 22, colour="ffeef6ff", size=14, group="pads"),
        label("grid_title", "PATTERN", grid_frame[0] + 26, grid_frame[1] + 16, 200, 22, colour="ffeef6ff", size=14, group="pattern"),
        label("mix_title", "KIT MIX", mix_frame[0] + 26, mix_frame[1] + 16, 200, 22, colour="ffeef6ff", size=14, group="mix"),
        {"id": "pad_bank", "type": "padGrid", "x": pad_frame[0] + 26, "y": pad_frame[1] + 58, "width": 400, "height": 336,
         "parameterId": "pad1Volume", "padRows": 4, "padCols": 4, "padBaseNote": 36,
         "accentColour": accent, "backgroundColour": "dd06090f", "borderColour": "66505b72",
         "label": "Pads", "labelPosition": "hidden", "groupId": "pads"},
        {"id": "drum_pattern", "type": "drumGrid", "x": grid_frame[0] + 24, "y": grid_frame[1] + 50, "width": 610, "height": 138,
         "parameterId": "pad1Volume", "drumTracks": 8, "drumSteps": 16, "drumPattern": pattern,
         "accentColour": accent2, "backgroundColour": "dd06090f", "borderColour": "66505b72",
         "label": "Pattern", "labelPosition": "hidden", "groupId": "pattern"},
        {"id": "keyboard", "type": "keyboard", "x": 190, "y": 644, "width": 818, "height": 78,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
    ]
    els += knob_grid([
        ("pad1Volume", "Kick"), ("pad3Volume", "Snare"), ("pad7Volume", "Hat"), ("pad4Volume", "Clap"),
        ("pad14Volume", "Crash"), ("pad1Pitch", "Kick Tune"), ("pad3Pitch", "Snr Tune"), ("samplePitch", "Kit Tune"),
        ("attack", "Atk"), ("release", "Rel"), ("filterCutoff", "Cutoff"), ("sampleGlitch", "Glitch"),
        ("delayMix", "Delay"), ("reverbMix", "Room"), ("macro_motion", "Swing"), ("macro_tone", "Tone"),
    ], mix_frame, cols=8, accent=accent, group="drum_controls")
    return {"canvas": {"width": W, "height": H}, "elements": els}


def sampler_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    wave_frame = (54, 112, 756, 250)
    drop_frame = (826, 112, 356, 250)
    deck_frame = (54, 384, 1000, 244)
    els += [
        shape("wave_frame", *wave_frame, bg="bb080b10", border=accent, radius=18, group="sample", glow=0.13),
        shape("drop_frame", *drop_frame, bg="aa0a1018", border=accent2, radius=18, group="drop"),
        shape("deck_frame", *deck_frame, bg="bb080b10", border=accent, radius=16, group="edit"),
        label("wave_title", "SAMPLE PERFORMANCE", wave_frame[0] + 24, wave_frame[1] + 18, 280, 22, colour="ffeef6ff", size=14, group="sample"),
        label("drop_title", "DROP SAMPLE", drop_frame[0] + 24, drop_frame[1] + 18, 220, 22, colour="ffeef6ff", size=14, group="drop"),
        label("deck_title", "SHAPE / GRAIN / FX", deck_frame[0] + 24, deck_frame[1] + 16, 280, 22, colour="ffeef6ff", size=14, group="edit"),
        {"id": "waveform", "type": "waveform", "x": wave_frame[0] + 24, "y": wave_frame[1] + 52, "width": 708, "height": 150,
         "parameterId": "sampleStart", "accentColour": accent, "backgroundColour": "cc06090f",
         "borderColour": "66505b72", "label": "Waveform", "labelPosition": "hidden", "groupId": "sample"},
        {"id": "sample_drop", "type": "sampleDropZone", "x": drop_frame[0] + 24, "y": drop_frame[1] + 52, "width": 308, "height": 134,
         "parameterId": "sampleStart", "accentColour": accent2, "backgroundColour": "cc06090f",
         "borderColour": accent2, "label": "Drop Sample", "labelPosition": "hidden", "groupId": "drop"},
        button("grain_toggle", "GRAIN", "granularOn", drop_frame[0] + 24, drop_frame[1] + 200, 140, 30, accent=accent2, group="drop"),
        dropdown("slice_select", "Slices", "sampleSliceCount", drop_frame[0] + 176, drop_frame[1] + 200, 156, 30, accent=accent, group="drop"),
        {"id": "keyboard", "type": "keyboard", "x": 178, "y": 642, "width": 840, "height": 80,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
    ]
    els += knob_grid([
        ("sampleStart", "Start"), ("sampleLength", "Length"), ("samplePitch", "Tune"), ("sampleSlice", "Slice"),
        ("sampleGlitch", "Glitch"), ("granularDensity", "Density"), ("granularSizeMs", "Size"), ("granularScan", "Scan"),
        ("filterCutoff", "Cutoff"), ("filterResonance", "Res"), ("attack", "Atk"), ("release", "Rel"),
        ("delayMix", "Delay"), ("reverbMix", "Space"), ("macro_motion", "Motion"), ("macro_tone", "Tone"),
    ], deck_frame, cols=8, accent=accent, group="sample_controls")
    return {"canvas": {"width": W, "height": H}, "elements": els}


def circle_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    deck_frame = (48, 572, 1114, 200)
    els += [
        shape("orbit_field", 48, 112, 790, 446, bg="bb070b12", border=accent, radius=22, group="orbit", glow=0.16),
        shape("lane_panel", 862, 112, 300, 446, bg="aa0a1018", border=accent2, radius=18, group="lanes"),
        shape("deck_frame", *deck_frame, bg="aa0a1018", border=accent, radius=16, group="deck"),
        label("orbit_title", "CIRCLE SEQ ORBIT ENGINE", 82, 134, 320, 22, colour="ffeef6ff", size=14, group="orbit"),
        label("lanes_title", "LANE MIX", 894, 134, 220, 22, colour="ffeef6ff", size=14, group="lanes"),
        label("deck_title", "TONE / MOTION / FX", deck_frame[0] + 24, deck_frame[1] + 14, 280, 22, colour="ffeef6ff", size=13, group="deck"),
    ]
    centres = [(224, 310), (372, 310), (520, 310), (668, 310), (446, 478)]
    targets = ["drums", "notes", "effects", "samples", "notes"]
    colours = [accent, "ffffaa3d", "ffff5f91", "ff55d6ff", accent2]
    for i, (cx, cy) in enumerate(centres, start=1):
        els.append({
            "id": f"circle_lane_{i}",
            "type": "arpLane",
            "x": cx - 72,
            "y": cy - 72,
            "width": 144,
            "height": 144,
            "label": f"Lane {i}",
            "parameterId": f"mpBank{i}_mpLaneMute",
            "accentColour": colours[i - 1],
            "backgroundColour": "cc05080d",
            "borderColour": colours[i - 1],
            "labelPosition": "hidden",
            "arpLaneIndex": i - 1,
            "arpLaneSteps": 16,
            "arpLaneMode": "orbit",
            "arpLaneTarget": targets[i - 1],
            "arpLaneRootNote": 36 + (i - 1) * 5,
            "arpLaneSampleSlots": 8,
            "arpLaneDirection": "forward",
            "arpLaneRotate": (i - 1) * 2,
            "arpLaneEuclideanPulses": [4, 7, 5, 6, 9][i - 1],
            "arpLaneProbability": 0.95,
            "arpLaneRatchet": [1, 1, 2, 1, 3][i - 1],
            "arpLaneFillPulses": [0, 2, 1, 0, 3][i - 1],
            "arpLaneFillProbability": 0.25,
            "groupId": "orbit",
        })
        els += [
            button(f"lane_{i}_bypass", f"L{i}", f"mpBank{i}_mpLaneMute", 896, 178 + (i - 1) * 72, 48, 28,
                   accent=colours[i - 1], group="lanes"),
            slider(f"lane_{i}_level", f"Level {i}", f"mpBank{i}_mpLaneVelocity", 968, 170 + (i - 1) * 72, 168, 42,
                   accent=colours[i - 1], group="lanes"),
        ]
    els += knob_grid([
        ("filterCutoff", "Cutoff"), ("attack", "Atk"), ("decay", "Decay"), ("release", "Rel"),
        ("delayMix", "Delay"), ("delayFeedback", "FB"), ("reverbMix", "Space"), ("lfoAmount", "Motion"),
        ("macro_motion", "Morph"), ("macro_tone", "Tone"), ("macro_character", "Accent"), ("macro_space", "FX"),
    ], deck_frame, cols=12, accent=accent, group="circle_controls")
    return {"canvas": {"width": W, "height": H}, "elements": els}


def echocraft_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    els += [
        shape("delay_row", 130, 128, 1008, 214, bg="bb07090d", border="66505b72", radius=12, group="delay"),
        shape("routing_frame", 130, 374, 524, 178, bg="bb07090d", border=accent, radius=12, group="routing"),
        shape("shaper_frame", 684, 374, 454, 178, bg="bb07090d", border=accent2, radius=12, group="shaper"),
        shape("bottom_frame", 30, 566, 1218, 224, bg="aa080b10", border="66505b72", radius=12, group="bottom"),
        label("bottom_title", "SHAPER / TONE / OUTPUT", 56, 580, 320, 20, colour="ffeef6ff", size=13, group="bottom"),
        slider("input_meter", "Input", "inputTrimDb", 42, 154, 42, 356, accent=accent, group="io"),
        slider("output_meter", "Output", "outputGainDb", 1196, 154, 42, 356, accent=accent2, group="io"),
        label("delay_title", "FOUR STAGE DELAY WORKSTATION", 154, 146, 330, 22, colour="ffeef6ff", size=14, group="delay"),
        {"id": "feedback_curve", "type": "eqCurve", "x": 728, "y": 414, "width": 360, "height": 96,
         "parameterId": "delayFeedback", "accentColour": accent, "backgroundColour": "cc04070b",
         "borderColour": "66505b72", "label": "Feedback", "labelPosition": "hidden", "groupId": "shaper"},
        {"id": "scope", "type": "spectrumAnalyzer", "x": 174, "y": 414, "width": 424, "height": 96,
         "parameterId": "multiTapMix", "accentColour": accent2, "backgroundColour": "cc04070b",
         "borderColour": "66505b72", "label": "Routing", "labelPosition": "hidden", "groupId": "routing"},
    ]
    for i, name in enumerate(["DIGITAL", "TAPE", "DUAL", "REVERSE"]):
        x = 154 + i * 244
        els += [
            shape(f"delay_card_{i+1}", x, 170, 216, 144, bg="dd0a0d11", border=accent if i % 2 == 0 else accent2,
                  radius=8, group="delay"),
            label(f"delay_card_label_{i+1}", name, x + 18, 184, 100, 20,
                  colour=accent if i % 2 == 0 else accent2, size=12, group="delay"),
            knob(f"delay_{i+1}_time", "Time", "delayTime" if i == 0 else "multiTapTime", x + 18, 214,
                 accent=accent if i % 2 == 0 else accent2, size=62, group="delay"),
            knob(f"delay_{i+1}_fb", "FB", "delayFeedback" if i < 2 else "multiTapFeedback", x + 118, 214,
                 accent=accent if i % 2 == 0 else accent2, size=62, group="delay"),
        ]
    els += knob_grid([
        ("drive", "Drive"), ("mix", "Mix"), ("filterCutoff", "LPF"), ("filterResonance", "Res"),
        ("tapeDrive", "Tape"), ("tapeFlutter", "Flutter"), ("chorusMix", "Chorus"), ("phaserMix", "Phaser"),
        ("dynMix", "Ducking"), ("dynThresholdDb", "Thresh"), ("spectralTilt", "Tilt"), ("convolutionMix", "Space"),
        ("multiTapSpread", "Spread"), ("reverbMix", "Reverb"), ("volume", "Output"), ("pan", "Pan"),
    ], (30, 600, 1218, 190), cols=8, accent=accent, group="fx_controls", top_pad=8)
    return {"canvas": {"width": W, "height": H}, "elements": els}


def modular_fx_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    els += [
        shape("node_field", 64, 126, 734, 436, bg="bb080b10", border=accent2, radius=18, group="nodes", glow=0.12),
        shape("macro_field", 828, 126, 334, 436, bg="aa0a1018", border=accent, radius=18, group="macros"),
        label("node_title", "MODULAR MOTION FX CHAIN", 96, 146, 320, 22, colour="ffeef6ff", size=14, group="nodes"),
        {"id": "analyzer", "type": "spectrumAnalyzer", "x": 108, "y": 180, "width": 642, "height": 126,
         "parameterId": "spectralMix", "accentColour": accent2, "backgroundColour": "cc04070b",
         "borderColour": "66505b72", "label": "Analyzer", "labelPosition": "hidden", "groupId": "nodes"},
        {"id": "eq_curve", "type": "eqCurve", "x": 108, "y": 338, "width": 642, "height": 116,
         "parameterId": "filterCutoff", "accentColour": accent, "backgroundColour": "cc04070b",
         "borderColour": "66505b72", "label": "Tone", "labelPosition": "hidden", "groupId": "nodes"},
        {"id": "meter", "type": "meter", "x": 730, "y": 182, "width": 34, "height": 270,
         "parameterId": "outputGainDb", "accentColour": accent, "backgroundColour": "cc04070b",
         "borderColour": "66505b72", "label": "Out", "labelPosition": "hidden", "groupId": "nodes"},
    ]
    els += [label("macro_title", "FX RACK", 852, 146, 220, 22, colour="ffeef6ff", size=14, group="macros")]
    els += knob_grid([
        ("drive", "Drive"), ("mix", "Mix"), ("filterCutoff", "Cutoff"), ("delayMix", "Delay"),
        ("multiTapMix", "Tap"), ("chorusMix", "Chorus"), ("phaserMix", "Phaser"), ("combMix", "Comb"),
        ("resonatorMix", "Res"), ("tapeMix", "Tape"), ("vinylMix", "Vinyl"), ("lofiMix", "LoFi"),
        ("vocalMix", "Formant"), ("dynMix", "Comp"), ("convolutionMix", "Space"), ("spectralMix", "Tilt"),
        ("macro_motion", "Motion"), ("macro_tone", "Tone"), ("macro_character", "Crush"), ("macro_space", "Wide"),
    ], (828, 126, 334, 436), cols=4, accent=accent, group="motion_fx")
    els += [
        slider("input_trim", "Input", "inputTrimDb", 96, 608, 250, 44, accent=accent2, group="bottom"),
        slider("feedback", "Feedback", "delayFeedback", 374, 608, 250, 44, accent=accent, group="bottom"),
        slider("width", "Width", "stereoWidth", 652, 608, 250, 44, accent=accent2, group="bottom"),
        slider("output", "Output", "outputGainDb", 930, 608, 250, 44, accent=accent, group="bottom"),
    ]
    return {"canvas": {"width": W, "height": H}, "elements": els}


STEP_PATTERNS: list[dict[str, Any]] = [
    {
        "name": "Driving 16ths",
        "desc": "Fast 16th pulse — hold Cm7, tight gate, bright filter.",
        "on": [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
        "notes": [0, 0, 3, 0, 5, 0, 7, 0, 10, 0, 12, 0, 15, 0, 17, 0],
        "velocity": [0.88, 0.2, 0.72, 0.2, 0.84, 0.2, 0.76, 0.2, 0.82, 0.2, 0.74, 0.2, 0.8, 0.2, 0.7, 0.2],
        "gate": 0.42, "rate": 1.0, "swing": 0.04,
        "attack": 0.01, "decay": 0.22, "sustain": 0.18, "release": 0.28,
        "cutoff": 5200.0, "res": 0.16, "delay": 0.12, "reverb": 0.08,
    },
    {
        "name": "Octave Ladder",
        "desc": "Quarter-note jumps up octaves — good for hook lines.",
        "on": [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
        "notes": [0, 0, 0, 0, 12, 0, 0, 0, 24, 0, 0, 0, 36, 0, 0, 0],
        "velocity": [0.9, 0.2, 0.2, 0.2, 0.82, 0.2, 0.2, 0.2, 0.78, 0.2, 0.2, 0.2, 0.74, 0.2, 0.2, 0.2],
        "gate": 0.56, "rate": 1.0, "swing": 0.06,
        "attack": 0.01, "decay": 0.22, "sustain": 0.18, "release": 0.28,
        "cutoff": 5600.0, "res": 0.14, "delay": 0.14, "reverb": 0.10,
    },
    {
        "name": "Syncopated Funk",
        "desc": "Off-beat 16ths with swing — try Em9.",
        "on": [1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0],
        "notes": [0, 0, 0, 7, 0, 0, 10, 0, 0, 12, 0, 0, 15, 0, 0, 17],
        "velocity": [0.86, 0.2, 0.2, 0.7, 0.2, 0.2, 0.74, 0.2, 0.2, 0.68, 0.2, 0.2, 0.72, 0.2, 0.2, 0.66],
        "gate": 0.36, "rate": 1.0, "swing": 0.14,
        "attack": 0.008, "decay": 0.20, "sustain": 0.16, "release": 0.24,
        "cutoff": 4800.0, "res": 0.22, "delay": 0.10, "reverb": 0.06,
    },
    {
        "name": "Wide Pad Arp",
        "desc": "Slow half-note arp with space and long gate — hold Am.",
        "on": [1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0],
        "notes": [0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 7, 0, 0, 0],
        "velocity": [0.72, 0.2, 0.2, 0.2, 0.2, 0.2, 0.64, 0.2, 0.2, 0.2, 0.2, 0.2, 0.6, 0.2, 0.2, 0.2],
        "gate": 0.72, "rate": 0.5, "swing": 0.02,
        "attack": 0.08, "decay": 0.55, "sustain": 0.62, "release": 0.72,
        "cutoff": 6800.0, "res": 0.12, "delay": 0.22, "reverb": 0.32,
    },
    {
        "name": "Acid Step",
        "desc": "Short resonant steps at half rate — classic 303 feel.",
        "on": [1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0],
        "notes": [0, 1, 0, 0, 3, 4, 0, 0, 7, 8, 0, 0, 10, 11, 0, 0],
        "velocity": [0.92, 0.78, 0.2, 0.2, 0.84, 0.76, 0.2, 0.2, 0.8, 0.74, 0.2, 0.2, 0.76, 0.7, 0.2, 0.2],
        "gate": 0.28, "rate": 1.0, "swing": 0.08,
        "attack": 0.002, "decay": 0.18, "sustain": 0.08, "release": 0.16,
        "cutoff": 3200.0, "res": 0.52, "delay": 0.08, "reverb": 0.04,
    },
    {
        "name": "Init Quarter Notes",
        "desc": "Simple quarter-note pulse — blank canvas for your pattern.",
        "on": [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
        "notes": [0] * 16,
        "velocity": [0.8, 0.2, 0.2, 0.2, 0.8, 0.2, 0.2, 0.2, 0.8, 0.2, 0.2, 0.2, 0.8, 0.2, 0.2, 0.2],
        "gate": 0.58, "rate": 0.5, "swing": 0.0,
        "attack": 0.01, "decay": 0.22, "sustain": 0.18, "release": 0.28,
        "cutoff": 5200.0, "res": 0.16, "delay": 0.12, "reverb": 0.08,
    },
]


def seed_step_pattern(values: dict[str, float], pattern: dict[str, Any]) -> None:
    values["arpLaneRate"] = pattern["rate"]
    values["arpLaneGate"] = pattern["gate"]
    values["arpLaneSwing"] = pattern["swing"]
    values["arpLaneProbability"] = 1.0
    values["projectBpm"] = 120.0
    values["bpmSync"] = 1.0
    for step in range(16):
        values[f"mpStep{step}On"] = float(pattern["on"][step])
        values[f"arpNote{step}"] = float(pattern["notes"][step])
        values[f"mpVelocity{step}"] = float(pattern["velocity"][step])
        values[f"mpGate{step}"] = pattern["gate"]
        values[f"mpStepProb{step}"] = 1.0


def append_step_sequencer_params(params: list[dict[str, Any]]) -> None:
    params += [
        param("arpLaneRate", "Seq Rate", 0.25, 4.0, 1.0, "x", "sequencer"),
        param("arpLaneGate", "Seq Gate", 0.05, 1.0, 0.58, "", "sequencer"),
        param("arpLaneSwing", "Swing", 0.0, 0.5, 0.06, "", "sequencer"),
        param("arpLaneProbability", "Chance", 0.0, 1.0, 1.0, "", "sequencer"),
    ]
    for step in range(16):
        params += [
            param(f"arpNote{step}", f"Step {step + 1} Note", -24, 36, 0, "st", "sequencer", "stepped", 1),
            param(f"mpStep{step}On", f"Step {step + 1}", 0, 1, 1, "", "sequencer", "toggle", 1),
            param(f"mpVelocity{step}", f"Step {step + 1} Vel", 0, 1, 0.7, "", "sequencer"),
            param(f"mpGate{step}", f"Step {step + 1} Gate", 0, 1, 0.34, "", "sequencer"),
            param(f"mpStepProb{step}", f"Step {step + 1} Chance", 0, 1, 1, "", "sequencer"),
        ]


def step_arp_block_values(pattern: dict[str, Any]) -> dict[str, float]:
    values: dict[str, float] = {
        "sync": 1.0, "rate": pattern["rate"], "arpSteps": 16.0,
        "arpGate": pattern["gate"], "mpProbability": 1.0,
        "mpActiveBank": 0.0, "mpMultiLane": 1.0,
    }
    for step in range(16):
        values[f"mpStep{step}On"] = float(pattern["on"][step])
        values[f"arpNote{step}"] = float(pattern["notes"][step])
        values[f"mpVelocity{step}"] = float(pattern["velocity"][step])
        values[f"mpGate{step}"] = pattern["gate"]
        values[f"mpStepProb{step}"] = 1.0
    return values


def arpstep_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    els += [
        shape("sequencer_panel", 40, 204, 680, 472, bg="bb070b12", border=accent, radius=18, glow=0.14, group="seq"),
        shape("macro_panel", 736, 204, 504, 472, bg="aa0a1018", border=accent2, radius=18, group="macros"),
        label("seq_title", "16-STEP ORBIT", 64, 224, 220, 22, colour="ffeef6ff", size=14, group="seq"),
        label("macro_title", "TONE + MOTION", 760, 224, 220, 22, colour="ffeef6ff", size=14, group="macros"),
        {
            "id": "step_orbit",
            "type": "arpLane",
            "x": 56,
            "y": 256,
            "width": 648,
            "height": 400,
            "label": "16 Steps",
            "parameterId": "arpLaneRate",
            "accentColour": accent,
            "backgroundColour": "ee090d12",
            "borderColour": "ff4a5568",
            "labelPosition": "hidden",
            "arpLaneIndex": 0,
            "arpLaneSteps": 16,
            "arpLaneMode": "multiRing",
            "arpLaneTarget": "notes",
            "groupId": "seq",
        },
        {"id": "out_meter", "type": "meter", "x": 760, "y": 642, "width": 456, "height": 22,
         "parameterId": "volume", "accentColour": accent, "backgroundColour": "cc04070b",
         "borderColour": "66505b72", "label": "Output", "labelPosition": "hidden", "groupId": "macros"},
        {"id": "keyboard", "type": "keyboard", "x": 180, "y": 696, "width": 838, "height": 74,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
    ]
    els += knob_grid([
        ("arpLaneRate", "Rate"), ("arpLaneGate", "Gate"), ("arpLaneSwing", "Swing"), ("arpLaneProbability", "Chance"),
        ("filterCutoff", "Cutoff"), ("filterResonance", "Res"), ("delayMix", "Delay"), ("reverbMix", "Space"),
        ("attack", "Attack"), ("decay", "Decay"), ("lfoRate", "LFO"), ("lfoAmount", "LFO Amt"),
        ("macro_motion", "Motion"), ("macro_tone", "Tone"), ("macro_character", "Color"), ("macro_space", "Wide"),
    ], (736, 250, 504, 372), cols=4, accent=accent, group="arp_macros")
    return {"canvas": {"width": W, "height": H}, "elements": els}


def arpstep_presets() -> list[dict[str, Any]]:
    out = []
    for i, pattern in enumerate(STEP_PATTERNS):
        values = {
            "oscType": 0.0, "osc2Type": 1.0 if i == 4 else 0.0, "oscBlend": 0.12,
            "octave": 0.0, "detune": 4.0, "osc2Detune": -3.0, "subBlend": 0.06,
            "attack": pattern["attack"], "decay": pattern["decay"],
            "sustain": pattern["sustain"], "release": pattern["release"],
            "filterCutoff": pattern["cutoff"], "filterResonance": pattern["res"],
            "delayMix": pattern["delay"], "reverbMix": pattern["reverb"],
            "volume": 0.78, "outputGainDb": -3.0,
            "lfoRate": [0.5, 1.0, 2.0, 0.25, 4.0, 1.0][i],
            "lfoAmount": [0.04, 0.06, 0.10, 0.02, 0.18, 0.05][i],
            "macro_motion": i / 6.0, "macro_tone": 0.45, "macro_character": 0.35, "macro_space": pattern["reverb"],
        }
        seed_step_pattern(values, pattern)
        out.append(preset(
            pattern["name"], pattern["desc"], values,
            ["arp", "sequencer", "step", "true-arp"], default=(i == 0),
        ))
    return out


def aurora_arp_presets() -> list[dict[str, Any]]:
    tones = [
        (0, 1, 0.06, 0, 4, -3, 0.10, 0.005, 0.28, 0.45, 0.28, 6200, 0.18, 0.25, 0.18, 0.32, 0.72, 124),
        (1, 0, 0.12, 0, 8, -7, 0.12, 0.70, 1.4, 0.82, 2.8, 2600, 0.22, 0.50, 0.30, 0.62, 0.66, 96),
        (1, 2, 0.08, 0, 5, -5, 0.04, 0.002, 0.28, 0.30, 0.28, 5200, 0.30, 0.1875, 0.28, 0.34, 0.70, 128),
        (0, 1, 0.10, 0, 3, -4, 0.08, 0.018, 0.42, 0.48, 0.62, 3800, 0.16, 0.375, 0.16, 0.24, 0.74, 100),
        (1, 0, 0.05, 1, 6, -6, 0.00, 0.004, 0.28, 0.68, 0.74, 8200, 0.20, 0.25, 0.22, 0.38, 0.70, 126),
        (0, 3, 0.16, 0, 9, -9, 0.18, 0.95, 1.8, 0.74, 3.4, 2100, 0.10, 0.50, 0.20, 0.70, 0.62, 84),
        (1, 2, 0.14, 0, 10, -11, 0.04, 0.002, 0.26, 0.28, 0.26, 4700, 0.38, 0.125, 0.36, 0.28, 0.68, 132),
        (0, 0, 0.18, 1, 2, 7, 0.00, 0.001, 0.80, 0.38, 1.2, 9000, 0.12, 0.25, 0.30, 0.58, 0.64, 110),
        (2, 0, 0.04, -1, 0, 0, 0.22, 0.003, 0.18, 0.45, 0.18, 1250, 0.42, 0.1875, 0.05, 0.05, 0.82, 124),
        (0, 2, 0.10, 0, 4, -8, 0.06, 0.001, 0.28, 0.32, 0.28, 5900, 0.26, 0.1875, 0.42, 0.36, 0.68, 118),
        (1, 0, 0.09, 1, 14, -9, 0.03, 0.004, 0.36, 0.76, 1.1, 7600, 0.24, 0.25, 0.30, 0.46, 0.72, 128),
        (0, 3, 0.08, 0, 1, -2, 0.06, 0.45, 1.4, 0.62, 2.4, 3600, 0.14, 0.50, 0.20, 0.60, 0.60, 88),
    ]
    names = [
        "Glass Pentatonic Arp", "Warm Fifth Pad Arp", "Clean Motion Pluck Arp", "Soft Analog Keys Arp",
        "Bright Hook Arp", "Velvet Chord Arp", "Wide Sync Arp", "Mellow Bell Arp",
        "Round Bass Arp", "Sequence Glass Arp", "Festival Lead Arp", "Soft Closing Arp",
    ]
    out = []
    for i, tone in enumerate(tones):
        osc, osc2, blend, octv, det, det2, sub, atk, dec, sus, rel, cutoff, res, dt, dm, rv, vol, bpm = tone
        pattern = STEP_PATTERNS[i % len(STEP_PATTERNS)]
        values = {
            "oscType": float(osc), "osc2Type": float(osc2), "oscBlend": blend, "octave": float(octv),
            "detune": float(det), "osc2Detune": float(det2), "subBlend": sub,
            "attack": atk, "decay": dec, "sustain": sus, "release": rel,
            "filterCutoff": float(cutoff), "filterResonance": res,
            "delayTime": dt, "delayFeedback": 0.22 + (i % 5) * 0.045, "delayMix": dm,
            "reverbMix": rv, "volume": vol, "projectBpm": float(bpm),
            "lfoRate": [0.5, 1, 2, 4, 6, 8][i % 6], "lfoAmount": [0.02, 0.05, 0.08, 0.12, 0.18, 0.24][i % 6],
            "macro_motion": (i % 6) / 6.0, "macro_tone": 0.35 + (i % 5) * 0.1,
            "macro_character": 0.25 + (i % 7) * 0.08, "macro_space": rv,
            "outputGainDb": -4.0 if vol > 0.78 else -3.0,
        }
        seed_step_pattern(values, pattern)
        out.append(preset(
            names[i],
            f"{pattern['desc']} Hold a chord at {int(bpm)} BPM.",
            values, ["synth", "arp", "melodic"], default=(i == 0),
        ))
    return out


# ---------------------------------------------------------------------------
# Piano-roll (Chord/MIDI clip) demo
# ---------------------------------------------------------------------------

PIANO_ROLL_STEPS = 16
PIANO_ROLL_STEPS_PER_BEAT = 4
PIANO_ROLL_LOW_NOTE = 48
PIANO_ROLL_ROWS = 27


def piano_roll_notes() -> str:
    """A I-vi-IV-V progression in C major, one 4-7 voicing chord per beat.

    Returns the compact "start,len,pitch,vel;..." encoding the runtime reads
    from the pianoRoll DSP block's metadata.
    """
    chords = [
        (0, [60, 64, 67, 71]),   # Cmaj7
        (4, [57, 60, 64, 67]),   # Am7
        (8, [53, 57, 60, 64]),   # Fmaj7
        (12, [55, 59, 62, 65]),  # G7
    ]
    parts: list[str] = []
    for start, pitches in chords:
        for idx, pitch in enumerate(pitches):
            # Stagger the top voice velocity slightly for a musical feel.
            vel = 0.86 if idx == len(pitches) - 1 else 0.78
            parts.append(f"{start},4,{pitch},{vel:.2f}")
    return ";".join(parts)


def piano_roll_block_values() -> dict[str, float]:
    return {
        "prSteps": float(PIANO_ROLL_STEPS),
        "prStepsPerBeat": float(PIANO_ROLL_STEPS_PER_BEAT),
        "prLowNote": float(PIANO_ROLL_LOW_NOTE),
        "prRows": float(PIANO_ROLL_ROWS),
        "prRate": 1.0,
        "prGate": 0.92,
        "prVelocity": 1.0,
        "prSync": 1.0,
        "prLoop": 1.0,
    }


def pianoroll_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    els += [
        # Hero piano roll across the top.
        shape("piano_frame", 54, 114, 1172, 252, bg="cc090d14", border=accent, radius=16, glow=0.12, group="roll"),
        label("piano_title", "PIANO ROLL  -  EDITABLE MIDI CLIP", 78, 124, 460, 22,
              colour="ffeef6ff", size=15, group="roll"),
        {
            "id": "piano_roll",
            "type": "pianoRoll",
            "x": 70,
            "y": 150,
            "width": 1140,
            "height": 204,
            "label": "Chord Clip",
            "accentColour": accent,
            "backgroundColour": "dd05080d",
            "borderColour": accent2,
            "labelPosition": "hidden",
            "pianoRollSteps": PIANO_ROLL_STEPS,
            "pianoRollStepsPerBeat": PIANO_ROLL_STEPS_PER_BEAT,
            "pianoRollLowNote": PIANO_ROLL_LOW_NOTE,
            "pianoRollRows": PIANO_ROLL_ROWS,
            "groupId": "roll",
        },
        # Sound panel.
        shape("sound_frame", 54, 378, 560, 234, bg="aa0a1018", border=accent, radius=14, group="sound"),
        label("sound_title", "SOUND ENGINE", 78, 392, 200, 20, colour="ffeef6ff", size=13, group="sound"),
        dropdown("wave_select", "Wave", "oscType", 360, 390, 120, 26, accent=accent, group="sound"),
        dropdown("wave2_select", "Wave 2", "osc2Type", 488, 390, 120, 26, accent=accent2, group="sound"),
        # Perform panel.
        shape("perform_frame", 628, 378, 598, 234, bg="aa0a1018", border=accent2, radius=14, group="perform"),
        label("perform_title", "PERFORM", 652, 392, 200, 20, colour="ffeef6ff", size=13, group="perform"),
        {"id": "transport_play", "type": "button", "x": 652, "y": 424, "width": 130, "height": 160,
         "label": "PLAY / STOP", "action": "transport.toggle", "accentColour": accent,
         "backgroundColour": "dd080b10", "borderColour": accent, "labelPosition": "hidden",
         "labelSize": 12.0, "groupId": "perform"},
        {"id": "keyboard", "type": "keyboard", "x": 180, "y": 636, "width": 838, "height": 80,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
    ]
    els += knob_grid([
        ("filterCutoff", "Cutoff"), ("filterResonance", "Res"), ("attack", "Atk"), ("release", "Rel"),
        ("lfoRate", "LFO Hz"), ("lfoAmount", "LFO Amt"), ("octave", "Oct"), ("detune", "Detune"),
    ], (54, 418, 560, 194), cols=4, accent=accent, group="sound", top_pad=6)
    els += knob_grid([
        ("delayMix", "Delay"), ("reverbMix", "Reverb"), ("macro_tone", "Tone"),
        ("macro_space", "Space"), ("macro_motion", "Motion"), ("macro_character", "Color"),
    ], (804, 418, 422, 194), cols=3, accent=accent2, group="perform", top_pad=6)
    return {"canvas": {"width": W, "height": H}, "elements": els}


def pianoroll_presets() -> list[dict[str, Any]]:
    # Each preset is a distinct, musical synth tone for the same editable clip.
    specs = [
        ("Warm Chord Keys", 0, 1, 0.10, 0, 4, -4, 0.08, .012, .40, .55, .50, 4200, .16, .375, .18, .34, .74, 120, True),
        ("Lush Chord Pad", 1, 0, 0.16, 0, 8, -7, 0.14, .60, 1.6, .82, 3.0, 2600, .18, .5, .24, .60, .66, 110, False),
        ("Clean Pluck Stab", 1, 2, 0.08, 0, 5, -5, 0.04, .002, .26, .28, .26, 5600, .26, .1875, .26, .30, .72, 124, False),
        ("Glass Bell Chords", 0, 0, 0.06, 1, 2, 7, 0.00, .001, .60, .34, .90, 9200, .12, .25, .30, .50, .64, 116, False),
        ("Round Sub Chords", 2, 0, 0.05, -1, 0, 0, 0.20, .004, .22, .48, .24, 1900, .30, .1875, .10, .14, .80, 118, False),
        ("Bright Hook Synth", 1, 0, 0.06, 1, 6, -6, 0.00, .004, .30, .66, .70, 7600, .20, .25, .24, .40, .72, 126, False),
    ]
    out = []
    for s in specs:
        (name, osc, osc2, blend, octv, det, det2, sub, atk, dec, sus, rel,
         cutoff, res, dt, dm, rv, vol, bpm, default) = s
        values = {
            "oscType": float(osc), "osc2Type": float(osc2), "oscBlend": blend, "octave": float(octv),
            "detune": float(det), "osc2Detune": float(det2), "subBlend": sub,
            "attack": atk, "decay": dec, "sustain": sus, "release": rel,
            "filterCutoff": float(cutoff), "filterResonance": res,
            "delayTime": dt, "delayFeedback": 0.24, "delayMix": dm, "reverbMix": rv,
            "volume": vol, "projectBpm": float(bpm),
            "macro_tone": 0.55, "macro_space": rv + 0.1, "macro_motion": 0.4, "macro_character": 0.5,
            "lfoRate": 1.0, "lfoAmount": 0.05,
        }
        out.append(preset(name, f"{name} - editable chord clip", values, ["pianoroll", "chords", "synth"], default))
    return out


def beatforge_layout(pack: dict[str, Any]) -> dict[str, Any]:
    """Flagship sampler beat workstation: chop pads + waveform + step arp + FX."""
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    arp_pattern = [1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1]
    els += [
        # Left: chop pads.
        shape("pad_frame", 52, 126, 452, 300, bg="bb080b10", border=accent, radius=18, glow=0.14, group="pads"),
        label("pad_title", "CHOP PADS", 78, 144, 220, 22, colour="ffeef6ff", size=14, group="pads"),
        {"id": "pad_bank", "type": "padGrid", "x": 78, "y": 180, "width": 400, "height": 230,
         "parameterId": "pad1Volume", "padRows": 4, "padCols": 4, "padBaseNote": 36,
         "accentColour": accent, "backgroundColour": "dd06090f", "borderColour": "66505b72",
         "label": "Pads", "labelPosition": "hidden", "groupId": "pads"},
        # Left lower: step arp orbit.
        shape("arp_frame", 52, 438, 452, 192, bg="aa0a1018", border=accent2, radius=16, group="arp"),
        label("arp_title", "STEP ARP", 78, 452, 220, 20, colour="ffeef6ff", size=13, group="arp"),
        {"id": "step_orbit", "type": "arpLane", "x": 76, "y": 478, "width": 404, "height": 142,
         "label": "Steps", "parameterId": "arpLaneRate", "accentColour": accent2,
         "backgroundColour": "cc05080d", "borderColour": accent2, "labelPosition": "hidden",
         "arpLaneIndex": 0, "arpLaneSteps": 16, "arpLaneMode": "multiRing", "arpLaneTarget": "notes",
         "arpLanePattern": arp_pattern, "groupId": "arp"},
        # Right top: sample performance + chop tools.
        shape("sample_frame", 524, 126, 704, 202, bg="bb080b10", border=accent, radius=18, glow=0.12, group="sample"),
        label("sample_title", "SAMPLE PERFORMANCE  -  CHOP & FLIP", 548, 144, 460, 22,
              colour="ffeef6ff", size=14, group="sample"),
        {"id": "waveform", "type": "waveform", "x": 548, "y": 178, "width": 548, "height": 118,
         "parameterId": "sampleStart", "accentColour": accent, "backgroundColour": "cc06090f",
         "borderColour": "66505b72", "label": "Waveform", "labelPosition": "hidden", "groupId": "sample"},
        {"id": "sample_drop", "type": "sampleDropZone", "x": 1108, "y": 172, "width": 104, "height": 128,
         "parameterId": "sampleStart", "accentColour": accent2, "backgroundColour": "cc06090f",
         "borderColour": accent2, "label": "Drop", "labelPosition": "hidden", "groupId": "sample"},
        button("grain_toggle", "GRAIN", "granularOn", 548, 302, 86, 20, accent=accent2, group="sample"),
        dropdown("slice_select", "Slices", "sampleSliceCount", 648, 300, 104, 24, accent=accent, group="sample"),
        # Right lower: sound / motion / FX control bank.
        shape("control_frame", 524, 344, 704, 300, bg="aa0a1018", border=accent, radius=16, group="controls"),
        label("control_title", "SOUND  /  MOTION  /  FX", 548, 358, 340, 20, colour="ffeef6ff", size=13, group="controls"),
        {"id": "keyboard", "type": "keyboard", "x": 180, "y": 662, "width": 838, "height": 74,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
    ]
    els += knob_grid([
        ("sampleStart", "Start"), ("sampleLength", "Length"), ("samplePitch", "Tune"),
        ("sampleSlice", "Slice"), ("sampleSliceCount", "Slices"), ("sampleGlitch", "Glitch"),
        ("velocitySensitivity", "Vel Sens"), ("filterCutoff", "Cutoff"), ("filterResonance", "Res"),
        ("attack", "Atk"), ("release", "Rel"), ("arpLaneRate", "Rate"),
        ("arpLaneGate", "Gate"), ("arpLaneSwing", "Swing"), ("arpLaneProbability", "Chance"),
        ("delayMix", "Delay"), ("reverbMix", "Space"), ("macro_motion", "Motion"),
    ], (524, 344, 704, 300), cols=6, accent=accent, group="bf_controls")
    return {"canvas": {"width": W, "height": H}, "elements": els}


def beatforge_presets() -> list[dict[str, Any]]:
    names = [
        "Boom Bap Foundry", "Trap Chop Kit", "House Bounce", "Lo-Fi Flip", "Drill Slide",
        "Soulful Chops", "Footwork Cuts", "Glitch Flip", "Half-Time Heavy", "Clean Export Beat",
    ]
    out = []
    for i, name in enumerate(names):
        values = {
            "attack": [0.002, .004, .001, .008, .003][i % 5],
            "decay": [0.18, .24, .14, .30, .20][i % 5],
            "sustain": [0.0, .2, .1, .3, .15][i % 5],
            "release": [0.16, .24, .12, .34, .20][i % 5],
            "sampleStart": [0.0, .02, .01, .04, .0][i % 5],
            "sampleLength": [1.0, .86, .72, .92, .64][i % 5],
            "sampleSliceCount": [8, 16, 4, 12][i % 4],
            "sampleSlice": [0.0, .25, .5, .12, .38][i % 5],
            "sampleGlitch": [0.0, .08, .16, .04, .12][i % 5],
            "sampleGlitchGrid": [0.25, .5, .125, .375][i % 4],
            "velocitySensitivity": [0.35, .55, .75, .5, .65][i % 5],
            "granularOn": 1.0 if i in {3, 5, 7} else 0.0,
            "granularDensity": .3 + (i % 5) * .08,
            "granularSizeMs": 50 + (i % 6) * 22,
            "granularScan": [-.6, -.2, .2, .6, 0][i % 5],
            "filterCutoff": [2600, 4200, 6400, 9200, 12000][i % 5],
            "filterResonance": [0.1, .18, .26, .32, .14][i % 5],
            "arpLaneRate": [0.5, 1.0, 2.0, 1.0, 0.25][i % 5],
            "arpLaneGate": [0.5, .62, .4, .7, .55][i % 5],
            "arpLaneSwing": [0.06, .12, .18, .0, .08][i % 5],
            "arpLaneProbability": 1.0,
            "delayTime": [0.1875, .25, .375, .125, .5][i % 5],
            "delayFeedback": [0.1, .2, .28, .14, .32][i % 5],
            "delayMix": [0.05, .1, .16, .08, .2][i % 5],
            "reverbMix": [0.08, .14, .22, .3, .18][i % 5],
            "volume": .8, "outputGainDb": -4.0,
            "macro_motion": (i % 7) / 7.0, "macro_tone": .3 + (i % 5) * .1,
            "macro_character": .25 + (i % 6) * .09, "macro_space": [0.1, .2, .32, .45, .55][i % 5],
        }
        for pad in range(1, 17):
            values[f"pad{pad}Volume"] = round(0.78 + ((i + pad) % 5) * .06, 4)
            values[f"pad{pad}Pitch"] = float(((i + pad) % 7) - 3)
            values[f"pad{pad}Pan"] = [-0.2, -0.1, 0.0, 0.1, 0.2][(i + pad) % 5]
        for step in range(16):
            values[f"mpStep{step}On"] = 1.0 if step % 2 == 0 or (step + i) % 3 == 0 else 0.0
            values[f"arpNote{step}"] = float([0, 3, 5, 7, 10, 12, 7, 3][step % 8])
            values[f"mpVelocity{step}"] = round(0.5 + ((step + i) % 5) * .09, 4)
            values[f"mpGate{step}"] = round(0.3 + (step % 4) * .08, 4)
            values[f"mpStepProb{step}"] = 1.0
        out.append(preset(name, "Beat workstation preset: chop pads, velocity layers, step arp, and FX.",
                          values, ["sampler", "beats", "chop"], i == 0))
    return out


def keysflagship_layout(pack: dict[str, Any]) -> dict[str, Any]:
    """Flagship keys: editable runtime piano roll + dual-osc synth + full FX chain."""
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    els += [
        # Hero piano roll.
        shape("piano_frame", 54, 114, 1172, 236, bg="cc090d14", border=accent, radius=16, glow=0.12, group="roll"),
        label("piano_title", "PIANO ROLL  -  EDITABLE MIDI CLIP", 78, 124, 460, 22,
              colour="ffeef6ff", size=15, group="roll"),
        {"id": "transport_play", "type": "button", "x": 1066, "y": 122, "width": 140, "height": 28,
         "label": "PLAY / STOP", "action": "transport.toggle", "accentColour": accent,
         "backgroundColour": "dd080b10", "borderColour": accent, "labelPosition": "hidden",
         "labelSize": 11.0, "groupId": "roll"},
        {
            "id": "piano_roll", "type": "pianoRoll", "x": 70, "y": 156, "width": 1140, "height": 182,
            "label": "Chord Clip", "accentColour": accent, "backgroundColour": "dd05080d",
            "borderColour": accent2, "labelPosition": "hidden",
            "pianoRollSteps": PIANO_ROLL_STEPS, "pianoRollStepsPerBeat": PIANO_ROLL_STEPS_PER_BEAT,
            "pianoRollLowNote": PIANO_ROLL_LOW_NOTE, "pianoRollRows": PIANO_ROLL_ROWS, "groupId": "roll",
        },
        # Sound engine.
        shape("sound_frame", 54, 362, 470, 272, bg="aa0a1018", border=accent, radius=14, group="sound"),
        label("sound_title", "SOUND ENGINE", 78, 376, 220, 20, colour="ffeef6ff", size=13, group="sound"),
        dropdown("wave_select", "Wave", "oscType", 300, 374, 100, 26, accent=accent, group="sound"),
        dropdown("wave2_select", "Wave 2", "osc2Type", 408, 374, 100, 26, accent=accent2, group="sound"),
        # Motion + FX chain.
        shape("fx_frame", 538, 362, 688, 272, bg="aa0a1018", border=accent2, radius=14, group="fx"),
        label("fx_title", "MOTION + FX CHAIN", 562, 376, 260, 20, colour="ffeef6ff", size=13, group="fx"),
        {"id": "spectrum", "type": "spectrumAnalyzer", "x": 562, "y": 400, "width": 640, "height": 56,
         "parameterId": "macro_motion", "accentColour": accent2, "backgroundColour": "cc06090f",
         "borderColour": "66505b72", "label": "Motion", "labelPosition": "hidden", "groupId": "fx"},
        {"id": "keyboard", "type": "keyboard", "x": 200, "y": 652, "width": 880, "height": 78,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
    ]
    els += knob_grid([
        ("oscBlend", "Blend"), ("octave", "Oct"), ("detune", "Detune"), ("subBlend", "Sub"),
        ("filterCutoff", "Cutoff"), ("filterResonance", "Res"), ("attack", "Atk"), ("release", "Rel"),
    ], (54, 418, 470, 210), cols=4, accent=accent, group="kf_sound", top_pad=6)
    els += knob_grid([
        ("lfoRate", "LFO Hz"), ("lfoAmount", "LFO Amt"), ("delayMix", "Delay"), ("reverbMix", "Reverb"),
        ("chorusMix", "Chorus"), ("tapeMix", "Tape"), ("multiTapMix", "Taps"), ("macro_space", "Space"),
    ], (538, 462, 688, 166), cols=4, accent=accent2, group="kf_fx", top_pad=6)
    return {"canvas": {"width": W, "height": H}, "elements": els}


def dsp_graph(engine: str, name: str, defaults: dict[str, float], *,
              step_arp: bool = False, arpstep: bool = False, pianoroll: bool = False) -> dict[str, Any]:
    blocks = [
        {"id": "source", "section": "source", "type": "oscillator" if engine == "synth" else ("sample" if engine == "sample" else "liveInput"),
         "name": f"{name} Source", "enabled": True, "targetId": "volume", "values": {
             "oscType": defaults.get("oscType", 0.0),
             "osc2Type": defaults.get("osc2Type", 0.0),
             "oscBlend": defaults.get("oscBlend", 0.0),
             "subBlend": defaults.get("subBlend", 0.0),
             "noiseBlend": 0.0,
             "sampleStart": defaults.get("sampleStart", 0.0),
             "sampleLength": defaults.get("sampleLength", 1.0),
         }},
        {"id": "shape", "section": "shape", "type": "filter", "name": "Tone Shape", "enabled": True,
         "targetId": "filterCutoff", "values": {
             "filterCutoff": defaults.get("filterCutoff", 5200.0),
             "filterResonance": defaults.get("filterResonance", 0.18),
             "attack": defaults.get("attack", 0.01),
             "decay": defaults.get("decay", 0.22),
             "sustain": defaults.get("sustain", 0.42),
             "release": defaults.get("release", 0.45),
         }},
        {"id": "fx", "section": "fx", "type": "fxChain", "name": "Factory FX", "enabled": True,
         "targetId": "delayMix", "values": {
             "drive": defaults.get("drive", 0.0),
             "mix": defaults.get("mix", 0.55 if engine == "fx" else 1.0),
             "delayTime": defaults.get("delayTime", 0.25),
             "delayFeedback": defaults.get("delayFeedback", 0.22),
             "delayMix": defaults.get("delayMix", 0.10),
             "reverbMix": defaults.get("reverbMix", 0.16),
             "tapeMix": defaults.get("tapeMix", 0.0),
             "chorusMix": defaults.get("chorusMix", 0.0),
             "multiTapMix": defaults.get("multiTapMix", 0.0),
         }},
    ]
    if "CircleSEQ" in name:
        values = {"sync": 1.0, "rate": 1.0, "arpSteps": 16.0, "mpActiveBank": 0.0,
                  "mpScaleRoot": 0.0, "mpScaleType": 2.0, "mpProbability": 1.0, "mpHumanize": 0.01}
        notes = [0, 3, 7, 10, 12, 15, 19, 22, 19, 15, 12, 10, 7, 3, 0, -5]
        for i, n in enumerate(notes):
            values[f"arpNote{i}"] = float(n)
            values[f"mpStep{i}On"] = 1.0 if i not in {11, 15} else 0.0
            values[f"mpVelocity{i}"] = 0.48 + (i % 4) * 0.08
            values[f"mpGate{i}"] = 0.34
            values[f"mpStepProb{i}"] = 1.0
        for lane in range(1, 6):
            values[f"mpBank{lane}_mpLaneMute"] = 0.0
            values[f"mpBank{lane}_mpLaneSolo"] = 0.0
            values[f"mpBank{lane}_mpLaneRetrigger"] = 1.0
            values[f"mpBank{lane}_mpLaneVelocity"] = 0.62 + lane * 0.04
            values[f"mpBank{lane}_mpLaneTarget"] = float(lane - 1)
        blocks.append({"id": "circle_engine", "section": "motion", "type": "midiPlayground",
                       "name": "Five Lane Circle Sequencer", "enabled": True,
                       "targetId": "filterCutoff", "values": values})
    elif arpstep:
        pattern = STEP_PATTERNS[0]
        blocks = [
            {"id": "seq_source", "section": "source", "type": "oscillator", "name": "SEQ OSC",
             "enabled": True, "targetId": "volume", "values": {
                 "oscType": 0.0, "oscBlend": 0.12, "octave": 0.0, "detune": 0.08, "noiseBlend": 0.0,
             }, "metadata": {"uiX": "40", "uiY": "80"}},
            {"id": "seq_filter", "section": "filter", "type": "stateVariable", "name": "SEQ FILTER",
             "enabled": True, "targetId": "filterCutoff", "values": {"cutoff": 0.62, "resonance": 0.22},
             "metadata": {"uiX": "340", "uiY": "80"}},
            {"id": "seq_amp", "section": "amp", "type": "adsr", "name": "SEQ AMP",
             "enabled": True, "targetId": "attack", "values": {
                 "attack": 0.01, "decay": 0.22, "sustain": 0.18, "release": 0.24,
             }, "metadata": {"uiX": "340", "uiY": "240"}},
            {"id": "seq_arp", "section": "mod", "type": "midiPlayground", "name": "STEP ARP",
             "enabled": True, "targetId": "filterCutoff", "values": step_arp_block_values(pattern),
             "metadata": {"uiX": "640", "uiY": "80"}},
            {"id": "seq_delay", "section": "fx", "type": "delay", "name": "SEQ DELAY",
             "enabled": True, "targetId": "delayMix", "values": {
                 "delayTime": 0.1875, "delayFeedback": 0.28, "delayMix": 0.14, "sync": 1.0,
             }, "metadata": {"uiX": "940", "uiY": "80"}},
            {"id": "seq_output", "section": "out", "type": "limiter", "name": "OUTPUT",
             "enabled": True, "targetId": "volume", "values": {"outputLimiter": 1.0, "outputCeilingDb": -0.8},
             "metadata": {"uiX": "1240", "uiY": "80"}},
        ]
        edges = [
            {"id": "seq_source_to_seq_filter", "sourceNodeId": "seq_source", "targetNodeId": "seq_filter",
             "signalType": "audio", "enabled": True},
            {"id": "seq_filter_to_seq_amp", "sourceNodeId": "seq_filter", "targetNodeId": "seq_amp",
             "signalType": "audio", "enabled": True},
            {"id": "seq_amp_to_seq_delay", "sourceNodeId": "seq_amp", "targetNodeId": "seq_delay",
             "signalType": "audio", "enabled": True},
            {"id": "seq_delay_to_seq_output", "sourceNodeId": "seq_delay", "targetNodeId": "seq_output",
             "signalType": "audio", "enabled": True},
            {"id": "seq_arp_event", "sourceNodeId": "seq_arp", "targetNodeId": "seq_source",
             "signalType": "event", "enabled": True},
        ]
        return {"blocks": blocks, "edges": edges, "macros": [], "modulation": [], "automation": []}
    elif step_arp:
        pattern = STEP_PATTERNS[0]
        blocks.append({"id": "step_arp", "section": "motion", "type": "midiPlayground",
                       "name": "Step Arp Engine", "enabled": True,
                       "targetId": "filterCutoff", "values": step_arp_block_values(pattern)})
    if pianoroll:
        blocks.append({"id": "piano_roll", "section": "modulation", "type": "pianoRoll",
                       "name": "Piano Roll Clip", "enabled": True, "targetId": "filterCutoff",
                       "values": piano_roll_block_values(),
                       "metadata": {"notes": piano_roll_notes()}})
    return {"blocks": blocks, "edges": [], "macros": [], "modulation": [], "automation": []}


def manifest(pack: dict[str, Any], default_preset: str) -> dict[str, Any]:
    return {
        "formatVersion": 1,
        "instrumentName": pack["display"],
        "creator": "AudiCode",
        "description": pack["description"],
        "category": pack["category"],
        "engine": pack["engine"],
        "backgroundImage": "assets/background-clean.png",
        "libraryThumbnail": "assets/library-artwork.png",
        "defaultPreset": default_preset,
        "createdWith": "Player Builder",
        "version": "1.0",
        "playerDisplayName": pack["display"],
        "playerTagline": pack["tagline"],
        "playerAccentColour": pack["accent"],
        "tags": sorted(set(pack["tags"] + ["factory-demo", "player-ready", "export-ready", "musical"])),
        "playerTitleBannerImage": "assets/player-title-banner.png",
        "playerPanelColour": "ff101722",
        "playerBackgroundColour": "ff07090f",
        "playerTitleBarTheme": pack["theme"],
        "playerTitleTextPlacement": pack["placement"],
        "playerTitleButtonStyle": "pill",
        "playerTitleFontFamily": "Segoe UI",
        "playerShowPatchCraftBranding": False,
    }


def preset(name: str, desc: str, values: dict[str, float], tags: list[str], default: bool = False) -> dict[str, Any]:
    safe = dict(values)
    safe["noiseBlend"] = 0.0
    safe["outputLimiter"] = 1.0
    safe["outputCeilingDb"] = -1.0
    safe.setdefault("outputGainDb", -3.0)
    safe.setdefault("expression", 1.0)
    return {
        "name": name,
        "description": desc,
        "theme": tags[0] if tags else "factory",
        "isDefault": default,
        "tags": sorted(set(tags + ["factory-demo", "musical", "ship-ready"])),
        "values": {k: round(v, 5) if isinstance(v, float) else v for k, v in safe.items()},
        "generated": False,
    }


def synth_presets() -> list[dict[str, Any]]:
    specs = [
        ("Glass Pentatonic Lead", 0, 1, 0.06, 0, 4, -3, 0.10, 0.005, 0.28, 0.45, 0.28, 6200, .18, .25, .18, .32, .72, 124),
        ("Warm Fifth Pad", 1, 0, 0.12, 0, 8, -7, 0.12, .70, 1.4, .82, 2.8, 2600, .22, .50, .30, .62, .66, 96),
        ("Clean Motion Pluck", 1, 2, 0.08, 0, 5, -5, 0.04, .002, .28, .30, .28, 5200, .30, .1875, .28, .34, .70, 128),
        ("Soft Analog Keys", 0, 1, 0.10, 0, 3, -4, 0.08, .018, .42, .48, .62, 3800, .16, .375, .16, .24, .74, 100),
        ("Bright Hook Synth", 1, 0, 0.05, 1, 6, -6, 0.00, .004, .28, .68, .74, 8200, .20, .25, .22, .38, .70, 126),
        ("Velvet Chord Bed", 0, 3, 0.16, 0, 9, -9, 0.18, .95, 1.8, .74, 3.4, 2100, .10, .5, .20, .70, .62, 84),
        ("Wide Sync Arp", 1, 2, 0.14, 0, 10, -11, 0.04, .002, .26, .28, .26, 4700, .38, .125, .36, .28, .68, 132),
        ("Mellow Bell Stack", 0, 0, 0.18, 1, 2, 7, 0.00, .001, .80, .38, 1.2, 9000, .12, .25, .30, .58, .64, 110),
        ("Round Bass Pulse", 2, 0, 0.04, -1, 0, 0, 0.22, .003, .18, .45, .18, 1250, .42, .1875, .05, .05, .82, 124),
        ("Dustless Dream Pad", 3, 1, 0.08, 0, 6, -6, 0.10, 1.4, 2.4, .86, 4.6, 3400, .14, .5, .18, .76, .60, 76),
        ("Sequence Glass", 0, 2, 0.10, 0, 4, -8, 0.06, .001, .28, .32, .28, 5900, .26, .1875, .42, .36, .68, 118),
        ("Low Fifth Drone", 1, 0, 0.20, -1, 12, -12, 0.26, 1.8, 2.2, .88, 5.2, 1700, .18, .75, .12, .64, .58, 68),
        ("Clean Festival Lead", 1, 0, 0.09, 1, 14, -9, 0.03, .004, .36, .76, 1.1, 7600, .24, .25, .30, .46, .72, 128),
        ("Muted Pulse Keys", 2, 0, 0.04, 0, 0, 0, 0.08, .006, .24, .45, .28, 2400, .44, .125, .12, .18, .76, 122),
        ("Aurora Ribbon", 0, 3, 0.22, 0, 7, -2, 0.04, .30, 1.6, .68, 2.2, 4200, .20, .375, .24, .56, .64, 90),
        ("Small Room Pluck", 1, 0, 0.07, 0, 5, -5, 0.02, .001, .24, .25, .24, 6900, .28, .25, .18, .14, .70, 130),
        ("Subtle PWM Lead", 2, 1, 0.12, 0, 4, -4, 0.06, .005, .38, .66, .80, 5600, .18, .25, .20, .28, .72, 118),
        ("Cinematic Soft Saw", 1, 3, 0.15, 0, 11, -8, 0.12, .80, 2.1, .80, 3.8, 3000, .16, .5, .22, .66, .62, 82),
        ("Tiny Bell Arp", 0, 0, 0.05, 2, 2, 9, 0.00, .001, .38, .28, .36, 11200, .08, .125, .46, .44, .58, 140),
        ("Hollow Analog Lead", 3, 1, 0.09, 0, 5, -10, 0.05, .006, .42, .50, .92, 4500, .26, .375, .20, .34, .70, 104),
        ("Minimal Pulse", 2, 0, 0.03, -1, 0, 0, 0.10, .002, .26, .30, .26, 1800, .36, .1875, .08, .06, .80, 122),
        ("Night Air Pad", 0, 1, 0.14, 0, 6, -6, 0.10, 2.2, 2.6, .90, 6.0, 2200, .12, .75, .14, .82, .56, 72),
        ("Clean Octave Stack", 1, 0, 0.18, 1, 3, -3, 0.02, .006, .34, .72, .66, 6800, .20, .25, .26, .38, .70, 126),
        ("Soft Closing Theme", 0, 3, 0.08, 0, 1, -2, 0.06, .45, 1.4, .62, 2.4, 3600, .14, .5, .20, .60, .60, 88),
    ]
    out = []
    for i, s in enumerate(specs):
        name, osc, osc2, blend, octv, det, det2, sub, atk, dec, sus, rel, cutoff, res, dt, dm, rv, vol, bpm = s
        values = {
            "oscType": float(osc), "osc2Type": float(osc2), "oscBlend": blend, "octave": float(octv),
            "detune": float(det), "osc2Detune": float(det2), "subBlend": sub,
            "attack": atk, "decay": dec, "sustain": sus, "release": rel,
            "filterCutoff": float(cutoff), "filterResonance": res,
            "delayTime": dt, "delayFeedback": 0.22 + (i % 5) * 0.045, "delayMix": dm,
            "reverbMix": rv, "volume": vol, "projectBpm": float(bpm),
            "lfoRate": [0.5, 1, 2, 4, 6, 8][i % 6], "lfoAmount": [0.02, .05, .08, .12, .18, .24][i % 6],
            "macro_motion": (i % 6) / 6.0, "macro_tone": 0.35 + (i % 5) * .1,
            "macro_character": 0.25 + (i % 7) * .08, "macro_space": rv,
            "outputGainDb": -4.0 if vol > .78 else -3.0,
        }
        out.append(preset(name, "Playable synth sound with safe gain, no broadband noise, and distinct envelope/tone/space.", values, ["synth", "melodic"], i == 0))
    return out


def fx_presets(kind: str) -> list[dict[str, Any]]:
    names = [
        "Clean Quarter Echo", "Warm Tape Eighth", "Wide Ping Space", "Subtle Vocal Throw",
        "Filtered Dub Delay", "Short Room Width", "Rhythmic Slap", "Dark Dream Wash",
        "Parallel Glue", "Bright Stereo Lift", "Soft LoFi Throw", "Reverse Feel Space",
        "Chorus Echo Bed", "Phaser Delay Motion", "Comb Texture Light", "Resonant Bloom",
        "Multi Tap Clouds", "Tape Flutter Lead", "Vinyl Memory", "Formant Delay",
        "Clean Mix Bus", "Master Finish Soft", "Destroyed Texture", "Cinematic Tail",
    ]
    out = []
    for i, name in enumerate(names):
        base = {
            "inputTrimDb": -3.0 + (i % 4),
            "drive": [0.02, .08, .12, .18, .24, .32][i % 6],
            "mix": 0.35 + (i % 6) * .08 if kind == "delay" else 0.55 + (i % 4) * .08,
            "filterCutoff": [4200, 6200, 8500, 12000, 2800, 5400][i % 6],
            "filterResonance": [0.08, .12, .18, .24, .30, .36][i % 6],
            "delayTime": [0.125, .1875, .25, .333, .375, .5, .75][i % 7],
            "delayFeedback": [0.16, .22, .28, .34, .42, .50][i % 6],
            "delayMix": [0.08, .14, .20, .28, .34, .42][i % 6],
            "reverbMix": [0.04, .10, .16, .22, .32, .48][i % 6],
            "volume": 0.92,
            "outputGainDb": -4.0,
            "dynMix": .18 if i in {8, 20, 21} else 0.0,
            "dynThresholdDb": -18.0 - (i % 4) * 3,
            "dynRatio": 2.0 + (i % 3),
            "chorusMix": .18 if i in {2, 12} else 0.0,
            "phaserMix": .16 if i in {13} else 0.0,
            "combMix": .14 if i in {14, 22} else 0.0,
            "resonatorMix": .18 if i in {15, 23} else 0.0,
            "multiTapMix": .22 if i in {2, 16, 23} else 0.0,
            "multiTapFeedback": .30 + (i % 4) * .08,
            "multiTapSpread": .35 + (i % 5) * .10,
            "tapeMix": .22 if i in {1, 5, 17} else 0.0,
            "tapeDrive": .18 + (i % 4) * .08,
            "tapeFlutter": .05 + (i % 4) * .04,
            "vinylMix": .20 if i in {10, 18, 22} else 0.0,
            "lofiMix": .18 if i in {10, 22} else 0.0,
            "vocalMix": .18 if i in {3, 19} else 0.0,
            "spectralMix": .25 if i in {9, 20, 21} else 0.0,
            "spectralTilt": [-0.25, -0.1, 0.0, 0.14, 0.28][i % 5],
            "convolutionMix": .20 if i in {7, 16, 23} else 0.0,
            "macro_motion": (i % 8) / 8.0,
            "macro_tone": 0.25 + (i % 6) * .1,
            "macro_character": 0.2 + (i % 7) * .09,
            "macro_space": [0.12, .22, .36, .48, .62][i % 5],
        }
        tag = "delay" if kind == "delay" else "fx"
        out.append(preset(name, "Mix-safe FX preset with a distinct delay/modulation/tone role.", base, [tag, "creative"], i == 0))
    return out


def sample_presets(kind: str) -> list[dict[str, Any]]:
    drum_names = [
        "Tight House Kit", "Warm Analog Kit", "Dry Punch Kit", "Deep Room Kit", "Soft Velocity Kit", "Clean Club Kit",
        "Muted Electro Kit", "Bright Perc Kit", "LoFi Room Kit", "Minimal Click Kit", "Wide Hat Kit", "Garage Snap Kit",
        "Subby Kick Kit", "Perc Loop Builder", "Rim Groove Kit", "Crash Light Kit", "Tuned Tom Kit", "Dub Tech Kit",
        "Break Accent Kit", "Airy Top Kit", "Short Tail Kit", "Warehouse Kit", "Soft Saturated Kit", "Clean Export Kit",
    ]
    key_names = [
        "Clean Tape Keys", "Soft Felt Layer", "Reverse Texture Pad", "Wide Chopped Keys", "Organic Pluck Bed",
        "Short Voice Cloud", "Warm Grain Pad", "Tight Slice Lead", "Low Octave Felt", "Dream Loop Keys",
        "Muted Piano Stack", "Bell Tape Layer", "Vintage Soft Keys", "Granular Halo", "Small Room Keys",
        "Filtered Nostalgia", "Open Chord Bloom", "Percussive Felt", "Dark Pad Keys", "Bright Fifths",
        "Dust-Free Lofi", "Soft Chorus Keys", "Slow Attack Pad", "Playable Init Keys",
    ]
    names = drum_names if kind == "drums" else key_names
    out = []
    for i, name in enumerate(names):
        values = {
            "attack": [0.002, .004, .008, .015, .025][i % 5] if kind == "drums" else [0.01, .025, .08, .18, .35][i % 5],
            "decay": [0.12, .18, .26, .34, .48][i % 5] if kind == "drums" else [0.35, .50, .75, 1.2, 1.8][i % 5],
            "sustain": 0.0 if kind == "drums" else [0.55, .65, .72, .80, .88][i % 5],
            "release": [0.10, .16, .24, .38, .60][i % 5] if kind == "drums" else [0.55, .82, 1.25, 2.0, 3.2][i % 5],
            "samplePitch": [-2, -1, 0, 1, 2, 4][i % 6] if kind == "drums" else [-7, -3, 0, 2, 5, 7][i % 6],
            "sampleStart": [0.0, .005, .012, .02, .035][i % 5],
            "sampleLength": [1.0, .92, .84, .72, .58][i % 5],
            "sampleSlice": [0.0, .12, .25, .38, .50, .62, .75, .88][i % 8],
            "sampleSliceCount": [4, 8, 12, 16][i % 4],
            "sampleGlitch": [0.0, .02, .05, .08, .12][i % 5],
            "filterCutoff": [2400, 3600, 5200, 7800, 12000][i % 5],
            "filterResonance": [0.08, .14, .20, .28, .36][i % 5],
            "delayTime": [0.125, .1875, .25, .375, .5][i % 5],
            "delayFeedback": [0.0, .08, .14, .22, .32][i % 5],
            "delayMix": [0.0, .04, .08, .14, .22][i % 5],
            "reverbMix": [0.04, .10, .18, .28, .40][i % 5],
            "volume": .82 if kind == "drums" else .76,
            "outputGainDb": -4.5 if kind == "drums" else -3.5,
            "granularOn": 1.0 if kind != "drums" and i in {5, 6, 13, 18} else 0.0,
            "granularDensity": .28 + (i % 5) * .09,
            "granularSizeMs": 45 + (i % 6) * 24,
            "granularScan": [-.8, -.4, 0, .35, .7][i % 5],
            "macro_motion": (i % 7) / 7.0,
            "macro_tone": .25 + (i % 6) * .1,
            "macro_character": .2 + (i % 7) * .09,
            "macro_space": [0.08, .16, .28, .40, .55][i % 5],
        }
        if kind == "drums":
            for pad in range(1, 17):
                values[f"pad{pad}Volume"] = round(0.72 + ((i + pad) % 5) * .08, 4)
                values[f"pad{pad}Pitch"] = float(((i + pad) % 5) - 2)
                values[f"pad{pad}Pan"] = [-0.25, -0.12, 0.0, 0.12, 0.25][(i + pad) % 5]
        out.append(preset(name, "Sample preset with distinct tuning, length, tone, and space settings.", values, [kind, "sample"], i == 0))
    return out


def circle_presets() -> list[dict[str, Any]]:
    base = synth_presets()[:24]
    names = [
        "Pentatonic Spiral Kit", "Minor Orbit Pluck", "House Drum Orbit", "Dream Bell Rondo", "Muted Analog Steps",
        "Bright Fifth Pulse", "Low Night Sequence", "Clean MIDI Loop Demo", "Soft Glass Machine", "Triad Orbit",
        "Deep Lane Arp", "Warm Drum Synth", "Sparse Syncopation", "Neon Circle Hook", "Floating Rhythm",
        "Percussive Fifths", "Slow Motion Orbit", "Tight Sixteen Pulse", "Wide Pluck Pattern", "Cinematic Rondo",
        "Minimal Step Machine", "Hybrid Drum Lane", "Bright Sample Ring", "Playable Init Orbit",
    ]
    out = []
    for i, p in enumerate(base):
        vals = dict(p["values"])
        vals.update({
            "projectBpm": [88, 96, 104, 112, 120, 124, 128, 132][i % 8],
            "attack": [0.002, .004, .008, .012][i % 4],
            "decay": [0.22, .28, .36, .45, .55][i % 5],
            "sustain": [0.28, .36, .45, .55, .65][i % 5],
            "release": [0.20, .32, .45, .62, .80][i % 5],
            "delayMix": [0.06, .10, .16, .24, .32][i % 5],
            "reverbMix": [0.10, .16, .22, .30, .42][i % 5],
            "lfoAmount": [0.04, .08, .12, .18, .24][i % 5],
        })
        notes = [
            [0, 3, 7, 10, 12, 15, 19, 22, 19, 15, 12, 10, 7, 3, 0, -5],
            [0, 5, 7, 10, 12, 17, 19, 24, 19, 17, 12, 10, 7, 5, 0, -2],
            [0, 2, 4, 7, 9, 12, 14, 16, 14, 12, 9, 7, 4, 2, 0, -3],
            [0, 7, 12, 15, 19, 22, 24, 27, 24, 22, 19, 15, 12, 7, 0, -5],
        ][i % 4]
        for step, note in enumerate(notes):
            vals[f"arpNote{step}"] = float(note)
            vals[f"mpStep{step}On"] = 0.0 if (step + i) % 11 == 0 else 1.0
            vals[f"mpVelocity{step}"] = round(0.42 + ((step + i) % 6) * .08, 4)
            vals[f"mpGate{step}"] = round(0.25 + ((step + i) % 4) * .08, 4)
            vals[f"mpStepProb{step}"] = 1.0 if step % 5 != 4 else 0.82
        for lane in range(1, 6):
            vals[f"mpBank{lane}_mpLaneMute"] = 0.0
            vals[f"mpBank{lane}_mpLaneSolo"] = 0.0
            vals[f"mpBank{lane}_mpLaneRetrigger"] = 1.0
            vals[f"mpBank{lane}_mpLaneVelocity"] = round(0.48 + ((i + lane) % 5) * .1, 4)
            vals[f"mpBank{lane}_mpLaneTarget"] = float((lane + i) % 5)
        out.append(preset(names[i], "Five-lane circular sequence with melodic note choices and safe synth source.", vals, ["circle", "arp", "melodic"], i == 0))
    return out


def _soft_glow(size: tuple[int, int], box: tuple[int, int, int, int],
               rgb: str, alpha: int, blur: float) -> Image.Image:
    """A single soft radial glow on a transparent layer."""
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(layer, "RGBA")
    r, g, b, _ = rgba(rgb)
    gd.ellipse(box, fill=(r, g, b, alpha))
    return layer.filter(ImageFilter.GaussianBlur(blur))


def draw_background(pack: dict[str, Any], out: Path) -> None:
    """Clean, professional instrument backdrop: a smooth vertical gradient with
    two soft accent glows and a subtle top sheen. No clutter, no structural
    outlines — the runtime UI controls are the visual focus."""
    out.mkdir(parents=True, exist_ok=True)
    c1, c2 = rgba(pack["bg"]), rgba(pack["bg2"])
    base = Image.new("RGBA", (W, H), (c1[0], c1[1], c1[2], 255))
    draw = ImageDraw.Draw(base, "RGBA")
    # Smooth vertical gradient with a gentle ease so the lower half deepens.
    for y in range(H):
        t = y / max(1, H - 1)
        t = t * t * (3 - 2 * t)  # smoothstep
        r = int(c1[0] * (1 - t) + c2[0] * t)
        g = int(c1[1] * (1 - t) + c2[1] * t)
        b = int(c1[2] * (1 - t) + c2[2] * t)
        draw.line((0, y, W, y), fill=(r, g, b, 255))
    # Two restrained accent glows for depth (top-left primary, lower-right secondary).
    base = Image.alpha_composite(base, _soft_glow((W, H), (-300, -320, 520, 500),
                                                  pack["accent_rgb"], 40, 200))
    base = Image.alpha_composite(base, _soft_glow((W, H), (W - 520, H - 360, W + 300, H + 280),
                                                  pack["accent2_rgb"], 30, 220))
    # Subtle top sheen + bottom vignette for a finished, premium feel.
    sheen = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sheen, "RGBA")
    sd.rectangle((0, 0, W, 1), fill=(255, 255, 255, 22))
    sd.rectangle((0, H - 160, W, H), fill=(0, 0, 0, 30))
    base = Image.alpha_composite(base, sheen.filter(ImageFilter.GaussianBlur(1.5)))
    overlay = base
    overlay.save(out / "background-clean.png")
    overlay.save(out / "background.png")
    overlay.save(out / "background-with-ui.png")
    overlay.save(out / "background-sectioned.png")
    overlay.save(out / "background-clean-source.png")
    make_banner(pack, out / "player-title-banner.png")
    make_library_art(pack, out / "library-artwork.png", (720, 420))
    make_library_art(pack, out / "thumbnail.png", (512, 320))
    make_library_art(pack, out / "player-library-modal.png", (900, 620))


def make_banner(pack: dict[str, Any], path: Path) -> None:
    img = Image.new("RGBA", (1224, 96), rgba(pack["bg"], 255))
    d = ImageDraw.Draw(img, "RGBA")
    d.rounded_rectangle((0, 0, 1223, 95), radius=14, fill=rgba(pack["bg2"], 240), outline=rgba(pack["accent_rgb"], 180), width=2)
    d.rounded_rectangle((22, 22, 72, 72), radius=10, fill=rgba(pack["accent_rgb"], 80), outline=rgba(pack["accent_rgb"], 200), width=1)
    d.text((39, 31), pack["display"][0], fill=rgba("f5f7ff"), font=font(26, True), anchor="la")
    d.text((92, 22), pack["display"], fill=rgba("f5f7ff"), font=font(22, True))
    d.text((94, 54), pack["tagline"], fill=rgba("aab5c4"), font=font(13))
    d.text((1094, 26), "v1.0", fill=rgba("aab5c4"), font=font(12))
    d.text((1062, 52), "DAW ready", fill=rgba("dce4f2"), font=font(13))
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def make_library_art(pack: dict[str, Any], path: Path, size: tuple[int, int]) -> None:
    w, h = size
    img = Image.new("RGBA", size, rgba(pack["bg"], 255))
    d = ImageDraw.Draw(img, "RGBA")
    for y in range(h):
        t = y / max(1, h - 1)
        c1 = rgba(pack["bg"])
        c2 = rgba(pack["bg2"])
        d.line((0, y, w, y), fill=(int(c1[0]*(1-t)+c2[0]*t), int(c1[1]*(1-t)+c2[1]*t), int(c1[2]*(1-t)+c2[2]*t), 255))
    d.rounded_rectangle((24, 24, w-24, h-24), radius=20, outline=rgba(pack["accent_rgb"], 180), width=2)
    d.text((44, 42), pack["display"], fill=rgba("f5f7ff"), font=font(max(20, w // 20), True))
    d.text((46, 78), pack["tagline"], fill=rgba("aab5c4"), font=font(max(12, w // 42)))
    cx, cy = w // 2, h // 2 + 28
    if pack["kind"] == "circle":
        for r in [42, 70, 98]:
            d.ellipse((cx-r, cy-r, cx+r, cy+r), outline=rgba(pack["accent_rgb"], 150), width=2)
    elif pack["kind"] == "arpstep":
        d.ellipse((cx-98, cy-98, cx+98, cy+98), outline=rgba(pack["accent_rgb"], 150), width=2)
        for i in range(16):
            ang = (i / 16.0) * (2 * math.pi) - math.pi / 2
            sx = cx + int(math.cos(ang) * 72)
            sy = cy + int(math.sin(ang) * 72)
            d.ellipse((sx-4, sy-4, sx+4, sy+4), fill=rgba(pack["accent2_rgb"], 180))
    elif pack["kind"] in ("pianoroll", "keysflagship"):
        rows = [(0, 2), (1, 3), (2, 1), (3, 4)]
        bar_w = (w - 120) // 4
        for beat, lift in rows:
            for v in range(4):
                x = 60 + beat * bar_w
                y = cy - 60 + (v + lift % 2) * 18
                d.rounded_rectangle((x + 4, y, x + bar_w - 10, y + 12), radius=3,
                                    fill=rgba(pack["accent_rgb"], 170))
    elif pack["kind"] in ("drums", "beatforge"):
        for row in range(2):
            for col in range(4):
                x = cx - 150 + col * 84
                y = cy - 54 + row * 62
                d.rounded_rectangle((x, y, x+58, y+42), radius=8, outline=rgba(pack["accent_rgb"], 150), width=2)
    elif pack["kind"] == "delay":
        for i in range(4):
            x = cx - 220 + i * 120
            d.rounded_rectangle((x, cy-54, x+92, cy+54), radius=10, outline=rgba(pack["accent_rgb"], 150), width=2)
    else:
        d.rounded_rectangle((cx-220, cy-70, cx+220, cy+70), radius=18, outline=rgba(pack["accent_rgb"], 150), width=2)
        for i in range(80):
            x = cx - 200 + i * 5
            amp = int(math.sin(i * .4) * 28)
            d.line((x, cy-amp, x, cy+amp), fill=rgba(pack["accent_rgb"], 110), width=2)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def write_pack(pack: dict[str, Any], layout: dict[str, Any], presets: list[dict[str, Any]]) -> None:
    folder = FACTORY / pack["folder"]
    assets = folder / "assets"
    draw_background(pack, assets)
    params = base_params(pack["engine"])
    # CircleSEQ lane-backed controls.
    if pack["kind"] == "circle":
        for lane in range(1, 6):
            params += [
                param(f"mpBank{lane}_mpLaneMute", f"Lane {lane} Bypass", 0, 1, 0, "", "circle", "toggle", 1),
                param(f"mpBank{lane}_mpLaneSolo", f"Lane {lane} Solo", 0, 1, 0, "", "circle", "toggle", 1),
                param(f"mpBank{lane}_mpLaneRetrigger", f"Lane {lane} Retrig", 0, 1, 1, "", "circle", "toggle", 1),
                param(f"mpBank{lane}_mpLaneVelocity", f"Lane {lane} Level", 0, 1, 0.72, "", "circle"),
                param(f"mpBank{lane}_mpLaneTarget", f"Lane {lane} Role", 0, 5, lane - 1, "", "circle", "stepped", 1),
            ]
        for step in range(16):
            params += [
                param(f"arpNote{step}", f"Step {step+1} Note", -24, 36, 0, "st", "circle", "stepped", 1),
                param(f"mpStep{step}On", f"Step {step+1}", 0, 1, 1, "", "circle", "toggle", 1),
                param(f"mpVelocity{step}", f"Step {step+1} Vel", 0, 1, .7, "", "circle"),
                param(f"mpGate{step}", f"Step {step+1} Gate", 0, 1, .34, "", "circle"),
                param(f"mpStepProb{step}", f"Step {step+1} Chance", 0, 1, 1, "", "circle"),
            ]
    if pack["kind"] in {"arpstep"} or pack.get("step_arp"):
        append_step_sequencer_params(params)
    params = dedupe_params(params)
    default_values = dict(presets[0]["values"])
    graph = dsp_graph(
        pack["engine"], pack["display"], default_values,
        step_arp=bool(pack.get("step_arp")),
        arpstep=pack["kind"] == "arpstep",
        pianoroll=pack["kind"] == "pianoroll" or bool(pack.get("piano_roll")),
    )
    write_json(folder / "manifest.json", manifest(pack, presets[0]["name"]))
    write_json(folder / "layout.json", layout)
    write_json(folder / "parameters.json", {"parameters": params})
    write_json(folder / "presets.json", {"presets": presets})
    write_json(folder / "dspGraph.json", graph)
    write_json(folder / "patches.json", {"patches": []})
    write_json(folder / "sectionPresets.json", {"sectionPresets": []})
    write_json(folder / "expansions.json", {"expansions": []})
    if not (folder / "midiMappings.json").exists():
        write_json(folder / "midiMappings.json", {"mappings": []})
    if not (folder / "mappings.json").exists():
        write_json(folder / "mappings.json", {"zones": []})
    if (folder / "project.json").exists():
        project = json.loads((folder / "project.json").read_text(encoding="utf-8"))
        project["manifest"] = manifest(pack, presets[0]["name"])
        project["layout"] = layout
        project["parameters"] = {"parameters": params}
        project["dspGraph"] = graph
        project["presetData"] = {"presets": presets}
        project["patchData"] = {"patches": []}
        write_json(folder / "project.json", project)


PACKS = [
    {
        "folder": "AuroraArpSynth.patchcraft", "display": "Aurora Arp Synth", "engine": "synth",
        "kind": "synth", "category": "Synth Instrument", "theme": "wide-banner", "placement": "left",
        "description": "Hold a chord and hear a true step arp — twelve musical synth+pattern presets with orbit preview.",
        "tagline": "Melodic synth with live step arp and musical presets",
        "try_hint": "1) Pick a preset  2) Hold Cm7 on the keyboard  3) Tweak Cutoff + Rate",
        "tags": ["synth", "arp", "melodic", "true-arp"], "accent": "ff62f7d2", "accent2": "ff8a63ff",
        "accent_rgb": "62f7d2", "accent2_rgb": "8a63ff", "bg": "05080d", "bg2": "121b27", "line": "2b4e63",
        "step_arp": True, "preset_builder": "aurora_arp",
    },
    {
        "folder": "AnalogHouseDrums.patchcraft", "display": "Analog House Drums", "engine": "sample",
        "kind": "drums", "category": "Drum Machine", "theme": "split-brand", "placement": "left",
        "description": "Sample drum machine with 4×4 pads, editable 16-step pattern grid, and mix-safe kit presets.",
        "tagline": "Playable drum machine with real pad samples",
        "try_hint": "1) Trigger pads  2) Edit the pattern grid  3) Try Kick Tune + Room presets",
        "tags": ["drums", "sample", "pads"], "accent": "ffffa51f", "accent2": "ffff4f9a",
        "accent_rgb": "ffa51f", "accent2_rgb": "ff4f9a", "bg": "09070b", "bg2": "1d1018", "line": "583d22",
    },
    {
        "folder": "DreamKeysSampler.patchcraft", "display": "Dream Keys Sampler", "engine": "sample",
        "kind": "sampler", "category": "Sample Instrument", "theme": "wide-banner", "placement": "center",
        "description": "Tape-style keys with waveform editing, drop zone, granular texture, and distinct musical presets.",
        "tagline": "Tape keys, slices, grains, and playable zones",
        "try_hint": "1) Load a preset  2) Play the keyboard  3) Move Start/Length + Grain",
        "tags": ["sampler", "keys", "sample"], "accent": "ff7ef7a8", "accent2": "ff6ab7ff",
        "accent_rgb": "7ef7a8", "accent2_rgb": "6ab7ff", "bg": "05090b", "bg2": "10201a", "line": "245c47",
    },
    {
        "folder": "CircleSeqFlagship.patchcraft", "display": "CircleSEQ Flagship", "engine": "synth",
        "kind": "circle", "category": "Circle Sequencer Demo", "theme": "split-brand", "placement": "left",
        "description": "Five-lane circular sequencer with lane roles, bypasses, and melodic orbit presets.",
        "tagline": "Five circular lanes for melodic performance sequencing",
        "try_hint": "1) Hold a chord  2) Toggle lane bypass buttons  3) Switch orbit presets",
        "tags": ["circle", "arp", "sequencer"], "accent": "ff9b6dff", "accent2": "ff20e0ff",
        "accent_rgb": "9b6dff", "accent2_rgb": "20e0ff", "bg": "05070d", "bg2": "131225", "line": "3c3270",
    },
    {
        "folder": "ChordCraftKeys.patchcraft", "display": "ChordCraft Keys", "engine": "synth",
        "kind": "pianoroll", "category": "Chord / MIDI Instrument", "theme": "wide-banner", "placement": "left",
        "description": "Chord-assistant instrument with a fully editable runtime piano roll, host-synced playback, and six musical chord-engine presets.",
        "tagline": "Editable piano roll chord engine with musical presets",
        "try_hint": "1) Press PLAY  2) Click the piano roll to edit chords  3) Switch sound presets",
        "tags": ["pianoroll", "chords", "midi", "composer", "keys"], "accent": "ff6ab7ff", "accent2": "ffb98cff",
        "accent_rgb": "6ab7ff", "accent2_rgb": "b98cff", "bg": "05070d", "bg2": "121626", "line": "2c3a63",
    },
    {
        "folder": "ArpStepSequencer.patchcraft", "display": "Arp Step Sequencer", "engine": "synth",
        "kind": "arpstep", "category": "Step Sequencer Demo", "theme": "split-brand", "placement": "left",
        "description": "Flagship 16-step orbit sequencer with rate, gate, swing, tone macros, routed DSP graph, and six musical presets.",
        "tagline": "16-step orbit arp with musical presets and tone macros",
        "try_hint": "1) Hold Cm7  2) Edit steps on the orbit  3) Switch presets and tweak Rate/Gate",
        "tags": ["arp", "sequencer", "step", "true-arp", "musical"], "accent": "ff9b6dff", "accent2": "ff20e0ff",
        "accent_rgb": "9b6dff", "accent2_rgb": "20e0ff", "bg": "05070d", "bg2": "131225", "line": "3c3270",
    },
    {
        "folder": "BeatFoundry.patchcraft", "display": "Beat Foundry", "engine": "sample",
        "kind": "beatforge", "category": "Beat Workstation", "theme": "split-brand", "placement": "left",
        "description": "Sampler beat workstation — chop and flip samples across 16 velocity-layered pads, drive them with a step arp, and finish with a studio FX chain.",
        "tagline": "Chop, flip, and perform samples with velocity layers + step arp",
        "try_hint": "1) Drop a sample  2) Trigger the chop pads  3) Push Vel Sens + Rate, blend FX",
        "tags": ["sampler", "beats", "chop", "pads", "velocity"], "accent": "ffff8a3d", "accent2": "ff32d6ff",
        "accent_rgb": "ff8a3d", "accent2_rgb": "32d6ff", "bg": "0a0705", "bg2": "1d1410", "line": "5c4022",
        "step_arp": True,
    },
    {
        "folder": "HalcyonKeys.patchcraft", "display": "Halcyon Keys", "engine": "synth",
        "kind": "keysflagship", "category": "Keys / MIDI Instrument", "theme": "wide-banner", "placement": "center",
        "description": "Flagship keys instrument pairing a dual-oscillator synth with an editable runtime piano roll and a full modulation + FX chain.",
        "tagline": "Dual-osc synth + editable piano roll + full FX chain",
        "try_hint": "1) Press PLAY  2) Edit chords in the piano roll  3) Shape tone, motion, and FX",
        "tags": ["keys", "synth", "pianoroll", "midi", "composer"], "accent": "ff7aa0ff", "accent2": "ffcf8cff",
        "accent_rgb": "7aa0ff", "accent2_rgb": "cf8cff", "bg": "05060d", "bg2": "121425", "line": "2c3263",
        "piano_roll": True,
    },
    {
        "folder": "EchoCraft.patchcraft", "display": "EchoCraft", "engine": "fx",
        "kind": "delay", "category": "Delay Workstation", "theme": "wide-banner", "placement": "left",
        "description": "Four-stage delay workstation with feedback shaping, modulation, tone, and mix-safe presets.",
        "tagline": "Delay workstation with clean, tape, dual, and reverse-style motion",
        "try_hint": "1) Send audio in  2) Pick a delay preset  3) Shape FB + Tape on the bottom row",
        "tags": ["fx", "delay", "echo"], "accent": "ff12e8f3", "accent2": "ffffaa2b",
        "accent_rgb": "12e8f3", "accent2_rgb": "ffaa2b", "bg": "050607", "bg2": "111417", "line": "1f6b70",
    },
    {
        "folder": "ModularMotionFX.patchcraft", "display": "Modular Motion FX", "engine": "fx",
        "kind": "modfx", "category": "Creative FX", "theme": "wide-banner", "placement": "left",
        "description": "Creative FX chain with motion, tone, dynamics, destruction, and space — studio-safe gain staging.",
        "tagline": "Creative motion FX with studio-safe gain staging",
        "try_hint": "1) Pick a motion preset  2) Automate Motion macro  3) Blend Comp + Space at the bottom",
        "tags": ["fx", "motion", "creative"], "accent": "ff8cff58", "accent2": "ff42d6ff",
        "accent_rgb": "8cff58", "accent2_rgb": "42d6ff", "bg": "040805", "bg2": "0d1c12", "line": "32653a",
    },
]


def validate_layout(layout: dict[str, Any]) -> None:
    controls = [e for e in layout["elements"] if e.get("type") in {
        "knob", "slider", "button", "dropdown", "padGrid", "drumGrid", "keyboard", "mixer",
        "macroControl", "modMatrix", "eqCurve", "spectrumAnalyzer", "sampleDropZone",
        "runtimeSampleLibrary", "pitchWheel", "modWheel", "arpLane", "meter", "valueDisplay",
    }]
    if any(e.get("type") == "tabPanel" for e in layout["elements"]):
        raise RuntimeError("factory demo layout still contains tabPanel")
    if len(controls) < 24:
        raise RuntimeError(f"layout only has {len(controls)} runtime controls")
    for e in controls:
        if e.get("action") or e.get("id") == "presets":
            continue
        if not e.get("parameterId"):
            raise RuntimeError(f"runtime control has no parameterId: {e.get('id')}")


def normalize_layout_groups(layout: dict[str, Any]) -> None:
    # Factory demos are custom single-page products. Non-main group ids are
    # treated as tab visibility targets by Studio/Player, so keep all generated
    # controls on the main page unless an actual tabbed demo is intentionally
    # generated.
    for element in layout.get("elements", []):
        if element.get("groupId"):
            element["groupId"] = "main"


def validate_presets(presets: list[dict[str, Any]]) -> None:
    seen = set()
    for p in presets:
        values = p["values"]
        if values.get("noiseBlend", 0.0) > 0.001:
            raise RuntimeError(f"{p['name']} enables noiseBlend")
        if values.get("oscType", 0.0) > 3.0 or values.get("osc2Type", 0.0) > 3.0:
            raise RuntimeError(f"{p['name']} has unsafe oscillator type")
        signature = json.dumps(values, sort_keys=True)
        if signature in seen:
            raise RuntimeError(f"duplicate preset values: {p['name']}")
        seen.add(signature)


def main() -> None:
    builders = {
        "synth": (synth_layout, synth_presets),
        "drums": (drums_layout, lambda: sample_presets("drums")),
        "sampler": (sampler_layout, lambda: sample_presets("keys")),
        "circle": (circle_layout, circle_presets),
        "arpstep": (arpstep_layout, arpstep_presets),
        "pianoroll": (pianoroll_layout, pianoroll_presets),
        "beatforge": (beatforge_layout, beatforge_presets),
        "keysflagship": (keysflagship_layout, pianoroll_presets),
        "delay": (echocraft_layout, lambda: fx_presets("delay")),
        "modfx": (modular_fx_layout, lambda: fx_presets("modfx")),
    }
    preset_overrides = {
        "aurora_arp": aurora_arp_presets,
    }
    for pack in PACKS:
        layout_builder, preset_builder = builders[pack["kind"]]
        layout = layout_builder(pack)
        override = pack.get("preset_builder")
        presets = preset_overrides[override]() if override else preset_builder()
        normalize_layout_groups(layout)
        validate_layout(layout)
        validate_presets(presets)
        write_pack(pack, layout, presets)
        print(f"rebuilt {pack['display']}: {len(layout['elements'])} elements, {len(presets)} presets")


if __name__ == "__main__":
    main()
