#!/usr/bin/env python3
"""Compile the device widget against its real LVGL and export headless frames."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path(tempfile.gettempdir()) / "robotfleet-brand-preview")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--lvgl-source", type=Path)
    parser.add_argument("--no-sanitize", action="store_true")
    parser.add_argument("--dark", action="store_true", help="Use the firmware dark theme's #000000 background")
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    output = (args.output or here / "output").resolve()
    output.mkdir(parents=True, exist_ok=True)
    configure = ["cmake", "-S", str(here), "-B", str(args.build_dir), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Debug", f"-DBRAND_PREVIEW_SANITIZE={'OFF' if args.no_sanitize else 'ON'}"]
    if args.lvgl_source:
        configure.append(f"-DLVGL_SOURCE={args.lvgl_source.resolve()}")
    subprocess.run(configure, check=True)
    subprocess.run(["cmake", "--build", str(args.build_dir), "--parallel", "8"], check=True)
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    command = [str(args.build_dir.resolve() / "brand_preview"), str(output)]
    if args.dark:
        command.append("dark")
    completed = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(completed.stdout, end="")
    (output / "test.log").write_text(completed.stdout)
    completed.check_returncode()
    names = ["idle", "listening", "thinking", "speaking", "offline"]
    mask = Image.new("L", (360, 360), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, 359, 359), fill=255)
    for name in names:
        source = output / f"{name}.ppm"
        frame = Image.open(source).convert("RGBA")
        frame.save(output / f"{name}-framebuffer.png")
        frame.putalpha(mask)
        frame.save(output / f"{name}.png")
        source.unlink()
    report = json.loads((output / "results.json").read_text())
    report["capture"] = {"renderer": "real LVGL software RGB565 framebuffer", "round_png": "framebuffer pixels with only the physical round-screen mask applied", "size": [360, 360]}
    report["sanitizers"] = [] if args.no_sanitize else ["AddressSanitizer", "UndefinedBehaviorSanitizer"]
    board = here.parent.parent / "main/boards/waveshare/esp32-s3-touch-lcd-1.85c"
    sources = [board / name for name in ["robotfleet_widget.cc", "robotfleet_widget.h", "robotfleet_ui_model.h", "robotfleet_logo.cc", "assets/robotfleet-logo-source.png"]]
    report["source_sha256"] = {str(path.relative_to(here.parent.parent)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources}
    (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"PASS: real widget headless tests and five state PNGs: {output}")


if __name__ == "__main__":
    main()
