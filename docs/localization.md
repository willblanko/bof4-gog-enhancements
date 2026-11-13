# Localization

This page explains what text the project shows, where each piece lives in the
code, how to translate it, and what the game's own text needs. It is for people
who want to make a Portuguese (PT-BR) or other-language version.

There are two different kinds of text, and they are handled differently:

| Kind | Examples | Where it lives | Status |
|---|---|---|---|
| **Interface and overlay text** | status lines, masters panel, journal, subtitles of the intro movie | `src/d3d9/d3d9_proxy.c` (source strings) | Done in English; PT-BR table below |
| **Game text** | dialogue, menus, item names, the story | The game's own data files | Not started on GOG. See [game text](#game-text) |

## 1. Interface and overlay text

All strings below are C string literals in `src/d3d9/d3d9_proxy.c`. Find them
by symbol name, since line numbers change:

| Symbol or function | What it is |
|---|---|
| `SUBS[]` | Intro movie subtitles: `{start_ms, end_ms, text}`. Timings are in milliseconds from the start of the movie. |
| `make_sub_tex`, `sub_paint` | Draw subtitles and the status line into one texture. |
| `status_show(...)` calls | Status line messages (F6 and F8 toggles). |
| `COUNTER_NAME[]` | Labels of the counters on the masters panel. |
| Masters panel draw code | Page header, column titles, status words, level-up bonus title. Look for `txt(` calls. |
| `OBJECTIVES[]` | Journal objectives, linked to a story progress bit. |
| `AREA_TEXT[]` | One general line per story area, used where no objective exists. These are provisional. |
| Journal draw code (`paint_journal`) | Page title, "Now", "Done", and the key help line. |

### Rules when you translate

- **Keep the same number of entries** in `SUBS[]`, `OBJECTIVES[]` and `AREA_TEXT[]`. Only the text changes; the timings and bits do not.
- **Keep the labels short.** The panel and the journal have fixed widths. A long label can overlap the next column.
- **Accents: use Windows-1252 escape codes**, as the subtitles already do.
  Non-ASCII letters must be written as octal escapes of their Windows-1252 byte. Common ones:

  | Letter | Code | Letter | Code |
  |---|---|---|---|
  | á | `\341` | Á | `\301` |
  | à | `\340` | À | `\300` |
  | ã | `\343` | Ã | `\303` |
  | â | `\342` | Â | `\302` |
  | é | `\351` | É | `\311` |
  | ê | `\352` | Ê | `\312` |
  | í | `\355` | Í | `\315` |
  | ó | `\363` | Ó | `\323` |
  | ô | `\364` | Ô | `\324` |
  | õ | `\365` | Õ | `\325` |
  | ú | `\372` | Ú | `\332` |
  | ç | `\347` | Ç | `\307` |

  Example: `"Nina: Eu acreditei em voc\352..."` shows "Nina: Eu acreditei em você...".
- **Subtitle timing is not text.** Do not change `start`/`end` to fit a longer
  line; shorten the line instead, or split it into two cues with new times.
- **Test in the game.** Each label is drawn at a fixed position. Check every
  masters page, the journal with a long objective, and the intro movie.

### Reference: the Portuguese (PT-BR) text

This is the PT-BR version of each string, so someone can rebuild it. Left
column is the current English text; right column is the original PT-BR.

