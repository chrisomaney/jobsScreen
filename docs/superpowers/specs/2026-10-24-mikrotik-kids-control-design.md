# MikroTik Kids Control Integration Design

**Date:** 2026-10-24  
**Status:** Approved  
**Path:** Panel MQTT → MikroTik IoT (pause/resume) + Home Assistant (status/notify)

## Goal

When a kid finishes their own required jobs, the panel publishes `kids/{slug}/internet` = `on`. MikroTik **Kid Control** should unlock that kid’s devices. When jobs are incomplete (`off`), lock them again. HA shows status and can notify; it does not drive the router (avoids double-toggles).

## Mapping

| MQTT topic | Payload | MikroTik action |
|---|---|---|
| `kids/xavier/internet` | `on` | `/ip kid-control resume xavier` |
| `kids/xavier/internet` | `off` | `/ip kid-control pause xavier` |
| (same for presley, jax, banx, hazel) | | Profile **name** = slug |

## Deliverables

- `examples/mikrotik-kids-control.rsc` — broker + subscriptions + setup notes  
- Update `examples/homeassistant-mqtt.yaml` — sensors + notify only  
- README section for wiring
