import json
from pathlib import Path

for p in sorted(Path("FactoryDemos").glob("*.patchcraft/manifest.json")):
    m = json.loads(p.read_text(encoding="utf-8"))
    print(
        f"{p.parent.name:40} | {m.get('instrumentName', '?'):28} | "
        f"{m.get('engine', '?'):8} | {m.get('category', '')}"
    )
