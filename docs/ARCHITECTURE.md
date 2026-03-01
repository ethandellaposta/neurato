# Ampl — System Architecture

## Module Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                          APPLICATION LAYER                          │
│  ┌──────────┐  ┌──────────────┐  ┌──────────┐  ┌───────────────┐  │
│  │    UI     │  │   Command    │  │ Project  │  │   AI Layer    │  │
│  │ (JUCE GL) │  │   System     │  │ Manager  │  │  (Planner +   │  │
│  │          │  │ (Undo/Redo)  │  │(Save/Load)│  │   Inference)  │  │
│  └────┬─────┘  └──────┬───────┘  └────┬─────┘  └───────┬───────┘  │
│       │               │               │                 │          │
│  ─────┴───────────────┴───────────────┴─────────────────┴────────  │
│                     SESSION MODEL (shared state)                    │
│           Tracks, Clips, Tempo Map, Markers, Automation             │
│  ────────────────────────────────────────────────────────────────── │
│       │                                           │                │
│  ┌────┴──────────────┐                  ┌─────────┴─────────────┐  │
│  │  Lock-Free Queues │                  │  Session State API    │  │
│  │  (UI ↔ Audio)     │                  │  (JSON-like snapshot) │  │
│  └────┬──────────────┘                  └───────────────────────┘  │
└───────┼─────────────────────────────────────────────────────────────┘
        │
┌───────┼─────────────────────────────────────────────────────────────┐
│       │              REAL-TIME AUDIO ENGINE                          │
│  ┌────┴─────────┐  ┌──────────────┐  ┌──────────────────────────┐  │
│  │  Transport   │  │  Audio Graph │  │  Plugin Host (sandboxed) │  │
│  │  (play/stop/ │  │  (nodes +    │  │  (separate process per   │  │
│  │   loop/BPM)  │  │   edges)     │  │   plugin, crash recovery)│  │
│  └──────────────┘  └──────────────┘  └──────────────────────────┘  │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │
│  │  Metronome   │  │  Automation  │  │  Latency Compensation    │  │
│  │  (synth)     │  │  (sample-    │  │  (PDC)                   │  │
│  │              │  │   accurate)  │  │                          │  │
│  └──────────────┘  └──────────────┘  └──────────────────────────┘  │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  Audio Device I/O (CoreAudio / WASAPI / ALSA / JACK)        │   │
│  └──────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                        OFFLINE RENDERER                              │
│  Same audio graph, runs faster-than-realtime, outputs to file       │
└─────────────────────────────────────────────────────────────────────┘
```

## Thread Model

| Thread           | Responsibility                              | RT-Safe? | Allocations? |
| ---------------- | ------------------------------------------- | -------- | ------------ |
| **Audio Thread** | Audio callback, graph processing, metronome | YES      | NONE         |
| **UI Thread**    | Rendering, user input, command dispatch     | No       | Yes          |
| **AI Thread**    | Inference, planning, DSL generation         | No       | Yes          |
| **File I/O**     | Project save/load, audio file reading       | No       | Yes          |
| **Plugin Host**  | Separate process per plugin instance        | Isolated | Isolated     |

## Communication

- **UI → Audio**: Lock-free SPSC (single-producer single-consumer) FIFO queue
    - Transport commands (play, stop, seek, loop)
    - Parameter changes
    - Track/clip updates (pointer swaps to immutable data)
- **Audio → UI**: Lock-free SPSC FIFO queue
    - Playhead position
    - Meter levels
    - Buffer underrun notifications
- **AI → Session Model**: Command objects posted to UI thread
    - AI thread produces Action DSL patches
    - UI thread validates, shows preview, applies on accept

## Data Flow (Audio Callback)

```
1. Read transport state
2. Read pending commands from UI→Audio queue
3. For each active track:
   a. Read clip audio data (pre-loaded in memory)
   b. Apply non-destructive edits (gain, fade, warp) — all RT-safe
   c. Process plugin chain (via sandboxed host IPC)
   d. Apply automation (sample-accurate interpolation)
