# Third-person camera

**Status:** Partial.

## What is done

- **Zoom:** LT / RT and the mouse wheel, one step per notch.
- **Analog turn:** the right stick turns the camera. LB / RB no longer turn it.
- **Follow (F7):** the camera follows the character while walking. Turned off
  with F7; the state is written to the log.
- **Wall fix:** corrections for wall cut-outs and table legs near the camera.

## Not done (from the recomp)

- Spring-arm camera: the camera should not pass through walls and roofs.
- Smooth camera between positions (sub-pixel view for jitter-free motion).
- Faster L1/R1 turns that keep spinning while held (LB/RB are no longer used).

## Code

The camera code lives in `d3d9_proxy.c`: `stick_frame` (right stick),
`zoom_frame` (triggers and wheel, via `wheel_hook`) and `follow_frame`
(F7 follow). `follow_frame` also counts LB/RB as a manual turn.

## Notes

The game's own camera data is read and written at fixed addresses. Document
them in the [memory map](../reference/memory-map.md) before changing the
camera.

## Spring-arm probe (item 4, log only)

Not a feature yet. F3 turns on a probe (off by default) that writes one line
every 30 field frames: the leader's cell, the cell attribute, the camera offset
triple (`0xBAD7CC/D0/D4`) and the yaw. Used to confirm the scale of the triple
and the wall attributes before the arm is applied. Code: `src/d3d9/arm_probe.h`.
