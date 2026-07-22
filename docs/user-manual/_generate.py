# -*- coding: utf-8 -*-
"""Generate docs/user-manual/index.html for the CURRENT PatchCraft UI.

Canonical product spine (toolbar):
  Build | Design | Brand | Test | Ship

Build sub-steps (inside Build):
  1  Import Sounds | 2  Chop Loop (when relevant) | 3  Sound Stack | 4  Perform (Advanced)

Do NOT reference obsolete primary tabs such as Workflow, Samples, One Shot, MIDI,
DSP, Brand Lab, or Launch as top-level navigation.
"""
from pathlib import Path

OUT = Path(__file__).resolve().parent / "index.html"
CURRENT = Path(__file__).resolve().parent / "assets" / "img" / "current"


def has_img(name: str) -> bool:
    return (CURRENT / f"{name}.jpg").exists() or (CURRENT / f"{name}.png").exists()


def img_src(name: str) -> str:
    jpg = CURRENT / f"{name}.jpg"
    if jpg.exists():
        return f"assets/img/current/{name}.jpg"
    return f"assets/img/current/{name}.png"


def section(sid, title, body):
    return f'<section id="{sid}">\n<h2>{title}</h2>\n{body}\n</section>\n'


def fig_current(name, alt, caption):
    if not has_img(name):
        return (
            f'<div class="callout tip"><strong>UI note:</strong> '
            f'{caption} Open Studio and switch to this page to follow along.</div>\n'
        )
    return (
        f'<figure class="figure-wide">\n'
        f'<img src="{img_src(name)}" alt="{alt}"/>\n'
        f'<figcaption>{caption}</figcaption>\n'
        f'</figure>\n'
    )


def recipe(title, study, steps):
    lis = "\n".join(f"<li>{s}</li>" for s in steps)
    return (
        f'<article class="recipe">\n'
        f"<h3>{title}</h3>\n"
        f'<div class="walkthrough-meta">'
        f'<span class="pill lead">Walkthrough</span>'
        f'<span class="pill pad">Study pack: {study}</span>'
        f"</div>\n"
        f"<ol>\n{lis}\n</ol>\n"
        f"</article>\n"
    )


parts = []
parts.append("""<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8"/>
    <meta name="viewport" content="width=device-width, initial-scale=1"/>
    <title>PatchCraft User Manual — Current Studio UI</title>
    <meta name="description" content="PatchCraft user manual for the current Build → Design → Brand → Test → Ship Studio UI, with step-by-step instrument walkthroughs."/>
    <link rel="stylesheet" href="assets/css/main.css"/>
    <link rel="icon" type="image/svg+xml" href="assets/img/logo.svg"/>
</head>
<body>
<button class="nav-toggle" type="button" aria-label="Open navigation">Menu</button>
<div class="nav-backdrop" aria-hidden="true"></div>
<header class="topbar">
    <div class="topbar-logo"><img src="assets/img/logo.svg" alt="" class="hex"/>PATCHCRAFT</div>
    <span class="topbar-tagline">User Manual</span>
    <div class="topbar-spacer"></div>
    <div class="topbar-actions">
        <a href="#overview">Overview</a>
        <a href="#quick-start">Quick Start</a>
        <a class="cta" href="#walkthroughs">Build Instruments</a>
    </div>
</header>

<div class="shell masterclass-shell">
    <nav class="sidebar">
        <input id="manual-search" class="sidebar-search" type="search" placeholder="Search sections…" aria-label="Search manual sections"/>
        <div class="sidebar-section">
            <h5>Start here</h5>
            <ul>
                <li><a href="#overview">Overview</a></li>
                <li><a href="#mental-model">Mental Model</a></li>
                <li><a href="#apps">Studio vs Player</a></li>
                <li><a href="#spine">The Five Tabs</a></li>
                <li><a href="#chrome">Studio Chrome</a></li>
                <li><a href="#quick-start">Quick Start</a></li>
            </ul>
        </div>
        <div class="sidebar-section">
            <h5>Build</h5>
            <ul>
                <li><a href="#build">Build overview</a></li>
                <li><a href="#import-sounds">1 Import Sounds</a></li>
                <li><a href="#chop">2 Chop Loop</a></li>
                <li><a href="#sound-stack">3 Sound Stack</a></li>
                <li><a href="#perform">4 Perform</a></li>
                <li><a href="#advanced-build">Advanced Build</a></li>
            </ul>
        </div>
        <div class="sidebar-section">
            <h5>Design · Brand · Test · Ship</h5>
            <ul>
                <li><a href="#design">Design</a></li>
                <li><a href="#elements">Elements panel</a></li>
                <li><a href="#bindings">Bindings &amp; nodes</a></li>
                <li><a href="#brand">Brand</a></li>
                <li><a href="#test">Test</a></li>
                <li><a href="#ship">Ship</a></li>
            </ul>
        </div>
        <div class="sidebar-section">
            <h5>Walkthroughs</h5>
            <ul>
                <li><a href="#walkthroughs">All recipes</a></li>
                <li><a href="#wt-synth">Synth</a></li>
                <li><a href="#wt-sampler">Sampler</a></li>
                <li><a href="#wt-drums">Drum machine</a></li>
                <li><a href="#wt-fx">FX plugin</a></li>
                <li><a href="#wt-granular">Granular</a></li>
                <li><a href="#wt-midi">MIDI / arp / chords</a></li>
                <li><a href="#wt-hybrid">Flagship / hybrid</a></li>
            </ul>
        </div>
        <div class="sidebar-section">
            <h5>Reference</h5>
            <ul>
                <li><a href="#player">Player runtime</a></li>
                <li><a href="#factory">Factory demos</a></li>
                <li><a href="#formats">Files &amp; formats</a></li>
                <li><a href="#shortcuts">Shortcuts</a></li>
                <li><a href="#troubleshooting">Troubleshooting</a></li>
                <li><a href="#checklist">Ship checklist</a></li>
            </ul>
        </div>
    </nav>

    <main class="content masterclass">
""")

