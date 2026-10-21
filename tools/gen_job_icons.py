#!/usr/bin/env python3
"""Generate kid-friendly job pictograms as LVGL ALPHA_8BIT C arrays.

Run: python3 tools/gen_job_icons.py
Writes: src/job_icons.cpp
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 40
BADGE = 48
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
    img, d = blank()
    d.rounded_rectangle((4, 18, 36, 34), radius=3, fill=255)
    d.rounded_rectangle((6, 12, 18, 22), radius=3, fill=220)
    d.rectangle((4, 30, 36, 34), fill=180)
    return img


def draw_tidy_floor() -> Image.Image:
    img, d = blank()
    d.polygon([(20, 4), (24, 4), (18, 28), (14, 28)], fill=255)
    d.polygon([(8, 28), (32, 28), (34, 36), (6, 36)], fill=220)
    return img


def draw_washing() -> Image.Image:
    img, d = blank()
    d.rounded_rectangle((8, 10, 32, 34), radius=4, fill=255)
    d.arc((10, 4, 30, 18), 200, 340, fill=200, width=3)
    d.ellipse((14, 16, 20, 22), fill=120)
    d.ellipse((22, 20, 28, 26), fill=120)
    return img


def draw_towels() -> Image.Image:
    img, d = blank()
    d.line((8, 8, 32, 8), fill=200, width=3)
    d.rounded_rectangle((12, 8, 28, 34), radius=2, fill=255)
    d.line((14, 14, 26, 14), fill=160, width=2)
    d.line((14, 20, 26, 20), fill=160, width=2)
    return img


def draw_brush_teeth() -> Image.Image:
    img, d = blank()
    d.rounded_rectangle((8, 8, 18, 28), radius=3, fill=255)
    for y in (10, 14, 18, 22):
        d.rectangle((10, y, 16, y + 2), fill=140)
    d.rectangle((11, 28, 15, 36), fill=220)
    return img


def draw_get_dressed() -> Image.Image:
    img, d = blank()
    d.polygon(
        [(20, 6), (28, 12), (34, 12), (28, 18), (28, 34), (12, 34), (12, 18), (6, 12), (12, 12)],
        fill=255,
    )
    return img


def draw_socks() -> Image.Image:
    img, d = blank()
    d.rounded_rectangle((14, 4, 26, 22), radius=4, fill=255)
    d.pieslice((8, 16, 30, 36), 20, 200, fill=255)
    d.rectangle((14, 4, 26, 10), fill=180)
    return img


def draw_medication() -> Image.Image:
    img, d = blank()
    d.pieslice((8, 12, 32, 28), 90, 270, fill=255)
    d.pieslice((8, 12, 32, 28), 270, 90, fill=180)
    d.rectangle((8, 18, 32, 22), fill=255)
    return img


def draw_lounge() -> Image.Image:
    img, d = blank()
    d.rounded_rectangle((6, 14, 34, 28), radius=4, fill=255)
    d.rectangle((6, 22, 34, 28), fill=200)
    d.rectangle((8, 28, 12, 34), fill=180)
    d.rectangle((28, 28, 32, 34), fill=180)
    d.rounded_rectangle((6, 10, 12, 22), radius=2, fill=220)
    d.rounded_rectangle((28, 10, 34, 22), radius=2, fill=220)
    return img


def draw_table() -> Image.Image:
    img, d = blank()
    d.ellipse((10, 10, 30, 20), fill=255)
    d.ellipse((14, 12, 26, 18), outline=100, width=2)
    d.rectangle((8, 22, 32, 26), fill=220)
    d.rectangle((12, 26, 16, 34), fill=180)
    d.rectangle((24, 26, 28, 34), fill=180)
    return img


def draw_lights() -> Image.Image:
    img, d = blank()
    d.ellipse((10, 6, 30, 26), fill=255)
    d.rectangle((16, 24, 24, 30), fill=200)
    d.rectangle((14, 30, 26, 34), fill=160)
    d.line((20, 2, 20, 6), fill=200, width=2)
    d.line((6, 14, 10, 14), fill=200, width=2)
    d.line((30, 14, 34, 14), fill=200, width=2)
    return img


def draw_cat() -> Image.Image:
    img, d = blank()
    d.ellipse((8, 12, 32, 34), fill=255)
    d.polygon([(10, 14), (14, 4), (18, 14)], fill=255)
    d.polygon([(22, 14), (26, 4), (30, 14)], fill=255)
    d.ellipse((14, 20, 18, 24), fill=40)
    d.ellipse((22, 20, 26, 24), fill=40)
    d.polygon([(20, 24), (18, 28), (22, 28)], fill=100)
    return img


def draw_dog() -> Image.Image:
    img, d = blank()
    d.ellipse((8, 12, 32, 34), fill=255)
    d.ellipse((4, 14, 14, 28), fill=200)
    d.ellipse((26, 14, 36, 28), fill=200)
    d.ellipse((14, 20, 18, 24), fill=40)
    d.ellipse((22, 20, 26, 24), fill=40)
    d.ellipse((17, 26, 23, 32), fill=140)
    return img


def draw_dishes_away() -> Image.Image:
    img, d = blank()
    d.ellipse((8, 20, 32, 34), fill=180)
    d.ellipse((8, 14, 32, 28), fill=220)
    d.ellipse((8, 8, 32, 22), fill=255)
    d.ellipse((14, 11, 26, 17), outline=100, width=2)
    return img


def draw_dishes_wash() -> Image.Image:
    img, d = blank()
    d.ellipse((6, 14, 34, 34), fill=255)
    d.ellipse((12, 18, 28, 28), outline=80, width=2)
    d.ellipse((10, 6, 16, 12), fill=200)
    d.ellipse((18, 4, 26, 12), fill=180)
    d.ellipse((26, 8, 32, 14), fill=200)
    return img


def draw_plants() -> Image.Image:
    img, d = blank()
    d.polygon([(12, 28), (28, 28), (24, 36), (16, 36)], fill=200)
    d.ellipse((14, 8, 26, 22), fill=255)
    d.ellipse((8, 12, 18, 24), fill=220)
    d.ellipse((22, 12, 32, 24), fill=220)
    d.ellipse((28, 4, 34, 10), fill=160)
    return img


def draw_bins() -> Image.Image:
    img, d = blank()
    d.rounded_rectangle((10, 12, 30, 34), radius=3, fill=255)
    d.rectangle((8, 10, 32, 14), fill=200)
    d.line((16, 16, 16, 30), fill=80, width=2)
    d.line((24, 16, 24, 30), fill=80, width=2)
    d.arc((14, 4, 26, 14), 0, 180, fill=180, width=3)
    return img


def draw_bathroom() -> Image.Image:
    img, d = blank()
    d.ellipse((6, 16, 34, 34), fill=255)
    d.rectangle((8, 14, 32, 22), fill=255)
    d.rectangle((18, 6, 22, 16), fill=200)
    d.ellipse((16, 4, 24, 10), fill=180)
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
    parts.append("    lv_obj_set_style_img_recolor(pic, lv_color_hex(0x0F172A), 0);")
    parts.append("    lv_obj_set_style_img_recolor_opa(pic, LV_OPA_COVER, 0);")
    parts.append("    lv_obj_center(pic);")
    parts.append("    lv_obj_clear_flag(pic, LV_OBJ_FLAG_CLICKABLE);")
    parts.append("  }")
    parts.append("  return badge;")
    parts.append("}")
    parts.append("")

    OUT.write_text("\n".join(parts), encoding="utf-8")
    print(f"Wrote {OUT} ({len(ICONS)} icons, {SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
