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


def base_layout(pack: dict[str, Any]) -> list[dict[str, Any]]:
    accent = pack["accent"]
    return [
        {"id": "background", "type": "image", "x": 0, "y": 0, "width": W, "height": H,
         "asset": "assets/background-clean.png", "locked": True},
        label("product_name", pack["display"], 46, 34, 440, 32, colour="ffeef6ff", size=22, group="brand"),
        label("product_role", pack["tagline"], 48, 66, 540, 22, colour="ff9aa5b5", size=12, group="brand"),
        value_display("preset_readout", "Preset", "projectBpm", 994, 38, 104, 34, accent=accent, group="brand"),
        slider("pitchwheel", "Pitch", "modWheel", 52, 612, 34, 118, accent=accent, group="global"),
        slider("modwheel", "Mod", "expression", 96, 612, 34, 118, accent=accent, group="global"),
        knob("global_volume", "Volume", "volume", 1126, 596, accent=accent, size=72, group="global"),
        knob("global_pan", "Pan", "pan", 1126, 690, accent=pack["accent2"], size=72, group="global"),
        value_display("output_value", "Out", "outputGainDb", 1028, 704, 82, 34, accent=accent, group="global"),
    ]


def synth_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    els += [
        shape("source_frame", 54, 118, 324, 424, bg="bb090d14", border=accent, radius=18, glow=0.12, group="source"),
        shape("motion_frame", 412, 118, 454, 214, bg="aa0a1018", border=accent2, radius=16, group="motion"),
        shape("tone_frame", 412, 356, 454, 186, bg="aa0a1018", border=accent, radius=16, group="tone"),
        shape("space_frame", 900, 118, 282, 424, bg="aa0a1018", border=accent2, radius=16, group="space"),
        label("source_title", "SOURCE STACK", 78, 138, 180, 22, colour="ffeef6ff", size=14, group="source"),
        label("motion_title", "MOTION", 438, 138, 160, 22, colour="ffeef6ff", size=14, group="motion"),
        label("tone_title", "TONE SHAPE", 438, 376, 180, 22, colour="ffeef6ff", size=14, group="tone"),
        label("space_title", "SPACE / OUTPUT", 926, 138, 180, 22, colour="ffeef6ff", size=14, group="space"),
        {"id": "spectrum", "type": "spectrumAnalyzer", "x": 444, "y": 174, "width": 390, "height": 120,
         "parameterId": "macro_motion", "accentColour": accent2, "backgroundColour": "cc06090f",
         "borderColour": "66505b72", "label": "Motion View", "labelPosition": "hidden", "groupId": "motion"},
        {"id": "keyboard", "type": "keyboard", "x": 180, "y": 640, "width": 838, "height": 82,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
        dropdown("wave_select", "Wave", "oscType", 78, 172, 126, 32, accent=accent, group="source"),
        dropdown("wave2_select", "Wave 2", "osc2Type", 222, 172, 126, 32, accent=accent2, group="source"),
    ]
    els += controls_from([
        ("oscBlend", "Blend"), ("octave", "Oct"), ("detune", "Detune"), ("subBlend", "Sub"),
        ("filterCutoff", "Cutoff"), ("filterResonance", "Res"), ("attack", "Atk"), ("release", "Rel"),
        ("lfoRate", "LFO Hz"), ("lfoAmount", "LFO Amt"), ("delayMix", "Delay"), ("reverbMix", "Reverb"),
        ("macro_motion", "Motion"), ("macro_tone", "Tone"), ("macro_character", "Color"), ("macro_space", "Space"),
    ], 78, 232, cols=4, accent=accent, group="synth_controls", size=70, xgap=104, ygap=100)
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
    els += [
        shape("pad_frame", 52, 126, 460, 438, bg="bb080b10", border=accent, radius=20, group="pads", glow=0.14),
        shape("grid_frame", 542, 126, 622, 230, bg="bb080b10", border=accent2, radius=16, group="pattern"),
        shape("mix_frame", 542, 386, 622, 178, bg="aa0a1018", border=accent, radius=16, group="mix"),
        label("pad_title", "DRUM PADS", 80, 146, 180, 22, colour="ffeef6ff", size=14, group="pads"),
        label("grid_title", "PATTERN GRID", 570, 146, 180, 22, colour="ffeef6ff", size=14, group="pattern"),
        {"id": "pad_bank", "type": "padGrid", "x": 80, "y": 182, "width": 404, "height": 318,
         "parameterId": "pad1Volume", "padRows": 4, "padCols": 4, "padBaseNote": 36,
         "accentColour": accent, "backgroundColour": "dd06090f", "borderColour": "66505b72",
         "label": "Pads", "labelPosition": "hidden", "groupId": "pads"},
        {"id": "drum_pattern", "type": "drumGrid", "x": 570, "y": 184, "width": 560, "height": 138,
         "parameterId": "pad1Volume", "drumTracks": 8, "drumSteps": 16, "drumPattern": pattern,
         "accentColour": accent2, "backgroundColour": "dd06090f", "borderColour": "66505b72",
         "label": "Pattern", "labelPosition": "hidden", "groupId": "pattern"},
        {"id": "keyboard", "type": "keyboard", "x": 190, "y": 650, "width": 818, "height": 74,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
    ]
    els += controls_from([
        ("pad1Volume", "Kick"), ("pad3Volume", "Snare"), ("pad7Volume", "Hat"), ("pad4Volume", "Clap"),
        ("pad1Pitch", "Kick Tune"), ("pad3Pitch", "Snare Tune"), ("pad11Pitch", "OH Tune"), ("pad14Volume", "Crash"),
        ("attack", "Atk"), ("release", "Rel"), ("filterCutoff", "Cutoff"), ("sampleGlitch", "Glitch"),
        ("delayMix", "Delay"), ("reverbMix", "Room"), ("macro_motion", "Swing"), ("macro_tone", "Tone"),
        ("sampleStart", "Start"), ("sampleLength", "Length"), ("samplePitch", "Kit Tune"), ("volume", "Kit Vol"),
    ], 572, 414, cols=5, accent=accent, group="drum_controls", size=62, xgap=112, ygap=86)
    return {"canvas": {"width": W, "height": H}, "elements": els}


def sampler_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    els += [
        shape("wave_frame", 56, 126, 754, 256, bg="bb080b10", border=accent, radius=18, group="sample", glow=0.13),
        shape("drop_frame", 840, 126, 314, 256, bg="aa0a1018", border=accent2, radius=18, group="drop"),
        shape("edit_frame", 168, 416, 920, 166, bg="bb080b10", border=accent, radius=16, group="edit"),
        label("wave_title", "SAMPLE PERFORMANCE", 84, 146, 240, 22, colour="ffeef6ff", size=14, group="sample"),
        {"id": "waveform", "type": "waveform", "x": 88, "y": 184, "width": 690, "height": 150,
         "parameterId": "sampleStart", "accentColour": accent, "backgroundColour": "cc06090f",
         "borderColour": "66505b72", "label": "Waveform", "labelPosition": "hidden", "groupId": "sample"},
        {"id": "sample_drop", "type": "sampleDropZone", "x": 876, "y": 164, "width": 242, "height": 174,
         "parameterId": "sampleStart", "accentColour": accent2, "backgroundColour": "cc06090f",
         "borderColour": accent2, "label": "Drop Sample", "labelPosition": "hidden", "groupId": "drop"},
        {"id": "keyboard", "type": "keyboard", "x": 178, "y": 650, "width": 840, "height": 78,
         "parameterId": "expression", "accentColour": accent, "backgroundColour": "dd06090f",
         "borderColour": "66505b72", "label": "Keyboard", "labelPosition": "hidden", "groupId": "global"},
        button("grain_toggle", "GRAIN", "granularOn", 924, 342, 86, 28, accent=accent2, group="drop"),
        dropdown("slice_select", "Slices", "sampleSliceCount", 1020, 342, 98, 28, accent=accent, group="drop"),
    ]
    els += controls_from([
        ("sampleStart", "Start"), ("sampleLength", "Length"), ("samplePitch", "Tune"), ("sampleSlice", "Slice"),
        ("sampleGlitch", "Glitch"), ("granularDensity", "Density"), ("granularSizeMs", "Size"), ("granularScan", "Scan"),
        ("filterCutoff", "Cutoff"), ("filterResonance", "Res"), ("attack", "Atk"), ("release", "Rel"),
        ("delayMix", "Delay"), ("reverbMix", "Space"), ("macro_motion", "Motion"), ("macro_tone", "Tone"),
    ], 198, 446, cols=8, accent=accent, group="sample_controls", size=58, xgap=108, ygap=82)
    return {"canvas": {"width": W, "height": H}, "elements": els}


def circle_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    els += [
        shape("orbit_field", 48, 120, 790, 494, bg="bb070b12", border=accent, radius=24, group="orbit", glow=0.16),
        shape("lane_panel", 862, 120, 300, 494, bg="aa0a1018", border=accent2, radius=18, group="lanes"),
        label("orbit_title", "CIRCLE SEQ ORBIT ENGINE", 82, 142, 280, 22, colour="ffeef6ff", size=14, group="orbit"),
        label("lanes_title", "LANE MIX / ROLES", 894, 142, 220, 22, colour="ffeef6ff", size=14, group="lanes"),
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
            dropdown(f"lane_{i}_role", f"Role {i}", f"mpBank{i}_mpLaneTarget", 954, 178 + (i - 1) * 72, 86, 28,
                     accent=colours[i - 1], group="lanes"),
            slider(f"lane_{i}_level", f"Level {i}", f"mpBank{i}_mpLaneVelocity", 1050, 170 + (i - 1) * 72, 76, 42,
                   accent=colours[i - 1], group="lanes"),
        ]
    els += controls_from([
        ("filterCutoff", "Cutoff"), ("attack", "Atk"), ("decay", "Decay"), ("release", "Rel"),
        ("delayMix", "Delay"), ("delayFeedback", "FB"), ("reverbMix", "Space"), ("lfoAmount", "Motion"),
        ("macro_motion", "Morph"), ("macro_tone", "Tone"), ("macro_character", "Accent"), ("macro_space", "FX"),
    ], 168, 642, cols=6, accent=accent, group="circle_controls", size=58, xgap=126, ygap=78)
    return {"canvas": {"width": W, "height": H}, "elements": els}


def echocraft_layout(pack: dict[str, Any]) -> dict[str, Any]:
    accent, accent2 = pack["accent"], pack["accent2"]
    els = base_layout(pack)
    els += [
        shape("delay_row", 130, 128, 1008, 214, bg="bb07090d", border="66505b72", radius=12, group="delay"),
        shape("routing_frame", 130, 374, 524, 178, bg="bb07090d", border=accent, radius=12, group="routing"),
        shape("shaper_frame", 684, 374, 454, 178, bg="bb07090d", border=accent2, radius=12, group="shaper"),
        shape("bottom_frame", 30, 590, 1218, 142, bg="aa080b10", border="66505b72", radius=12, group="bottom"),
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
    els += controls_from([
        ("drive", "Drive"), ("mix", "Mix"), ("filterCutoff", "LPF"), ("filterResonance", "Res"),
        ("tapeDrive", "Tape"), ("tapeFlutter", "Flutter"), ("chorusMix", "Chorus"), ("phaserMix", "Phaser"),
        ("dynMix", "Ducking"), ("dynThresholdDb", "Thresh"), ("spectralTilt", "Tilt"), ("convolutionMix", "Space"),
        ("multiTapSpread", "Spread"), ("reverbMix", "Reverb"), ("volume", "Output"), ("pan", "Pan"),
    ], 56, 620, cols=8, accent=accent, group="fx_controls", size=54, xgap=148, ygap=72)
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
    els += controls_from([
        ("drive", "Drive"), ("mix", "Mix"), ("filterCutoff", "Cutoff"), ("delayMix", "Delay"),
        ("multiTapMix", "Tap"), ("chorusMix", "Chorus"), ("phaserMix", "Phaser"), ("combMix", "Comb"),
        ("resonatorMix", "Res"), ("tapeMix", "Tape"), ("vinylMix", "Vinyl"), ("lofiMix", "LoFi"),
        ("vocalMix", "Formant"), ("dynMix", "Comp"), ("convolutionMix", "Space"), ("spectralMix", "Tilt"),
        ("macro_motion", "Motion"), ("macro_tone", "Tone"), ("macro_character", "Crush"), ("macro_space", "Wide"),
    ], 862, 164, cols=4, accent=accent, group="motion_fx", size=58, xgap=74, ygap=82)
    els += [
        slider("input_trim", "Input", "inputTrimDb", 96, 608, 250, 44, accent=accent2, group="bottom"),
        slider("feedback", "Feedback", "delayFeedback", 374, 608, 250, 44, accent=accent, group="bottom"),
        slider("width", "Width", "stereoWidth", 652, 608, 250, 44, accent=accent2, group="bottom"),
        slider("output", "Output", "outputGainDb", 930, 608, 250, 44, accent=accent, group="bottom"),
    ]
    return {"canvas": {"width": W, "height": H}, "elements": els}


def dsp_graph(engine: str, name: str, defaults: dict[str, float]) -> dict[str, Any]:
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
        ("Glass Pentatonic Lead", 0, 1, 0.06, 0, 4, -3, 0.10, 0.005, 0.18, 0.22, 0.28, 6200, .18, .25, .18, .32, .72, 124),
        ("Warm Fifth Pad", 1, 0, 0.12, 0, 8, -7, 0.12, .70, 1.4, .82, 2.8, 2600, .22, .50, .30, .62, .66, 96),
        ("Clean Motion Pluck", 1, 2, 0.08, 0, 5, -5, 0.04, .002, .16, .10, .18, 5200, .30, .1875, .28, .34, .70, 128),
        ("Soft Analog Keys", 0, 1, 0.10, 0, 3, -4, 0.08, .018, .42, .48, .62, 3800, .16, .375, .16, .24, .74, 100),
        ("Bright Hook Synth", 1, 0, 0.05, 1, 6, -6, 0.00, .004, .28, .68, .74, 8200, .20, .25, .22, .38, .70, 126),
        ("Velvet Chord Bed", 0, 3, 0.16, 0, 9, -9, 0.18, .95, 1.8, .74, 3.4, 2100, .10, .5, .20, .70, .62, 84),
        ("Wide Sync Arp", 1, 2, 0.14, 0, 10, -11, 0.04, .002, .12, .08, .16, 4700, .38, .125, .36, .28, .68, 132),
        ("Mellow Bell Stack", 0, 0, 0.18, 1, 2, 7, 0.00, .001, .80, .18, 1.2, 9000, .12, .25, .30, .58, .64, 110),
        ("Round Bass Pulse", 2, 0, 0.04, -1, 0, 0, 0.22, .003, .18, .20, .18, 1250, .42, .1875, .05, .05, .82, 124),
        ("Dustless Dream Pad", 3, 1, 0.08, 0, 6, -6, 0.10, 1.4, 2.4, .86, 4.6, 3400, .14, .5, .18, .76, .60, 76),
        ("Sequence Glass", 0, 2, 0.10, 0, 4, -8, 0.06, .001, .14, .12, .22, 5900, .26, .1875, .42, .36, .68, 118),
        ("Low Fifth Drone", 1, 0, 0.20, -1, 12, -12, 0.26, 1.8, 2.2, .88, 5.2, 1700, .18, .75, .12, .64, .58, 68),
        ("Clean Festival Lead", 1, 0, 0.09, 1, 14, -9, 0.03, .004, .36, .76, 1.1, 7600, .24, .25, .30, .46, .72, 128),
        ("Muted Pulse Keys", 2, 0, 0.04, 0, 0, 0, 0.08, .006, .24, .35, .28, 2400, .44, .125, .12, .18, .76, 122),
        ("Aurora Ribbon", 0, 3, 0.22, 0, 7, -2, 0.04, .30, 1.6, .68, 2.2, 4200, .20, .375, .24, .56, .64, 90),
        ("Small Room Pluck", 1, 0, 0.07, 0, 5, -5, 0.02, .001, .11, .05, .12, 6900, .28, .25, .18, .14, .70, 130),
        ("Subtle PWM Lead", 2, 1, 0.12, 0, 4, -4, 0.06, .005, .38, .66, .80, 5600, .18, .25, .20, .28, .72, 118),
        ("Cinematic Soft Saw", 1, 3, 0.15, 0, 11, -8, 0.12, .80, 2.1, .80, 3.8, 3000, .16, .5, .22, .66, .62, 82),
        ("Tiny Bell Arp", 0, 0, 0.05, 2, 2, 9, 0.00, .001, .24, .08, .36, 11200, .08, .125, .46, .44, .58, 140),
        ("Hollow Analog Lead", 3, 1, 0.09, 0, 5, -10, 0.05, .006, .42, .50, .92, 4500, .26, .375, .20, .34, .70, 104),
        ("Minimal Pulse", 2, 0, 0.03, -1, 0, 0, 0.10, .002, .12, .12, .14, 1800, .36, .1875, .08, .06, .80, 122),
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
            "decay": [0.12, .18, .26, .34, .48][i % 5],
            "sustain": 0.0 if kind == "drums" else [0.38, .48, .60, .72, .82][i % 5],
            "release": [0.10, .16, .24, .38, .60][i % 5] if kind == "drums" else [0.40, .72, 1.1, 1.8, 2.8][i % 5],
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
            "decay": [0.12, .16, .22, .30, .42][i % 5],
            "sustain": [0.05, .10, .18, .25, .36][i % 5],
            "release": [0.14, .22, .34, .48, .62][i % 5],
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


def draw_background(pack: dict[str, Any], out: Path) -> None:
    out.mkdir(parents=True, exist_ok=True)
    seed = sum(ord(c) for c in pack["display"])
    rng = random.Random(seed)
    base = Image.new("RGBA", (W, H), rgba(pack["bg"], 255))
    draw = ImageDraw.Draw(base, "RGBA")
    for y in range(H):
        t = y / H
        r = int(rgba(pack["bg"])[0] * (1 - t) + rgba(pack["bg2"])[0] * t)
        g = int(rgba(pack["bg"])[1] * (1 - t) + rgba(pack["bg2"])[1] * t)
        b = int(rgba(pack["bg"])[2] * (1 - t) + rgba(pack["bg2"])[2] * t)
        draw.line((0, y, W, y), fill=(r, g, b, 255))
    for _ in range(90):
        x = rng.randint(-80, W)
        y = rng.randint(-40, H)
        w = rng.randint(60, 260)
        h = rng.randint(2, 12)
        col = rgba(pack["line"], rng.randint(18, 70))
        draw.rounded_rectangle((x, y, x + w, y + h), radius=h // 2, fill=col)
    for _ in range(42):
        x = rng.randint(0, W)
        y = rng.randint(0, H)
        r = rng.randint(1, 3)
        draw.ellipse((x-r, y-r, x+r, y+r), fill=rgba(pack["accent_rgb"], rng.randint(50, 130)))
    for _ in range(12):
        x1 = rng.randint(0, W)
        y1 = rng.randint(0, H)
        x2 = x1 + rng.randint(-220, 220)
        y2 = y1 + rng.randint(-140, 140)
        draw.line((x1, y1, x2, y2), fill=rgba(pack["accent_rgb"], 45), width=1)
    # Product-specific structural hints, not a duplicated UI template.
    if pack["kind"] == "delay":
        for i in range(4):
            x = 142 + i * 246
            draw.rounded_rectangle((x, 162, x + 218, 322), radius=14, outline=rgba(pack["accent_rgb"], 86), width=2)
    elif pack["kind"] == "circle":
        for cx, cy, rr in [(224, 310, 78), (372, 310, 78), (520, 310, 78), (668, 310, 78), (446, 478, 78)]:
            draw.ellipse((cx-rr, cy-rr, cx+rr, cy+rr), outline=rgba(pack["accent_rgb"], 92), width=2)
            draw.ellipse((cx-rr//2, cy-rr//2, cx+rr//2, cy+rr//2), outline=rgba(pack["accent2_rgb"], 42), width=1)
    elif pack["kind"] == "drums":
        for row in range(4):
            for col in range(4):
                x = 82 + col * 100
                y = 184 + row * 78
                draw.rounded_rectangle((x, y, x + 78, y + 56), radius=10, outline=rgba(pack["accent_rgb"], 64), width=2)
    elif pack["kind"] == "sampler":
        draw.rounded_rectangle((88, 184, 778, 334), radius=16, outline=rgba(pack["accent_rgb"], 70), width=2)
        for i in range(128):
            x = 102 + i * 5
            h = 20 + int(math.sin(i * 0.37) * 18 + rng.random() * 24)
            draw.line((x, 260-h, x, 260+h), fill=rgba(pack["accent_rgb"], 90), width=2)
    elif pack["kind"] == "modfx":
        for i in range(6):
            x = 108 + i * 104
            y = 368 + (i % 2) * 38
            draw.rounded_rectangle((x, y, x + 74, y + 36), radius=8, outline=rgba(pack["accent2_rgb"], 78), width=2)
            if i > 0:
                draw.line((x - 30, y + 18, x, y + 18), fill=rgba(pack["accent_rgb"], 70), width=2)
    else:
        for i in range(5):
            x = 88 + i * 134
            draw.rounded_rectangle((x, 420, x + 92, 64 + 420), radius=14, outline=rgba(pack["accent_rgb"], 64), width=2)
    overlay = base.filter(ImageFilter.GaussianBlur(0.25))
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
    for i in range(9):
        x = 30 + i * 142
        d.line((x, 8, x + 80, 88), fill=rgba(pack["line"], 34), width=1)
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
    elif pack["kind"] == "drums":
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
    params = dedupe_params(params)
    default_values = dict(presets[0]["values"])
    graph = dsp_graph(pack["engine"], pack["display"], default_values)
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
        "description": "A playable melodic synth demo with custom performance controls and clean musical presets.",
        "tagline": "Melodic synth with plucks, pads, keys, and arps",
        "tags": ["synth", "arp", "melodic"], "accent": "ff62f7d2", "accent2": "ff8a63ff",
        "accent_rgb": "62f7d2", "accent2_rgb": "8a63ff", "bg": "05080d", "bg2": "121b27", "line": "2b4e63",
    },
    {
        "folder": "AnalogHouseDrums.patchcraft", "display": "Analog House Drums", "engine": "sample",
        "kind": "drums", "category": "Drum Machine", "theme": "split-brand", "placement": "left",
        "description": "A sample-based drum machine demo with pads, pattern grid, tuning, and mix controls.",
        "tagline": "Playable drum machine with real pad samples",
        "tags": ["drums", "sample", "pads"], "accent": "ffffa51f", "accent2": "ffff4f9a",
        "accent_rgb": "ffa51f", "accent2_rgb": "ff4f9a", "bg": "09070b", "bg2": "1d1018", "line": "583d22",
    },
    {
        "folder": "DreamKeysSampler.patchcraft", "display": "Dream Keys Sampler", "engine": "sample",
        "kind": "sampler", "category": "Sample Instrument", "theme": "wide-banner", "placement": "center",
        "description": "A playable sample instrument demo with waveform editing, drop zone, granular texture, and musical presets.",
        "tagline": "Tape keys, slices, grains, and playable zones",
        "tags": ["sampler", "keys", "sample"], "accent": "ff7ef7a8", "accent2": "ff6ab7ff",
        "accent_rgb": "7ef7a8", "accent2_rgb": "6ab7ff", "bg": "05090b", "bg2": "10201a", "line": "245c47",
    },
    {
        "folder": "CircleSeqFlagship.patchcraft", "display": "CircleSEQ Flagship", "engine": "synth",
        "kind": "circle", "category": "Circle Sequencer Demo", "theme": "split-brand", "placement": "left",
        "description": "A five-lane circular sequencer demo with lane roles, bypasses, and melodic pattern presets.",
        "tagline": "Five circular lanes for melodic performance sequencing",
        "tags": ["circle", "arp", "sequencer"], "accent": "ff9b6dff", "accent2": "ff20e0ff",
        "accent_rgb": "9b6dff", "accent2_rgb": "20e0ff", "bg": "05070d", "bg2": "131225", "line": "3c3270",
    },
    {
        "folder": "EchoCraft.patchcraft", "display": "EchoCraft", "engine": "fx",
        "kind": "delay", "category": "Delay Workstation", "theme": "wide-banner", "placement": "left",
        "description": "A delay workstation demo with four delay stages, feedback shaping, modulation, tone, and output controls.",
        "tagline": "Delay workstation with clean, tape, dual, and reverse-style motion",
        "tags": ["fx", "delay", "echo"], "accent": "ff12e8f3", "accent2": "ffffaa2b",
        "accent_rgb": "12e8f3", "accent2_rgb": "ffaa2b", "bg": "050607", "bg2": "111417", "line": "1f6b70",
    },
    {
        "folder": "ModularMotionFX.patchcraft", "display": "Modular Motion FX", "engine": "fx",
        "kind": "modfx", "category": "Creative FX", "theme": "wide-banner", "placement": "left",
        "description": "A creative FX demo with modular motion, tone, dynamics, destruction, and space controls.",
        "tagline": "Creative motion FX with studio-safe gain staging",
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
        if e.get("action"):
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
        "delay": (echocraft_layout, lambda: fx_presets("delay")),
        "modfx": (modular_fx_layout, lambda: fx_presets("modfx")),
    }
    for pack in PACKS:
        layout_builder, preset_builder = builders[pack["kind"]]
        layout = layout_builder(pack)
        presets = preset_builder()
        normalize_layout_groups(layout)
        validate_layout(layout)
        validate_presets(presets)
        write_pack(pack, layout, presets)
        print(f"rebuilt {pack['display']}: {len(layout['elements'])} elements, {len(presets)} presets")


if __name__ == "__main__":
    main()