parts.append("""
<section id="overview" class="hero masterclass-hero">
    <div class="pill-row">
        <span class="pill lead">Build</span>
        <span class="pill bass">Design</span>
        <span class="pill pad">Brand</span>
        <span class="pill arp">Test</span>
        <span class="pill fx">Ship</span>
    </div>
    <h1>PatchCraft User Manual</h1>
    <p class="lede">Written for the <strong>current</strong> PatchCraft Studio toolbar:
    <strong>Build → Design → Brand → Test → Ship</strong>.
    This guide explains every page and panel you see today, then walks through building each major instrument type end to end.</p>
    <div class="actions">
        <a class="btn primary" href="#quick-start">Start building</a>
        <a class="btn secondary" href="#walkthroughs">Instrument recipes</a>
    </div>
</section>
""")

parts.append("""
<div class="callout important">
    <strong>UI contract:</strong> The five primary tabs are always <strong>Build</strong>, <strong>Design</strong>, <strong>Brand</strong>, <strong>Test</strong>, and <strong>Ship</strong>.
    Older docs that talk about Workflow / Samples / DSP / Brand Lab / Launch as top tabs are obsolete.
</div>
""")

parts.append(section("mental-model", "Mental Model", """
<p>Studio authors a pack. Player loads that pack. The Design canvas is the customer-facing control surface for the Sound Stack — not the sound itself.</p>
<div class="feature-grid compact">
    <div class="feature"><h3>Project</h3><p>What you edit in Studio: engine, canvas, layout, samples, graph, presets, branding.</p></div>
    <div class="feature"><h3>Sound Stack</h3><p>The live signal graph (sources, shape, motion, FX, output) opened from Build step 3.</p></div>
    <div class="feature"><h3>Layout</h3><p>Knobs, pads, modules, and starters on the Design canvas, bound to real parameters.</p></div>
    <div class="feature"><h3>Preset / Patch</h3><p>A named playable state buyers can recall in the Player.</p></div>
    <div class="feature"><h3>.patchcraft pack</h3><p>The exported product folder Player opens.</p></div>
    <div class="feature"><h3>Player</h3><p>VST3 / standalone runtime musicians use in a DAW.</p></div>
</div>
"""))