4. Sum to master bus
5. Apply master chain
6. Write to output buffer
7. Push playhead position + meters to Audio→UI queue
```

## AI Layer Architecture

```
┌──────────────────────────────────────────────────┐
│                  AI LAYER                         │
│                                                   │
│  ┌─────────────┐    ┌─────────────────────────┐  │
│  │   NL Input   │───▶│       Planner           │  │
│  │  (Command    │    │  (NL → Action DSL)      │  │
│  │   Palette)   │    │  + confidence scores    │  │
│  └─────────────┘    └──────────┬──────────────┘  │
│                                │                  │
│                     ┌──────────▼──────────────┐   │
│                     │    Action DSL Patch      │   │
│                     │  [{op: "quantize",       │   │
│                     │    track: "bass",        │   │
│                     │    grid: "1/16",         │   │
│                     │    swing: 0.55}]         │   │
│                     └──────────┬──────────────┘   │
│                                │                  │
│                     ┌──────────▼──────────────┐   │
│                     │   Preview / Diff UI      │   │
│                     │   (before/after, A/B)    │   │
│                     └──────────┬──────────────┘   │
│                                │                  │
│                     ┌──────────▼──────────────┐   │
│                     │   Command System         │   │
│                     │   (apply / undo / redo)  │   │
│                     └─────────────────────────┘   │
│                                                   │
│  ┌─────────────────────────────────────────────┐  │
│  │  Inference Runtime                          │  │
│  │  ┌─────────┐  ┌──────────┐  ┌───────────┐  │  │
│  │  │  Local   │  │  Remote  │  │  Audio    │  │  │
│  │  │  (ONNX/  │  │  (opt-in │  │  Models   │  │  │
│  │  │  llama)  │  │   API)   │  │  (stems,  │  │  │
│  │  └─────────┘  └──────────┘  │   pitch)  │  │  │
│  │                              └───────────┘  │  │
│  └─────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────┘
```

## Session State API (Snapshot Format)

```json
{
    "formatVersion": 2,
    "bpm": 120.0,
    "timeSignature": [4, 4],
    "sampleRate": 48000,
    "masterGainDb": 0.0,
    "masterPan": 0.0,
    "tracks": [
        {
            "id": "track-1",
            "name": "Bass",
            "type": "audio",
            "gain": -3.0,
            "pan": 0.0,
            "mute": false,
            "solo": false,
            "clips": [
                {
                    "id": "clip-1",
                    "assetPath": "assets/bass.wav",
                    "timelineStartSample": 0,
                    "sourceLengthSamples": 192000,
                    "gainDb": 0.0,
                    "fadeInSamples": 0,
                    "fadeOutSamples": 0
                }
            ],
            "midiClips": []
        },
        {
            "id": "track-2",
            "name": "Synth Lead",
            "type": "midi",
            "gain": 0.0,
            "pan": 0.0,
            "mute": false,
            "solo": false,
            "clips": [],
            "midiClips": [
                {
                    "id": "mclip-1",
                    "name": "MIDI",
                    "timelineStartSample": 0,
                    "lengthSamples": 192000,
                    "notes": [
                        {
                            "id": "note-1",
                            "noteNumber": 60,
                            "velocity": 0.8,
                            "startSample": 0,
                            "lengthSamples": 48000
                        }
                    ]
                }
            ]
        }
    ],
    "markers": []
}
```

## Action DSL (Edit Operations)

```json
[
    { "op": "addClip", "trackId": "track-1", "sourceFile": "kick.wav", "startBeat": 0 },
    { "op": "quantize", "clipId": "clip-1", "grid": "1/16", "swing": 0.55, "strength": 1.0 },
    { "op": "setParam", "trackId": "track-1", "param": "gain", "value": -6.0 },
    { "op": "insertPlugin", "trackId": "track-1", "pluginId": "com.vendor.eq", "position": 0 },
    {
        "op": "automateParam",
        "trackId": "track-1",
        "param": "gain",
        "points": [
            [0, -6],
            [48000, 0]
        ]
    }
]
```

Each operation is:

- **Deterministic**: same input → same output
- **Reversible**: generates an inverse operation for undo
- **Previewable**: can be applied to a snapshot for diff display without committing
