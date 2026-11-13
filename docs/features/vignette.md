# Vignette

**Status:** Done.

## What it does

A dark gradient over the edges of the frame, similar to the HD-2D look in the
recomp (which is part of its extras).

- K: turns it on or off. Default: off.

## How it works

The vignette texture builder creates a 64-step gradient texture: transparent in the middle,
black at the edges, with alpha up to 200. It is drawn stretched over the frame
after the game's picture.
