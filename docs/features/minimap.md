# Minimap

**Status:** Done.

## What it does

Shows a round map of the walkable ground in the top-left corner while on the
field. It turns with the camera, like the recomp's minimap. It replaces the
game's compass.

- M: turns it on or off. Default: on.

## How it works

`make_minimap_tex` builds the texture and `minimap_frame` draws it each frame.
It reads fixed game addresses for the map and the leader's position.

The minimap is hidden while the journal is open, because both use the top of
the screen.

## Open issues

- The recomp's minimap uses the Select button. On GOG the key is M, and there
  is no gamepad binding yet.
