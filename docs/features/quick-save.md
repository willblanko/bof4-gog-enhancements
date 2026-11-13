# Quick save and quick load (research)

**Status:** Research. Not implemented, not tested.

## Goal

Save and load without the camp menu, from the field, with one key.

## What the game does now

- Save and load go through `0x529D60` (save or load dispatch), which calls
  `0x52A5E0` (write) or `0x52A560` (read).
- The save file is `SAV\NN.DAT`, 0x1880 bytes. The image is built by `0x52A800`,
  and a checksum (XOR of the first 6223 bytes) is stored at offset `0x184F`.
- The save screen lists 15 slots (`01` to `15`).
- Resume from the title goes through `0x66E1E0`, then `0x5222D0`, which runs the
  state table of the field.

## Findings

- **Quick save:** possible in principle, medium confidence. Writing a 16th file
  (`16.DAT`) needs a specific bridge, because the screen only knows 15 slots and
  the summary table and icon use the same index.
- **Quick load:** calling the read routine on its own on the field is **not
  safe** (high confidence). The loader resets the map, objects, camera and
  scene. The load must go through the title's state machine.
- **Full memory snapshot:** not practical as a simple extension. The state
  includes heap objects, threads, device handles and graphics resources.

## Conservative rule

Allow a quick save only when the leader has control on a normal field map, with
no menu, battle, event or transition, for a stable number of frames. The recomp
uses the same idea for its autosave (30 frames of control).

## Next steps

- Confirm the rules in game with a test copy of the saves.
- Decide whether it is worth the risk, given the existing save-anywhere feature.
