#!/usr/bin/env python3
"""Generate Neurato app icon from neuro.png.

Crops the text from neuro.png, scales up the neural network graphic,
centers it on a dark rounded-rect background, and generates all
required iconset sizes.
"""

import os
import subprocess
from PIL import Image, ImageDraw

ASSETS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_IMAGE = os.path.join(ASSETS_DIR, "neuro.png")
ICONSET_DIR = os.path.join(ASSETS_DIR, "Neurato.iconset")

# The neuro.png is 1024x1024. The neural network graphic (without text)
# occupies roughly the top ~65% of the image. The "Neuro" text is in the
# bottom portion. We crop to keep only the graphic.
# Approximate bounding box of the graphic (inspected visually):
#   Left edge of glow: ~170px, Right edge of waveform: ~830px
#   Top edge of nodes: ~130px, Bottom edge of lowest node: ~670px
CROP_BOX = (155, 115, 845, 630)  # (left, top, right, bottom) — tight around graphic only

# Background color matching the icon.svg dark background
BG_COLOR = (12, 12, 12, 255)  # ~#0C0C0C, very dark

# Icon sizes needed for macOS .iconset
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


def make_rounded_rect_mask(size, radius):
    """Create a rounded rectangle mask."""
    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return mask


def generate_icon(src_path):
    """Generate the base 1024x1024 icon.

    Strategy:
    1. Erase the "Neuro" text in-place by interpolating background colors
       across the full width of each text row (no cropping = no seams).
    2. Scale the cleaned image so the graphic fills ~95% of the canvas,
       centered, using the source's own background to fill any margin.
    """
    import numpy as np

    src = Image.open(src_path).convert("RGBA")
    arr = np.array(src)

    # --- Step 1: Erase the "Neuro" text and edge rows ---
    # Erase from y=630 downward (text starts ~y=710, but we want clean margin)
    # Also erase top rows 0..100 to avoid any top-edge artifacts when scaling.
    h, w = arr.shape[:2]

    # Helper: fill a horizontal band with left-right gradient from edge pixels
    def fill_band(y_start, y_end):
        for y in range(max(0, y_start), min(y_end, h)):
            lc = arr[y, 8, :3].astype(float)
            rc = arr[y, w - 9, :3].astype(float)
            for x in range(w):
                t = x / (w - 1)
                arr[y, x, :3] = (lc * (1 - t) + rc * t).astype(np.uint8)
                arr[y, x, 3] = 255

    fill_band(630, h)       # erase text region + everything below
    fill_band(0, 80)        # clean top edge

    # Also fill left/right edge columns to avoid edge artifacts
    for x_band in [(0, 60), (w - 60, w)]:
        for x in range(x_band[0], x_band[1]):
            for y in range(h):
                # vertical gradient from top to bottom edge
                tc = arr[8, x, :3].astype(float)
                bc = arr[h - 9, x, :3].astype(float)
                t = y / (h - 1)
                arr[y, x, :3] = (tc * (1 - t) + bc * t).astype(np.uint8)
                arr[y, x, 3] = 255

    cleaned = Image.fromarray(arr)

    # --- Step 2: Scale the entire cleaned image so the graphic fills the canvas ---
    crop_l, crop_t, crop_r, crop_b = CROP_BOX
    crop_w = crop_r - crop_l
    crop_h = crop_b - crop_t
    crop_cx = (crop_l + crop_r) / 2
    crop_cy = (crop_t + crop_b) / 2

    canvas_size = 1024
    # Scale so graphic fills ~95% of canvas width
    scale = (canvas_size * 0.95) / crop_w

    full_w = int(w * scale)
    full_h = int(h * scale)
    bg_scaled = cleaned.resize((full_w, full_h), Image.LANCZOS)

    # Position so graphic center maps to canvas center
    bg_paste_x = int(canvas_size / 2 - crop_cx * scale)
    bg_paste_y = int(canvas_size / 2 - crop_cy * scale)

    bg_color = tuple(arr[5, 5, :3]) + (255,)
    final = Image.new("RGBA", (canvas_size, canvas_size), bg_color)
    final.paste(bg_scaled, (bg_paste_x, bg_paste_y))

    return final


def main():
    # Generate base icon
    base_icon = generate_icon(SRC_IMAGE)

    # Save base icon for reference
    base_path = os.path.join(ASSETS_DIR, "icon_1024.png")
    base_icon.save(base_path, "PNG")
    print(f"Saved base icon: {base_path}")

    # Create iconset directory
    os.makedirs(ICONSET_DIR, exist_ok=True)

    # Generate all sizes
    for filename, size in ICON_SIZES:
        resized = base_icon.resize((size, size), Image.LANCZOS)
        out_path = os.path.join(ICONSET_DIR, filename)
        resized.save(out_path, "PNG")
        print(f"  {filename} ({size}x{size})")

    # Generate .icns using iconutil
    icns_path = os.path.join(ASSETS_DIR, "Neurato.icns")
    try:
        subprocess.run(
            ["iconutil", "-c", "icns", ICONSET_DIR, "-o", icns_path],
            check=True,
        )
        print(f"\nGenerated: {icns_path}")
    except subprocess.CalledProcessError as e:
        print(f"iconutil failed: {e}")
        print("You may need to run: iconutil -c icns Neurato.iconset -o Neurato.icns")


if __name__ == "__main__":
    main()
