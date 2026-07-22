# PatchCraft User Manual (current UI)

HTML/CSS/JS manual for **today’s Studio toolbar**:

**Build → Design → Brand → Test → Ship**

Build sub-steps inside Build:

1. Import Sounds  
2. Chop Loop (when the product recipe needs it)  
3. Sound Stack  
4. Perform (Advanced Build)

## Open

```bash
python -m http.server 8080 --directory docs/user-manual
```

Open `http://localhost:8080/`.

Screenshots live in `assets/img/current/` and were captured from the current Release Studio build — not from older MasterClass docs.

## Regenerate

```bash
# optional: recapture screenshots from a running/current Studio
python docs/user-manual/_capture_current_ui.py

# rebuild HTML
python docs/user-manual/_generate.py
```

`_style-ref/` is style-only reference (CSS/visual system). Do not copy its obsolete page structure into the manual.