parts.append(section("apps", "Studio vs Player", """
<ul>
    <li><strong>PatchCraft Studio</strong> — authoring app. Tabs: Build, Design, Brand, Test, Ship.</li>
    <li><strong>PatchCraft Player</strong> — runtime that loads exported packs.</li>
    <li><strong>Player FX</strong> — FX-oriented Player for insert processing.</li>
</ul>
<p>You do not ship Studio. You ship a pack (and optionally a wrapped VST3) that Player loads.</p>
"""))

parts.append(section("spine", "The Five Tabs", """
<div class="spine">
    <span class="step-chip"><strong>1</strong> Build</span>
    <span class="step-chip"><strong>2</strong> Design</span>
    <span class="step-chip"><strong>3</strong> Brand</span>
    <span class="step-chip"><strong>4</strong> Test</span>
    <span class="step-chip"><strong>5</strong> Ship</span>
</div>
""" + fig_current("studio-build", "Build tab", "Primary toolbar tabs sit above the workspace: Build, Design, Brand, Test, Ship.") + """
<div class="table-wrap">
<table class="reference-table">
<thead><tr><th>Tab</th><th>What you do</th><th>Done when</th></tr></thead>
<tbody>
<tr><td>Build</td><td>Import sounds, optional chop, shape Sound Stack, optional Perform.</td><td>The instrument makes sound with the right character.</td></tr>
<tr><td>Design</td><td>Place Controls / Modules / Starters; bind them; organize layers.</td><td>Every visible control drives a real parameter or useful UI state.</td></tr>
<tr><td>Brand</td><td>Name, colors, artwork, Player chrome, library options.</td><td>Preview looks like a finished product.</td></tr>
<tr><td>Test</td><td>Play the export runtime with keyboard / MIDI.</td><td>Sound, tabs, presets, and mappings behave like the buyer experience.</td></tr>
<tr><td>Ship</td><td>Health checks and export.</td><td>Checks are clear; pack opens in Player.</td></tr>
</tbody>
</table>
</div>
"""))

parts.append(section("chrome", "Studio Chrome (Always Visible)", """
<p>Besides the five tabs, use these constantly:</p>
<ul>
    <li><strong>New</strong> — start a starter project (Sample / Synth / Drum Machine / Effect).</li>
    <li><strong>Open</strong> — load a Studio project or a factory <code>.patchcraft</code> pack.</li>
    <li><strong>Save</strong> — save the project.</li>
    <li><strong>Import Samples</strong> — add WAV / AIFF / FLAC.</li>
    <li><strong>Import BG</strong> — add Design canvas artwork.</li>
    <li><strong>Preview</strong> — hear Studio audio while editing.</li>
    <li><strong>Export Pack</strong> — jump toward shipping a pack.</li>
    <li><strong>Left rail on Design</strong> — Elements, Layers, Library, pScript.</li>
    <li><strong>Right rail</strong> — Inspector for the selection.</li>
</ul>
"""))

parts.append(section("quick-start", "Quick Start", """
<ol class="steps">
    <li><h3>New</h3><p>Click <strong>New</strong> and pick Synth, Sample, Drum Machine, or Effect.</p></li>
    <li><h3>Build → 1 Import Sounds</h3><p>If it is a sample product, import audio and map it. Synths can skip ahead.</p></li>
    <li><h3>Build → 3 Sound Stack</h3><p>Shape the graph until it sounds right. Use Listen while playing notes.</p></li>
    <li><h3>Design</h3><p>Add Controls / Modules / Starters. Bind every control.</p></li>
    <li><h3>Brand</h3><p>Set product name, accent, banner, and Player chrome.</p></li>
    <li><h3>Test</h3><p>Play MIDI. Twist controls. Fix anything dead.</p></li>
    <li><h3>Ship</h3><p>Clear health checks → Export Pack → open in Player.</p></li>
</ol>
"""))

# BUILD
parts.append(section("build", "Build", """
""" + fig_current("studio-build", "Build page", "Build is selected in the primary toolbar. Inside Build, use the numbered sub-steps.") + """
<p>Build is the authoring hub. Its own sub-step radios are:</p>
<div class="spine">
    <span class="step-chip"><strong>1</strong> Import Sounds</span>
    <span class="step-chip"><strong>2</strong> Chop Loop</span>
    <span class="step-chip"><strong>3</strong> Sound Stack</span>
    <span class="step-chip"><strong>4</strong> Perform</span>
</div>
<ul>
    <li><strong>2 Chop Loop</strong> appears for loop/chop product recipes.</li>
    <li><strong>4 Perform</strong> appears after you unlock Advanced Build.</li>
</ul>
<p>On the Build header you may also see <strong>Import Samples</strong>, <strong>Advanced Map</strong>, <strong>Preview Player</strong>, and <strong>Ship</strong> shortcuts.</p>
"""))

