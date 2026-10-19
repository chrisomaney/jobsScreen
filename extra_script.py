# Windows-safe include/define setup + install lv_conf.h into the lvgl package.
# Do NOT put -I${PROJECT_DIR}/... in platformio.ini build_flags — SCons ParseFlags
# mangles backslash paths on Windows.

Import("env")  # noqa: F821

import shutil
from pathlib import Path

root = Path(env["PROJECT_DIR"])  # noqa: F821
pioenv = env["PIOENV"]  # noqa: F821
lv_conf = root / "include" / "lv_conf.h"
network_src = root / "lib" / "Network" / "src"
include_dir = root / "include"

# Loud banner so Windows logs prove which tree/env is building.
print("=" * 60)
print(f"[extra] PROJECT_DIR = {root}")
print(f"[extra] PIOENV      = {pioenv}")
print("=" * 60)
if pioenv != "ws7":
    raise SystemExit(
        f"[extra] Refusing env '{pioenv}'. Use: pio run -e ws7\n"
        "If PlatformIO IDE shows waveshare-esp32s3-touch-lcd-7, you opened an OLD folder."
    )
if "Downloads" in str(root) and "jobsScreen-" in str(root):
    print(
        "[extra] WARNING: project is under a long Downloads\\jobsScreen-... path. "
        "Windows MAX_PATH often breaks LVGL. Prefer C:\\js"
    )

# Remove any previously mirrored Network headers from include/ — they break LDF
# (headers alone satisfy #include <Network.h> so lib/Network is never linked).
for name in (
    "Network.h",
    "NetworkClient.h",
    "NetworkEvents.h",
    "NetworkInterface.h",
    "NetworkManager.h",
    "NetworkServer.h",
    "NetworkUdp.h",
):
    stale = include_dir / name
    if stale.is_file():
        stale.unlink()
        print(f"[extra] removed stale {stale}")

# Arduino-ESP32 3.x: framework WiFi includes "Network.h". When Network is vendored
# under lib/, the WiFi builder often misses that include path. Force it with -I as
# a single argv token (Windows-safe; never put -I${PROJECT_DIR} in platformio.ini).
network_inc = str(network_src)
env.Append(  # noqa: F821
    CPPPATH=[
        str(include_dir),
        str(root),
        network_inc,
    ],
    CCFLAGS=["-I" + network_inc],
    CXXFLAGS=["-I" + network_inc],
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
