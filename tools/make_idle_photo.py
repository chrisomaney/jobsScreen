#!/usr/bin/env python3
"""Convert any image to 800x480 RGB565 for the idle/photo screen.

Usage (from the project root):

  Windows PowerShell / cmd:
    python tools/make_idle_photo.py C:\\1\\photo.jpg
    python tools/make_idle_photo.py "C:/1/photo.jpg"

  Then flash the filesystem:
    pio run -t uploadfs

Requires: pip install pillow
"""

from __future__ import annotations

import re
import struct
import sys
from pathlib import Path

W, H = 800, 480


def to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def normalize_path(raw: str) -> Path:
    """Accept Windows, POSIX, and Git-Bash style paths."""
    s = raw.strip().strip('"').strip("'")

    # Git Bash / MSYS: /c/Users/... or /c:/Users/...
    m = re.match(r"^/([A-Za-z])[:]?/(.*)$", s.replace("\\", "/"))
    if m:
        drive, rest = m.group(1).upper(), m.group(2)
        s = f"{drive}:/{rest}"

    return Path(s).expanduser().resolve()


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    try:
        from PIL import Image
    except ImportError:
        print("Install Pillow first:", file=sys.stderr)
        print("  python -m pip install pillow", file=sys.stderr)
        return 1

    src = normalize_path(sys.argv[1])
    if not src.is_file():
        print(f"File not found: {src}", file=sys.stderr)
        print("Example: python tools/make_idle_photo.py C:\\1\\photo.jpg", file=sys.stderr)
        return 1

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
