# Intro movie subtitles

**Status:** Done.

## What it does

The opening movie has Japanese voice lines that were not translated in the US
release. This feature shows them in English at the bottom of the picture.

- L: turns the subtitles on or off. Default: on.

## How it works

- The subtitle follows the game's movie flag (`0xB5069C`). When the movie
  starts, the text is shown. When it ends, it is hidden.
- Subtitle text is drawn into a 1280×56 texture (a DIB and a Direct3D texture),
  and shown over the frame by `subtitle_frame`.
- The code checks the game image base (`0x400000`) before it reads the movie flag.

## Open issues

- The status line (F8 and F6 messages) uses the same texture path with a
  1.5 s timer (`status_show`, `status_frame`).