parts.append(section("import-sounds", "Build · 1 Import Sounds", """
""" + fig_current("studio-import-sounds", "Import Sounds step", "Sub-step 1 Import Sounds is active under the Build tab.") + """
<ol class="steps">
    <li><h3>Import</h3><p>Use <strong>Import Samples</strong> or drag WAV / AIFF / FLAC into the mapper.</p></li>
    <li><h3>Map</h3><p>Easy mode guides mapping. Unlock Advanced Build for keyzones, velocity layers, and round-robin.</p></li>
    <li><h3>Trim</h3><p>Remove silence, set fades, loops, and one-shot vs sustain behavior.</p></li>
    <li><h3>Prove</h3><p>Play the range. Silent notes usually mean missing zones or wrong root keys.</p></li>
</ol>
<div class="callout tip"><strong>Filename tip:</strong> <code>piano_C3_v127_rr1.wav</code> automaps better than <code>final2.wav</code>.</div>
"""))

parts.append(section("chop", "Build · 2 Chop Loop", """
<p>Visible for loop/chop products. Use it to slice loops into pads or keys: grids, transients, and tempo-aware chops.</p>
<ol class="steps">
    <li><h3>Load a loop</h3><p>Import the source audio first (step 1).</p></li>
    <li><h3>Slice</h3><p>Set grid / transient markers and audition slices.</p></li>
    <li><h3>Map</h3><p>Send slices to pads or key ranges.</p></li>
    <li><h3>Continue</h3><p>Move to <strong>3 Sound Stack</strong> for tone and FX, then Design the pad UI.</p></li>
</ol>
"""))

parts.append(section("sound-stack", "Build · 3 Sound Stack", """
""" + fig_current("studio-sound-stack", "Sound Stack", "Sound Stack is Build sub-step 3 — the node graph for the instrument.") + """
<p>Sound Stack is the signal path. Columns: <strong>Sources · Shape · Motion · FX · Output</strong>.</p>
<h3>Connect nodes (current editor)</h3>
<ol class="steps">
    <li><h3>Drag from OUT</h3><p>Grab the right-side port on a node.</p></li>
    <li><h3>Watch glow</h3><p>Compatible IN ports pulse; incompatible nodes dim.</p></li>
    <li><h3>Drop or click-to-connect</h3><p>Drop on a glowing IN, or click OUT then click a highlighted node. <span class="kbd">Esc</span> cancels.</p></li>
</ol>
<ul>
    <li><strong>Tidy / Fit</strong> — layout and zoom helpers</li>
    <li><strong>Listen</strong> — hear the graph through the Player engine</li>
    <li><strong>Stack Templates</strong> — Init Synth, Warm Pad, Drum Bus, Arp Step Sequencer, etc.</li>
    <li><strong>+ Motion</strong> — add Arpeggiator, Drum Sequencer, or Circle Sequencer</li>
    <li><strong>Advanced</strong> — full graph features when needed</li>
</ul>
<p>Audio generally flows Source → Shape → FX → Output. Modulation / event sources feed Shape, FX, or Sources — not Output.</p>
"""))

parts.append(section("perform", "Build · 4 Perform", """
<p>Shown after Advanced Build is unlocked. This is where step grids, drum patterns, piano-roll phrases, and circle/orbit sequencers live.</p>
<ul>
    <li>Step / piano-roll patterns</li>
    <li>Drum grids with velocity / probability</li>
    <li>Circle / orbit banks (pitch, filter, pan, FX, slice)</li>
    <li>Chord / harmony helpers on MIDI-oriented products</li>
</ul>
<p>Always re-check patterns on the <strong>Test</strong> tab before shipping. Expose only buyer-facing performance controls on Design.</p>
"""))

