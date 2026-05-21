from __future__ import annotations

import json
import math
import random
import shutil
import struct
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DEMO = ROOT / "FactoryDemos" / "NeonPulseDrumMachine.patchcraft"
TARGET = ROOT / "FactoryDemos" / "ImportPerformanceDrumLab.patchcraft"
SR = 44100


def write_json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def rounded(draw: ImageDraw.ImageDraw, xy, radius, fill, outline=None, width=1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


def generate_backgrounds(assets: Path) -> None:
    random.seed(240515)
    width, height = 1280, 800
    img = Image.new("RGB", (width, height), "#05070c")
    pix = img.load()
    for y in range(height):
        for x in range(width):
            nx = x / width
            ny = y / height
            glow = max(0.0, 1.0 - math.hypot(nx - 0.72, ny - 0.24) * 1.8)
            amber = max(0.0, 1.0 - math.hypot(nx - 0.22, ny - 0.72) * 1.9)
            r = int(5 + 28 * glow + 44 * amber)
            g = int(8 + 36 * glow + 22 * amber)
            b = int(13 + 62 * glow + 10 * amber)
            pix[x, y] = (r, g, b)

    overlay = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    for x in range(0, width, 40):
        alpha = 26 if x % 160 == 0 else 12
        draw.line((x, 0, x, height), fill=(62, 176, 200, alpha), width=1)
    for y in range(0, height, 40):
        alpha = 24 if y % 160 == 0 else 10
        draw.line((0, y, width, y), fill=(255, 160, 42, alpha), width=1)

    for i in range(80):
        x = random.randint(20, width - 20)
        y = random.randint(20, height - 20)
        length = random.randint(24, 140)
        colour = (255, 160, 42, random.randint(25, 90)) if i % 3 else (0, 210, 255, random.randint(22, 72))
        draw.line((x, y, x + length, y + random.randint(-10, 18)), fill=colour, width=random.choice([1, 1, 2]))

    for row in range(4):
        for col in range(4):
            x = 82 + col * 108
            y = 154 + row * 74
            rounded(draw, (x, y, x + 92, y + 58), 12, (8, 14, 22, 118), (255, 160, 42, 90), 1)
            draw.rectangle((x + 12, y + 42, x + 80, y + 45), fill=(255, 160, 42, 80))

    for row in range(8):
        y = 160 + row * 24
        draw.text((570, y), f"{row + 1}", fill=(160, 178, 190, 80))
        for step in range(16):
            x = 610 + step * 33
            active = (step + row * 2) % (4 if row < 2 else 3) == 0
            fill = (255, 160, 42, 120) if active else (12, 18, 28, 125)
            rounded(draw, (x, y, x + 24, y + 16), 4, fill, (0, 210, 255, 45), 1)

    for i in range(7):
        y = 515 + i * 30
        points = []
        for x in range(80, 1190, 12):
            wave_y = y + math.sin((x / 42.0) + i) * (7 + i)
            points.append((x, wave_y))
        draw.line(points, fill=(0, 210, 255, 36 + i * 8), width=2)

    img = Image.alpha_composite(img.convert("RGBA"), overlay)
    vignette = Image.new("L", (width, height), 0)
    vdraw = ImageDraw.Draw(vignette)
    vdraw.ellipse((-180, -160, width + 180, height + 160), fill=220)
    vignette = vignette.filter(ImageFilter.GaussianBlur(90))
    dark = Image.new("RGBA", (width, height), (0, 0, 0, 112))
    img = Image.composite(img, Image.alpha_composite(img, dark), vignette)
    img.save(assets / "background.png")
    img.save(assets / "background-sectioned.png")
    img.resize((420, 262), Image.Resampling.LANCZOS).save(assets / "thumbnail.png")


def write_wav(path: Path, samples: list[float]) -> None:
    peak = max(0.001, max(abs(s) for s in samples))
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(SR)
        frames = bytearray()
        for sample in samples:
            value = int(max(-1.0, min(1.0, sample / peak * 0.92)) * 32767)
            frames.extend(struct.pack("<h", value))
        handle.writeframes(frames)


def generate_import_samples(folder: Path) -> None:
    random.seed(99)

    def env(i: int, total: int, decay: float) -> float:
        return math.exp(-decay * i / max(1, total))

    n = int(SR * 0.75)
    kick = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        freq = 118 * math.exp(-8.0 * t) + 34
        phase += 2 * math.pi * freq / SR
        click = 0.45 * math.sin(2 * math.pi * 1800 * t) * math.exp(-95 * t)
        kick.append(math.sin(phase) * env(i, n, 7.5) + click)
    write_wav(folder / "Import_Sub_Kick_C1.wav", kick)

    n = int(SR * 0.55)
    snare = []
    for i in range(n):
        t = i / SR
        noise = (random.random() * 2 - 1) * math.exp(-15 * t)
        body = math.sin(2 * math.pi * 190 * t) * math.exp(-17 * t)
        snap = math.sin(2 * math.pi * 3200 * t) * math.exp(-58 * t)
        snare.append(noise * 0.7 + body * 0.35 + snap * 0.25)
    write_wav(folder / "Import_Dust_Snare_D1.wav", snare)

    n = int(SR * 0.32)
    hat = []
    for i in range(n):
        t = i / SR
        burst = (random.random() * 2 - 1) * math.exp(-34 * t)
        metallic = math.sin(2 * math.pi * 8800 * t) * math.exp(-28 * t)
        hat.append(burst * 0.75 + metallic * 0.18)
    write_wav(folder / "Import_Ratchet_Hat_Fs1.wav", hat)

    n = int(SR * 0.42)
    perc = []
    for i in range(n):
        t = i / SR
        tone = math.sin(2 * math.pi * (520 + 90 * math.sin(2 * math.pi * 11 * t)) * t)
        tick = (random.random() * 2 - 1) * math.exp(-55 * t)
        perc.append(tone * math.exp(-10 * t) * 0.7 + tick * 0.35)
    write_wav(folder / "Import_Click_Perc_A1.wav", perc)


def vlq(value: int) -> bytes:
    out = [value & 0x7F]
    value >>= 7
    while value:
        out.insert(0, (value & 0x7F) | 0x80)
        value >>= 7
    return bytes(out)


def write_midi(path: Path, notes: list[tuple[int, int, int, int]], bpm: int = 124) -> None:
    ppq = 480
    events: list[tuple[int, bytes]] = []
    tempo = int(60_000_000 / bpm)
    track = bytearray()
    track += vlq(0) + b"\xff\x51\x03" + tempo.to_bytes(3, "big")
    track += vlq(0) + b"\xff\x58\x04\x04\x02\x18\x08"
    for tick, note, velocity, length in notes:
        events.append((tick, bytes([0x99, note & 0x7F, max(1, min(127, velocity))])))
        events.append((tick + length, bytes([0x89, note & 0x7F, 0])))
    events.sort(key=lambda item: (item[0], item[1][0] == 0x99))
    last = 0
    for tick, payload in events:
        track += vlq(max(0, tick - last)) + payload
        last = tick
    end_tick = ppq * 4
    track += vlq(max(0, end_tick - last)) + b"\xff\x2f\x00"
    data = b"MThd" + struct.pack(">IHHH", 6, 0, 1, ppq) + b"MTrk" + struct.pack(">I", len(track)) + bytes(track)
    path.write_bytes(data)


def generate_midi_examples(folder: Path) -> None:
    step = 120
    four = []
    for s in [0, 4, 8, 12]:
        four.append((s * step, 36, 116, 90))
    for s in [4, 12]:
        four.append((s * step, 38, 108, 90))
        four.append((s * step, 39, 82, 80))
    for s in range(0, 16, 2):
        four.append((s * step, 42, 76 if s % 4 else 92, 45))
    for s in [3, 7, 11, 15]:
        four.append((s * step, 45, 70, 50))
    write_midi(folder / "Import_Four_On_Floor_Drum_Groove.mid", four)

    trap = []
    for s in [0, 7, 10, 14]:
        trap.append((s * step, 36, 118, 90))
    for s in [4, 12]:
        trap.append((s * step, 38, 110, 90))
    for s in range(16):
        trap.append((s * step, 42, 68 + (s % 4) * 8, 38))
    for sub in [13.0, 13.33, 13.66, 14.0, 14.33, 14.66]:
        trap.append((int(sub * step), 46, 92, 28))
    write_midi(folder / "Import_Trap_Hat_Rolls.mid", trap)

    broken = []
    for s in [0, 3, 8, 10, 15]:
        broken.append((s * step, 36, 112, 85))
    for s in [5, 12]:
        broken.append((s * step, 38, 115, 90))
    for s in [2, 4, 6, 9, 11, 13, 15]:
        broken.append((s * step, 42, 72, 42))
    for s in [1, 7, 14]:
        broken.append((s * step, 48, 86, 70))
    write_midi(folder / "Import_Broken_Perc_Sequence.mid", broken)


def shape(id_: str, x: int, y: int, w: int, h: int, group: str = "", border="#ffa62a", bg="#0a0f17", radius=16, alpha=0.86):
    return {
        "id": id_, "type": "shape", "x": x, "y": y, "width": w, "height": h,
        "shapeKind": "roundedRect", "groupId": group, "locked": True,
        "backgroundColour": bg, "borderColour": border, "strokeWidth": 1.2,
        "cornerRadius": radius, "opacity": alpha, "glowAmount": 0.12,
    }


def label(id_: str, text: str, x: int, y: int, w: int, h: int, group: str = "", size=13, colour="#f2f5ff", justify="centredLeft"):
    return {
        "id": id_, "type": "label", "label": text, "x": x, "y": y,
        "width": w, "height": h, "groupId": group, "labelSize": size,
        "textColour": colour, "justification": justify,
    }


def knob(id_: str, text: str, param: str, x: int, y: int, group: str, accent="#ffa62a"):
    return {
        "id": id_, "type": "knob", "label": text, "parameterId": param,
        "x": x, "y": y, "width": 78, "height": 88, "groupId": group,
        "style": "Factory Glass", "knobStyle": "Vintage 01",
        "labelPosition": "bottom", "labelSize": 12,
        "accentColour": accent, "backgroundColour": "#0b0f17",
        "borderColour": "#273449", "cornerRadius": 10,
    }


def slider(id_: str, text: str, param: str, x: int, y: int, group: str, accent="#00d7ff"):
    return {
        "id": id_, "type": "slider", "label": text, "parameterId": param,
        "x": x, "y": y, "width": 36, "height": 132, "groupId": group,
        "labelPosition": "bottom", "labelSize": 11,
        "accentColour": accent, "backgroundColour": "#0b0f17",
        "borderColour": "#273449", "cornerRadius": 8,
    }


def build_layout() -> dict:
    elements = [
        {"id": "background", "type": "image", "x": 0, "y": 0, "width": 1280, "height": 800, "asset": "assets/background.png", "locked": True},
        shape("top_chrome", 28, 18, 1224, 72, "", "#ffa62a", "#070b11", 18, 0.9),
        label("title", "IMPORT PERFORMANCE DRUM LAB", 54, 30, 500, 28, "", 23, "#f6f8ff"),
        label("tagline", "Drag WAV or MIDI into the Player - or use the IMPORT toolbar.", 56, 60, 650, 20, "", 11, "#9aa4b4"),
        {"id": "presets", "type": "dropdown", "x": 760, "y": 35, "width": 300, "height": 34, "label": "Preset", "backgroundColour": "#0b0f17", "borderColour": "#ffa62a", "cornerRadius": 8},
        {"id": "tabs", "type": "tabPanel", "x": 120, "y": 506, "width": 1040, "height": 38, "tabs": ["Play", "Import", "MIDI", "Mix", "Perform"], "backgroundColour": "#080d14", "borderColour": "#ffa62a", "accentColour": "#ffa62a", "cornerRadius": 9},
        {"id": "keyboard", "type": "keyboard", "x": 54, "y": 712, "width": 1172, "height": 66, "backgroundColour": "#0b0f17", "borderColour": "#273449"},
    ]

    elements += [
        shape("play_pad_panel", 46, 112, 466, 352, "play", "#ffa62a", "#0a0e15"),
        label("play_pad_title", "PLAYABLE 16 PAD KIT", 68, 126, 320, 22, "play", 13, "#ffa62a"),
        {"id": "play_pad_grid", "type": "padGrid", "x": 70, "y": 156, "width": 420, "height": 270, "groupId": "play", "padRows": 4, "padCols": 4, "padBaseNote": 36, "backgroundColour": "#0b0f17", "borderColour": "#ffa62a", "accentColour": "#ffa62a", "cornerRadius": 12},
        shape("play_pattern_panel", 532, 112, 704, 352, "play", "#00d7ff", "#0a0e15"),
        label("play_pattern_title", "PERFORMANCE OVERVIEW", 556, 126, 320, 22, "play", 13, "#00d7ff"),
        {"id": "play_wave", "type": "waveform", "x": 556, "y": 152, "width": 640, "height": 88, "label": "Live Output", "groupId": "play", "backgroundColour": "#0b0f17", "accentColour": "#00d7ff", "cornerRadius": 10, "audioReactive": True, "audioReactiveAmount": 0.42},
        label("play_hint", "Factory pads are already mapped. To add your own kit, drag WAV files onto this window or open IMPORT in the Player toolbar.", 556, 252, 620, 54, "play", 13, "#d8e3f0"),
        knob("play_start", "Start", "sampleStart", 566, 324, "play"),
        knob("play_length", "Length", "sampleLength", 676, 324, "play"),
        knob("play_pitch", "Pitch", "samplePitch", 786, 324, "play"),
        knob("play_glitch", "Glitch", "sampleGlitch", 896, 324, "play"),
    ]

    elements += [
        shape("import_drop_a", 78, 130, 356, 184, "import", "#ffa62a", "#0a0e15"),
        label("import_a_title", "1  SAMPLE IMPORT", 106, 154, 260, 22, "import", 14, "#ffa62a"),
        label("import_a_body", "Drop WAV, AIFF, or FLAC onto the Player. They become extra playable pads.", 106, 190, 286, 50, "import", 13, "#d8e3f0"),
        shape("import_a_callout", 106, 252, 280, 42, "import", "#ffa62a", "#121a24", 10, 0.92),
        label("import_a_action", "DRAG AUDIO FILES HERE", 124, 265, 246, 18, "import", 13, "#ffa62a", "centred"),
        shape("import_drop_b", 462, 130, 356, 184, "import", "#00d7ff", "#0a0e15"),
        label("import_b_title", "2  MIDI GROOVE IMPORT", 492, 154, 286, 22, "import", 14, "#00d7ff"),
        label("import_b_body", "Drop MID or MIDI files. Then Apply MIDI to replace the visible drum pattern.", 492, 190, 286, 50, "import", 13, "#d8e3f0"),
        shape("import_b_callout", 492, 252, 280, 42, "import", "#00d7ff", "#101923", 10, 0.92),
        label("import_b_action", "DRAG MIDI FILES HERE", 524, 265, 216, 18, "import", 13, "#00d7ff", "centred"),
        shape("import_drop_c", 846, 130, 356, 184, "import", "#8d6cff", "#0a0e15"),
        label("import_c_title", "3  AUDITION", 878, 154, 260, 22, "import", 14, "#cbbdff"),
        label("import_c_body", "Press DAW Play, or use the Player PLAY button. Pads and keys trigger imported sounds.", 878, 190, 286, 50, "import", 13, "#d8e3f0"),
        shape("import_c_callout", 878, 252, 280, 42, "import", "#8d6cff", "#151326", 10, 0.92),
        label("import_c_action", "PLAY PADS OR KEYS", 918, 265, 210, 18, "import", 13, "#cbbdff", "centred"),
        shape("import_main", 78, 338, 1124, 122, "import", "#00d7ff", "#0a0e15"),
        label("import_title", "IF DRAG/DROP IS BLOCKED BY THE HOST", 106, 356, 460, 22, "import", 14, "#00d7ff"),
        label("import_steps", "Click IMPORT in the Player toolbar. Alternate path: TOOL > Samples + MIDI Imports.", 106, 392, 720, 24, "import", 13, "#f2f5ff"),
        label("import_files", "Included test files: import_examples/samples and import_examples/midi.", 106, 424, 650, 22, "import", 12, "#9aa4b4"),
        knob("import_motion", "Import FX", "sampleGlitch", 884, 356, "import", "#8d6cff"),
        knob("import_tone", "Tone", "filterCutoff", 1004, 356, "import", "#00d7ff"),
    ]

    elements += [
        shape("midi_panel", 48, 114, 1184, 352, "midi", "#00d7ff", "#0a0e15"),
        label("midi_title", "MIDI PATTERN PERFORMANCE", 74, 128, 380, 22, "midi", 14, "#00d7ff"),
        {"id": "midi_drum_grid", "type": "drumGrid", "x": 74, "y": 164, "width": 856, "height": 236, "label": "Imported / Editable Drum Pattern", "groupId": "midi", "drumTracks": 8, "drumSteps": 16, "drumPattern": 0, "backgroundColour": "#0b0f17", "borderColour": "#00d7ff", "accentColour": "#ffa62a", "cornerRadius": 12},
        label("midi_hint", "Click cells to edit. Ctrl-click supported divisions/ratchets in the runtime grid. Imported MIDI writes into this visible pattern.", 74, 412, 840, 24, "midi", 12, "#9aa4b4"),
        knob("midi_motion", "Motion", "macro_motion", 970, 170, "midi", "#00d7ff"),
        knob("midi_grid", "Grid", "sampleGlitchGrid", 1080, 170, "midi", "#00d7ff"),
        knob("midi_slices", "Slices", "sampleSliceCount", 970, 292, "midi", "#00d7ff"),
        knob("midi_space", "Space", "macro_space", 1080, 292, "midi", "#00d7ff"),
    ]

    elements += [
        shape("mix_panel", 82, 128, 1116, 330, "mix", "#ffa62a", "#0a0e15"),
        label("mix_title", "MIX / OUTPUT CONTROL", 110, 146, 340, 22, "mix", 14, "#ffa62a"),
        {"id": "mix_runtime", "type": "mixer", "x": 112, "y": 184, "width": 600, "height": 230, "groupId": "mix", "mixerChannels": 4, "mixerMode": "parameters", "mixerChannelLabels": ["Master", "Tone", "Delay", "Import"], "mixerVolumeParams": ["volume", "filterCutoff", "delayMix", "sampleGlitch"], "mixerPanParams": ["pan", "", "", ""], "backgroundColour": "#0b0f17", "borderColour": "#273449", "accentColour": "#ffa62a", "cornerRadius": 12},
        slider("mix_expr", "Expression", "expression", 780, 204, "mix"),
        slider("mix_width", "Width", "stereoWidth", 850, 204, "mix"),
        slider("mix_out", "Out dB", "outputGainDb", 920, 204, "mix"),
        knob("mix_filter", "Cutoff", "filterCutoff", 1010, 216, "mix"),
        knob("mix_res", "Reso", "filterResonance", 1110, 216, "mix"),
    ]

    elements += [
        shape("perform_panel", 64, 118, 1152, 344, "perform", "#8d6cff", "#0a0e15"),
        label("perform_title", "PERFORM / SOUND DESIGN", 92, 136, 360, 22, "perform", 14, "#cbbdff"),
        {"id": "perform_macro", "type": "macroControl", "x": 110, "y": 178, "width": 280, "height": 210, "label": "Motion Macro", "parameterId": "macro_motion", "groupId": "perform", "backgroundColour": "#0b0f17", "borderColour": "#8d6cff", "accentColour": "#8d6cff", "cornerRadius": 14},
        knob("perform_character", "Character", "macro_character", 440, 200, "perform", "#8d6cff"),
        knob("perform_tone", "Tone", "macro_tone", 550, 200, "perform", "#8d6cff"),
        knob("perform_space", "Space", "macro_space", 660, 200, "perform", "#8d6cff"),
        knob("perform_delay", "Delay", "delayMix", 770, 200, "perform", "#8d6cff"),
        knob("perform_feedback", "Feedback", "delayFeedback", 880, 200, "perform", "#8d6cff"),
        knob("perform_reverb", "Reverb", "reverbMix", 990, 200, "perform", "#8d6cff"),
        label("perform_hint", "Use hardware pads/keys, drag in your own one-shots, import MIDI grooves, then shape the result with macros and FX.", 440, 344, 650, 36, "perform", 13, "#d8e3f0"),
    ]
    return {"canvas": {"width": 1280, "height": 800}, "elements": elements}


def drum_values(patterns: list[dict[int, list[int]]]) -> dict:
    values = {
        "rate": 1.0, "sync": 1.0, "dmTracks": 8.0, "dmSteps": 16.0, "dmPattern": 0.0,
        "dmTransport": 1.0, "dmSongMode": 1.0, "dmChainLength": float(len(patterns)),
        "dmSwing": 0.08, "dmProbability": 1.0, "dmSeed": 5152026.0,
    }
    notes = [36, 38, 42, 39, 45, 48, 46, 49]
    for i in range(8):
        values[f"dmChain{i}"] = float(i % len(patterns))
        values[f"dmTrack{i}Note"] = float(notes[i])
    for pattern_index, pattern in enumerate(patterns):
        for track in range(8):
            for step in range(16):
                prefix = f"dmP{pattern_index}T{track}S{step}"
                active = step in pattern.get(track, [])
                values[prefix + "On"] = 1.0 if active else 0.0
                values[prefix + "Vel"] = 1.0 if track in (0, 1) and step % 4 == 0 else (0.74 if active else 0.72)
                values[prefix + "Gate"] = 0.34 if track != 2 else 0.14
                values[prefix + "Prob"] = 0.9 if track >= 4 and active else 1.0
                values[prefix + "Div"] = 2.0 if track in (2, 6) and step in (12, 14) else 1.0
    return values


def build_dsp_graph() -> dict:
    patterns = [
        {0: [0, 4, 8, 12], 1: [4, 12], 2: list(range(0, 16, 2)), 3: [12], 4: [3, 7, 11, 15], 5: [10], 6: [6, 14], 7: [0, 8]},
        {0: [0, 3, 10, 14], 1: [4, 12], 2: list(range(16)), 3: [4, 12], 4: [7, 15], 5: [2, 9], 6: [13, 14, 15], 7: [8]},
        {0: [0, 6, 8, 13], 1: [5, 12], 2: [2, 4, 6, 9, 11, 13, 15], 3: [5], 4: [1, 7, 14], 5: [3, 10], 6: [15], 7: [0, 11]},
        {0: [0, 4, 8, 12], 1: [4, 12], 2: [0, 3, 6, 9, 12, 15], 3: [4], 4: [5, 9, 13], 5: [1, 11], 6: [7, 15], 7: [8]},
    ]
    return {
        "blocks": [
            {"id": "source_kit", "section": "source", "type": "sampler", "name": "Import-ready 16 Pad Kit", "enabled": True, "targetId": "sampleStart", "values": {"sampleSliceCount": 16.0}},
            {"id": "midi_drum_machine", "section": "mod", "type": "drumMachine", "name": "Import Performance Pattern Engine", "enabled": True, "targetId": "sampleGlitch", "values": drum_values(patterns)},
            {"id": "filter_tone", "section": "filter", "type": "filter", "name": "Kit Tone", "enabled": True, "targetId": "filterCutoff", "values": {"filterCutoff": 15200.0, "filterResonance": 0.08}},
            {"id": "amp_snap", "section": "amp", "type": "amp", "name": "One-Shot Envelope", "enabled": True, "targetId": "attack", "values": {"attack": 0.002, "decay": 0.16, "sustain": 0.0, "release": 0.14}},
            {"id": "fx_delay", "section": "fx", "type": "delay", "name": "Tempo Throw Delay", "enabled": True, "targetId": "delayMix", "values": {"delayTime": 0.25, "delayFeedback": 0.18, "delayMix": 0.06}},
            {"id": "fx_room", "section": "fx", "type": "reverb", "name": "Short Room", "enabled": True, "targetId": "reverbMix", "values": {"reverbMix": 0.08}},
            {"id": "out_main", "section": "out", "type": "output", "name": "Safe Output", "enabled": True, "targetId": "outputGainDb", "values": {"outputGainDb": 0.0}},
        ],
        "edges": [],
        "macros": [
            {"id": "macro_motion_glitch", "macroId": "macro_motion", "targetId": "sampleGlitch", "sourceMin": 0.0, "sourceMax": 1.0, "targetMin": 0.0, "targetMax": 0.82, "curve": 1.0, "bipolar": False},
            {"id": "macro_motion_delay", "macroId": "macro_motion", "targetId": "delayMix", "sourceMin": 0.0, "sourceMax": 1.0, "targetMin": 0.02, "targetMax": 0.28, "curve": 1.0, "bipolar": False},
            {"id": "macro_tone_cutoff", "macroId": "macro_tone", "targetId": "filterCutoff", "sourceMin": 0.0, "sourceMax": 1.0, "targetMin": 2600.0, "targetMax": 18000.0, "curve": 1.0, "bipolar": False},
            {"id": "macro_space_reverb", "macroId": "macro_space", "targetId": "reverbMix", "sourceMin": 0.0, "sourceMax": 1.0, "targetMin": 0.02, "targetMax": 0.36, "curve": 1.0, "bipolar": False},
        ],
        "modulation": [],
        "automation": [],
        "typedNodes": [],
        "userConfigured": True,
        "quickEditControls": {
            "Import": ["sampleStart", "sampleLength", "samplePitch", "sampleGlitch"],
            "MIDI": ["bpmSync", "retrigger", "sampleGlitchGrid", "sampleSliceCount"],
            "Perform": ["macro_motion", "macro_tone", "macro_space", "delayMix"],
        },
    }


def build_presets() -> dict:
    base = {
        "sampleStart": 0.0, "sampleLength": 1.0, "sampleSlice": 0.0, "sampleSliceCount": 16.0,
        "samplePitch": 0.0, "sampleReverse": 0.0, "sampleGlitch": 0.08, "sampleGlitchGrid": 0.25,
        "attack": 0.002, "decay": 0.16, "sustain": 0.0, "release": 0.14,
        "filterCutoff": 15200.0, "filterResonance": 0.08,
        "delayTime": 0.25, "delayFeedback": 0.18, "delayMix": 0.06, "reverbMix": 0.08,
        "stereoWidth": 1.1, "outputGainDb": 0.0, "volume": 0.88, "pan": 0.0,
        "bpmSync": 1.0, "retrigger": 1.0, "expression": 1.0, "modWheel": 0.0,
        "macro_motion": 0.42, "macro_tone": 0.62, "macro_character": 0.5, "macro_space": 0.3,
    }
    specs = [
        ("Import Lab Init", "Balanced starter kit for testing runtime sample/MIDI import.", base, True),
        ("Warehouse Import Punch", "Dry, punchy, easy to audition imported one-shots against.", {**base, "filterCutoff": 13200.0, "sampleGlitch": 0.03, "reverbMix": 0.04, "volume": 0.92}, False),
        ("Trap Roll Import", "Brighter hats and grid motion for imported MIDI rolls.", {**base, "sampleGlitch": 0.22, "sampleGlitchGrid": 0.42, "delayMix": 0.1, "macro_motion": 0.72}, False),
        ("Broken Perc Import", "Looser motion and space for chopped imported percussion.", {**base, "filterCutoff": 9800.0, "filterResonance": 0.18, "delayMix": 0.16, "reverbMix": 0.18, "macro_space": 0.58}, False),
        ("Clean Pad Audition", "Clean pad response for checking user-loaded samples.", {**base, "sampleGlitch": 0.0, "delayMix": 0.0, "reverbMix": 0.03, "release": 0.1}, False),
    ]
    return {"presets": [{"name": name, "description": desc, "theme": "drum import", "isDefault": default, "tags": ["factory-demo", "drums", "import", "midi"], "values": values} for name, desc, values, default in specs]}


def main() -> None:
    if not SOURCE_DEMO.is_dir():
        raise SystemExit(f"Missing source demo: {SOURCE_DEMO}")
    if TARGET.exists():
        shutil.rmtree(TARGET)

    assets = TARGET / "assets"
    samples = TARGET / "samples"
    import_samples = TARGET / "import_examples" / "samples"
    import_midi = TARGET / "import_examples" / "midi"
    for folder in (assets, samples, import_samples, import_midi):
        folder.mkdir(parents=True, exist_ok=True)

    for wav in (SOURCE_DEMO / "samples").glob("*.wav"):
        shutil.copy2(wav, samples / wav.name)

    generate_backgrounds(assets)
    generate_import_samples(import_samples)
    generate_midi_examples(import_midi)

    manifest = {
        "formatVersion": 1,
        "instrumentName": "Import Performance Drum Lab",
        "creator": "PatchCraft",
        "description": "A factory demo that shows the complete runtime import flow: drag samples or MIDI into the Player, map them as pads/grooves, then perform and mix the result.",
        "category": "Drum Machine",
        "engine": "sample",
        "backgroundImage": "assets/background.png",
        "libraryThumbnail": "assets/thumbnail.png",
        "defaultPreset": "Import Lab Init",
        "createdWith": "PatchCraft Studio",
        "version": "1.0",
        "playerDisplayName": "Import Performance Drum Lab",
        "playerTagline": "Drag WAV or MIDI into the Player",
        "playerBackgroundColour": "ff05070c",
        "playerPanelColour": "ff0a0f17",
        "playerAccentColour": "ffffa62a",
        "playerTextColour": "fff2f5ff",
        "playerTextDimColour": "ff9aa4b4",
        "playerBorderColour": "ff273449",
        "playerAllowMidiLearn": True,
        "playerShowParameterGuidance": True,
        "tags": ["factory-demo", "drums", "sample import", "midi import", "performance", "player-ready"],
    }
    write_json(TARGET / "manifest.json", manifest)
    write_json(TARGET / "layout.json", build_layout())
    write_json(TARGET / "dspGraph.json", build_dsp_graph())
    write_json(TARGET / "parameters.json", json.loads((SOURCE_DEMO / "parameters.json").read_text(encoding="utf-8")))
    write_json(TARGET / "mappings.json", json.loads((SOURCE_DEMO / "mappings.json").read_text(encoding="utf-8")))
    write_json(TARGET / "presets.json", build_presets())
    write_json(TARGET / "midiMappings.json", {"mappings": []})
    write_json(TARGET / "patches.json", {"patches": []})
    write_json(TARGET / "sectionPresets.json", {"sectionPresets": []})
    write_json(TARGET / "expansions.json", {"expansions": []})
    (TARGET / "import_examples" / "README.txt").write_text(
        "Use the Player top-bar IMPORT button, or drag WAV/AIFF/FLAC/MID files directly onto the Player.\n"
        "Samples map as runtime pads. MIDI files can be applied to the visible drum pattern grid.\n",
        encoding="utf-8",
    )
    print(f"Created {TARGET}")


if __name__ == "__main__":
    main()
