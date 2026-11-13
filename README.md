# Breath of Fire IV — GOG PC Enhancements

Bring the quality-of-life features of the native **BOF4 recomp** port to the
original **Breath of Fire IV PC (GOG)** release, without replacing the game
or shipping any of its files.

The recomp is a recompiled, native port of the PS1 game. It adds a third-person
field camera, analog walking, faster battles, a minimap, a journal, and more.
This project works on the GOG build, which runs the original code, and adds the
same kinds of features by hooking the game at run time.

> **You need your own copy of the game.** This repository contains no game
> files, no assets, and no executables from the game.

## Goals

- Port the recomp features that make the field and menus more comfortable to
  the GOG build.
- Keep every change small, reversible, and checked before it is written to
  game memory.
- Document the game's internals (functions, addresses, data layout) so other
  people can keep contributing.

## How it works

The GOG build is loaded through a DirectX wrapper. This project adds two DLLs
to the game folder:

| File | Role |
|---|---|
| `d3d9.dll` | Proxy for Direct3D 9. Forwards to the real system DLL, hooks `Present`, draws overlays, and patches the game's code at fixed call sites. |
| `ddraw.dll` | Pass-through DirectDraw wrapper (no hooks). Used to keep the wrapper stable. |

See [architecture](docs/architecture.md) for details.

## Feature status

Status against the recomp feature list (`docs/port-changes.md` of the recomp).
Full table: [roadmap](docs/roadmap.md).

| Feature | Status |
|---|---|
| Analog stick walking, any angle | Partial (grid steps, see [walk stick](docs/features/walk-stick.md)) |
| Random encounters on/off | Done ([random encounters](docs/features/random-encounters.md)) |
| Minimap | Done ([minimap](docs/features/minimap.md)) |
| Save anywhere | Done ([save anywhere](docs/features/save-anywhere.md)) |
| Masters panel | Done ([masters panel](docs/features/masters-panel.md)) |
| Journal (objectives) | Partial ([journal](docs/features/journal.md)) |
| Subtitles for the intro movie | Done ([subtitles](docs/features/subtitles.md)) |
| Third-person camera (zoom, follow, analog turn) | Partial ([camera](docs/features/camera.md)) |
| Fullscreen 1920×1080 | Done ([fullscreen](docs/features/fullscreen.md)) |
| Vignette | Done ([vignette](docs/features/vignette.md)) |
| Wall transparency (leader behind walls) | Research ([wall transparency](docs/features/wall-transparency.md)) |
| Quick save / quick load | Research ([quick save](docs/features/quick-save.md)) |
| Instant doors, fades, spring-arm camera, autosave, sky backdrop, unified options | Not started |

## Source\n\n- `src/d3d9/`: the Direct3D 9 proxy (`d3d9_proxy.c` and headers). Build with `build.bat`.\n- `src/ddraw/`: the pass-through DirectDraw wrapper. Build with `build.bat`.\n\nThe portrait data for the masters panel is not included (see [masters panel](docs/features/masters-panel.md)).\n\n## Documentation

- [Architecture](docs/architecture.md): proxies, hooks, call-site patching, logs.
- [Building](docs/building.md): toolchain, build, deploy, test.
- [Controls](docs/controls.md): every key and button added by this project.
- [Roadmap](docs/roadmap.md): recomp parity table and next steps.
- [Features](docs/features/README.md): one page per feature.
- [Memory map](docs/reference/memory-map.md): game addresses used by the hooks.

## Credits

- **Recomp:** this project exists because of the native BOF4 recomp by
  [plaught-armor](https://github.com/plaught-armor/bof4-recomp). Its feature list
  (`docs/port-changes.md`) is the reference for every feature here, and its
  source and documentation were used to understand the game's behaviour. Thank
  you to its authors.
- **Breath of Fire IV** is by Capcom. This project is an unofficial community
  effort and is not affiliated with Capcom or the recomp project.

### Tools used in the research

- **IDA Pro** (with the IDA MCP plugin): disassembly, decompilation and
  cross-references of `BOF4.exe` (GOG 1.0). All addresses in the documentation
  come from IDA analysis.
- **Microsoft Visual Studio 2022 Build Tools** (`cl` and `link`, x86): builds the
  proxy DLLs.
- **Codex** (OpenAI): static investigations and written reports on the field
  movement, random encounters, wall transparency and quick save.
- **Claude Code** (Anthropic): code changes, build and test support, and the
  documentation in this repository.
- **Git and GitHub CLI (`gh`)**: version control and publishing.
- **The recomp's source** (Odin, `port/src/qol/`): reference for the walk,
  camera, save and journal features.
## License

This project is released under the [MIT License](LICENSE). It does not include any Capcom or recomp code or game assets.