parts.append(section("advanced-build", "Advanced Build", """
<p>Easy Build keeps the path short. Unlock Advanced when you need:</p>
<ul>
    <li>Keyzones / velocity layers (<strong>Advanced Map</strong>)</li>
    <li>Perform step (step 4)</li>
    <li>Deeper graph / multi-layer rack tools</li>
</ul>
<p>You can unlock Advanced from Build actions (for example Advanced Map). Once unlocked, Perform and related tools stay available for that session/project path.</p>
"""))

# DESIGN
parts.append(section("design", "Design", """
""" + fig_current("studio-design", "Design tab", "Design is a primary toolbar tab. The canvas is the Player body; Elements / Layers / Library sit on the left; Inspector on the right.") + """
<p>Use Design to build what buyers touch:</p>
<ul>
    <li>Canvas size / snap / zoom / fit</li>
    <li>Layers for ordering and groups</li>
    <li>Library for backgrounds, templates, reusable assets</li>
    <li>Inspector for geometry, appearance, behavior, and parameter assignment</li>
    <li>Control Bindings → <strong>Open Node Editor</strong> for deep wiring</li>
</ul>
"""))

parts.append(section("elements", "Elements: Controls · Modules · Starters", """
""" + fig_current("studio-elements-controls", "Elements Controls tab", "On Design, the Elements panel tabs are Controls, Modules, and Starters. The selected tab uses an underline highlight so the label stays readable.") + """
<div class="feature-grid compact">
    <div class="feature"><h3>Controls</h3><p>Knobs, sliders, toggles, XY pads, meters, keyboard, labels, panels, ADSR, granular field, etc. Drag to canvas, then bind.</p></div>
    <div class="feature"><h3>Modules</h3><p>Ready panels that also seed DSP (OSC Stack, Filter, ADSR, Sample Player, Delay, Drum Rack, Macro Bank, Master Bus…).</p></div>
    <div class="feature"><h3>Starters</h3><p>Full product scaffolds (Synth Plugin, Sampler, Drum Machine, Vocal Chop, Delay FX, Vocal/Master FX…).</p></div>
</div>
""" + fig_current("studio-elements-modules", "Elements Modules", "Modules tab lists DSP-backed layout modules.") + """
""" + fig_current("studio-elements-starters", "Elements Starters", "Starters tab lists product scaffolds you can drop onto the canvas.") + """
"""))

parts.append(section("bindings", "Bindings &amp; Node Editor", """
<ol class="steps">
    <li><h3>Select a control</h3><p>Click it on the Design canvas.</p></li>
    <li><h3>Assign</h3><p>Use Inspector parameter assignment, or click <strong>Open Node Editor</strong>.</p></li>
    <li><h3>Link</h3><p>In the node editor, use <strong>LINK UI</strong> on a parameter row.</p></li>
    <li><h3>Prove</h3><p>Switch to <strong>Test</strong>, hold a note, move the control.</p></li>
</ol>
"""))

parts.append(section("brand", "Brand", """
""" + fig_current("studio-brand", "Brand tab", "Brand is a primary toolbar tab — Player name, colors, artwork, and chrome options.") + """
<ul>
    <li>Product name, creator, tagline</li>
    <li>Accent / panel / background colors</li>
    <li>Library artwork, title banner, thumbnail</li>
    <li>Player frame toggles (top bar, browser, right panel, keyboard, footer)</li>
    <li>Whether buyers can load packs or import samples at runtime</li>
</ul>
<p>Use Brand’s live preview as the customer-facing proof before Ship.</p>
"""))

parts.append(section("test", "Test", """
""" + fig_current("studio-test", "Test tab", "Test plays the export runtime path — keyboard, mappings, tabs, presets.") + """
<p>Verify while audio is running:</p>
<ul>
    <li>Software keyboard and hardware MIDI both produce sound</li>
    <li>Bound controls change sound immediately</li>
    <li>Tabbed UI pages switch correctly</li>
    <li>Presets load into complete playable states</li>
    <li>Patterns / arps / drum grids play when expected</li>
</ul>
"""))

