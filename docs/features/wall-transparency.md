# Wall transparency (research)

**Status:** Research. Not implemented. Updated 14/11 with the Antigravity report and IDA checks.

## Goal

When the leader is behind a wall, a tree or other scenery, make the obstacle
transparent around the character, as the recomp does with the leader's
silhouette.

## Findings (updated 14/11)

- **The wall that disappears is not transparency.** The game removes the geometry in
  front of the camera: sprites with depth below the near plane `0xBAD7C0` are dropped
  (`sub_6640F0`, check at `0x676C55`), and quads and wall faces with depth below
  `0xB52FF2` (a copy of the near plane) are dropped when the packet is built
  (`0x65F487`, `0x661358`). Back-face culling (`FUN_00412670`) also removes faces.
  To confirm: change `0xBAD7C0` in the proxy and see whether the wall stays.
- **The field device is Direct3D 7, not Direct3D 3.** The IID `{84E63DE0-46AA-11CF-816F-0000C020156E}`
  (IDirect3DHALDevice) is used in `sub_40C130` (`CreateDevice` at `0x40C15F`, slot 4 of
  IDirect3D7). The device pointer is at `0xABC200`. The field is drawn with
  `DrawPrimitive` (slot 25, offset `0x64`), called from the drawing code around `0x403165`.
- The proxy's Direct3D 9 device sees only three `DrawPrimitiveUP` calls, because the
  GOG wrapper consolidates the frame. A transparency hook has to go on the game's
  device, in the vtable at `0xABC200`, not on the proxy's device.

- The field is not rasterized by the CPU. The game builds packets of polygons
  and sprites, sorts them into 1024 buckets, and sends textured vertices to
  `DrawPrimitive` on the Direct3D 7 device (see Findings above).
- Rasterization and texture sampling happen below that interface.
- The result is drawn to the DirectDraw surface at `0xABC1E0` and shown from the
  primary surface at `0xABC1DC`.
- The proxy's Direct3D 9 device (only three `DrawPrimitiveUP` calls seen) is
  not the device the game draws the field with. That device still has to be
  identified (its vtable).

## Options considered

1. **Preferred:** intercept the packet build (`sub_65F070` and the model
   producers) and apply a circular transparency mask in the draw pass, centred on
   the leader's projected position, with a soft edge.
   - Risks: a whole face fades; Z order; overlapping faces stack their opacity;
     trees drawn as models may escape; floors may be taken for obstacles.
2. **Rejected:** draw the leader again on top. It produces a ghost of the
   leader, not transparency, and the wall stays opaque.

## Next steps

- Identify the active draw device and its vtable.
- Find how walls and trees are marked in the packets.
- Build a test scene: a wall between the leader and the camera, a tree beside the
  leader, two overlapping faces, an NPC next to the leader, a shadow, a cutscene,
  the world map and a battle.

## Confidence

Medium. The packet and draw path is well supported. The device and the object
classification are not.

## Near-plane test (experiment, F2)

F2 turns on a test that sets the camera near plane (`0xBAD7C0`) to 0x100 every field
frame. The test is in `src/d3d9/near_test.h`. If the wall stays visible when the camera
turns with the test on, the wall's disappearance comes from the near-plane cut.

**Result of the near-plane test (14/11):** with the near plane at 0x100 (log shows
256), the wall that disappears when the camera turns did not change on screen. So
the near-plane cut is not the cause of this disappearance. The test is kept as a
negative result. The other explanations in the report (back-face culling, a
per-object cull) are still open.
