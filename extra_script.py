# Windows-safe include/define setup + install lv_conf.h into the lvgl package.
# Do NOT put -I${PROJECT_DIR}/... in platformio.ini build_flags — SCons ParseFlags
# mangles backslash paths on Windows.

Import("env")  # noqa: F821

import shutil
from pathlib import Path

root = Path(env["PROJECT_DIR"])  # noqa: F821
pioenv = env["PIOENV"]  # noqa: F821
lv_conf = root / "include" / "lv_conf.h"

env.Append(  # noqa: F821
    CPPPATH=[
        str(root / "include"),
        str(root),
        str(root / "lib" / "Network" / "src"),
    ],
    CPPDEFINES=[
        "ARDUINO_USB_CDC_ON_BOOT=1",
        "ARDUINO_USB_MODE=1",
        "BOARD_HAS_PSRAM",
        "CORE_DEBUG_LEVEL=1",
        "LV_CONF_INCLUDE_SIMPLE",
        "ESP_PANEL_BOARD_USE_SUPPORTED_FILE",
        "ESP_PANEL_BOARD_INCLUDE_SUPPORTED_SIMPLE",
        "DISABLE_ALL_LIBRARY_WARNINGS",
    ],
)

# Make sure LVGL's own build finds lv_conf.h (library compile may not see project includes).
lvgl_root = root / ".pio" / "libdeps" / pioenv / "lvgl"
if lv_conf.is_file() and lvgl_root.is_dir():
    shutil.copy2(lv_conf, lvgl_root / "lv_conf.h")
    src_dir = lvgl_root / "src"
    if src_dir.is_dir():
        shutil.copy2(lv_conf, src_dir / "lv_conf.h")
    print(f"[extra] lv_conf.h installed into {lvgl_root}")
elif not lvgl_root.is_dir():
    print("[extra] lvgl not in libdeps yet — run build again after first lib install if needed")
