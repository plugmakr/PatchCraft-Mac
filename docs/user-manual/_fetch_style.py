import re
import urllib.request
from pathlib import Path
from urllib.parse import urljoin

BASE = "https://thryqkqwjekfkbqekzun.supabase.co/storage/v1/object/public/product-tutorials/patchcraft/"
OUT = Path(__file__).resolve().parent / "_style-ref"
OUT.mkdir(parents=True, exist_ok=True)

html_path = OUT / "index.html"
if not html_path.exists():
    with urllib.request.urlopen(urllib.request.Request(BASE + "index.html", headers={"User-Agent": "Mozilla/5.0"})) as r:
        html_path.write_bytes(r.read())

html = html_path.read_text(encoding="utf-8", errors="replace")
assets = sorted(set(re.findall(r'(?:href|src)=["\']([^"\']+)["\']', html)))
print("found", len(assets), "refs")

# Always grab core style assets
must = [
    "assets/css/main.css",
    "assets/img/logo.svg",
]
for a in must + [x for x in assets if x.startswith("assets/")]:
    if a.startswith("http") or a.startswith("#"):
        continue
    dest = OUT / a
    if dest.exists() and dest.stat().st_size > 0:
        print("have", a)
        continue
    dest.parent.mkdir(parents=True, exist_ok=True)
    url = urljoin(BASE, a)
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=90) as r:
            data = r.read()
        dest.write_bytes(data)
        print("OK", a, len(data))
    except Exception as e:
        print("FAIL", a, e)

css = OUT / "assets/css/main.css"
if css.exists():
    print("CSS bytes", css.stat().st_size)
    print(css.read_text(encoding="utf-8", errors="replace")[:1500])
