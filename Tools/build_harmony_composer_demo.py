#!/usr/bin/env python3
"""Build the focused Harmony Composer factory instrument."""

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "FactoryDemos" / "HarmonyComposer.patchcraft"
ASSETS = PACK / "assets"
W, H = 1280, 800
ACCENT = "ff43e0c0"
ACCENT_2 = "ffffc857"


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def font(size: int, bold: bool = False) -> ImageFont.ImageFont:
    names = [
        "C:/Windows/Fonts/seguisb.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf",
    ]
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def build_art() -> None:
    ASSETS.mkdir(parents=True, exist_ok=True)
    image = Image.new("RGB", (W, H), "#07100f")
    pixels = image.load()
    for y in range(H):
        for x in range(W):
            glow = max(0.0, 1.0 - (((x - 640) / 760) ** 2 + ((y - 330) / 560) ** 2))
            pixels[x, y] = (
                int(5 + glow * 7),
                int(12 + glow * 12),
                int(13 + glow * 14),
            )
    overlay = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    for x in range(-200, W + 200, 52):
        draw.line((x, 0, x + 420, H), fill=(67, 224, 192, 10), width=1)
    for y in range(110, H, 64):
        draw.line((0, y, W, y), fill=(255, 200, 87, 8), width=1)
    overlay = overlay.filter(ImageFilter.GaussianBlur(0.4))
    Image.alpha_composite(image.convert("RGBA"), overlay).convert("RGB").save(ASSETS / "background.png", quality=95)

    banner = Image.new("RGB", (1280, 104), "#091513")
    bd = ImageDraw.Draw(banner)
    bd.rectangle((0, 101, 1280, 103), fill="#43e0c0")
    bd.text((34, 24), "HARMONIC MOTION", fill="#f3faf8", font=font(30, True))
    bd.text((36, 64), "DSP SYNTH + HARMONY COMPOSER", fill="#8fa9a3", font=font(13, True))
    banner.save(ASSETS / "player-title-banner.png", quality=95)

    artwork = image.resize((960, 540)).convert("RGBA")
    ad = ImageDraw.Draw(artwork)
    ad.rounded_rectangle((44, 54, 916, 486), radius=22, fill=(5, 15, 14, 215), outline=(67, 224, 192, 180), width=2)
    ad.text((82, 116), "HARMONIC", fill="#f3faf8", font=font(54, True))
    ad.text((82, 178), "MOTION", fill="#43e0c0", font=font(54, True))
    ad.text((84, 270), "Harmony Composer", fill="#ffc857", font=font(24, True))
    ad.text((84, 314), "driving a playable DSP synth", fill="#9cafaa", font=font(18))
    for index, degree in enumerate(("I", "V", "ii", "IV")):
        x = 84 + index * 128
        ad.rounded_rectangle((x, 380, x + 108, 430), radius=9, fill=(11, 28, 26, 240), outline=(255, 200, 87, 170), width=2)
        ad.text((x + 38, 390), degree, fill="#f3faf8", font=font(20, True))
    artwork.convert("RGB").save(ASSETS / "library-artwork.png", quality=95)

    modal = image.convert("RGBA")
    md = ImageDraw.Draw(modal)
    md.rounded_rectangle((120, 118, 1160, 682), radius=28, fill=(5, 15, 14, 228), outline=(67, 224, 192, 175), width=2)
    md.text((178, 190), "HARMONIC MOTION", fill="#f3faf8", font=font(46, True))
    md.text((180, 258), "Musical progressions. Playable DSP sound.", fill="#9cafaa", font=font(22))
    md.text((180, 334), "KEY + SCALE", fill="#43e0c0", font=font(16, True))
    md.text((180, 382), "CHORD PATH", fill="#ffc857", font=font(16, True))
    md.text((180, 430), "VOICING + MOTION", fill="#43e0c0", font=font(16, True))
    md.text((180, 478), "TONE + SPACE", fill="#ffc857", font=font(16, True))
    md.text((180, 564), "8 musical starting points", fill="#f3faf8", font=font(20, True))
    modal.convert("RGB").save(ASSETS / "player-library-modal.png", quality=95)


