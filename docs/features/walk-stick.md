# Analog stick walking

**Status:** Partial. Walking follows the stick's direction, but the character
still moves one grid cell at a time.

## What it does

The original game walks in eight directions, chosen by the d-pad. With this
feature, the left stick picks the direction at any angle: the grid direction
nearest to the stick's angle plus the camera's yaw.

Option A (done) also removes the three-frame wait before the first step, so the
character starts moving as soon as the stick is tilted.

## Controls

- Left stick: walk.
- F6: turn the feature on or off (on by default). A status line shows the state.

## How it works

Constants and code are in `src/d3d9/walk_stick.h`.

1. Each frame, `walk_frame` reads the left stick through XInput, applies a
   deadzone, and computes `theta = atan2(x, y)` in degrees.
2. It reads the camera yaw (`0xBAD7C6`, where `0x1000` is a full turn):
   `world = round(theta/45 + yaw/0x200) & 7` and
   `screen = (world - ((yaw + 0x100) >> 9)) & 7`.
3. The game's own screen-direction function (`sub_6397E0`) is called at two sites
   (the leader's first step, and following steps). For the leader with the
   stick tilted, the stub returns `screen` instead.
4. After the game copies the pad (`sub_635E70`, called from `sub_635AF0`), the
   stub sets the matching d-pad nibble in the leader's held pad when no d-pad
   button is held.
5. Option A: while the leader is turning (substate `obj+5 == 1`) with a turn
   counter (`obj+8`) above zero, the counter is set to zero. This matches the
   "instant start" of the recomp (`movement.odin`).

Only the field (game mode 2) is affected. Menus, battles and the world map keep
the original input.

## Call sites

| Address | Original call | Patched to |
|---|---|---|
| `0x63333A` | `sub_6397E0` in `sub_633230` (leader's first step) | `wk_stub_screen` |
| `0x633A51` | `sub_6397E0` in `sub_633970` (following steps) | `wk_stub_screen` |
| `0x635B56` | `sub_635E70` in `sub_635AF0` (pad copy) | `wk_stub_pad` |

## Open issues

- Movement is still in cells (8 frames walking, 4 running). The recomp moves
  continuously.
- Continuous movement (option B) needs a new step state (`sub_634000`),
  collision (`sub_636A80`), height (`sub_6645C0`), NPC boxes and door
  handling. It is not a small patch. The static analysis report for it is
  still to be added to this repository.
- Still to test: diagonal feel, stairs, camera turned, battle, menus and world map.