| Where | English (current) | PT-BR (original) |
|---|---|---|
| Subtitle | Nina: I believed in you... | Nina: Eu acreditei em você... |
| Subtitle | Nina: Right from the start. | Nina: Desde o começo. |
| Subtitle | Ryu: What... | Ryu: O que... |
| Subtitle | Ryu: What are you trying to do? | Ryu: O que... você vai fazer agora? |
| Subtitle | Ryu: Where do you think you're going? | Ryu: Para onde o seu caminho vai te levar? |
| Subtitle | The one who never changes... | Aquele que nunca muda... |
| Subtitle | Ryu: So you're saying people are so corrupt they can never change? | Ryu: Quer dizer que as pessoas são tão corrompidas que nunca podem mudar? |
| Subtitle | Ryu: So that's what you believe?! | Ryu: É isso que você acha?! |
| Subtitle | Ryu: You're wrong! | Ryu: Você está errado! |
| Subtitle | Ryu: People... | Ryu: Pessoas... |
| Subtitle | Ryu: People are not so corrupt... | Ryu: As pessoas não são tão corrompidas... |
| Subtitle | Nina: Who... | Nina: Quem... |
| Subtitle | Nina: Who... are you? | Nina: Quem... é você? |
| Status | Analog stick walking: ON / OFF | Caminhada pelo analogico: LIGADA / DESLIGADA |
| Status | Random battles: ON / OFF | Batalhas aleatorias: LIGADAS / DESLIGADAS |
| Masters | Masters  page %u/2 | Mestres  pag %u/2 |
| Masters | No save loaded | Sem save carregado |
| Masters | Not known yet. | Ainda nao conhecido. |
| Masters | Apprentice | Aprendiz |
| Masters | none | nenhum |
| Masters | Count | Conta |
| Masters | Has | Tem |
| Masters | Needs | Precisa |
| Masters | Reward | Recompensa |
| Masters | Status | Estado |
| Masters | Earned | Obtida |
| Masters | ready | pronta |
| Masters | done | cumprida |
| Masters | Level-up bonus | Bonus por nivel |
| Masters | RAM not detected | RAM nao detectada |
| Masters | F12 close  F10 page  1-6 | F12 fecha  F10 pagina  1-6 |
| Counters | combo hits | acertos no combo |
| Counters | play time | horas jogadas |
| Counters | battles | batalhas |
| Counters | species | especies |
| Counters | fairies | fadas |
| Counters | fishing points | pontos de pesca |
| Counters | item types | tipos de item |
| Counters | combo damage | dano no combo |
| Counters | Ryu level | nivel do Ryu |
| Counters | ball rank | rank da bola |
| Counters | move damage | dano do golpe |
| Journal | Journal | Diario |
| Journal | Now | Agora |
| Journal | No objective noted yet. | Nenhum objetivo anotado ainda. |
| Journal | Done | Feito |
| Journal | Areas visited | Regioes ja visitadas |
| Journal | Q/E scroll  J close | Q/E rola  J fecha |
| Objective | Follow the cliffs on the way to Sarai. | Siga pelos penhascos a caminho de Sarai. |
| Objective | Jump over the gaps on the cliff road. | Pule as falhas da estrada dos penhascos. |
| Objective | Keep climbing while there is light. | Continue subindo enquanto houver luz. |
| Objective | Find a place to camp for the night. | Ache um lugar para acampar a noite. |
| Objective | Fou-Lu: Leave the tomb. | Fou-Lu: Saia da tumba. |
| Objective | Fou-Lu: Go to the capital. | Fou-Lu: Va para a capital. |
| Objective | Fou-Lu: Escape the burning forest. | Fou-Lu: Fuja da floresta em chamas. |
| Objective | Climb the cliffs to the city of Sarai. | Suba os penhascos ate a cidade de Sarai. |
| Objective | Ask in Sarai about sandflier parts. Try the tavern. | Pergunte em Sarai por pecas do sandflier. Tente a taverna. |
| Objective | Get a shovel and the dog in the Sandflier Valley, south of the city. | Pegue uma pa e o cao no Vale do Sandflier, ao sul da cidade. |
| Objective | Dig behind the part in the Sandflier Valley. | Cave atras da peca no Vale do Sandflier. |

The area lines (`AREA_TEXT`) in PT-BR are in the first commit of this repository,
in `src/d3d9/d3d9_proxy.c`. They are short and provisional, so review them when you play each chapter.

### Where the PT-BR of the recomp came from

The local PT-BR build of the recomp (not published) changes the same kind of
strings directly in its Odin sources, for example `port/src/qol/subtitles.odin`,
`tracker.odin`, `masters.odin`, `options.odin`, `teacher.odin` and
`camp_menu.odin`. The official recomp uses English there. Compare with
`github.com/plaught-armor/bof4-recomp` when you update a translation.

## 2. Game text

The game's own text (dialogue, menus, names, the story) is **not** in this repository and
is **not** part of the GOG overlay work yet. It is the next project. What the
official recomp does with it is the reference:

- **Text is stored in message banks** on the disc. The recomp finds them with
  its own tools (`bof4tools disc-extract`, then the message table format
  documented in its `docs/text-and-ui.md`).
- **Japanese text is kept as codes.** The bank holds byte codes for hiragana,
  katakana, a few kanji, colours and choices. The recomp's table of codes is in
  `docs/text-and-ui.md`.
- **English replacement by message id.** `port/src/qol/village.odin` replaces one
  map's message bank with its own English translation, id by id, keeping the
  text codes. It checks the bank's FNV-1a hash first, so the replacement only
  applies to the exact disc file it was written for.
- **External patches are not shipped.** The recomp's Dengeki store uses a fan
  translation patch (`dengeki.odin`). The project does not include it; the
  user builds the files from their own copy of the patch and disc.

To start a PT-BR translation of the game text on GOG, you need:

1. The GOG data files, and a way to read the message banks from them (not
   documented here yet).
2. The code table for the banks (see the recomp's `docs/text-and-ui.md`).
3. A way to write replacement banks, checked by hash like the recomp does.

Document each step here when you do it.

## 3. Checklist for a new language

1. Copy the English strings from section 1 into a new table (keep the same order).
2. Translate, using the accent codes in section 1.
3. Check the lengths against the fixed label widths.
4. Rebuild `d3d9.dll` with `src/d3d9/build.bat`.
5. Copy it to the game folder and test: intro movie, masters panel on every
   page, the journal with the longest objective, status lines (F6, F8).
6. Open a pull request with screenshots of each place you tested.