def shape(eid: str, x: int, y: int, w: int, h: int, title: str, accent: str = ACCENT) -> dict:
    return {
        "id": eid, "type": "shape", "x": x, "y": y, "width": w, "height": h,
        "shapeKind": "roundedRect", "backgroundColour": "d90a1515", "borderColour": accent,
        "strokeWidth": 1.2, "cornerRadius": 8.0, "groupId": "main", "locked": True,
        "semanticRole": title,
    }


def label(eid: str, text: str, x: int, y: int, w: int, h: int, size: float = 12.0,
          colour: str = "ffeaf5f2") -> dict:
    return {
        "id": eid, "type": "label", "label": text, "x": x, "y": y, "width": w, "height": h,
        "textColour": colour, "labelSize": size, "labelPosition": "hidden", "groupId": "main",
    }


def knob(eid: str, text: str, parameter: str, x: int, y: int, accent: str = ACCENT, size: int = 66) -> dict:
    return {
        "id": eid, "type": "knob", "label": text, "parameterId": parameter,
        "x": x, "y": y, "width": size, "height": size + 20, "knobStyle": "Modern Dark",
        "accentColour": accent, "backgroundColour": "00000000", "borderColour": "663b5a54",
        "labelPosition": "bottom", "labelSize": 10.0, "contentPadding": 0.0, "groupId": "main",
    }


def dropdown(eid: str, text: str, parameter: str, x: int, y: int, w: int, h: int = 34,
             accent: str = ACCENT) -> dict:
    return {
        "id": eid, "type": "dropdown", "label": text, "parameterId": parameter,
        "x": x, "y": y, "width": w, "height": h, "accentColour": accent,
        "backgroundColour": "ee0b1716", "borderColour": "8843e0c0",
        "labelPosition": "hidden", "labelSize": 10.0, "groupId": "main",
    }


def toggle(eid: str, text: str, parameter: str, x: int, y: int, w: int = 48, h: int = 30) -> dict:
    return {
        "id": eid, "type": "toggle", "label": text, "parameterId": parameter,
        "x": x, "y": y, "width": w, "height": h, "accentColour": ACCENT,
        "backgroundColour": "ee0b1716", "borderColour": "8843e0c0",
        "labelPosition": "hidden", "labelSize": 9.0, "groupId": "main",
    }


def parameter(pid: str, name: str, minimum: float, maximum: float, default: float,
              section: str, unit: str = "", mode: str = "continuous", step: float = 0.0,
              visible: bool = True) -> dict:
    return {
        "id": pid, "name": name, "type": "float", "min": minimum, "max": maximum,
        "default": default, "unit": unit, "section": section, "category": section.title(),
        "displayMode": mode, "step": step, "smoothing": 0.02,
        "hostAutomatable": True, "midiLearnable": True, "modulatable": mode != "toggle",
        "visible": visible,
    }


