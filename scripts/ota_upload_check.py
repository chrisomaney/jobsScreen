# PlatformIO pre-upload guard + flag defaults for the espota environment.
Import("env")  # type: ignore  # noqa: F821 — provided by SCons/PlatformIO

# os.environ sees PowerShell/cmd `set` / `$env:` vars on Windows.
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
        print("ERROR: waveshare-esp32s3-touch-lcd-7-ota uploads over Wi‑Fi, not USB.")
        print("  • USB cable / COM31  → switch env to: waveshare-esp32s3-touch-lcd-7")
        print("  • Network OTA        → panel IP (or WAN IP), not a COM port:")
        print("      PowerShell:")
        print("        $env:OTA_HOST='192.168.1.50'")
        print("        $env:OTA_PASSWORD='your-ota-password'")
        print("        pio run -e waveshare-esp32s3-touch-lcd-7-ota -t upload")
        print("")
        env.Exit(1)


# Default TCP 3232 when OTA_PORT unset/empty (avoids espota: invalid int value: '').
ota_port = (os.environ.get("OTA_PORT") or "").strip() or "3232"
ota_auth = (os.environ.get("OTA_PASSWORD") or "").strip()
flags = ["--port=%s" % ota_port]
if ota_auth:
    flags.append("--auth=%s" % ota_auth)
else:
    print("WARNING: OTA_PASSWORD not set — espota auth will likely fail.")
env.Replace(UPLOAD_FLAGS=flags)  # type: ignore  # noqa: F821

env.AddPreAction("upload", _before_upload)  # type: ignore  # noqa: F821
