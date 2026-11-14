# Memory map (GOG BOF4.exe)

Addresses for the **GOG 1.0 `BOF4.exe`**, image base `0x400000`, found with IDA
analysis. They do not apply to the PS1 build or to other versions. Names are
ours, not the game's.

## Field objects

| Address | Type | Meaning |
|---|---|---|
| `0xB554C0` | array | Field objects, 3 slots, stride `0x10C` (next at `0xB555CC`, `0xB556D8`) |
| `obj+0x01` | u8 | Object id |
| `obj+0x04` | u8 | Main state (1 = player control) |
| `obj+0x05` | u8 | Substate: 0 standing, 1 turning, 2 stepping |
| `obj+0x08` | u8 | Game frames left before the first step (turn counter) |
| `obj+0x0C` | s32 ×3 | X, Y, Z position (16.16 fixed point) |
| `obj+0x4C` | u8 | Facing (world direction) |
| `obj+0xE6` | u8 | Selected move direction |
| `obj+0xF6` | u16 | Held pad for this object |
| `0xBB6598` | u8 | Leader id (controlled object) |
| `0xBAD7C6` | u16 | Camera yaw (`0x1000` = full turn) |
| `0xB5064A` | u16 | Held pad (global) |
| `0xB5069C` | flag | Movie playing |
| `0xBAB408` | u16 | Game mode / field loop state (field = 2) |
| `0xBB65A0` | counter | Random encounter step budget |
| `0xBAB54E`, `0xBAB54F` | flags | Encounter flags |
| `0xBAB428` | u8 | Transition submode; 4 = door transition (set at `0x676CE6`) |
| `0xBAB480` | u16 | Fade active (non-zero while a fade runs) |
| `0xBAB488` | u16 | Fade type index, used by the jump table at `0x520DAA` |

## Functions

| Address | Name used here | Role |
|---|---|---|
| `0x633230` | stand | Leader standing; starts a turn |
| `0x633970` | choose step | Picks a step and checks exits |
| `0x634000` | step | Moves one cell (interpolation over 8 frames walking, 4 running) |
| `0x6342D0` | dispatch | Calls `funcs_6342F4[obj+5]` |
| `0x635AF0` | object update | Per-object update |
| `0x635E70` | pad copy | Copies the pad to `obj+0xF6` |
| `0x6397E0` | screen direction | Pad bits → screen direction 0..7 |
| `0x637A20` | encounter check | Random encounter per step |
| `0x636A80` | passable | Can the leader move from A to B |
| `0x636770` | exit check | Doors and exits |
| `0x66EDC0` | cell attribute | Attribute byte of a cell |
| `0x6645C0` | ground height | Height of the ground at a position |
| `0x66F070`, `0x66F110` | interpolation | Prepare and advance a step |
| `0x520290` | fade start | Sets the fade type and starts the fade task |
| `0x5202D0` | fade loop | Runs the fade step each frame until done |
| `0x520360` | fade step | One frame of a fade; returns 1 when done |
| `0x5203C0` | fade draw | Adds the full-screen darkening primitive |
| `0x65F070` | packet build | Builds field draw packets |
| `0x529D60` | save/load dispatch | Choose read or write |
| `0x52A5E0` | save write | Writes a save file |
| `0x52A560` | save read | Reads a save file |
| `0x52A800` | save image | Builds the 0x1880-byte save image |
| `0x66E1E0` | title resume | Resume from a save at the title |
| `0x5222D0` | field loop | Main loop of the resumed field |

## Map data

| Address | Meaning |
|---|---|
| `0xBAE300` | Current map and section |
| `0xBAE310` | Pointer to the map data block |
| `0xB570C0` | Exit list (12-byte records) |
| `0xB51440` | NPC pool: 32 records of `0xD8` bytes |

## NPC records (from the pool)

| Offset | Meaning |
|---|---|
| `+0x0C..+0x18` | Position and layer |
| `+0xAC` | Target position (used by occupancy checks) |
| `+0xBC`, `+0xBD` | Extra half-width and half-height, in cells |
| `+0xBE` bit 0 | Collision off |

## Save file

| Item | Value |
|---|---|
| Path | `SAV\NN.DAT`, NN = 01 to 15 in the game's menu |
| Size | `0x1880` bytes |
| Checksum | XOR of the first 6223 bytes, stored at `0x184F` |

## Drawing

| Address | Meaning |
|---|---|
| `0xABC1DC` | Primary DirectDraw surface |
| `0xABC1E0` | DirectDraw surface the game draws to |

## Notes

- Several addresses in the proxy source are PS1 values from the recomp. Check
  each one against the GOG executable before using it (for example the
  journal's progress word).
- When a function's name here differs from the game's own, the name is ours.
