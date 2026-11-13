# Wall transparency (research)

**Status:** Research. Not implemented.

## Goal

When the leader is behind a wall, a tree or other scenery, make the obstacle
transparent around the character, as the recomp does with the leader's
silhouette.

## Findings

- The field is not rasterized by the CPU. The game builds packets of polygons
  and sprites, sorts them into 1024 buckets, and sends textured vertices to
  `IDirect3DDevice3::DrawPrimitive` (the legacy DirectDraw interface).
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
