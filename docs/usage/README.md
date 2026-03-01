# Ampl Usage Guide

This guide is focused on day-to-day usage: building, launching, and running common workflows in Ampl.

## Visuals

Current project visual asset:

![Ampl app icon](../../assets/icon_1024.png)

Usage screenshots should be added under `docs/usage/images/` using the names documented in `docs/usage/images/README.md`.

## 1) Build and Launch

### Recommended (VS Code tasks)

Use the task runner for the fastest local workflow:

1. `Tasks: Run Task`
2. Run `Build + Open Ampl (Debug)`

You can also use:

- `Build Debug`
- `Open Ampl.app (Debug)`
- `Run Ampl (Debug)`

### Terminal (CMake presets)

```bash
cmake --preset default
cmake --build build/debug
open build/debug/Ampl_artefacts/Debug/Ampl.app
```

Release build:

```bash
cmake --preset release
cmake --build build/release
open build/release/Ampl_artefacts/Release/Ampl.app
```

## 2) Quick Start Workflow

After launch, a typical first session is:

1. Configure audio input/output in the audio settings panel.
2. Add or load tracks/clips into the timeline.
3. Use transport controls to play/stop and set tempo.
4. Open the mixer (`Cmd+M`) to adjust gain, pan, mute, and solo.
5. Save the project before closing.

## 3) Working with Audio and MIDI

- **Audio timeline editing** is non-destructive (gain, fades, trims, position).
- **MIDI tracks** support piano-roll editing of notes and velocity.
- **Undo/redo** is command-based for clip and edit operations.
- **Track operations** (add/remove/reorder/rename) are integrated with command history.

## 4) Project Save/Load and Bounce

- Projects are saved in Ampl project format (`.ampl`) with referenced audio assets.
- Use standard save/open actions to continue sessions across restarts.
- Use offline bounce to render faster-than-realtime output.

## 5) Testing and Validation

### VS Code tasks

- `Run Tests (Debug)`
- `Run Tests (Release)`

### Terminal

```bash
ctest --test-dir build/debug --output-on-failure
```

## 6) Common Maintenance Tasks

- `Full Clean and Rebuild (Debug)` for a full local reset and rebuild.
- `Reset Debug CMake Cache` if paths/toolchains changed and CMake cache is stale.

Equivalent terminal cleanup:

```bash
rm -rf build
cmake --preset default
cmake --build build/debug
```

## 7) Troubleshooting

- If app launch fails, rebuild with `Build Debug` and re-open the app bundle.
- If CMake behaves inconsistently after moving directories, clear the build cache.
- If tests are missing, ensure you built with the `default` (Debug) preset (`BUILD_TESTS=ON`).

## 8) Related Docs

- [Architecture](../ARCHITECTURE.md)
- [Product Spec](../PRODUCT_SPEC.md)
- [Milestones](../MILESTONES.md)
- [Repository Structure](../REPO_STRUCTURE.md)