parts.append(section("ship", "Ship", """
""" + fig_current("studio-ship", "Ship tab", "Ship (Launch Center) runs health checks and export.") + """
<p>Clear missing assets, unbound controls, graph issues, and branding gaps, then export.</p>
<div class="table-wrap">
<table class="reference-table">
<thead><tr><th>Export</th><th>Creates</th><th>Use when</th></tr></thead>
<tbody>
<tr><td>PatchCraft Pack</td><td><code>.patchcraft</code> folder/archive</td><td>Standard Player distribution</td></tr>
<tr><td>Standalone VST3</td><td>Self-contained instrument</td><td>Branded plugin product</td></tr>
<tr><td>Player FX VST3</td><td>FX insert plugin</td><td>Delay / motion / master FX</td></tr>
<tr><td>Launch Bundle</td><td>Installer-oriented payload</td><td>Retail packaging</td></tr>
</tbody>
</table>
</div>
"""))

# WALKTHROUGHS — all use current tab names only
parts.append('<section id="walkthroughs">\n<h2>Step-by-Step Instrument Walkthroughs</h2>\n')
parts.append('<p>Every recipe uses the current tabs only: <strong>Build → Design → Brand → Test → Ship</strong>.</p>\n')

parts.append('<div id="wt-synth"></div>\n')
parts.append(recipe(
    "A — Synth instrument",
    "NebulaPrimeSynth / AuroraFlagship",
    [
        "Click <strong>New</strong> → Synth (or Open a factory synth pack to study, then New for your own).",
        "Stay on <strong>Build</strong> → <strong>3 Sound Stack</strong>. Load a Stack Template (Init Synth / Warm Pad) or add Oscillator + Filter + Envelope + Output.",
        "Cable Source → Shape → FX → Output. Compatible ports glow while dragging.",
        "Optional: <strong>+ Motion</strong> for LFO or Arp; connect mod/event cables to glowing targets.",
        "Click <strong>Listen</strong> and play notes. Shape amp envelope and filter until it feels right.",
        "Switch to <strong>Design</strong>. Elements → <strong>STARTERS</strong> → Synth Plugin Starter (or build from Modules/Controls).",
        "Bind Cutoff, Resonance, Amp Env, Osc Blend, Delay/Reverb Mix, and one Macro.",
        "Open <strong>Brand</strong>: name, accent, banner, library art.",
        "Open <strong>Test</strong>: hold chords, twist macros, save presets.",
        "Open <strong>Ship</strong>: clear checks → Export Pack → load in Player.",
    ],
))

parts.append('<div id="wt-sampler"></div>\n')
parts.append(recipe(
    "B — Sample instrument",
    "DreamKeysSampler",
    [
        "<strong>New</strong> → Sample.",
        "<strong>Build → 1 Import Sounds</strong>: drop tuned WAVs; Easy map, then Advanced Map if needed.",
        "Trim silence / set loops where appropriate.",
        "<strong>Build → 3 Sound Stack</strong>: Sample source → Filter → Amp → Space → Output.",
        "<strong>Design</strong>: Starters → Sampler Instrument or Easy Sampler Workstation; bind Start/Filter/Env/Space.",
        "<strong>Brand</strong> → <strong>Test</strong> full key range → <strong>Ship</strong>.",
    ],
))

parts.append('<div id="wt-drums"></div>\n')
parts.append(recipe(
    "C — Drum machine",
    "RomplurDrumMachine / AnalogHouseDrums / BeatFoundry",
    [
        "<strong>New</strong> → Drum Machine.",
        "<strong>Build → 1 Import Sounds</strong>: kick/snare/hats/perc one-shots; map to pads; set choke groups.",
        "Unlock Advanced Build if you need <strong>4 Perform</strong> for drum grids.",
        "<strong>Build → 3 Sound Stack</strong>: kit tone, bus compression, sends, output limiter.",
        "<strong>Design</strong>: Starters → Drum Machine / MPC Pads; expose pads + pattern controls.",
        "<strong>Brand</strong> → <strong>Test</strong> pads + patterns → <strong>Ship</strong>.",
    ],
))

parts.append('<div id="wt-fx"></div>\n')
parts.append(recipe(
    "D — FX plugin",
    "EchoCraft / ModularMotionFX",
    [
        "<strong>New</strong> → Effect.",
        "<strong>Build → 3 Sound Stack</strong>: design for incoming audio → FX chain → Output (no obligatory oscillator).",
        "Keep a clear Mix / Bypass story; add Motion for performance throws if needed.",
        "<strong>Design</strong>: Starters → Delay FX / Loop Remix FX / Vocal·Master FX.",
        "<strong>Brand</strong> for insert clarity → <strong>Test</strong> with preview audio → <strong>Ship</strong> (Player FX VST3 when required).",
    ],
))

