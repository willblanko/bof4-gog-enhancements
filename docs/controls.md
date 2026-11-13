# Controls

Keys added by this project. The game's own controls are unchanged.

## Keyboard

| Key | Feature | What it does |
|---|---|---|
| F6 | [Walk stick](features/walk-stick.md) | Turns analog walking on or off (on by default). |
| F7 | [Camera](features/camera.md) | Turns camera follow on or off. |
| F8 | [Random encounters](features/random-encounters.md) | Turns random battles on or off (on by default). Shows a status line. |
| F10 | [Masters panel](features/masters-panel.md) | Switches between master pages. |
| F11 | Diagnostics | Starts or stops the draw-call capture (research only). |
| F12 | [Masters panel](features/masters-panel.md) | Shows or hides the masters panel. |
| J | [Journal](features/journal.md) | Shows or hides the journal. |
| K | [Vignette](features/vignette.md) | Turns the vignette on or off. |
| L | [Subtitles](features/subtitles.md) | Turns the intro movie subtitles on or off. |
| M | [Minimap](features/minimap.md) | Turns the minimap on or off (on by default). |
| 1 to 6 | Masters panel | Chooses a master on the current page. |
| Q / E | Masters panel, journal | Moves the selection or scrolls up and down. |
| R / T | Masters panel | Previous and next page. |
| Arrow keys | Masters panel | Move the selection. The character may still walk, because the game also reads them. |

**Do not use F9.** It is the game's "Exit Game" key.

## Gamepad

| Input | Feature | What it does |
|---|---|---|
| Left stick | [Walk stick](features/walk-stick.md) | Walks at the exact stick angle. |
| Right stick | [Camera](features/camera.md) | Turns the camera. |
| LT / RT and mouse wheel | [Camera](features/camera.md) | Zoom in and out. |
| LB / RB | Game | No longer turn the camera. |

## Changing a key

Keys are read with `GetAsyncKeyState` once per frame in
`d3d9_proxy.c` (see `masters_frame` and the `Present` hook). To change a key,
change its `VK_` code there, rebuild, and update this page.
