# Fullscreen 1920×1080

**Status:** Done, through the DirectX wrapper's configuration file.

## What it does

Runs the game in fullscreen at 1920×1080.

## How it is set

The wrapper reads `dxcfg.ini` in the game folder:

| Key | Value | Meaning |
|---|---|---|
| `display` | (monitor) | Monitor to use. |
| `presentation` | `fullscreen` | Fullscreen (default in this setup was `windowed`). |
| `aspect` | enabled | Keep the picture's aspect ratio. |
| `scaling` | `fit` | Scale the picture to fit the screen. |

The `dxcfg.exe` file in the game folder is the wrapper's configuration tool; use it or edit the file by hand.

## Notes

- F11 does not change the window mode in this build. Use `dxcfg.ini`.
- The recomp's fullscreen toggle (Alt+Enter, F11) is not implemented on GOG.
