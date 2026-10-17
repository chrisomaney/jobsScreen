# Windows-safe include/define setup.
# Do NOT put -I${PROJECT_DIR}/... in platformio.ini build_flags — SCons ParseFlags
# mangles backslash paths (e.g. \Users) and fails with:
#   TypeError: Tried to lookup File '...KidsjobsScreen' as a Dir.

Import("env")  # noqa: F821

from pathlib import Path

root = Path(env["PROJECT_DIR"])  # noqa: F821

env.Append(  # noqa: F821
    CPPPATH=[
        str(root / "include"),
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
