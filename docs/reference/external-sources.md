# External sources and credits

This project was built on other people's work. This page lists each source,
its license, what we took from it, and what we checked and did not use.

## Sources

### bof4-notes (Red Herring / glitch-in-the-herring)

- Repository: <https://github.com/glitch-in-the-herring/bof4-notes>
- License: **Creative Commons Attribution-ShareAlike 4.0** (CC BY-SA 4.0)
- Used for: reference facts about the US release. We took **facts only**
  (master order and milestones, the layout of the info box, the button table,
  the file-table format). Every line here is written in our own words. No text
  was copied from the notes. Because the notes are CC BY-SA, copying their text
  would require the copied part to stay under CC BY-SA.
- Caution: the notes describe the US release. Addresses differ on the GOG build.

### Breath of Fire IV recomp (plaught-armor)

- Repository: <https://github.com/plaught-armor/bof4-recomp>
- License: **GNU GPL v3.0**
- Used for: the list of features this project brings to GOG
  (`docs/port-changes.md`), the design of the journal, the masters panel and
  the subtitle timings, and the English subtitle lines (see the licensing note below).

### Better Descriptions (BoF IV mod on Nexus Mods)

- Page: <https://www.nexusmods.com/breathoffire4/mods/3>
- Credits in the mod: Red Herring / Navarchos (reverse engineering notes),
  ZC Liu / Slivers7 (the Enemy and Information Guide on GameFAQs), TheRealBiggs
  (the patching tool).
- **Not used.** We checked the seven DAT files the mod patches (`AB000_00`,
  `CAMP`, `COMMU03`, `MASTER`, `MSHOP`, `SGAMEN`, `SHOP`). Their CRC32 values
  match the original files in the `.bps` patches, so the mod is not applied in
  the game folder we tested. The gear descriptions in the recomp were written
  from its battle code, not from this mod.

### Tools

The notes list the tools their author uses (PCSX-Redux, Ghidra, Armips and
others). For this project we used IDA Pro, Microsoft Visual Studio Build Tools,
Codex, Claude Code, Git and the GitHub CLI. The full list is in the README.

### PT-BR translation (nhojunior and team; PC port by Mailson Maia Alves)

- PS1 translation (v1.1a): <https://romhackers.org/traducoes/console/playstation/breath-of-fire-4-nhojunior/>
- PC port, patcher and Save Editor: <https://www.nexusmods.com/breathoffire4/mods/10>
- Used for: the description of the translation's state and its open issues, the
  credits, and the description of how the game text is organized
  (see [PT-BR translation](../translation/ptbr.md)).
- **Not included** in this repository. The translation files, the patcher and
  the Save Editor belong to their authors.
## Licensing note

- The project is under **MIT** (see `LICENSE`).
- The English subtitle lines in `src/d3d9/d3d9_proxy.c` came from the recomp
  documentation, which is GPL v3. Copying that text into an MIT project is a
  licensing question. Before publishing it widely, either rewrite those lines
  or relicense the project under GPL v3.
- Breath of Fire IV and its art (including the master portraits) belong to
  Capcom. The repository is private for that reason.