def parameters() -> list[dict]:
    values = [
        parameter("oscType", "Oscillator 1", 0, 3, 1, "source", mode="stepped", step=1, visible=False),
        parameter("osc2Type", "Oscillator 2", 0, 3, 0, "source", mode="stepped", step=1, visible=False),
        parameter("oscBlend", "Oscillator Blend", 0, 1, 0.12, "source"),
        parameter("subBlend", "Sub Level", 0, 1, 0.08, "source"),
        parameter("noiseBlend", "Noise Level", 0, 1, 0, "source", visible=False),
        parameter("detune", "Detune", -50, 50, 4, "source", "cent"),
        parameter("filterCutoff", "Filter Cutoff", 20, 20000, 5600, "filter", "Hz"),
        parameter("filterResonance", "Resonance", 0, 1, 0.18, "filter"),
        parameter("attack", "Attack", 0.001, 5, 0.01, "amp", "s"),
        parameter("decay", "Decay", 0.001, 5, 0.26, "amp", "s"),
        parameter("sustain", "Sustain", 0, 1, 0.38, "amp"),
        parameter("release", "Release", 0.01, 10, 0.52, "amp", "s"),
        parameter("delayTime", "Delay Time", 0.02, 2, 0.375, "fx", "s"),
        parameter("delayFeedback", "Delay Feedback", 0, 0.92, 0.24, "fx"),
        parameter("delayMix", "Delay Mix", 0, 1, 0.18, "fx"),
        parameter("reverbMix", "Reverb Mix", 0, 1, 0.24, "fx"),
        parameter("volume", "Instrument Level", 0, 1, 0.62, "out"),
        parameter("pan", "Pan", -1, 1, 0, "out"),
        parameter("outputGainDb", "Output Gain", -24, 12, -5, "out", "dB"),
        parameter("outputLimiter", "Safety Limiter", 0, 1, 1, "out", mode="toggle", step=1),
        parameter("outputCeilingDb", "Limiter Ceiling", -12, 0, -1, "out", "dB"),
        parameter("projectBpm", "Tempo", 40, 220, 112, "mod", "BPM", "stepped", 1),
        parameter("pitchWheel", "Pitch Wheel", -1, 1, 0, "performance"),
        parameter("modWheel", "Mod Wheel", 0, 1, 0, "performance"),
        parameter("composerRoot", "Key", 0, 11, 0, "mod", mode="stepped", step=1),
        parameter("composerScale", "Scale", 0, 23, 5, "mod", mode="stepped", step=1),
        parameter("composerChordCount", "Chord Count", 1, 8, 4, "mod", mode="stepped", step=1),
        parameter("composerRate", "Beats Per Chord", 0.125, 16, 1, "mod", "beat"),
        parameter("composerGate", "Chord Gate", 0.05, 1, 0.78, "mod"),
        parameter("composerVelocity", "Chord Velocity", 0.01, 1, 0.74, "mod"),
        parameter("composerVoices", "Voices", 1, 8, 4, "mod", mode="stepped", step=1),
        parameter("composerOctave", "Register", 1, 7, 4, "mod", "oct", "stepped", 1),
        parameter("composerSpread", "Voicing Spread", 0, 1, 0.42, "mod"),
        parameter("composerOutputChannel", "MIDI Channel", 1, 16, 1, "mod", mode="stepped", step=1),
        parameter("composerMidiThru", "MIDI Thru", 0, 1, 1, "mod", mode="toggle", step=1),
    ]
    defaults = [0, 5, 3, 4, 0, 5, 3, 4]
    for chord in range(1, 9):
        values.extend([
            parameter(f"composerDegree{chord}", f"Chord {chord}", 0, 6, defaults[chord - 1], "mod", mode="stepped", step=1),
            parameter(f"composerInversion{chord}", f"Chord {chord} Inversion", -1, 7, -1, "mod", mode="stepped", step=1, visible=False),
            parameter(f"composerChord{chord}On", f"Chord {chord} Enabled", 0, 1, 1 if chord <= 4 else 0, "mod", mode="toggle", step=1),
        ])
    return values


