# Brazilian Portuguese (PT-BR) translation

This page covers the PT-BR translation of Breath of Fire IV: who made it, what
state it is in, what needs work, and how to help. It is a starting point for the
next round of improvements.

> This repository does **not** include the translation files or the patcher.
> Get them from the authors' pages listed below and follow their installation
> steps. This project only documents them.

## Credits

| Who | What they did | Where |
|---|---|---|
| **nhojunior and team** | Original PT-BR translation for the PlayStation 1 release (v1.1a, which includes the 1.1 fixes and fixes a freeze in the final battle against Fou-Lu). | [romhackers.org](https://romhackers.org/traducoes/console/playstation/breath-of-fire-4-nhojunior/) |
| **Mailson Maia Alves** | Ported the translation to the PC release: reverse engineering of the PC DAT and PS1 EMI containers, string table adaptation, injection into the item catalog (`INIT.DAT`), the multiplatform GUI patcher and the Save Editor. | [Nexus Mods page](https://www.nexusmods.com/breathoffire4/mods/10) |
| **Capcom** | Breath of Fire IV. | |
| **GOG** | The PC release this translation targets. | |

Thank you to both teams. The PS1 translation is the base of everything here.

## Status of the translation

These notes come from the review of the PS1 translation (v1.1a) on romhackers.org.
The PC port brings the same text, so the same points apply until someone checks it.

**Good:**
- Correct use of accents, and most of the text is translated.
- Good quality overall.

**Needs work:**
- Some terms are left in English, for example "HP".
- Some places are not translated.
- The graphics of the scenes between chapters are not translated.
- Some punctuation and Portuguese grammar errors, and some formal forms used
  wrongly.
- A few awkward translations.
- Minor pointer problems.
- The text was **not fully proofread** by its author.
- Reports of problems near the end of the game.

## How to help

Each of these is a useful contribution:

1. **Proofreading.** Play through the game with the PT-BR text and note each
   error (spelling, punctuation, grammar, awkward phrases). Record the map and
   the message so it can be found again.
2. **English leftovers.** Find every place that still shows English and list it.
   Check especially terms like "HP" and item or spell names.
3. **Chapter scene graphics.** Identify the images that show text between
   chapters and decide how to translate or replace them.
4. **Late-game checks.** Play the final part and report any freeze, clipped
   text, or wrong line.
5. **Pointer and layout checks.** Check that translated lines fit their boxes
   (see the text box sizes in [localization](../localization.md)).

When you report something, include: the map or scene, the English original, the
PT-BR text you saw, and a screenshot if possible.

## For translators: how the game text is organized

These points come from the PC port work and from the notes of the bof4 community.
Details on the PC files are still being documented.

- **Dialogue and map text** is stored in message banks inside the game's data
  files (`DAT` on PC, `EMI` containers on PS1). Each message has an id.
- **Item, spell and skill names** are in a catalog file (`INIT.DAT` on PC).
  Changing a name means changing its entry there.
- **Pointer tables** tell the game where each message starts. If a translated
  message is longer or shorter, its pointer must be updated, or the text is
  clipped or the script freezes. This is the most common source of bugs.
- **Text boxes have fixed sizes.** A single-line box is 20 pixels high, a
  double-line box is 34 pixels high (see [localization](../localization.md)).
  Keep each line within its box.
- **Text codes** (colours, choices, names, line breaks) must be kept as they
  are. Only the visible words change.

For how the recomp handles game text, see the "Game text" section of
[localization](../localization.md).

## Installing the translation (for players)

Use the author's instructions, not ours. Summary, from the Nexus Mods page:

1. Download the patcher from the Nexus Mods page.
2. Run it and select your game folder (the one with `BOF4.exe`).
3. Install. The patcher backs up the original English `DAT` folder first.
4. To go back to English, use the patcher's restore button, or replace the
   `DAT` folder with the backup, or use the game's "Verify integrity" option.

The patcher and the translation files belong to their authors. Do not copy them
into this repository. Use the links.

## Notes for contributors

- Keep English and PT-BR text in separate tables, so the PT-BR can be rebuilt.
- Record every change with the map or item it affects.
- Test on a copy of the game folder, never on the only copy of your saves.
- Credit the authors of any text you reuse.
