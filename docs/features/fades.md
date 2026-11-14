# Fades: smoother screen fades and faster door fades

**Status:** Partial. Fades are gentler and door fades are faster. The map load
from disc still happens, so doors are quicker, not instant. Not tested in game
yet.

## What it does

- **Normal fades** (to black and back, for battles, menus and map changes) are
  limited to a speed of 0x800 per frame. The game's fastest fades run at 0x2000,
  so they now take about four times as long, and the screen darkens and clears
  more gently.
- **Door fades** (the leader goes through a door to another map) are raised to
  at least 0x2000 per frame: the fastest fade the game has.
- F5: turns the feature on or off. A status line shows `Smooth fades: ON` or
  `Smooth fades: OFF` for 1.5 s. Default: on. Off restores the game's own speeds.

## How it works

Code is in `src/d3d9/fade_speed.h`.

1. The game's fade loop, `sub_5202D0`, calls the fade step `sub_520360` once a
   frame with the fade speed as a 16-bit argument. That call is at `0x5202FD`.
2. Only that one call is redirected to `fs_stub`. The stub reads the speed from
   the stack and changes it in place before tail-jumping to `sub_520360`, so the
   loop and the caller are unchanged.
3. When the transition submode byte `0xBAB428` is 4, the stub is in a door
   transition (the door code at `0x676CE6` sets it, and it is still 4 during the
   fade in). Then the speed is raised to at least 0x2000 in magnitude.
   Otherwise the speed is capped to 0x800 in magnitude. The sign is kept, so the
   fade direction is the same.

The install checks the module base and the call site (`E8` with target
`0x520360`, and `push esi` at the start of the step) before writing anything.

## Call site

| Address | Original call | Patched to |
|---|---|---|
| `0x5202FD` | `sub_520360` in `sub_5202D0` (fade loop) | `fs_stub` |

Other fade paths that call `sub_520360` directly (`0x520546`, `0x520CFB`) are
left alone.

## Open issues

- Doors are still held by the disc load. A true instant door needs the map and
  world state to be kept between visits, which is a larger change than this
  patch.
- Battle transitions also use normal fades, so they are gentler too. Check that
  the battle start still reads well.
- Still to test: a town door in and out, a field door with a long load, a battle
  start, the camp menu, and F5 toggling mid-fade.
