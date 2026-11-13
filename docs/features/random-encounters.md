# Random encounters on/off

**Status:** Done (on/off). The recomp's 0 to 16x rate is not implemented.

## What it does

Turns random battles on the field off, so the player can walk without them.
Story battles, scripted battles and boss fights are not random encounters and
are not affected by this switch. Testing them with the switch off is still open.

- F8: turns random battles on or off. Default: on (the game's normal behaviour).
- A status line shows `Random battles: ON` or `Random battles: OFF` for 1.5 s.

## How it works

The game checks for a random encounter at each step with `sub_637A20`
(`0x637A20`). Three call sites are patched (`enc_off.h`):

| Address | Role |
|---|---|
| `0x6341DB` | Call to `sub_637A20` (site A in `enc_off.h`) |
| `0x64201B` | Call to `sub_637A20` (site B) |
| `0x43E6AC` | Call to `sub_637A20` (site C) |

Each site is changed to call a stub. The stub returns `0` (no encounter) when
the switch is off, and otherwise jumps to the original `sub_637A20`. The site
is patched only after the target address is confirmed.

When the check returns `0` early, the game also skips the step budget
(`0xBB65A0`) and the encounter flags (`0xBAB54E`, `0xBAB54F`). With the switch off, the step counter and those flags
are not updated, so keep this in mind when testing features that depend on them.

## Notes for contributors

- Script battles do not go through `sub_637A20`; they use `sub_4D6EE0` and
  `sub_4D6FA0`. Turning off random battles does not touch them.
- Test with the switch off: an ordinary field walk, a mandatory story battle
  and a boss, then turn the switch back on and confirm encounters return.
