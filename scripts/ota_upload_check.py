# PlatformIO pre-upload guard + auth/port flags for the espota environment.
Import("env")  # type: ignore  # noqa: F821

import os


def _before_upload(source, target, env):  # noqa: ARG001
    port = (env.subst("$UPLOAD_PORT") or "").strip()
    upper = port.upper()
    looks_serial = (
        not port
        or upper.startswith("COM")
        or port.startswith("/dev/tty")
        or port.startswith("/dev/cu.")
    )
    if looks_serial:
        print("")
        print("ERROR: You selected an *-ota environment with a USB serial port (%s)." % (port or "empty"))
        print("  That env talks Wi‑Fi (espota), not the cable.")
        print("")
        print("  Fix — USB flash (what you want with COM31):")
        print("    1. PlatformIO env dropdown → ws7")
        print("    2. Upload again")
        print("")
        print("  Or later for Wi‑Fi OTA (panel already running OTA firmware):")
        print("    $env:OTA_HOST='192.168.x.x'")
        print("    $env:OTA_PASSWORD='your-ota-password'")
        print("    pio run -e ws7-ota -t upload")
        print("")
        env.Exit(1)


# Prefer env OTA_PORT when set; otherwise keep ini default 3232.
ota_port = (os.environ.get("OTA_PORT") or "").strip() or "3232"
ota_auth = (os.environ.get("OTA_PASSWORD") or "").strip()
flags = ["--port=%s" % ota_port]
if ota_auth:
    flags.append("--auth=%s" % ota_auth)
else:
    print("WARNING: OTA_PASSWORD not set — set $env:OTA_PASSWORD to match secrets.h")
env.Replace(UPLOAD_FLAGS=flags)  # type: ignore  # noqa: F821
env.AddPreAction("upload", _before_upload)  # type: ignore  # noqa: F821
