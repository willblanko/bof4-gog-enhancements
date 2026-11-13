# Architecture

## Overview

```
GOG launcher / DirectX wrapper (dxcfg.ini)
        │
        ├── ddraw.dll        pass-through (14 exports forwarded)
        │
        └── d3d9.dll        proxy
              ├── forwards Direct3DCreate9 to the system d3d9.dll
              ├── hooks IDirect3D9::CreateDevice (vtable 16)
              ├── hooks IDirect3DSwapChain9::Present (vtable 3)
              ├── draws overlays (minimap, masters panel, journal, subtitles, vignette, status)
              ├── reads input (XInput, keyboard)
              └── patches game code at validated call sites
```

## The proxy DLL

`d3d9.dll` is built from `d3d9_proxy.c` plus small headers:

| Header | Feature |
|---|---|
| `save_anywhere.h` | Enables the camp menu's Save command in every place |
| `enc_off.h` | Random encounter switch (F8) |
| `walk_stick.h` | Analog stick walking (F6) |
| `master_faces_data.h` | Portrait data for the masters panel |

The proxy has no C runtime (`/NODEFAULTLIB`, Win32 API only). This keeps the
binary small and avoids CRT conflicts with the game.

## Per-frame work

Everything that draws or reads input runs from the hooked `Present`, once per
frame, in this order:

1. Mouse wheel hook and analog stick read (`stick_frame`).
2. Walk stick (`walk_frame`) and zoom (`zoom_frame`).
3. Capture helper (`cap_frame`, F11 diagnostic).
4. Camera follow toggle (F7) and walk toggle (F6).
5. Encounter toggle (F8) and status message.
6. Camera follow update (`follow_frame`).
7. Masters panel and journal keys (`masters_frame`), minimap, subtitles, status line.
8. Overlay draw, then the real `Present`.

## Patching game code

The game is `BOF4.exe` (GOG, image base `0x400000`). Each feature that changes
behaviour replaces a single **call instruction** with a call to our stub:

1. Read 5 bytes at the site.
2. Check the first byte is `E8` (near call) and that the target equals the
   function we expect.
3. Only if all checks pass, write the new `E8 rel32` with `VirtualProtect`, then
   `FlushInstructionCache`.

If any check fails, nothing is written and the log says why. The game then runs
unchanged. Stubs that need to call the original function use a tail jump or
call it with the same arguments, and they keep the registers the caller expects.

## Logs

The proxy writes `d3d9_proxy.log` in the game folder. Feature messages (for
example `walk stick: site 0063333A redirected`) are written at install time.
Keep the log when reporting a bug.

## Safety rules for contributors

- Never write to game memory without first validating the target.
- Keep the original DLLs as backups before replacing them.
- Test each feature in the field, in battle, in menus and on the world map.
- Do not commit game files, saves, or extracted assets.
