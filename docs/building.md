# Building and testing

## Requirements

- Windows 10 or 11.
- Visual Studio 2022 Build Tools (C++ desktop workload) with the 32-bit x86 compiler.
- A GOG copy of Breath of Fire IV (the 1.0 GOG build, `BOF4.exe` of the original release).
- A DirectX wrapper setup that loads `d3d9.dll` and `ddraw.dll` from the game folder, with a `dxcfg.ini` (see [fullscreen](features/fullscreen.md)).

## Build the proxies (scripts in `src/`)

Run `src/d3d9/build.bat` (or `src/ddraw/build.bat`), or from a Developer Command Prompt for VS 2022 (x86):

```bat
cl /nologo /O1 /GS- /W3 /c d3d9_proxy.c
link /nologo /DLL /OUT:d3d9.dll /DEF:d3d9.def /ENTRY:DllMain@12 ^
     /NODEFAULTLIB /MAP d3d9_proxy.obj kernel32.lib user32.lib gdi32.lib
```

Notes:

- `/GS-` disables the stack cookie, which needs the CRT.
- `/NODEFAULTLIB` keeps the CRT out of the DLL.
- The `.def` file exports `Direct3DCreate9` and the other symbols the game expects.

## Build the pass-through ddraw

```bat
cl /nologo /O1 /GS- /W3 /c ddraw_pass.c
link /nologo /DLL /OUT:ddraw.dll /DEF:ddraw_pass.def /ENTRY:proxy_entry@12 ^
     /NODEFAULTLIB kernel32.lib
```

The pass-through forwards 14 exports through trampolines (`tramp_i`). Use it
when a hook in the old `ddraw` proxy causes overlays to appear in the game.

## Deploy

1. Back up the DLLs that are already in the game folder.
2. Copy the new `d3d9.dll` (and `ddraw.dll` if changed) into the game folder.
3. Start the game and check `d3d9_proxy.log`.

Never overwrite the original `BOF4.exe`, `DAT` or `SAV` files.

## Test checklist

Run this list after any change that touches the field or the menus:

1. Field, flat floor: walk in 8 directions, then diagonals, then run.
2. Walls and doors: push into a wall, slide along it, go through a door.
3. Stairs or ramps.
4. Town with NPCs: walk next to an NPC, talk to it.
5. Switch leader from the camp menu.
6. Random battle (encounters on), then with F8 off.
7. A mandatory story battle and a boss, with F8 off.
8. Menus, shops and the world map (no field-only feature should leak into them).
9. Save and load, camp menu Save from a field and a town.

## Diagnostics

- `d3d9_proxy.log`: install messages and feature events.
- F11 (diagnostic capture) writes draw-call statistics to the log. It is for
  research only.
