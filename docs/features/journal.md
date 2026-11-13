# Journal (Diário)

**Status:** Partial. Works for the first chapters only, and the text is provisional.

## What it does

Shows the current objective in a page on the right of the screen, like the
recomp's quest tracker. The objective changes as the story progresses.

- J: shows or hides the journal. Opening it closes the masters panel.
- Q / E: scroll up and down.

## How it works

The objective is chosen from the story progress words in the game's memory.
Each objective is linked to one bit in the progress word of its area, and the
objective shown is the one after the last bit that is set.

Currently 11 objectives are written, for the first part of the story (the
cliffs to Sarai and the Sandflier Valley). Each line is wrapped at spaces.

Chapters without objectives show a general line per story area. These lines
are **provisional**: they were written from which maps use each area's flags
and a sample of the text, not from playing.

To refine them, the proxy writes `bof4_flaglog.txt` in the game folder: each
time the story progress changes, it records the map and the leader's cell.

## Open issues

- The recomp opens the tracker from the field's Menu button. On GOG it is J.
- Extend the objective list beyond the first chapters, and replace the
  provisional text after playing each chapter.
- Confirm the progress-word address for the PC build before relying on it. The
  constant in the source looks like a PS1 address.
