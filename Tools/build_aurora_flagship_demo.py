#!/usr/bin/env python3
"""Build the single Aurora flagship factory demo pack.

This intentionally creates one focused Player-ready instrument instead of the
older multi-demo set. The old FactoryDemos/*.patchcraft folders are archived,
not deleted.
"""

from __future__ import annotations

import json
import math
import random
import shutil
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FACTORY = ROOT / "FactoryDemos"
BACKLOG = ROOT / "FactoryDemosBacklog" / "legacy-before-aurora-flagship"
PACK = FACTORY / "AuroraFlagship.patchcraft"
ASSETS = PACK / "assets"
W, H = 1280, 800


def rgba(hex_rgb: str, alpha: int = 255) -> tuple[int, int, int, int]:
    value = hex_rgb.strip().lstrip("#")
    return (int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16), alpha)


def colour_string(hex_rgb: str, alpha: int = 255) -> str:
    r, g, b, a = rgba(hex_rgb, alpha)
    return f"{a:02x}{r:02x}{g:02x}{b:02x}"


def font(size: int, bold: bool = False) -> ImageFont.ImageFont:
    names = [
        "C:/Windows/Fonts/seguisb.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/bahnschrift.ttf",
        "C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf",
    ]
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except Exception:
            pass
    return ImageFont.load_default()


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def rounded(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], radius: int,
            fill: tuple[int, int, int, int], outline: tuple[int, int, int, int] | None = None,
            width: int = 1) -> None:
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def draw_text(draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str, size: int,
              color: str = "e8f5ff", bold: bool = False,
              anchor: str | None = None) -> None:
    draw.text(xy, text, fill=rgba(color), font=font(size, bold), anchor=anchor)


def aurora_background() -> Image.Image:
    rng = random.Random(1127)
    img = Image.new("RGBA", (W, H), rgba("05080d"))
    d = ImageDraw.Draw(img, "RGBA")

    for y in range(H):
        t = y / max(1, H - 1)
        top = rgba("091927")
        bottom = rgba("040609")
        r = int(top[0] * (1 - t) + bottom[0] * t)
        g = int(top[1] * (1 - t) + bottom[1] * t)
        b = int(top[2] * (1 - t) + bottom[2] * t)
        d.line((0, y, W, y), fill=(r, g, b, 255))

    # Distant planets and terrain.
    d.ellipse((820, 64, 1030, 274), fill=rgba("19374d", 72), outline=rgba("57dff0", 38), width=1)
    d.ellipse((998, 78, 1142, 222), fill=rgba("10283b", 50), outline=rgba("50c9e6", 30), width=1)
    for x in range(0, W, 40):
        y = 330 + int(18 * math.sin(x * 0.013) + 11 * math.sin(x * 0.031))
        d.polygon([(x - 80, H), (x, y), (x + 120, H)], fill=rgba("071019", 142))

    stars = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(stars, "RGBA")
    for _ in range(220):
        x = rng.randint(0, W - 1)
        y = rng.randint(0, 380)
        a = rng.randint(30, 130)
        sd.ellipse((x, y, x + 1, y + 1), fill=rgba("dff9ff", a))
    img.alpha_composite(stars)

    wave = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    wd = ImageDraw.Draw(wave, "RGBA")
    points: list[tuple[int, int]] = []
    for x in range(210, 1010, 7):
        y = 245 + int(34 * math.sin(x * 0.016) + 19 * math.sin(x * 0.041))
        points.append((x, y))
    for offset, alpha, width in [(-38, 42, 4), (-18, 82, 5), (0, 170, 5), (20, 92, 4), (42, 44, 3)]:
        shifted = [(x, y + offset + int(10 * math.sin(x * 0.033))) for x, y in points]
        wd.line(shifted, fill=rgba("43eaff", alpha), width=width, joint="curve")
    for offset, alpha in [(-8, 120), (10, 96)]:
        shifted = [(x, y + offset + int(20 * math.sin(x * 0.021))) for x, y in points]
        wd.line(shifted, fill=rgba("67fff0", alpha), width=2)
    glow = wave.filter(ImageFilter.GaussianBlur(18))
    img.alpha_composite(glow)
    img.alpha_composite(wave)
    return img


