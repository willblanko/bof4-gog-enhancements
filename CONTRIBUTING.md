# Contributing

Thank you for helping bring the recomp features to the GOG build.

## Ground rules

- **No game files.** Do not commit `BOF4.exe`, `DAT`, `SAV`, movies, audio or
  any extracted asset. Each contributor must use their own GOG copy.
- **No personal data.** Do not commit saves, logs with personal paths, or
  screenshots that show personal information.
- **Validate before patching.** Every hook must check the target address and
  the bytes at the site before it writes anything, and must log when it cannot
  apply.
- **Keep changes small and reversible.** One feature per change, with a key to
  turn it off.

## Workflow

1. Open an issue describing the feature, with the recomp reference.
2. Read the feature page under [`docs/features`](docs/features/README.md) and
   the [memory map](docs/reference/memory-map.md).
3. Build and test with the [checklist](docs/building.md#test-checklist).
4. Update the feature page, the [controls](docs/controls.md) if you add a key,
   and the [roadmap](docs/roadmap.md).
5. Open a pull request with the test results (which situations you tested).

## Documentation style

- English, short sentences, present tense.
- One feature per page, with the same sections: what it does, controls, how it
  works, open issues.
- Give addresses in hex with the `0x` prefix, and say which build they apply to.
