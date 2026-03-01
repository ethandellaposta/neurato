#!/usr/bin/env python3
import argparse
import subprocess
from pathlib import Path

from PIL import Image


ICON_SIZES = [
    ("icon_16x16.png", 16),
    ("icon_16x16@2x.png", 32),
    ("icon_32x32.png", 32),
    ("icon_32x32@2x.png", 64),
    ("icon_64x64.png", 64),
    ("icon_128x128.png", 128),
    ("icon_128x128@2x.png", 256),
    ("icon_256x256.png", 256),
    ("icon_256x256@2x.png", 512),
    ("icon_512x512.png", 512),
    ("icon_512x512@2x.png", 1024),
    ("icon_1024x1024.png", 1024),
]


def generate_iconset(source_png: Path, iconset_dir: Path):
    iconset_dir.mkdir(parents=True, exist_ok=True)
    src = Image.open(source_png).convert("RGBA")

    if src.size[0] != src.size[1]:
        raise ValueError("Source image must be square.")

    for filename, size in ICON_SIZES:
        resized = src.resize((size, size), Image.Resampling.LANCZOS)
        resized.save(iconset_dir / filename, "PNG")


def make_icns(iconset_dir: Path, icns_path: Path):
    subprocess.run(
        ["iconutil", "-c", "icns", str(iconset_dir), "-o", str(icns_path)],
        check=True,
    )


def main():
    parser = argparse.ArgumentParser(description="Set Ampl app icon from a source PNG.")
    parser.add_argument("source", type=Path, help="Path to a square PNG (recommended 1024x1024)")
    args = parser.parse_args()

    source = args.source.expanduser().resolve()
    if not source.exists():
        raise FileNotFoundError(f"Source icon not found: {source}")

    assets_dir = Path(__file__).resolve().parent
    iconset_dir = assets_dir / "Ampl.iconset"
    icns_path = assets_dir / "Ampl.icns"

    generate_iconset(source, iconset_dir)
    make_icns(iconset_dir, icns_path)

    print(f"Updated iconset: {iconset_dir}")
    print(f"Updated icns: {icns_path}")


if __name__ == "__main__":
    main()
