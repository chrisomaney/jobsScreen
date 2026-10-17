#!/usr/bin/env python3
"""Convert any image to 800x480 RGB565 for the idle/photo screen.

Usage:
  python3 tools/make_idle_photo.py path/to/family.jpg
  pio run -t uploadfs   # after build, uploads data/idle.rgb565

Requires: pip install pillow
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

W, H = 800, 480


def to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    try:
        from PIL import Image
    except ImportError:
        print("Install Pillow: pip install pillow", file=sys.stderr)
        return 1

    src = Path(sys.argv[1])
    out = Path(__file__).resolve().parents[1] / "data" / "idle.rgb565"
    out.parent.mkdir(parents=True, exist_ok=True)

    img = Image.open(src).convert("RGB")
    img = img.resize((W, H), Image.Resampling.LANCZOS)
    px = img.load()
    with out.open("wb") as f:
        for y in range(H):
            for x in range(W):
                r, g, b = px[x, y]
                f.write(struct.pack("<H", to_rgb565(r, g, b)))
    print(f"Wrote {out} ({W}x{H} RGB565, {out.stat().st_size} bytes)")
    print("Flash filesystem with: pio run -t uploadfs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
