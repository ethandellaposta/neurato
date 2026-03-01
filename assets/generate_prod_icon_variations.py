#!/usr/bin/env python3
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageChops, ImageOps


ASSETS_DIR = Path(__file__).resolve().parent
SRC_PATH = ASSETS_DIR / "icon_1024.png"
OUT_DIR = ASSETS_DIR / "icon_variations" / "prod_ready_from_icon1024"
SIZE = 1024
FOREGROUND_MARGIN = 20
FOREGROUND_SCALE_BOOST = 1.18


def corner_average_color(image: Image.Image):
    rgb = image.convert("RGB")
    w, h = rgb.size
    sample_points = [
        (8, 8), (w - 9, 8), (8, h - 9), (w - 9, h - 9),
        (w // 2, 8), (w // 2, h - 9), (8, h // 2), (w - 9, h // 2),
    ]
    r = g = b = 0
    for x, y in sample_points:
        pr, pg, pb = rgb.getpixel((x, y))
        r += pr
        g += pg
        b += pb
    n = len(sample_points)
    return (r // n, g // n, b // n)


def extract_foreground(src: Image.Image):
    base_bg = corner_average_color(src)
    solid_bg = Image.new("RGB", src.size, base_bg)
    diff = ImageChops.difference(src.convert("RGB"), solid_bg).convert("L")

    mask = diff.point(lambda p: 255 if p > 14 else 0)
    mask = mask.filter(ImageFilter.MaxFilter(3))
    mask = mask.filter(ImageFilter.GaussianBlur(1.2))
    mask = ImageOps.autocontrast(mask)
    mask = mask.point(lambda p: min(255, int(p * 1.25)))

    fg = src.convert("RGBA").copy()
    fg.putalpha(mask)
    return fg, mask


def make_gradient_background(size, top_color, bottom_color, radial_glow=None):
    w, h = size
    bg = Image.new("RGBA", size, (0, 0, 0, 255))
    draw = ImageDraw.Draw(bg)

    for y in range(h):
        t = y / (h - 1)
        r = int(top_color[0] * (1 - t) + bottom_color[0] * t)
        g = int(top_color[1] * (1 - t) + bottom_color[1] * t)
        b = int(top_color[2] * (1 - t) + bottom_color[2] * t)
        draw.line((0, y, w, y), fill=(r, g, b, 255))

    if radial_glow is not None:
        gx, gy, gr, glow_color = radial_glow
        glow = Image.new("RGBA", size, (0, 0, 0, 0))
        gdraw = ImageDraw.Draw(glow)
        gdraw.ellipse((gx - gr, gy - gr, gx + gr, gy + gr), fill=glow_color)
        glow = glow.filter(ImageFilter.GaussianBlur(radius=gr // 5))
        bg.alpha_composite(glow)

    return bg


def rounded_mask(size, radius):
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius=radius, fill=255)
    return mask


def fit_and_center_foreground(
    fg: Image.Image,
    size: int,
    margin: int = FOREGROUND_MARGIN,
    scale_boost: float = FOREGROUND_SCALE_BOOST,
):
    alpha = fg.split()[-1]
    bbox = alpha.getbbox()
    if bbox is None:
        return Image.new("RGBA", (size, size), (0, 0, 0, 0))

    cropped = fg.crop(bbox)
    src_w, src_h = cropped.size

    target = size - (margin * 2)
    scale = min(target / src_w, target / src_h) * scale_boost
    max_scale = min(size / src_w, size / src_h)
    scale = min(scale, max_scale)
    dst_w = max(1, int(round(src_w * scale)))
    dst_h = max(1, int(round(src_h * scale)))

    resized = cropped.resize((dst_w, dst_h), Image.Resampling.LANCZOS)
    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    x = (size - dst_w) // 2
    y = (size - dst_h) // 2
    layer.alpha_composite(resized, (x, y))
    return layer


def make_variation(fg: Image.Image, idx: int):
    themes = [
        ((8, 10, 16), (18, 24, 36), (512, 410, 320, (105, 219, 255, 62))),
        ((10, 10, 12), (24, 24, 30), (512, 460, 300, (255, 190, 120, 55))),
        ((7, 14, 20), (14, 34, 48), (488, 430, 330, (103, 255, 211, 52))),
        ((14, 8, 18), (28, 16, 36), (530, 430, 310, (203, 154, 255, 50))),
        ((5, 12, 14), (14, 26, 24), (495, 425, 320, (129, 255, 151, 54))),
        ((12, 7, 8), (32, 20, 22), (510, 440, 315, (255, 134, 134, 48))),
        ((8, 8, 8), (20, 22, 26), (512, 435, 310, (167, 183, 255, 44))),
        ((10, 12, 18), (20, 32, 48), (500, 425, 315, (115, 173, 255, 48))),
        ((8, 14, 10), (20, 32, 18), (512, 425, 300, (186, 255, 112, 44))),
        ((10, 9, 17), (28, 20, 44), (516, 428, 310, (125, 236, 255, 50))),
    ]

    top, bottom, glow = themes[idx]
    bg = make_gradient_background((SIZE, SIZE), top, bottom, radial_glow=glow)

    panel_margin = 42
    panel = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    pdraw = ImageDraw.Draw(panel)
    pdraw.rounded_rectangle(
        (panel_margin, panel_margin, SIZE - panel_margin, SIZE - panel_margin),
        radius=190,
        fill=(255, 255, 255, 8),
        outline=(255, 255, 255, 28),
        width=2,
    )
    panel = panel.filter(ImageFilter.GaussianBlur(0.4))
    bg.alpha_composite(panel)

    # Thin inset edge margin (Logic-like framing)
    edge = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    edraw = ImageDraw.Draw(edge)
    inset = 8
    edraw.rounded_rectangle(
        (inset, inset, SIZE - inset - 1, SIZE - inset - 1),
        radius=214,
        outline=(255, 255, 255, 32),
        width=2,
    )
    bg.alpha_composite(edge)

    # Slight drop shadow for foreground
    shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(shadow)
    sdraw.ellipse((236, 236, 788, 788), fill=(0, 0, 0, 70))
    shadow = shadow.filter(ImageFilter.GaussianBlur(26))
    bg.alpha_composite(shadow)

    # Foreground blend: larger and centered to better match Logic-style fill
    fg_layer = fit_and_center_foreground(fg, SIZE)
    bg.alpha_composite(fg_layer)

    # Rounded app icon clipping
    mask = rounded_mask((SIZE, SIZE), 220)
    final = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    final.paste(bg, (0, 0), mask)
    return final


def main():
    if not SRC_PATH.exists():
        raise FileNotFoundError(f"Source icon not found: {SRC_PATH}")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    src = Image.open(SRC_PATH).convert("RGBA").resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    fg, _ = extract_foreground(src)

    for i in range(10):
        out = make_variation(fg, i)
        out_path = OUT_DIR / f"ampl_prod_icon_v{i + 1:02d}_1024.png"
        out.save(out_path, "PNG")
        print(out_path.name)


if __name__ == "__main__":
    main()
