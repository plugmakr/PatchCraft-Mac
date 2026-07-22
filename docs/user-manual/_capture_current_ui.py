"""Capture current PatchCraft Studio UI pages into assets/img/current/."""
from __future__ import annotations

import subprocess
import time
from pathlib import Path

import cv2
import dxcam
from pywinauto import Application
from pywinauto.keyboard import send_keys

REPO = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent / "assets" / "img" / "current"
OUT.mkdir(parents=True, exist_ok=True)
EXE = REPO / "build-codex" / "PatchCraftStudio_artefacts" / "Release" / "PatchCraftStudio.exe"
NEBULA = REPO / "FactoryDemos" / "NebulaPrimeSynth.patchcraft"


def connect(timeout=30):
    app = Application(backend="uia").connect(title_re=".*PatchCraft Studio.*", timeout=timeout)
    win = app.window(title_re=".*PatchCraft Studio.*")
    win.set_focus()
    return win


def click(win, name, control_type=None, timeout=4.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            kwargs = {"title": name}
            if control_type:
                kwargs["control_type"] = control_type
            ctrl = win.child_window(**kwargs)
            ctrl.wait("exists enabled visible ready", timeout=1.0)
            ctrl.click_input()
            print("clicked", name)
            return True
        except Exception:
            time.sleep(0.12)
    print("skip", name)
    return False


def shot(win, name):
    r = win.rectangle()
    region = (
        r.left,
        r.top,
        r.right - ((r.right - r.left) % 2),
        r.bottom - ((r.bottom - r.top) % 2),
    )
    cam = dxcam.create(output_idx=0, output_color="BGR")
    time.sleep(0.35)
    frame = cam.grab(region=region)
    if frame is None:
        print("fail shot", name)
        return
    path = OUT / f"{name}.jpg"
    # Downscale for docs weight
    h, w = frame.shape[:2]
    if w > 1600:
        frame = cv2.resize(frame, (1600, int(h * 1600 / w)), interpolation=cv2.INTER_AREA)
    cv2.imwrite(str(path), frame, [int(cv2.IMWRITE_JPEG_QUALITY), 85])
    print("saved", path, path.stat().st_size)


def main():
    try:
        win = connect(4)
    except Exception:
        if not EXE.exists():
            raise SystemExit(f"missing {EXE}")
        subprocess.Popen([str(EXE)], cwd=str(EXE.parent))
        time.sleep(5)
        win = connect(45)

    try:
        win.maximize()
    except Exception:
        pass
    time.sleep(0.8)
    win = connect()

    # Open Nebula so Design/Test look populated
    send_keys("^o")
    time.sleep(1.0)
    send_keys(str(NEBULA).replace("/", "\\"), with_spaces=True)
    send_keys("{ENTER}")
    time.sleep(3.5)
    win = connect()

    shots = [
        ("Build", "RadioButton", "studio-build"),
        ("1  Import Sounds", "RadioButton", "studio-import-sounds"),
        ("3  Sound Stack", "RadioButton", "studio-sound-stack"),
        ("Design", "RadioButton", "studio-design"),
        ("CONTROLS", "Button", "studio-elements-controls"),
        ("MODULES", "Button", "studio-elements-modules"),
        ("STARTERS", "Button", "studio-elements-starters"),
        ("Brand", "RadioButton", "studio-brand"),
        ("Test", "RadioButton", "studio-test"),
        ("Ship", "RadioButton", "studio-ship"),
    ]

    for name, ctype, file_stem in shots:
        # Some Build substeps are RadioButton, Elements tabs are Button
        ok = click(win, name, ctype, timeout=3.5)
        if not ok and ctype == "RadioButton":
            click(win, name, None, timeout=2.0)
        elif not ok and ctype == "Button":
            click(win, name, None, timeout=2.0)
        time.sleep(1.0)
        shot(win, file_stem)

    print("done")


if __name__ == "__main__":
    main()
