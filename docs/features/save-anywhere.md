# Save anywhere

**Status:** Done. Tested in a save in slot 13.

## What it does

The camp menu's **Save** command is available everywhere, not only at camp.

## How it works

`save_anywhere.h` changes the code that decides whether Save is enabled in the
camp menu. The header patches two call sites, checked before any write, the
same way as the other features (see [architecture](../architecture.md)).

## Open issues

- The recomp also autosaves on entering each field map (slot 2). This is not
  on GOG. See [roadmap](../roadmap.md).
- Quick save and quick load (saving without the menu) are a separate
  investigation: [quick save](quick-save.md).