def draw_instrument_background(path: Path) -> None:
    img = aurora_background()
    d = ImageDraw.Draw(img, "RGBA")

    # Main instrument frame.
    rounded(d, (8, 8, 1272, 792), 26, rgba("05080c", 132), rgba("123243", 190), 2)
    # Center visual stage.
    rounded(d, (304, 58, 974, 326), 18, rgba("06131d", 66), rgba("133b4a", 120), 1)
    visual = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    vd = ImageDraw.Draw(visual, "RGBA")
    rng = random.Random(441)
    for _ in range(90):
        x = rng.randint(320, 960)
        y = rng.randint(80, 250)
        a = rng.randint(24, 94)
        vd.ellipse((x, y, x + 1, y + 1), fill=rgba("dff9ff", a))
    for pass_index, (offset, alpha, width) in enumerate([(-34, 58, 5), (-14, 120, 6), (0, 210, 7), (18, 138, 5), (40, 64, 4)]):
        pts = []
        for x in range(330, 950, 6):
            y = 190 + int(35 * math.sin((x + pass_index * 33) * 0.017)
                          + 18 * math.sin(x * 0.047))
            pts.append((x, y + offset))
        vd.line(pts, fill=rgba("41eaff", alpha), width=width, joint="curve")
    for x in range(300, 1000, 50):
        y = 292 + int(18 * math.sin(x * 0.018))
        vd.polygon([(x - 90, 326), (x, y), (x + 120, 326)], fill=rgba("07111a", 150))
    img.alpha_composite(visual.filter(ImageFilter.GaussianBlur(10)))
    img.alpha_composite(visual)
    rounded(d, (24, 84, 296, 394), 10, rgba("061014", 196), rgba("14333f", 190), 1)
    rounded(d, (982, 90, 1256, 390), 14, rgba("061014", 196), rgba("153947", 190), 1)
    rounded(d, (316, 348, 964, 482), 16, rgba("071014", 188), rgba("173949", 190), 1)
    rounded(d, (14, 508, 286, 664), 10, rgba("05090d", 210), rgba("1b3340", 180), 1)
    rounded(d, (300, 508, 548, 664), 10, rgba("05090d", 210), rgba("1b3340", 180), 1)
    rounded(d, (562, 508, 1110, 664), 10, rgba("05090d", 210), rgba("1b3340", 180), 1)
    rounded(d, (1126, 508, 1268, 664), 10, rgba("05090d", 210), rgba("1b3340", 180), 1)
    rounded(d, (190, 674, 1268, 786), 10, rgba("04070a", 238), rgba("1a3440", 210), 1)
    rounded(d, (16, 674, 178, 786), 8, rgba("04070a", 224), rgba("1a3440", 200), 1)

    # Title and fixed text.
    draw_text(d, (38, 30), "A U R O R A", 28, "e8f5ff")
    draw_text(d, (40, 62), "C I N E M A T I C   T E X T U R E   E N G I N E", 11, "a5b7c8")
    draw_text(d, (46, 104), "LAYER A", 13, "8feeff", True)
    draw_text(d, (46, 250), "LAYER B", 13, "8feeff", True)
    draw_text(d, (1010, 112), "MODULATION", 13, "cde6f2", True)
    draw_text(d, (330, 374), "ATMOSPHERE", 12, "cfe8f2", True)
    draw_text(d, (494, 374), "MOVEMENT", 12, "cfe8f2", True)
    draw_text(d, (666, 374), "TEXTURE", 12, "cfe8f2", True)
    draw_text(d, (828, 374), "SPACE", 12, "cfe8f2", True)
    draw_text(d, (34, 522), "ENV", 13, "cfe8f2", True)
    draw_text(d, (318, 522), "FILTER", 13, "cfe8f2", True)
    draw_text(d, (586, 522), "EFFECTS", 13, "cfe8f2", True)
    draw_text(d, (1150, 522), "MASTER", 13, "cfe8f2", True)
    draw_text(d, (54, 690), "PERFORM", 11, "a8b8c4")
    draw_text(d, (116, 690), "MOD", 11, "a8b8c4")

    # Layer cards and thumbnails.
    for idx, y in enumerate([124, 270]):
        rounded(d, (42, y, 276, y + 112), 8, rgba("081017", 210), rgba("16333d", 190), 1)
        rounded(d, (54, y + 18, 104, y + 58), 6, rgba("0b2734", 230), rgba("27dce8", 150), 1)
        for i in range(18):
            x1 = 58 + i * 3
            y1 = y + 38 + int(9 * math.sin(i * 0.7 + idx))
            d.line((x1, y1, x1 + 16, y1 - 8), fill=rgba("38eef7", 95), width=2)
        draw_text(d, (116, y + 20), "Glass Horizon" if idx == 0 else "Evolving Choir", 13, "eef8ff")
        draw_text(d, (116, y + 40), "Atmosphere" if idx == 0 else "Vocal", 11, "8ca0af")
        draw_text(d, (64, y + 74), "VOL", 10, "aab8c4")
        draw_text(d, (144, y + 74), "PAN", 10, "aab8c4")
        draw_text(d, (222, y + 74), "TUNE", 10, "aab8c4")

    # Mod rows.
    for i, (src, dst, val) in enumerate([
        ("LFO 1", "Filter Cutoff", "24%"),
        ("LFO 2", "Volume A", "32%"),
        ("Env 2", "Atmosphere", "45%"),
        ("Keytrack", "Pitch B", "18%"),
    ]):
        y = 158 + i * 44
        rounded(d, (1006, y, 1236, y + 32), 7, rgba("0a1015", 215), rgba("1b333d", 190), 1)
        draw_text(d, (1020, y + 8), src, 11, "d7e8f0")
        draw_text(d, (1106, y + 8), dst, 11, "aebcc7")
        draw_text(d, (1204, y + 8), val, 11, "d7e8f0")

    # Effect cards.
    for i, name in enumerate(["REVERB", "DELAY", "CHORUS", "CONVOLUTION"]):
        x = 592 + i * 128
        rounded(d, (x, 544, x + 112, 632), 8, rgba("0a1015", 224), rgba("204c57" if i == 3 else "19434c", 190), 1)
        draw_text(d, (x + 10, 556), name, 10, "f5d186" if i == 3 else "d8e7ef", True)
        rounded(d, (x + 12, 582, x + 86, 620), 5, rgba("102934", 120), None, 1)

    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def make_title_banner(path: Path) -> None:
    img = Image.new("RGBA", (1280, 190), rgba("05080c"))
    d = ImageDraw.Draw(img, "RGBA")
    for y in range(190):
        t = y / 189
        a = int(255 * (1 - t))
        d.line((0, y, 1280, y), fill=(5, 13 + int(12 * t), 18 + int(22 * t), a))
    for x in range(0, 1280, 170):
        d.line((x, 6, x + 110, 156), fill=rgba("1a3a4b", 55), width=1)
    rounded(d, (20, 90, 262, 166), 12, rgba("06131b", 222), rgba("1a3945", 190), 1)
    rounded(d, (36, 106, 92, 150), 6, rgba("0d3946", 220), rgba("26e2eb", 150), 1)
    draw_text(d, (108, 104), "LIBRARY", 11, "a7c2ce", True)
    draw_text(d, (108, 126), "LUMINOSITY", 15, "f2fbff")
    draw_text(d, (108, 146), "CINEMATIC COLLECTION", 10, "8da2ad")
    img.save(path)