parts.append('<div id="wt-granular"></div>\n')
parts.append(recipe(
    "E — Granular sampler",
    "GranularVocalClouds",
    [
        "Open the study pack, or <strong>New</strong> → Sample and add a Granular module from Elements → Modules.",
        "<strong>Build → 1 Import Sounds</strong> for the texture/vocal source.",
        "<strong>Build → 3 Sound Stack</strong>: Granular → Filter → Amp → Space → Output.",
        "<strong>Design</strong>: expose Density, Size, Spread, Position/Scan, Filter, Mix.",
        "<strong>Brand</strong> → <strong>Test</strong> polyphony/CPU → <strong>Ship</strong>.",
    ],
))

parts.append('<div id="wt-midi"></div>\n')
parts.append(recipe(
    "F — MIDI / chord / arp instrument",
    "HarmonyComposer / ArpStepSequencer / CircleSeqFlagship",
    [
        "<strong>New</strong> → Synth (or open a study pack).",
        "<strong>Build → 3 Sound Stack</strong>: get a basic source sounding.",
        "Unlock Advanced Build → <strong>4 Perform</strong> for steps / circles / chord tools as needed.",
        "Cable Motion blocks (Arp / Circle / Sequencer) to glowing compatible targets.",
        "<strong>Design</strong>: expose Rate, Gate, Swing, pattern select, Hold, and a few tone macros.",
        "<strong>Brand</strong> → <strong>Test</strong> single notes and chords → <strong>Ship</strong>.",
    ],
))

parts.append('<div id="wt-hybrid"></div>\n')
parts.append(recipe(
    "G — Flagship / hybrid",
    "NebulaPrimeSynth / AuroraFlagship / BeatFoundry",
    [
        "Write one sentence for the hero story before decorating UI.",
        "<strong>Build → 3 Sound Stack</strong>: layered sources → shared filter → amp → motion → FX → output.",
        "Use Advanced Build / Perform only where the story needs sequenced motion.",
        "<strong>Design</strong>: tabbed pages (Main / Motion / FX…); Modules for dense sections; Controls for hero macros.",
        "Capture many uniquely audible presets; default preset must impress in seconds.",
        "<strong>Brand</strong> like retail → <strong>Test</strong> every tab/macro → <strong>Ship</strong> only when checks are green.",
    ],
))

parts.append("</section>\n")

parts.append(section("player", "Player Runtime", """
<p>Player loads the exported pack. Confirm in Player (not only Studio Design):</p>
<ul>
    <li>Library browsing and preset prev/next</li>
    <li>Mapped controls + MIDI Learn</li>
    <li>Optional runtime sample/MIDI import (if enabled in Brand)</li>
    <li>Keyboard / pads / pattern Play behavior</li>
</ul>
"""))

parts.append(section("factory", "Factory Demos", """
<p>Open these from Studio with <strong>Open</strong> (they are <code>.patchcraft</code> packs). Study structure, then build your own with New.</p>
<div class="table-wrap">
<table class="reference-table">
<thead><tr><th>Pack</th><th>Engine</th><th>Use it to learn</th></tr></thead>
<tbody>
<tr><td>Nebula Prime</td><td>synth</td><td>Flagship tabbed synth + arp + FX</td></tr>
<tr><td>Aurora Flagship / Aurora Arp</td><td>synth</td><td>Polished synth products</td></tr>
<tr><td>Dream Keys Sampler</td><td>sample</td><td>Multi-sample mapping</td></tr>
<tr><td>Granular Vocal Clouds</td><td>sample</td><td>Granular source + UI</td></tr>
<tr><td>Romplur / Analog House Drums</td><td>sample</td><td>Drum-machine layouts</td></tr>
<tr><td>Beat Foundry</td><td>sample</td><td>Beat workstation</td></tr>
<tr><td>EchoCraft / Modular Motion FX</td><td>fx</td><td>Insert FX products</td></tr>
<tr><td>Harmony Composer / ChordCraft</td><td>synth</td><td>Chord / MIDI performance</td></tr>
<tr><td>Arp Step Sequencer / CircleSEQ</td><td>synth</td><td>Step vs circle performance</td></tr>
</tbody>
</table>
</div>
"""))