def preset(name: str, root: int, scale: int, progression: list[int], bpm: int,
           cutoff: float, attack: float, release: float, delay: float, reverb: float,
           spread: float, voices: int, waveform: int, waveform2: int) -> dict:
    values = {
        "composerRoot": root, "composerScale": scale, "composerChordCount": len(progression),
        "composerRate": 1.0, "composerGate": 0.78, "composerVelocity": 0.72,
        "composerVoices": voices, "composerOctave": 4, "composerSpread": spread,
        "composerOutputChannel": 1, "composerMidiThru": 1, "projectBpm": bpm,
        "oscType": waveform, "osc2Type": waveform2, "oscBlend": 0.10,
        "subBlend": 0.06, "noiseBlend": 0.0, "detune": 4.0,
        "filterCutoff": cutoff, "filterResonance": 0.18,
        "attack": attack, "decay": 0.28, "sustain": 0.38, "release": release,
        "delayTime": 0.375, "delayFeedback": 0.24, "delayMix": delay, "reverbMix": reverb,
        "volume": 0.60, "pan": 0.0, "outputGainDb": -5.0,
        "outputLimiter": 1.0, "outputCeilingDb": -1.0,
    }
    for chord in range(1, 9):
        active = chord <= len(progression)
        values[f"composerDegree{chord}"] = progression[chord - 1] if active else 0
        values[f"composerInversion{chord}"] = -1
        values[f"composerChord{chord}On"] = 1 if active else 0
    return {
        "name": name,
        "description": "A musical harmony and DSP snapshot with its own progression, voicing, tone, and space.",
        "theme": "harmony-composer", "tags": ["composer", "musical", "factory-demo"],
        "isDefault": False, "generated": False, "values": values,
    }


def build_layout() -> dict:
    elements: list[dict] = [
        {"id": "background", "type": "image", "x": 0, "y": 0, "width": W, "height": H,
         "asset": "assets/background.png", "locked": True},
        label("title", "HARMONIC MOTION", 40, 24, 420, 34, 24),
        label("subtitle", "Harmony Composer driving a playable DSP synth", 42, 58, 520, 22, 12, "ff8fa9a3"),
        dropdown("presets", "Preset", "", 1022, 32, 214, 36, ACCENT_2),
        shape("source_panel", 34, 102, 276, 410, "DSP SOURCE"),
        shape("composer_panel", 330, 102, 614, 410, "HARMONY COMPOSER", ACCENT_2),
        shape("finish_panel", 964, 102, 282, 410, "TONE + SPACE"),
        label("source_header", "1  DSP SYNTH SOURCE", 54, 120, 220, 24, 13, ACCENT),
        label("source_truth", "This is the sound generator", 54, 145, 220, 18, 10, "ff8fa9a3"),
        knob("osc_blend", "Osc Blend", "oscBlend", 54, 180),
        knob("sub_blend", "Sub", "subBlend", 132, 180),
        knob("detune", "Detune", "detune", 210, 180, ACCENT_2),
        knob("attack", "Attack", "attack", 54, 282),
        knob("decay", "Decay", "decay", 132, 282),
        knob("sustain", "Sustain", "sustain", 210, 282),
        knob("release", "Release", "release", 132, 392),
        label("composer_header", "2  CHORD PATH", 350, 120, 180, 24, 13, ACCENT_2),
        label("composer_help", "Choose a key, scale, progression, then press Play", 350, 145, 400, 18, 10, "ff9cafaa"),
        dropdown("composer_key", "Key", "composerRoot", 350, 174, 110),
        dropdown("composer_scale", "Scale", "composerScale", 472, 174, 184),
        dropdown("composer_count", "Length", "composerChordCount", 668, 174, 112),
        {"id": "composer_play", "type": "button", "label": "Play", "action": "transport.toggle",
         "x": 794, "y": 174, "width": 126, "height": 34, "accentColour": ACCENT_2,
         "backgroundColour": "ee182019", "borderColour": ACCENT_2, "labelPosition": "hidden",
         "labelSize": 11.0, "groupId": "main"},
    ]

    for index in range(8):
        column = index % 4
        row = index // 4
        x = 350 + column * 142
        y = 238 + row * 58
        elements.append(dropdown(f"chord_{index + 1}", str(index + 1), f"composerDegree{index + 1}", x, y, 96, 34, ACCENT_2))
        elements.append(toggle(f"chord_{index + 1}_on", "On", f"composerChord{index + 1}On", x + 100, y + 2, 34, 30))

    elements.extend([
        knob("composer_rate", "Rate", "composerRate", 356, 370, ACCENT_2),
        knob("composer_gate", "Gate", "composerGate", 438, 370, ACCENT_2),
        knob("composer_velocity", "Velocity", "composerVelocity", 520, 370, ACCENT_2),
        knob("composer_voices", "Voices", "composerVoices", 602, 370, ACCENT_2),
        knob("composer_spread", "Spread", "composerSpread", 684, 370, ACCENT_2),
        knob("composer_register", "Register", "composerOctave", 766, 370, ACCENT_2),
        label("finish_header", "3  SHAPE + FINISH", 984, 120, 220, 24, 13, ACCENT),
        knob("cutoff", "Cutoff", "filterCutoff", 988, 174),
        knob("resonance", "Resonance", "filterResonance", 1070, 174),
        knob("level", "Level", "volume", 1152, 174, ACCENT_2),
        knob("delay", "Delay", "delayMix", 988, 284),
        knob("feedback", "Feedback", "delayFeedback", 1070, 284),
        knob("reverb", "Reverb", "reverbMix", 1152, 284),
        knob("output", "Output", "outputGainDb", 1028, 394, ACCENT_2),
        toggle("limiter", "Limiter", "outputLimiter", 1122, 410, 94, 34),
        label("play_header", "PLAY THE SOUND", 42, 540, 180, 22, 12, ACCENT),
        {"id": "pitch_wheel", "type": "pitchWheel", "label": "Pitch", "parameterId": "pitchWheel",
         "x": 42, "y": 580, "width": 38, "height": 150, "accentColour": ACCENT,
         "labelPosition": "bottom", "labelSize": 9.0, "groupId": "main"},
        {"id": "mod_wheel", "type": "modWheel", "label": "Mod", "parameterId": "modWheel",
         "x": 88, "y": 580, "width": 38, "height": 150, "accentColour": ACCENT_2,
         "labelPosition": "bottom", "labelSize": 9.0, "groupId": "main"},
        {"id": "keyboard", "type": "keyboard", "label": "Keyboard", "x": 148, "y": 568,
         "width": 1098, "height": 176, "accentColour": ACCENT, "backgroundColour": "ee091312",
         "borderColour": "8843e0c0", "groupId": "main"},
    ])
    return {"canvas": {"width": W, "height": H}, "elements": elements}


