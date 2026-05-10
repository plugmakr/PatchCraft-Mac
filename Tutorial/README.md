# PatchCraft Tutorial Bundle

Self-contained, zero-dependency HTML/CSS/JS tutorial site for PatchCraft.
Drop the folder onto any web host (or open `index.html` locally) — it
works without a server, build step, or external CDN.

## Layout

```
Tutorial/
├── index.html                    Landing page
├── 01-overview.html              What PatchCraft is
├── 02-getting-started.html       Install & first project
├── 03-design.html                Design canvas
├── 04-sample-mapper.html         Sample mapping & editing
├── 05-midi-playground.html       MIDI tools, phrases, piano roll
├── 06-dsp-builder.html           Advanced patch building
├── 07-brand-lab.html             White-labeling the Player
├── 08-presets-expansions.html    Preset banks & expansion packs
├── 09-export-distribute.html     Export, ship, distribute
├── 10-glossary.html              Term reference
├── assets/
│   ├── css/main.css              Single stylesheet
│   ├── js/nav.js                 Sidebar highlight + smooth scroll
│   └── img/
│       ├── logo.svg              Hex glyph logo
│       └── hero-mock.svg         Studio interface mockup
└── README.md                     This file
```

## Hosting

### Locally
Just double-click `index.html`. All links are relative.

### On a static host
Upload the entire `Tutorial/` folder. Point the public URL at
`/Tutorial/index.html`. Works on:
- GitHub Pages
- Cloudflare Pages
- Netlify (drop-folder deploy)
- Any nginx / Apache static site

### As an embedded section
Put the folder under `your-site.com/learn/` and point your nav to
`your-site.com/learn/index.html`.

## Theme

Colour palette and typography match the PatchCraft Studio app:
- Background: `#0b0d10` (deep navy)
- Accent: `#f5a623` (warm orange)
- Card surface: `#1c1f25`
- Body font: system-ui sans-serif
- Code font: ui-monospace

CSS lives in `assets/css/main.css`. Tweak the `:root` block at the top
to rebrand the entire site in seconds.

## Adding screenshots

The site uses SVG mock illustrations as placeholders so it's complete
out of the box. To swap in real screenshots:

1. Take a PNG screenshot of the Studio at 1280×800 (or whatever your
   preferred ratio).
2. Save into `assets/img/`.
3. Replace `<img src="assets/img/hero-mock.svg" ...>` in any page with
   your new file path.

Suggested screenshots:
- `hero-studio.png` — full Studio window for the landing page
- `design-canvas.png` — Design tab with elements visible
- `sample-mapper.png` — Sample Mapper with zones loaded
- `piano-roll.png` — MIDI Playground piano-roll editor
- `brand-lab.png` — Brand Lab with the player preview
- `expansion-library.png` — Expansion library card grid

## Customisation

- **Rebrand the tutorial** for your own product: update the `--accent`
  CSS variable, replace `assets/img/logo.svg`, edit the topbar text in
  every page (or run a search-and-replace on `PATCHCRAFT`).
- **Add new pages**: copy any existing page as a template, drop into
  the Tutorial root, add a sidebar entry to every other page (or
  factor the sidebar into a JS partial).
- **Print-to-PDF**: every page is set up to print as a single document.
  Use the browser's Print dialog → Save as PDF.

## License

Use it as your product documentation. No attribution required.
