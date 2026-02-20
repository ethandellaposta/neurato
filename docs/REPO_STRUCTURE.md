# Neurato — Repository Structure

```
neurato/
├── CMakeLists.txt                  # Root CMake — fetches JUCE, defines targets
├── CMakePresets.json               # Build presets (Debug, Release, per-platform)
├── README.md                       # Build instructions + overview
├── docs/
│   ├── PRODUCT_SPEC.md
│   ├── ARCHITECTURE.md
│   ├── MILESTONES.md
│   └── REPO_STRUCTURE.md
├── src/
│   ├── app/                        # Application entry point + main window
│   │   └── Main.cpp
│   ├── engine/                     # Real-time audio engine
│   │   ├── AudioEngine.h/.cpp
│   │   ├── Transport.h/.cpp
│   │   ├── Metronome.h/.cpp
│   │   ├── SessionRenderer.h/.cpp  # RT-safe session playback (audio + MIDI synth)
│   │   ├── OfflineRenderer.h/.cpp  # Faster-than-realtime bounce
│   │   └── AudioTrack.h/.cpp       # Legacy single-track (unused)
│   ├── model/                      # Session model (tracks, clips, etc.)
│   │   ├── Session.h/.cpp
│   │   ├── Track.h                 # TrackState with TrackType (Audio/MIDI)
│   │   ├── Clip.h                  # Audio clip + AudioAsset
│   │   ├── MidiClip.h              # MidiNote + MidiClip structs
│   │   └── ProjectSerializer.h/.cpp # JSON save/load (format v2)
│   ├── ui/                         # UI components
│   │   ├── Theme.h                 # Shared modern dark UI theme
│   │   ├── TransportBar.h/.cpp
│   │   ├── TimelineView.h/.cpp     # Timeline with audio + MIDI clip rendering
│   │   ├── TrackView.h/.cpp
│   │   ├── MixerPanel.h/.cpp       # Mixer with channel strips + master bus
│   │   ├── PianoRollEditor.h/.cpp  # MIDI note editor (grid, keyboard, velocity)
│   │   ├── AudioClipEditor.h/.cpp  # Audio waveform editor (gain, fade, trim)
│   │   ├── AudioSettingsPanel.h/.cpp
│   │   ├── AudioFileBrowser.h/.cpp
│   │   └── BounceProgressDialog.h/.cpp
│   ├── commands/                   # Command pattern (undo/redo)
│   │   ├── Command.h
│   │   ├── CommandManager.h/.cpp
│   │   ├── ClipCommands.h          # Audio clip + track commands
│   │   └── MidiCommands.h          # MIDI clip + note commands
│   ├── ai/                         # AI layer — Milestone 7+
│   │   ├── SessionStateAPI.h
│   │   ├── ActionDSL.h
│   │   ├── Planner.h
│   │   └── InferenceRuntime.h
│   └── util/                       # Utilities
│       ├── LockFreeQueue.h         # SPSC lock-free FIFO
│       ├── Types.h                 # Common type aliases
│       └── RecentProjects.h/.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_transport.cpp
│   ├── test_lockfree_queue.cpp
│   └── test_metronome.cpp
└── assets/
    └── test_audio/                 # Test audio files
```
