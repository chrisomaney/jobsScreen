# Windows/PlatformIO: Arduino-ESP32 3.x puts esp_ble_mesh include dirs on -I.
# Then #include <bits/...> resolves against those trees (or Windows returns
# EINVAL → "Invalid argument") instead of libstdc++. We don't use BLE Mesh.
Import("env")  # type: ignore  # noqa: F821

_BLOCK = "esp_ble_mesh"


def _scrub(e, label: str) -> None:
    raw = list(e.get("CPPPATH", []))
    kept = [p for p in raw if _BLOCK not in str(p).replace("\\", "/")]
    removed = len(raw) - len(kept)
    if removed:
        print("[windows-includes] %s: stripped %d %s include path(s)" % (label, removed, _BLOCK))
        e.Replace(CPPPATH=kept)


_scrub(env, "env")  # type: ignore  # noqa: F821

try:
    Import("projenv")  # type: ignore
    _scrub(projenv, "projenv")  # type: ignore  # noqa: F821
except Exception:
    pass
