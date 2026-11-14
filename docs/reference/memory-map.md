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
| `0x521562` | field map load call | Redirected for autosave (see [autosave](../features/autosave-slot2.md)) |
| `0x52A5E0` | save write | Builds and writes the slot; record byte `+0x16` = slot - 1 |
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

## Options (Setting screen)

Confirmed in the GOG save file (`SAV\NN.DAT`) by diffing two saves made with
one option changed. The file offset is `0x180 + (PS1 address - 0x8011A94C)`,
which gives the same offset as the recomp's `OPTION_TEXT_SPEED` (0x8011AD74).

| File offset | RAM (GOG exe) | Option | Values seen |
|---|---|---|---|
| `0x5A8` | `0xB55C28` | Text speed | 1 Normal (was 2 Fast) |
| `0x5AE` | `0xB55C2E` | Vibration | 0 off (was 1 on) |

The writes to `0xB55C28` and `0xB55C2E` are both in `sub_60C310`, which is
the Setting screen's update function (candidate; to confirm).

## Options file (bof4.cfg)

- `bof4.cfg` (256 bytes, game folder) holds the PC options and the controls.
- `sub_60C310` (`0x60C310`) loads it at start (`fopen "rb"`, then `malloc`).
- `sub_60C470` (`0x60C470`) writes it (`fopen "wb"`). It is called from the
  quit path in `sub_60B8E0` (`0x60BD8B`), so the file is written when the game closes.
- The same option pattern as the save appears in the file (`02 00 7F 7F 00 01 01`):
  the options are kept in the cfg and copied into each save.
- Only the loader writes the option bytes directly; the Setting screen must change
  them through a pointer. Finding that pointer is the next step.

## Setting screen (candidates, static)

- `sub_677480` (`0x677480`, 1733 bytes) is a UI task run by the dispatcher
  `sub_68E720`. It calls `sub_527B40`, which calls `sub_527FA0` (1330 bytes,
  jump table) and `sub_527A50`. These read `0xB55C28` on the Setting screen.
  Candidate for the Setting screen; to confirm at run time.
- `sub_523D20` (`0x523D20`) reads and writes the three option dwords. It is
  called from the quit path (`0x60BB44` in `sub_60B8E0`) and from other places.
  Candidate for save/restore of the options.
- `sub_501E10` (`0x501E10`) writes the option dwords and is called from `0x50201A`.

## Options in bof4.cfg (confirmed by diff, 13/11 22:29)

| cfg offset | Option | Values seen |
|---|---|---|
| `0x080` | Text speed | 2 Fast, then 1 Normal |
| `0x081` | Sound | 0 stereo, then 1 mono (inferred from the change) |
| `0x086` | Vibration | 1 on, then 0 off |

The save file copies these at `0x5A8`, `0x5A9` (sound, inferred) and `0x5AE`.

Confirmed live (13/11 22:34): after vibration was set to off in the Setting screen,
the autosave at 22:34:33 wrote `0x5AE = 00`. So the option byte changes at once in
RAM (`0xB55C2E`), and the save copies it. The trace only logs while the screen is open.

## Setting screen: progress (13/11, item 3)

- `sub_527A50` builds one menu-row task record in `byte_B4EF00` (stride `0x50`) and
  copies the text speed (`0xB55C28`) into the record at `+0x4E`. Only this routine
  writes that field, so it is a snapshot for display, not the apply path.
- `sub_670C30` is not the options writer: it works on a different block (`0xB533A8`..).
- Writes to `0xB55C28`..`0xB55C34` as direct globals are only in the loader
  (`sub_60C310`). The in-screen change must go through a pointer that is still not
  identified. Live RAM changes are confirmed (autosave of 22:34:33).

## Setting screen: the option writer (confirmed, item 3)

Found with the Antigravity CLI report (`report_item3.md`) and checked in IDA.

- `sub_501440` (`0x501440`, 820 bytes) is the handler for the option rows
  (`funcs_50201A[1]`). It writes an option when the player presses left or right.
- It reaches each option through a pointer table at `0x9566A0` (8 dwords):

| Row | Pointer | Option | Range (`0x9497A8`) |
|---|---|---|---|
| 0 | `0xB55C2E` | Vibration (0 off, 1 on) | 0..1 |
| 1 | `0xB55C2D` | Unknown (aux 1) | 0..1 |
| 2 | `0xB55C30` | Unknown (aux 2) | 0..1 |
| 3 | `0xB55C2F` | Unknown (aux 3) | 0..2 |
| 4 | `0xB55C28` | Text speed (0 slow, 1 normal, 2 fast) | 0..2 |
| 5 | `0xB55C29` | Sound (0 stereo, 1 mono) | 0..1 |
| 6 | `0xB55C2A` | BGM volume (steps of 4, max 0x7F) | 0..0x7F |
| 7 | `0xB55C2B` | SE volume (steps of 4, max 0x7F) | 0..0x7F |

- The row index is the byte at `0xB5082C`. Right is `inc byte ptr [eax]` at `0x501566`,
  with wrap to 0; left is at `0x501648`.
- `sub_501E10` writes the default values for all options (the "Default" entry).
- `sub_677480`, `sub_527FA0` and `sub_523D20` do not write options (checked by the report).
  The trace hooks on them only read.
- Rows 1 to 3 are not named yet; confirm in game (see the report's section 7).

## Third-person camera (item 4, spring arm)

Report from the opencode Go model (`opencode_item4\report_item4.md`), checked in IDA.

- The game has its own 3D camera. Its state is at `0xBAD7A0..0xBAD880`.
- Camera distance: the offset triple `0xBAD7CC` (lateral), `0xBAD7D0` (vertical),
  `0xBAD7D4` (distance along the view). Reset in `sub_65E5F0` writes `0x1400` to
  `0xBAD7D4` at `0x65E67A`. Each room preset sets it too.
- Yaw: `0xBAD7C6`. Near plane: `0xBAD7C0`.
- The game has no line-of-sight test for the camera. Collision candidates:
  `sub_66EDC0` (cell attribute), `sub_6645C0` (ground height), `sub_636A80` (walkable A to B).
- `sub_6676E0` is the camera script state machine; modes 6 and 7 add the leader's
  offset to the triple every frame.
