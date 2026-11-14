# Autosave to slot 2

**Status:** Partial. Tested in game: it writes on each room change, and the backups rotate. Accepted as it is.

## What it does

Each time the leader enters a field map (a town, a dungeon floor, a door into
another room), the game saves to slot 2 (`SAV\02.DAT`), as the recomp does.

- The save is the game's own, the same one the camp menu's Save writes.
- Before each autosave, the current `02.DAT` is copied to backups. Only the
  last three are kept:
  - `SAV\02_bak1.DAT`, the file before the last autosave;
  - `SAV\02_bak2.DAT`, the one before that;
  - `SAV\02_bak3.DAT`, the oldest one. Older copies are deleted.
- The first autosave makes no backup, because there is no `02.DAT` to copy yet
  (or there is one and it is copied as `02_bak1.DAT`).
- Whatever was in slot 2 is replaced. Keep your own saves in other slots.
- F4: turns autosave on or off. Default: on. A status line shows
  `Autosave slot 2: ON` or `OFF` for 1.5 s.

## How it works

Code is in `src/d3d9/autosave_slot2.h`.

1. The map load in field mode is `sub_676CF0`, called from `0x521562`. That call
   is redirected to `as_stub`.
2. The stub runs the original map load, then (if enabled) calls `as_autosave`.
3. `as_autosave` backs up `02.DAT` with the rotation above, then calls the game's
   save writer `sub_52A5E0` with a record in a static buffer. The record's slot
   byte (`+0x16`) is 1, which the writer turns into `02`. The writer builds the
   image with `sub_52A800`, writes the file with `sub_52A760`, and sets a status
   byte on the record.
4. All registers are kept around the map load and the save, so the field code
   after the call sees the same state.

The install checks the module base, the call site (`E8` to `0x676CF0`), and
that the map load starts with `push ecx` and the writer starts with `push esi`.
Nothing is patched otherwise.

## Call site

| Address | Original call | Patched to |
|---|---|---|
| `0x521562` | `sub_676CF0` in the field map-load case | `as_stub` |

## Tested

- Entered and left several rooms in Breath of Fire IV - Traduzido. The log showed six
  `autosave slot 2: written` lines, and `02.DAT` and the three backups are 6272 bytes.
- Every room change saves, including leaving a room, not only entering one. Kept as it is.
- F4 toggles the feature, and the log records both states.

## Open issues

- Loading a save also goes through a map change. Check whether a load from slot
  2 writes the same state back to slot 2 (it should only copy the same data, but
  confirm).
- The save menu's record table is updated by the writer, as it is for a normal
  save. Check that the menu still shows the right save info after an autosave.
- Still to test: a load from slot 2 (see above), and the save menu after an autosave.
