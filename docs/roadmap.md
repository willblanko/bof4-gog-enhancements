# Roadmap and recomp parity

Status of each recomp feature (`docs/port-changes.md` of the recomp) on the GOG
build.

- **Yes**: works on GOG and was tested in game.
- **Partial**: a reduced form exists.
- **No**: not implemented; the GOG build runs the original code for this.
- **Not verified**: no evidence yet; needs in-game testing.

## Display and graphics

| Recomp feature | GOG | Notes |
|---|---|---|
| Fullscreen toggle | Partial | Fullscreen through `dxcfg.ini` (`presentation=fullscreen`). |
| Fit or integer scaling, nearest/linear filter | Partial | `dxcfg.ini`: `scaling=fit`, `aspect` on. No in-game choice. |
| Pause when unfocused | Not verified | |
| High-resolution drawing (1, 2, 4x) | No | |
| Smooth motion (in-between frames) | No | |
| Smoothing of effects and textures | Not verified | |
| HD-2D extras (bloom, depth of field, colour grade, fog) | Partial | Vignette only. |
| Sky backdrop | No | |
| Depth buffer under the third-person camera | No | Linked to [wall transparency](features/wall-transparency.md). |
| See-through leader (only the leader's silhouette fades) | No | Linked to [wall transparency](features/wall-transparency.md). |
| Longer, eased screen fades | No | |
| Screen shake off | Not verified | |

## Field

| Recomp feature | GOG | Notes |
|---|---|---|
| Third-person camera (follow, free turn, zoom) | Partial | Zoom, analog turn, follow. See [camera](features/camera.md). |
| Spring-arm camera | No | |
| Smooth camera between positions (sub-pixel view) | No | |
| L1/R1 turn speed and hold-to-spin | Not verified | LB/RB no longer turn the camera. |
| Walk at any angle | Partial | Exact angle, no wait. Still grid steps. See [walk stick](features/walk-stick.md). |
| Continuous movement (no grid) | No | Option B in [walk stick](features/walk-stick.md). |
| Native field engine and map editor | No | Port-only. |
| Seamless sections (rooms and corridors join) | No | |
| Instant doors | No | |
| Random encounter rate 0 to 16x | Partial | On or off only. See [random encounters](features/random-encounters.md). |
| Minimap | Yes | M key. See [minimap](features/minimap.md). |
| Quest tracker on the Menu button | Partial | Journal on J, first chapters only, provisional text. See [journal](features/journal.md). |
| Camp menu opens on Start | Not verified | |
| Dialogue portraits in third person | Not verified | |

## Text and menus

| Recomp feature | GOG | Notes |
|---|---|---|
| Instant message pages or hold to reveal | Not verified | |
| Intro movie subtitles | Yes | L key. See [subtitles](features/subtitles.md). |
| Gear descriptions with hidden effects | No | |
| Save anywhere | Yes | See [save anywhere](features/save-anywhere.md). |
| Autosave on entering each field map (slot 2) | No | |
| Unified options menu (Setting > Extras) | No | |
| Masters panel | Yes | F12. See [masters panel](features/masters-panel.md). |
| Button prompts follow the device | Not verified | |
| Full key and controller rebinding | No | |
| Mouse menus | No | |

## Battle

All battle features are **No**: speed (1 to 8x), faster turns and victory,
damage numbers over combatants, skill cinematic skip, enemy-skill learning,
guard pays, Identify restored, combos, tuning, enemy AI tiers, timed attacks,
Rebalanced mode, KO max-HP loss.

## Minigames, disc and audio

| Recomp feature | GOG | Notes |
|---|---|---|
| Slow motion in fishing, Sandflier hunt, Kecak | No | |
| Audio latency setting | No | |
| Instant loading | Not verified | Not the same mechanism on GOG. |
| Skip black screens | Not verified | |
| Movies and streamed audio at normal pace | Not verified | |

## Next steps

1. Test [walk stick](features/walk-stick.md) and [random encounters](features/random-encounters.md) in every situation in the test checklist.
2. Port the [journal](features/journal.md) beyond the first chapters, and add the menu entry.
3. Investigate the display device for [wall transparency](features/wall-transparency.md).
4. Decide whether [quick save](features/quick-save.md) is worth the risk.
5. Port the spring-arm camera and smooth camera ([camera](features/camera.md)).
6. Port instant doors and longer fades.
7. Port autosave to slot 2 on each field map.