def build() -> None:
    build_art()
    presets = [
        preset("C Lydian Glass", 0, 5, [0, 4, 1, 3], 112, 6400, 0.004, 0.42, 0.16, 0.24, 0.48, 4, 1, 0),
        preset("A Minor Night Drive", 9, 2, [0, 5, 3, 4], 96, 3600, 0.018, 0.78, 0.26, 0.34, 0.56, 4, 1, 2),
        preset("D Dorian Pulse", 2, 3, [0, 3, 6, 4, 0, 3, 1, 4], 124, 7200, 0.002, 0.28, 0.12, 0.16, 0.30, 3, 2, 0),
        preset("G Mixolydian Lift", 7, 6, [0, 6, 3, 4], 118, 5200, 0.008, 0.52, 0.18, 0.22, 0.42, 4, 1, 3),
        preset("F Major Soul", 5, 1, [0, 5, 1, 4], 88, 2900, 0.055, 1.20, 0.22, 0.46, 0.62, 5, 0, 3),
        preset("E Minor Nocturne", 4, 2, [0, 3, 5, 4], 76, 2400, 0.28, 2.40, 0.34, 0.58, 0.70, 5, 0, 2),
        preset("C Major Forward", 0, 1, [0, 4, 5, 3], 128, 8200, 0.002, 0.24, 0.10, 0.12, 0.24, 3, 2, 1),
        preset("B Phrygian Tension", 11, 4, [0, 1, 5, 4], 104, 4100, 0.012, 0.66, 0.28, 0.30, 0.50, 4, 1, 2),
    ]
    presets[0]["isDefault"] = True
    default_values = presets[0]["values"]
    composer_values = {key: value for key, value in default_values.items() if key.startswith("composer")}

    manifest = {
        "formatVersion": 1, "instrumentName": "Harmonic Motion", "creator": "AudiCode",
        "description": "A focused Harmony Composer demo showing one clear path: harmony generates MIDI and the DSP synth generates sound.",
        "category": "Harmony Instrument", "engine": "synth", "backgroundImage": "assets/background.png",
        "libraryThumbnail": "assets/library-artwork.png", "defaultPreset": presets[0]["name"],
        "createdWith": "PatchCraft Studio", "version": "1.0.0", "playerDisplayName": "Harmonic Motion",
        "playerTagline": "Harmony-driven DSP instrument", "playerAccentColour": ACCENT,
        "playerPanelColour": "ff0a1515", "playerBackgroundColour": "ff06100f",
        "playerTitleBannerImage": "assets/player-title-banner.png", "playerTitleBarTheme": "minimal",
        "playerTitleTextPlacement": "left", "playerTitleButtonStyle": "outlined",
        "playerTitleFontFamily": "Segoe UI", "playerShowPatchCraftBranding": False,
        "tags": ["composer", "harmony", "midi", "musical", "synth", "factory-demo", "player-ready"],
    }
    graph = {
        "userConfigured": True,
        "blocks": [
            {"id": "dsp_synth_source", "section": "source", "type": "oscStack", "name": "DSP Synth Source",
             "enabled": True, "targetId": "oscBlend", "metadata": {"family": "synth", "role": "source", "ioMode": "stereo"},
             "values": {key: default_values[key] for key in ("oscType", "osc2Type", "oscBlend", "subBlend", "noiseBlend", "detune")}},
            {"id": "dsp_tone_shape", "section": "shape", "type": "filterEnvelope", "name": "Tone and Envelope",
             "enabled": True, "targetId": "filterCutoff", "metadata": {"family": "studio", "role": "tone", "ioMode": "stereo"},
             "values": {key: default_values[key] for key in ("filterCutoff", "filterResonance", "attack", "decay", "sustain", "release")}},
            {"id": "harmony_composer", "section": "mod", "type": "harmonyComposer", "name": "Harmony Composer",
             "enabled": True, "targetId": "composerRoot", "metadata": {"family": "midi", "role": "harmonyComposer", "ioMode": "event"},
             "values": composer_values},
            {"id": "dsp_space", "section": "fx", "type": "fxChain", "name": "Delay and Reverb",
             "enabled": True, "targetId": "delayMix", "metadata": {"family": "creative", "role": "space", "ioMode": "stereo"},
             "values": {key: default_values[key] for key in ("delayTime", "delayFeedback", "delayMix", "reverbMix")}},
            {"id": "dsp_output", "section": "out", "type": "limiter", "name": "Safe Output",
             "enabled": True, "targetId": "outputGainDb", "metadata": {"family": "studio", "role": "dynamics", "ioMode": "stereo"},
             "values": {key: default_values[key] for key in ("volume", "pan", "outputGainDb", "outputLimiter", "outputCeilingDb")}},
        ],
        "modulation": [],
    }

    write_json(PACK / "manifest.json", manifest)
    write_json(PACK / "layout.json", build_layout())
    write_json(PACK / "parameters.json", {"parameters": parameters()})
    write_json(PACK / "presets.json", {"presets": presets})
    write_json(PACK / "dspGraph.json", graph)
    write_json(PACK / "patches.json", {"patches": []})
    write_json(PACK / "sectionPresets.json", {"sectionPresets": []})
    write_json(PACK / "expansions.json", {"expansions": []})
    write_json(PACK / "midiMappings.json", {"mappings": []})
    write_json(PACK / "mappings.json", {"zones": []})
    print(PACK)


if __name__ == "__main__":
    build()