parts.append(section("formats", "Files &amp; Formats", """
<ul>
    <li><code>.patchcraft</code> — exported pack Player loads</li>
    <li><code>.patchcraftproject</code> — Studio project file</li>
    <li>Audio — WAV / AIFF / FLAC</li>
    <li>Artwork — PNG backgrounds, banners, thumbnails</li>
    <li>VST3 — optional wrapped instrument or FX product</li>
</ul>
"""))

parts.append(section("shortcuts", "Shortcuts", """
<div class="table-wrap">
<table class="reference-table">
<thead><tr><th>Action</th><th>Gesture</th></tr></thead>
<tbody>
<tr><td>Open</td><td><span class="kbd">Ctrl</span>+<span class="kbd">O</span></td></tr>
<tr><td>Save</td><td><span class="kbd">Ctrl</span>+<span class="kbd">S</span></td></tr>
<tr><td>Cancel cable</td><td><span class="kbd">Esc</span></td></tr>
<tr><td>Delete node/cable</td><td><span class="kbd">Delete</span></td></tr>
<tr><td>Pan Sound Stack</td><td>Middle mouse / Alt-drag / Space-drag</td></tr>
<tr><td>Zoom Sound Stack</td><td><span class="kbd">Ctrl</span> + wheel</td></tr>
</tbody>
</table>
</div>
"""))

parts.append(section("troubleshooting", "Troubleshooting", """
<div class="table-wrap">
<table class="reference-table">
<thead><tr><th>Problem</th><th>Fix</th></tr></thead>
<tbody>
<tr><td>Knob does nothing</td><td>Bind in Inspector / Node Editor; confirm Sound Stack route; prove on <strong>Test</strong>.</td></tr>
<tr><td>Cannot connect nodes</td><td>Only glowing INs are valid. Audio: Source→Shape→FX→Out.</td></tr>
<tr><td>MIDI lights keys, no sound</td><td>Use <strong>Test</strong>/Preview; confirm a source or mapped samples exist.</td></tr>
<tr><td>Wrong sample notes</td><td>Rename files; remapping; set root keys in Advanced Map.</td></tr>
<tr><td>Perform step missing</td><td>Unlock Advanced Build first.</td></tr>
<tr><td>Chop step missing</td><td>Product recipe is not a loop/chop type — use Import Sounds instead.</td></tr>
<tr><td>Export blocked</td><td>Resolve <strong>Ship</strong> health errors before exporting.</td></tr>
</tbody>
</table>
</div>
"""))

parts.append(section("checklist", "Ship Checklist", """
<ul class="checklist">
    <li>Primary tabs used: Build → Design → Brand → Test → Ship</li>
    <li>Default preset impresses in seconds</li>
    <li>Every visible Design control does something real</li>
    <li>Sound Stack has a safe Output path</li>
    <li>No missing samples or artwork</li>
    <li>Brand chrome is final</li>
    <li>Test matches exported Player behavior</li>
    <li>Ship checks are clear</li>
</ul>
"""))

parts.append("""
        <div class="section-nav">
            <a class="prev" href="#overview"><strong>Top</strong><span>Overview</span></a>
            <a class="next" href="#walkthroughs"><strong>Next</strong><span>Walkthroughs</span></a>
        </div>
    </main>
</div>
<footer class="footer">
    PatchCraft User Manual for the current Studio UI — Build, Design, Brand, Test, Ship.
</footer>
<script src="assets/js/app.js"></script>
</body>
</html>
""")

OUT.write_text("".join(parts), encoding="utf-8")
print("Wrote", OUT, OUT.stat().st_size, "bytes")
missing = [n for n in [
    "studio-build", "studio-import-sounds", "studio-sound-stack", "studio-design",
    "studio-elements-controls", "studio-elements-modules", "studio-elements-starters",
    "studio-brand", "studio-test", "studio-ship",
] if not has_img(n)]
if missing:
    print("Missing current screenshots (placeholders used):", ", ".join(missing))
else:
    print("All current screenshots present")
