#!/usr/bin/env python3
"""Generate kid-friendly job line icons as LVGL ALPHA_8BIT C arrays.

Outline / stroke style (not filled blobs) so shapes are easier to recognise
— especially for kids who can't read yet (e.g. Jax).

Run: python3 tools/gen_job_icons.py
Writes: src/job_icons.cpp
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 40
BADGE = 48
STROKE = 3
OUT = Path(__file__).resolve().parents[1] / "src" / "job_icons.cpp"


def blank() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    img = Image.new("L", (SIZE, SIZE), 0)
    return img, ImageDraw.Draw(img)


def to_c_array(name: str, img: Image.Image) -> str:
    data = img.tobytes()
    lines = []
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i : i + 16])
        lines.append(f"  {chunk},")
    body = "\n".join(lines)
    uname = name.upper()
    return (
        f"static const uint8_t ICON_{uname}_MAP[] = {{\n{body}\n}};\n\n"
        f"static const lv_img_dsc_t ICON_{uname} = {{\n"
        f"  .header = {{\n"
        f"    .cf = LV_IMG_CF_ALPHA_8BIT,\n"
        f"    .always_zero = 0,\n"
        f"    .reserved = 0,\n"
        f"    .w = {SIZE},\n"
        f"    .h = {SIZE},\n"
        f"  }},\n"
        f"  .data_size = {len(data)},\n"
        f"  .data = ICON_{uname}_MAP,\n"
        f"}};\n"
    )


def draw_make_bed() -> Image.Image:
    """Bed frame + pillow outline."""
    img, d = blank()
    d.rounded_rectangle((5, 20, 35, 32), radius=3, outline=255, width=STROKE)
    d.rounded_rectangle((7, 12, 18, 22), radius=3, outline=255, width=STROKE)
    d.line((5, 32, 35, 32), fill=255, width=STROKE)
    return img


def draw_tidy_floor() -> Image.Image:
    """Broom: handle + brush."""
    img, d = blank()
    d.line((22, 5, 14, 28), fill=255, width=STROKE)
    d.polygon([(8, 28), (32, 28), (34, 35), (6, 35)], outline=255)
    d.line((8, 28, 32, 28), fill=255, width=STROKE)
    d.line((10, 31, 12, 35), fill=255, width=2)
    d.line((16, 31, 17, 35), fill=255, width=2)
    d.line((22, 31, 23, 35), fill=255, width=2)
    d.line((28, 31, 30, 35), fill=255, width=2)
    return img


def draw_washing() -> Image.Image:
    """Laundry basket with handles."""
    img, d = blank()
    d.rounded_rectangle((9, 12, 31, 34), radius=4, outline=255, width=STROKE)
    d.arc((11, 5, 29, 17), 200, 340, fill=255, width=STROKE)
    d.ellipse((14, 18, 20, 24), outline=255, width=2)
    d.ellipse((22, 22, 28, 28), outline=255, width=2)
    return img


def draw_towels() -> Image.Image:
    """Towel hanging on a hook rail."""
    img, d = blank()
    d.line((7, 8, 33, 8), fill=255, width=STROKE)
    d.rounded_rectangle((13, 8, 27, 34), radius=2, outline=255, width=STROKE)
    d.line((16, 16, 24, 16), fill=255, width=2)
    d.line((16, 22, 24, 22), fill=255, width=2)
    return img


def draw_brush_teeth() -> Image.Image:
    """Toothbrush: bristles + handle."""
    img, d = blank()
    d.rounded_rectangle((9, 7, 19, 26), radius=3, outline=255, width=STROKE)
    for y in (10, 14, 18, 22):
        d.line((11, y, 17, y), fill=255, width=2)
    d.line((14, 26, 14, 36), fill=255, width=STROKE)
    return img


def draw_get_dressed() -> Image.Image:
    """T-shirt outline."""
    img, d = blank()
    pts = [
        (20, 7),
        (27, 12),
        (34, 12),
        (28, 18),
        (28, 34),
        (12, 34),
        (12, 18),
        (6, 12),
        (13, 12),
    ]
    d.line(pts + [pts[0]], fill=255, width=STROKE, joint="curve")
    d.arc((14, 6, 26, 14), 0, 180, fill=255, width=STROKE)
    return img


def draw_socks() -> Image.Image:
    """Sock outline."""
    img, d = blank()
    d.rounded_rectangle((15, 5, 25, 20), radius=3, outline=255, width=STROKE)
    d.arc((8, 16, 30, 36), 20, 200, fill=255, width=STROKE)
    d.line((15, 20, 15, 28), fill=255, width=STROKE)
    d.line((25, 20, 26, 24), fill=255, width=STROKE)
    d.line((15, 6, 25, 6), fill=255, width=2)
    return img


def draw_medication() -> Image.Image:
    """Pill / capsule outline, split down the middle."""
    img, d = blank()
    d.rounded_rectangle((10, 14, 30, 26), radius=6, outline=255, width=STROKE)
    d.line((20, 14, 20, 26), fill=255, width=STROKE)
    return img


def draw_lounge() -> Image.Image:
    """Sofa outline."""
    img, d = blank()
    d.rounded_rectangle((7, 16, 33, 28), radius=3, outline=255, width=STROKE)
    d.rounded_rectangle((7, 10, 13, 22), radius=2, outline=255, width=STROKE)
    d.rounded_rectangle((27, 10, 33, 22), radius=2, outline=255, width=STROKE)
    d.line((10, 28, 10, 34), fill=255, width=STROKE)
    d.line((30, 28, 30, 34), fill=255, width=STROKE)
    return img


def draw_table() -> Image.Image:
    """Table with plate on top."""
    img, d = blank()
    d.ellipse((11, 8, 29, 18), outline=255, width=STROKE)
    d.ellipse((15, 11, 25, 16), outline=255, width=2)
    d.line((7, 22, 33, 22), fill=255, width=STROKE)
    d.line((12, 22, 12, 34), fill=255, width=STROKE)
    d.line((28, 22, 28, 34), fill=255, width=STROKE)
    return img


def draw_lights() -> Image.Image:
    """Light bulb outline with rays."""
    img, d = blank()
    d.ellipse((11, 6, 29, 24), outline=255, width=STROKE)
    d.rectangle((16, 24, 24, 29), outline=255, width=STROKE)
    d.line((17, 31, 23, 31), fill=255, width=STROKE)
    d.line((20, 2, 20, 6), fill=255, width=2)
    d.line((6, 14, 11, 14), fill=255, width=2)
    d.line((29, 14, 34, 14), fill=255, width=2)
    d.line((9, 7, 12, 10), fill=255, width=2)
    d.line((28, 10, 31, 7), fill=255, width=2)
    return img


def draw_cat() -> Image.Image:
    """Cat face: round head, pointed ears, eyes, whiskers."""
    img, d = blank()
    d.ellipse((8, 12, 32, 34), outline=255, width=STROKE)
    d.line([(10, 14), (14, 4), (18, 14)], fill=255, width=STROKE)
    d.line([(22, 14), (26, 4), (30, 14)], fill=255, width=STROKE)
    d.ellipse((14, 20, 18, 24), outline=255, width=2)
    d.ellipse((22, 20, 26, 24), outline=255, width=2)
    d.line((6, 22, 12, 24), fill=255, width=2)
    d.line((6, 26, 12, 26), fill=255, width=2)
    d.line((28, 24, 34, 22), fill=255, width=2)
    d.line((28, 26, 34, 26), fill=255, width=2)
    return img


def draw_dog() -> Image.Image:
    """Dog face: round head, floppy ears, eyes, nose."""
    img, d = blank()
    d.ellipse((9, 12, 31, 34), outline=255, width=STROKE)
    d.ellipse((3, 14, 13, 28), outline=255, width=STROKE)
    d.ellipse((27, 14, 37, 28), outline=255, width=STROKE)
    d.ellipse((14, 20, 18, 24), outline=255, width=2)
    d.ellipse((22, 20, 26, 24), outline=255, width=2)
    d.ellipse((17, 26, 23, 32), outline=255, width=2)
    return img


def draw_dishes_away() -> Image.Image:
    """Stack of plates (side view outlines)."""
    img, d = blank()
    d.ellipse((8, 22, 32, 34), outline=255, width=STROKE)
    d.ellipse((8, 15, 32, 27), outline=255, width=STROKE)
    d.ellipse((8, 8, 32, 20), outline=255, width=STROKE)
    return img


def draw_dishes_wash() -> Image.Image:
    """Bowl with soap bubbles."""
    img, d = blank()
    d.arc((6, 14, 34, 36), 0, 180, fill=255, width=STROKE)
    d.line((6, 24, 34, 24), fill=255, width=STROKE)
    d.ellipse((10, 6, 16, 12), outline=255, width=2)
    d.ellipse((18, 3, 26, 11), outline=255, width=2)
    d.ellipse((27, 7, 33, 13), outline=255, width=2)
    return img


def draw_plants() -> Image.Image:
    """Pot + leafy plant."""
    img, d = blank()
    d.polygon([(13, 26), (27, 26), (24, 35), (16, 35)], outline=255)
    d.line([(13, 26), (27, 26), (24, 35), (16, 35), (13, 26)], fill=255, width=STROKE)
    d.ellipse((14, 8, 26, 22), outline=255, width=STROKE)
    d.ellipse((7, 12, 17, 24), outline=255, width=STROKE)
    d.ellipse((23, 12, 33, 24), outline=255, width=STROKE)
    d.line((20, 22, 20, 26), fill=255, width=2)
    return img


def draw_bins() -> Image.Image:
    """Wheelie bin with lid + handle."""
    img, d = blank()
    d.rounded_rectangle((11, 13, 29, 34), radius=3, outline=255, width=STROKE)
    d.line((8, 13, 32, 13), fill=255, width=STROKE)
    d.arc((14, 4, 26, 14), 0, 180, fill=255, width=STROKE)
    d.line((17, 17, 17, 30), fill=255, width=2)
    d.line((23, 17, 23, 30), fill=255, width=2)
    return img


def draw_bathroom() -> Image.Image:
    """Bathtub with tap."""
    img, d = blank()
    d.arc((6, 16, 34, 36), 0, 180, fill=255, width=STROKE)
    d.line((6, 22, 34, 22), fill=255, width=STROKE)
    d.line((20, 8, 20, 16), fill=255, width=STROKE)
    d.ellipse((16, 4, 24, 10), outline=255, width=STROKE)
    return img


ICONS = [
    ("make_bed", draw_make_bed),
    ("tidy_floor", draw_tidy_floor),
    ("washing_in_baskets", draw_washing),
    ("towels_on_hooks", draw_towels),
    ("brush_teeth", draw_brush_teeth),
    ("get_dressed", draw_get_dressed),
    ("socks_on", draw_socks),
    ("medication", draw_medication),
    ("loungeroom_tidy", draw_lounge),
    ("table_cleared", draw_table),
    ("lights_fans_off", draw_lights),
    ("feed_cat", draw_cat),
    ("feed_dog", draw_dog),
    ("dishes_away", draw_dishes_away),
    ("dishes_wash", draw_dishes_wash),
    ("water_plants", draw_plants),
    ("bins_out", draw_bins),
    ("bathroom", draw_bathroom),
]

BADGE_COLORS = {
    "make_bed": 0x60A5FA,
    "tidy_floor": 0x34D399,
    "washing_in_baskets": 0xA78BFA,
    "towels_on_hooks": 0xF472B6,
    "brush_teeth": 0x22D3EE,
    "get_dressed": 0xFB923C,
    "socks_on": 0xFBBF24,
    "medication": 0xF87171,
    "loungeroom_tidy": 0x818CF8,
    "table_cleared": 0x2DD4BF,
    "lights_fans_off": 0xFDE047,
    "feed_cat": 0xFB923C,
    "feed_dog": 0xA3E635,
    "dishes_away": 0x38BDF8,
    "dishes_wash": 0x67E8F9,
    "water_plants": 0x4ADE80,
    "bins_out": 0x94A3B8,
    "bathroom": 0xC084FC,
}


def main() -> None:
    parts = [
        "/* Auto-generated by tools/gen_job_icons.py — do not edit by hand. */",
        '#include "job_icons.h"',
        "",
        "#include <cstring>",
        "",
        f"static constexpr int kBadgeSize = {BADGE};",
        "",
        "namespace {",
        "",
    ]
    for name, fn in ICONS:
        parts.append(to_c_array(name, fn()))
        parts.append("")

    parts.append("const lv_img_dsc_t *iconForId(const char *id) {")
    for name, _ in ICONS:
        parts.append(f'  if (std::strcmp(id, "{name}") == 0) return &ICON_{name.upper()};')
    parts.append("  return nullptr;")
    parts.append("}")
    parts.append("")
    parts.append("uint32_t badgeColorForId(const char *id) {")
    for name, _ in ICONS:
        parts.append(f'  if (std::strcmp(id, "{name}") == 0) return 0x{BADGE_COLORS[name]:06X}u;')
    parts.append("  return 0x64748Bu;")
    parts.append("}")
    parts.append("")
    parts.append("}  // namespace")
    parts.append("")
    parts.append("lv_obj_t *job_icon_create(lv_obj_t *parent, const char *job_id) {")
    parts.append("  lv_obj_t *badge = lv_obj_create(parent);")
    parts.append("  lv_obj_set_size(badge, kBadgeSize, kBadgeSize);")
    parts.append("  lv_obj_set_style_radius(badge, 12, 0);")
    parts.append("  lv_obj_set_style_bg_color(badge, lv_color_hex(badgeColorForId(job_id)), 0);")
    parts.append("  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);")
    parts.append("  lv_obj_set_style_border_width(badge, 0, 0);")
    parts.append("  lv_obj_set_style_pad_all(badge, 0, 0);")
    parts.append("  lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);")
    parts.append("  lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);")
    parts.append("")
    parts.append("  const lv_img_dsc_t *img = iconForId(job_id);")
    parts.append("  if (img) {")
    parts.append("    lv_obj_t *pic = lv_img_create(badge);")
    parts.append("    lv_img_set_src(pic, img);")
    # White strokes on the coloured badge — clearer line-icon look for Jax
    parts.append("    lv_obj_set_style_img_recolor(pic, lv_color_hex(0xFFFFFF), 0);")
    parts.append("    lv_obj_set_style_img_recolor_opa(pic, LV_OPA_COVER, 0);")
    parts.append("    lv_obj_center(pic);")
    parts.append("    lv_obj_clear_flag(pic, LV_OBJ_FLAG_CLICKABLE);")
    parts.append("  }")
    parts.append("  return badge;")
    parts.append("}")
    parts.append("")

    OUT.write_text("\n".join(parts), encoding="utf-8")
    print(f"Wrote {OUT} ({len(ICONS)} line icons, {SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