def make_library_art(path: Path, size: tuple[int, int]) -> None:
    w, h = size
    img = Image.new("RGBA", size, rgba("071018"))
    d = ImageDraw.Draw(img, "RGBA")
    for y in range(h):
        t = y / max(1, h - 1)
        d.line((0, y, w, y), fill=(5, int(18 + 15 * t), int(28 + 34 * t), 255))
    cx, cy = w // 2, h // 2
    for r, a in [(180, 40), (128, 80), (80, 120)]:
        d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=rgba("39e6ee", a), width=max(1, w // 180))
    draw_text(d, (w // 12, h // 8), "AURORA", max(20, w // 18), "edf9ff", True)
    draw_text(d, (w // 12, h // 8 + max(28, w // 16)), "Cinematic Texture Engine", max(11, w // 44), "9fb8c7")
    img.save(path)


def element(eid: str, etype: str, x: int, y: int, w: int, h: int, **extra: Any) -> dict[str, Any]:
    data: dict[str, Any] = {
        "id": eid,
        "type": etype,
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "label": extra.pop("label", ""),
        "parameterId": extra.pop("parameterId", ""),
        "accentColour": extra.pop("accentColour", "ff33e9f3"),
        "backgroundColour": extra.pop("backgroundColour", "cc05090d"),
        "borderColour": extra.pop("borderColour", "6633e9f3"),
        "textColour": extra.pop("textColour", "ffe8f5ff"),
        "labelPosition": extra.pop("labelPosition", "hidden"),
        "labelSize": extra.pop("labelSize", 10.5),
        "contentPadding": extra.pop("contentPadding", 0.0),
        "cornerRadius": extra.pop("cornerRadius", 8.0),
        "groupId": extra.pop("groupId", ""),
    }
    data.update(extra)
    return data


def label(eid: str, text: str, x: int, y: int, w: int, h: int,
          group: str = "", size: float = 12.0, color: str = "ffe8f5ff") -> dict[str, Any]:
    return element(eid, "label", x, y, w, h, label=text, textColour=color,
                   labelPosition="hidden", labelSize=size, groupId=group)


def knob(eid: str, param_id: str, x: int, y: int, size: int = 62,
         group: str = "", accent: str = "ff33e9f3", label_text: str = "") -> dict[str, Any]:
    return element(eid, "knob", x, y, size, size, parameterId=param_id, label=label_text or param_id,
                   accentColour=accent, backgroundColour="ee080d12", borderColour="7733e9f3",
                   labelPosition="hidden", knobStyle="Modern Dark", groupId=group)


def slider(eid: str, param_id: str, x: int, y: int, w: int, h: int,
           group: str = "", accent: str = "ff33e9f3", label_text: str = "") -> dict[str, Any]:
    return element(eid, "slider", x, y, w, h, parameterId=param_id, label=label_text or param_id,
                   accentColour=accent, backgroundColour="ee080d12", borderColour="7733e9f3",
                   labelPosition="hidden", groupId=group)


def value(eid: str, param_id: str, x: int, y: int, w: int, h: int,
          group: str = "", accent: str = "ff33e9f3") -> dict[str, Any]:
    return element(eid, "valueDisplay", x, y, w, h, parameterId=param_id, label=param_id,
                   accentColour=accent, backgroundColour="cc060a0f", borderColour="5526414a",
                   labelPosition="hidden", groupId=group)


def button(eid: str, text: str, param_id: str, x: int, y: int, w: int, h: int,
           group: str = "", accent: str = "ff33e9f3") -> dict[str, Any]:
    return element(eid, "button", x, y, w, h, label=text, parameterId=param_id,
                   accentColour=accent, backgroundColour="d00a1117", borderColour=accent,
                   labelPosition="hidden", groupId=group)


def dropdown(eid: str, text: str, param_id: str, x: int, y: int, w: int, h: int,
             group: str = "", accent: str = "ff33e9f3") -> dict[str, Any]:
    return element(eid, "dropdown", x, y, w, h, label=text, parameterId=param_id,
                   accentColour=accent, backgroundColour="d00a1117", borderColour="5533e9f3",
                   labelPosition="hidden", groupId=group)


def shape(eid: str, x: int, y: int, w: int, h: int, group: str = "",
          border: str = "4433e9f3") -> dict[str, Any]:
    return element(eid, "shape", x, y, w, h, shapeKind="roundedRect",
                   backgroundColour="77070d12", borderColour=border,
                   strokeWidth=1.0, cornerRadius=12.0, groupId=group)


def param(pid: str, name: str, mn: float, mx: float, default: float,
          unit: str = "", section: str = "main", mode: str = "continuous",
          step: float = 0.0) -> dict[str, Any]:
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


def parameters() -> list[dict[str, Any]]:
    specs = [
        ("pitchWheel", "Pitch Wheel", -1, 1, 0, "", "perform"),
        ("modWheel", "Mod Wheel", 0, 1, 0.18, "", "perform"),
        ("expression", "Expression", 0, 1, 1, "", "perform"),
        ("layerAVol", "Layer A Volume", 0, 1.5, 0.72, "", "layer"),
        ("layerAPan", "Layer A Pan", -1, 1, 0, "", "layer"),
        ("layerATune", "Layer A Tune", -24, 24, 0, "st", "layer"),
        ("layerBVol", "Layer B Volume", 0, 1.5, 0.62, "", "layer"),
        ("layerBPan", "Layer B Pan", -1, 1, 0, "", "layer"),
        ("layerBTune", "Layer B Tune", -24, 24, 7, "st", "layer"),
        ("atmosphere", "Atmosphere", 0, 1, 0.72, "", "macro"),
        ("movement", "Movement", 0, 1, 0.54, "", "macro"),
        ("texture", "Texture", 0, 1, 0.66, "", "macro"),
        ("space", "Space", 0, 1, 0.78, "", "macro"),
        ("oscType", "Wave", 0, 3, 1, "", "source", "stepped", 1),
        ("osc2Type", "Wave 2", 0, 3, 0, "", "source", "stepped", 1),
        ("oscBlend", "Blend", 0, 1, 0.08, "", "source"),
        ("osc2Detune", "Osc 2 Detune", -50, 50, -4, "ct", "source"),
        ("subBlend", "Sub", 0, 1, 0.08, "", "source"),
        ("noiseBlend", "Noise", 0, 1, 0, "", "source"),
        ("attack", "Attack", 0.001, 5, 0.18, "s", "amp"),
        ("decay", "Decay", 0.001, 5, 1.1, "s", "amp"),
        ("sustain", "Sustain", 0, 1, 0.62, "", "amp"),
        ("release", "Release", 0.01, 10, 2.2, "s", "amp"),
        ("filterCutoff", "Cutoff", 20, 20000, 4200, "Hz", "filter"),
        ("filterResonance", "Resonance", 0, 1, 0.20, "", "filter"),
        ("lfoRate", "LFO Rate", 0.05, 20, 1, "Hz", "motion"),
        ("lfoAmount", "LFO Amount", 0, 1, 0.22, "", "motion"),
        ("motionDepth", "Motion Depth", 0, 1, 0.48, "", "motion"),
        ("motionSpeed", "Motion Speed", 0, 1, 0.36, "", "motion"),
        ("delayTime", "Delay Time", 0.02, 2, 0.375, "s", "fx"),
        ("delayFeedback", "Feedback", 0, 0.95, 0.28, "", "fx"),
        ("delayMix", "Delay Mix", 0, 1, 0.25, "", "fx"),
        ("reverbMix", "Reverb", 0, 1, 0.40, "", "fx"),
        ("chorusMix", "Chorus", 0, 1, 0.30, "", "fx"),
        ("convolutionMix", "Convolution", 0, 1, 0.40, "", "fx"),
        ("arpGate", "Arp Gate", 0, 1, 0.58, "", "arp"),
        ("arpRate", "Arp Rate", 0.25, 16, 4, "", "arp"),
        ("arpOctaves", "Arp Octaves", 1, 4, 2, "", "arp", "stepped", 1),
        ("volume", "Volume", 0, 1.5, 0.78, "", "output"),
        ("pan", "Pan", -1, 1, 0, "", "output"),
        ("outputGainDb", "Output", -24, 12, -6, "dB", "output"),
        ("stereoWidth", "Width", 0, 2, 1.18, "", "output"),
        ("outputLimiter", "Limiter", 0, 1, 1, "", "output", "toggle", 1),
        ("projectBpm", "BPM", 40, 240, 128, "bpm", "perform", "continuous", 1),
    ]
    return [param(*s) for s in specs]


def layout() -> dict[str, Any]:
    e: list[dict[str, Any]] = [
        element("background", "image", 0, 0, W, H, asset="assets/background-clean.png", locked=True),
        element("tabs", "tabPanel", 430, 18, 420, 42, tabs=["Main", "Motion", "FX", "Arp"],
                labelPosition="hidden", backgroundColour="dd061017", borderColour="66223f4d"),
        # Left layer controls.
        knob("layer_a_vol", "layerAVol", 52, 176, 42, label_text="Layer A Vol"),
        knob("layer_a_pan", "layerAPan", 132, 176, 42, label_text="Layer A Pan"),
        knob("layer_a_tune", "layerATune", 212, 176, 42, label_text="Layer A Tune"),
        knob("layer_b_vol", "layerBVol", 52, 322, 42, label_text="Layer B Vol"),
        knob("layer_b_pan", "layerBPan", 132, 322, 42, label_text="Layer B Pan"),
        knob("layer_b_tune", "layerBTune", 212, 322, 42, label_text="Layer B Tune"),
        # Main page between layers and modulation.
        element("main_visual", "spectrumAnalyzer", 336, 92, 600, 198, parameterId="movement",
                label="Aurora Visual", groupId="main", accentColour="ff33e9f3",
                backgroundColour="22040a0e", borderColour="0033e9f3", audioReactive=True,
                audioReactiveAmount=0.22, animationMode="breathe"),
        knob("macro_atmosphere", "atmosphere", 382, 386, 76, group="main", label_text="Atmosphere"),
        knob("macro_movement", "movement", 538, 386, 76, group="main", label_text="Movement"),
        knob("macro_texture", "texture", 696, 386, 76, group="main", label_text="Texture"),
        knob("macro_space", "space", 854, 386, 76, group="main", label_text="Space"),
        value("value_atmosphere", "atmosphere", 396, 454, 46, 20, group="main"),
        value("value_movement", "movement", 552, 454, 46, 20, group="main"),
        value("value_texture", "texture", 710, 454, 46, 20, group="main"),
        value("value_space", "space", 868, 454, 46, 20, group="main"),
        # Motion page.
        shape("motion_panel", 326, 94, 626, 382, group="motion"),
        label("motion_title", "MOTION DESIGN", 352, 118, 250, 28, group="motion", size=18),
        element("motion_xy", "xyPad", 370, 168, 240, 190, parameterId="motionDepth",
                label="Motion XY", groupId="motion", accentColour="ff33e9f3"),
        knob("motion_rate", "lfoRate", 660, 170, 82, group="motion", label_text="LFO Rate"),
        knob("motion_amount", "lfoAmount", 790, 170, 82, group="motion", label_text="LFO Amount"),
        slider("motion_depth", "motionDepth", 660, 300, 210, 42, group="motion", label_text="Depth"),
        slider("motion_speed", "motionSpeed", 660, 362, 210, 42, group="motion", label_text="Speed"),
        # FX page.
        shape("fx_panel", 326, 94, 626, 382, group="fx"),
        label("fx_title", "SPATIAL FX CHAIN", 352, 118, 250, 28, group="fx", size=18),
        knob("fx_reverb", "reverbMix", 372, 166, 82, group="fx", label_text="Reverb"),
        knob("fx_delay", "delayMix", 516, 166, 82, group="fx", label_text="Delay"),
        knob("fx_chorus", "chorusMix", 660, 166, 82, group="fx", label_text="Chorus"),
        knob("fx_convo", "convolutionMix", 804, 166, 82, group="fx", label_text="Convolution"),
        slider("fx_time", "delayTime", 372, 310, 210, 42, group="fx", label_text="Delay Time"),
        slider("fx_feedback", "delayFeedback", 624, 310, 210, 42, group="fx", label_text="Feedback"),
        # Arp page.
        shape("arp_panel", 326, 94, 626, 382, group="arp"),
        label("arp_title", "ARP PERFORMANCE", 352, 118, 250, 28, group="arp", size=18),
        element("arp_lane", "arpLane", 374, 150, 220, 220, parameterId="arpGate", label="Arp Lane",
                groupId="arp", accentColour="ff33e9f3", arpLaneIndex=0, arpLaneSteps=16,
                arpLaneMode="orbit", arpLaneTarget="notes", arpLaneRootNote=60,
                arpLaneEuclideanPulses=7, arpLaneProbability=0.96),
        knob("arp_gate", "arpGate", 654, 164, 82, group="arp", label_text="Gate"),
        knob("arp_rate", "arpRate", 784, 164, 82, group="arp", label_text="Rate"),
        dropdown("arp_octaves", "Octaves", "arpOctaves", 662, 304, 180, 32, group="arp"),
        # Modulation side panel.
        dropdown("mod_source", "Mod Wheel", "modWheel", 1012, 132, 198, 32),
        value("mod_lfo1", "lfoAmount", 1198, 164, 44, 20),
        value("mod_lfo2", "movement", 1198, 208, 44, 20),
        value("mod_env2", "atmosphere", 1198, 252, 44, 20),
        value("mod_keytrack", "layerBTune", 1198, 296, 44, 20),
        button("mod_power", "ON", "outputLimiter", 1220, 132, 28, 28),
        # Lower modules.
        element("env_curve", "eqCurve", 34, 548, 220, 80, parameterId="attack", label="Envelope",
                accentColour="ff33e9f3", backgroundColour="99060b0f", borderColour="3333e9f3"),
        dropdown("filter_type", "Low Pass 24dB", "filterCutoff", 318, 538, 210, 28),
        knob("filter_cutoff", "filterCutoff", 350, 572, 68, label_text="Cutoff"),
        knob("filter_res", "filterResonance", 464, 584, 48, label_text="Res"),
        knob("bottom_reverb", "reverbMix", 590, 584, 48, label_text="Reverb"),
        knob("bottom_delay", "delayMix", 718, 584, 48, label_text="Delay"),
        knob("bottom_chorus", "chorusMix", 846, 584, 48, label_text="Chorus"),
        knob("bottom_convolution", "convolutionMix", 974, 584, 48, accent="ffffc45a", label_text="Convolution"),
        knob("master_volume", "volume", 1150, 548, 76, label_text="Master"),
        element("master_meter", "meter", 1230, 536, 24, 96, parameterId="outputGainDb",
                label="Output", accentColour="ffffa536", backgroundColour="cc05080d", borderColour="4433e9f3"),
        button("limiter", "LIMITER", "outputLimiter", 1160, 640, 76, 24),
        # Performance and keyboard.
        element("pitch_wheel", "pitchWheel", 42, 704, 44, 72, parameterId="pitchWheel",
                label="Pitch", accentColour="ff33e9f3"),
        element("mod_wheel", "modWheel", 108, 704, 44, 72, parameterId="modWheel",
                label="Mod", accentColour="ff33e9f3"),
        element("keyboard", "keyboard", 202, 686, 1046, 86, parameterId="expression",
                label="Keyboard", accentColour="ff33e9f3", backgroundColour="ee04070a",
                borderColour="5533e9f3"),
    ]
    return {"canvas": {"width": W, "height": H}, "elements": e}


def preset(name: str, desc: str, values: dict[str, float], default: bool = False) -> dict[str, Any]:
    values = dict(values)
    values["noiseBlend"] = 0.0
    values["oscType"] = min(values.get("oscType", 1.0), 3.0)
    values["osc2Type"] = min(values.get("osc2Type", 0.0), 3.0)
    values.setdefault("outputLimiter", 1.0)
    values.setdefault("outputGainDb", -6.0)
    return {
        "name": name,
        "description": desc,
        "theme": "cinematic",
        "isDefault": default,
        "tags": ["cinematic", "pads", "motion", "musical"],
        "values": {k: round(float(v), 5) for k, v in values.items()},
        "generated": False,
    }


def presets() -> list[dict[str, Any]]:
    base = {
        "oscType": 1, "osc2Type": 0, "oscBlend": 0.08, "osc2Detune": -4,
        "subBlend": 0.06, "attack": 0.22, "decay": 1.1, "sustain": 0.62,
        "release": 2.4, "filterCutoff": 4200, "filterResonance": 0.20,
        "lfoRate": 0.75, "lfoAmount": 0.22, "delayTime": 0.375,
        "delayFeedback": 0.28, "delayMix": 0.24, "reverbMix": 0.40,
        "chorusMix": 0.28, "convolutionMix": 0.36, "layerAVol": 0.72,
        "layerBVol": 0.62, "layerBTune": 7, "atmosphere": 0.72,
        "movement": 0.54, "texture": 0.66, "space": 0.78,
        "arpGate": 0.58, "arpRate": 4, "arpOctaves": 2, "volume": 0.78,
        "stereoWidth": 1.18, "projectBpm": 128,
    }
    variants = [
        ("Ethereal Horizon", "Wide cinematic pad with gentle motion.", {}, True),
        ("Glass Horizon", "Bright pentatonic texture with a clear top layer.", {"filterCutoff": 6200, "movement": 0.48, "texture": 0.58, "reverbMix": 0.34}, False),
        ("Evolving Choir", "Soft vocal-like motion with long release.", {"oscType": 0, "osc2Type": 3, "attack": 0.7, "release": 4.8, "chorusMix": 0.38}, False),
        ("Nebula Pluck Pad", "Pluck-front pad with tempo motion.", {"attack": 0.01, "decay": 0.42, "sustain": 0.36, "release": 1.1, "arpRate": 6}, False),
        ("Deep Space Keys", "Playable key sound with controlled low end.", {"subBlend": 0.16, "filterCutoff": 2600, "delayMix": 0.16, "reverbMix": 0.22}, False),
        ("Cinematic Lift", "Open layered sound for progressions.", {"oscBlend": 0.16, "filterCutoff": 8400, "space": 0.84, "convolutionMix": 0.46}, False),
        ("Motion Bed", "Slow animated underscore patch.", {"lfoRate": 0.22, "lfoAmount": 0.36, "movement": 0.78, "release": 5.6}, False),
        ("Pulse Aurora", "Rhythmic arp-ready texture.", {"attack": 0.003, "decay": 0.18, "sustain": 0.28, "arpGate": 0.42, "arpRate": 8}, False),
    ]
    out = []
    for name, desc, changes, is_default in variants:
        data = dict(base)
        data.update(changes)
        out.append(preset(name, desc, data, is_default))
    return out


def dsp_graph(default_values: dict[str, float]) -> dict[str, Any]:
    source_values = {
        "oscType": default_values["oscType"],
        "osc2Type": default_values["osc2Type"],
        "oscBlend": default_values["oscBlend"],
        "osc2Detune": default_values["osc2Detune"],
        "subBlend": default_values["subBlend"],
        "noiseBlend": 0.0,
        "volume": default_values["volume"],
    }
    return {
        "blocks": [
            {"id": "aurora_source", "section": "source", "type": "oscillator", "name": "Aurora Dual Layer Source",
             "enabled": True, "targetId": "oscBlend", "values": source_values,
             "metadata": {"graphX": "90", "graphY": "82"}},
            {"id": "aurora_shape", "section": "filter", "type": "filter", "name": "Cinematic Tone Shape",
             "enabled": True, "targetId": "filterCutoff",
             "values": {"filterCutoff": default_values["filterCutoff"], "filterResonance": default_values["filterResonance"],
                        "attack": default_values["attack"], "decay": default_values["decay"],
                        "sustain": default_values["sustain"], "release": default_values["release"]},
             "metadata": {"graphX": "320", "graphY": "92"}},
            {"id": "aurora_motion", "section": "mod", "type": "midiPlayground", "name": "Aurora Arp Motion",
             "enabled": True, "targetId": "filterCutoff",
             "values": {"arpSteps": 8, "arpGate": default_values["arpGate"], "rate": default_values["arpRate"],
                        "sync": 1, "mpScaleRoot": 0, "mpScaleType": 1, "mpProbability": 1,
                        "arpNote0": 0, "arpNote1": 2, "arpNote2": 4, "arpNote3": 7,
                        "arpNote4": 9, "arpNote5": 7, "arpNote6": 4, "arpNote7": 2},
             "metadata": {"graphX": "320", "graphY": "350"}},
            {"id": "aurora_fx", "section": "fx", "type": "fxChain", "name": "Spatial Texture FX",
             "enabled": True, "targetId": "reverbMix",
             "values": {"delayTime": default_values["delayTime"], "delayFeedback": default_values["delayFeedback"],
                        "delayMix": default_values["delayMix"], "reverbMix": default_values["reverbMix"],
                        "chorusMix": default_values["chorusMix"], "convolutionMix": default_values["convolutionMix"]},
             "metadata": {"graphX": "650", "graphY": "102"}},
            {"id": "aurora_output", "section": "out", "type": "output", "name": "Output Safety",
             "enabled": True, "targetId": "volume",
             "values": {"volume": default_values["volume"], "outputLimiter": 1, "outputGainDb": default_values["outputGainDb"]},
             "metadata": {"graphX": "845", "graphY": "102"}},
        ],
        "edges": [
            {"id": "aurora_source_to_shape", "sourceNodeId": "aurora_source", "targetNodeId": "aurora_shape", "gain": 1.0, "enabled": True},
            {"id": "aurora_shape_to_fx", "sourceNodeId": "aurora_shape", "targetNodeId": "aurora_fx", "gain": 1.0, "enabled": True},
            {"id": "aurora_fx_to_output", "sourceNodeId": "aurora_fx", "targetNodeId": "aurora_output", "gain": 1.0, "enabled": True},
        ],
        "macros": [
            {"macroId": "atmosphere", "targetId": "filterCutoff", "targetMin": 2200, "targetMax": 9200, "curve": 0.5},
            {"macroId": "movement", "targetId": "lfoAmount", "targetMin": 0, "targetMax": 0.65, "curve": 0.5},
            {"macroId": "space", "targetId": "reverbMix", "targetMin": 0.08, "targetMax": 0.70, "curve": 0.5},
        ],
        "modulation": [],
        "automation": [],
    }


def manifest(default_preset: str) -> dict[str, Any]:
    return {
        "formatVersion": 1,
        "instrumentName": "Aurora Cinematic Texture Engine",
        "creator": "AudiCode",
        "description": "EchoCraft-branded flagship cinematic sampler-style instrument demo with layered sources, tabbed performance pages, modulation, effects, arp, wheels, and keyboard.",
        "category": "Synth Instrument",
        "engine": "synth",
        "backgroundImage": "assets/background-clean.png",
        "libraryThumbnail": "assets/library-artwork.png",
        "defaultPreset": default_preset,
        "createdWith": "PatchCraft Studio",
        "version": "1.0",
        "website": "https://plugin.club",
        "tags": ["echocraft", "flagship", "cinematic", "synth", "sampler-style", "musical", "factory-demo"],
        "playerDisplayName": "ECHOCRAFT",
        "playerTagline": "PROFESSIONAL SAMPLER",
        "playerLogoImage": "assets/thumbnail.png",
        "playerTitleBannerImage": "assets/player-title-banner.png",
        "playerTitleBarTheme": "wide-banner",
        "playerTitleTextPlacement": "left",
        "playerTitleButtonStyle": "pill",
        "playerTitleFontFamily": "Segoe UI",
        "playerBackgroundColour": "ff05080d",
        "playerPanelColour": "ff081017",
        "playerAccentColour": "ff33e9f3",
        "playerTextColour": "ffe8f5ff",
        "playerTextDimColour": "ff8fa5b3",
        "playerBorderColour": "ff1a3440",
        "playerShowPackMenu": True,
        "playerAllowPackLoading": True,
        "playerShowLibraryBrowser": True,
        "playerAllowMidiLearn": True,
        "playerShowAbout": True,
        "playerShowParameterGuidance": False,
        "playerShowPatchCraftBranding": False,
    }


def archive_old_demos() -> None:
    FACTORY.mkdir(parents=True, exist_ok=True)
    BACKLOG.mkdir(parents=True, exist_ok=True)
    for folder in FACTORY.glob("*.patchcraft"):
        if folder.name == PACK.name:
            continue
        target = BACKLOG / folder.name
        if target.exists():
            shutil.rmtree(target)
        shutil.move(str(folder), str(target))


def build() -> None:
    archive_old_demos()
    if PACK.exists():
        shutil.rmtree(PACK)
    ASSETS.mkdir(parents=True, exist_ok=True)
    draw_instrument_background(ASSETS / "background-clean.png")
    shutil.copyfile(ASSETS / "background-clean.png", ASSETS / "background.png")
    shutil.copyfile(ASSETS / "background-clean.png", ASSETS / "background-with-ui.png")
    shutil.copyfile(ASSETS / "background-clean.png", ASSETS / "background-sectioned.png")
    shutil.copyfile(ASSETS / "background-clean.png", ASSETS / "background-clean-source.png")
    make_title_banner(ASSETS / "player-title-banner.png")
    make_library_art(ASSETS / "library-artwork.png", (720, 420))
    make_library_art(ASSETS / "thumbnail.png", (512, 320))
    make_library_art(ASSETS / "player-library-modal.png", (900, 620))

    preset_list = presets()
    default_values = preset_list[0]["values"]
    write_json(PACK / "manifest.json", manifest(preset_list[0]["name"]))
    write_json(PACK / "layout.json", layout())
    write_json(PACK / "parameters.json", {"parameters": parameters()})
    write_json(PACK / "presets.json", {"presets": preset_list})
    write_json(PACK / "dspGraph.json", dsp_graph(default_values))
    write_json(PACK / "patches.json", {"patches": []})
    write_json(PACK / "sectionPresets.json", {"sectionPresets": []})
    write_json(PACK / "expansions.json", {"expansions": []})
    write_json(PACK / "midiMappings.json", {"mappings": []})
    write_json(PACK / "mappings.json", {"zones": []})
    print(f"Built {PACK}")


if __name__ == "__main__":
    build()
