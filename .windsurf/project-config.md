# Project Configuration

## Project Identity

- **Name**: Neurato
- **Type**: Native cross-platform DAW (Digital Audio Workstation)
- **One-liner**: An AI-first DAW built for how producers actually work in 2025
- **Target users**: Music producers, beatmakers, singer-songwriters
- **Stage**: Early development — building toward v1 feature completeness

## Stack

- **Language**: C++20
- **Framework**: JUCE 8.0.12 (fetched via CMake FetchContent)
- **Build system**: CMake 3.22+ with presets
- **Platforms**: macOS, Windows, Linux (native — NOT Electron/web)
- **Build command**: `cmake --preset default` then `cmake --build build/debug`
- **Output**: `build/debug/Neurato_artefacts/Debug/Neurato.app` (macOS)

## Dependencies

- **External**: JUCE 8.0.12 (fetched via CMake)
- **Framework**: Max Framework (AI persona system)
- **System**: CMake 3.22+, C++20 compiler
- **Python**: 3.8+ (for Max Framework tools)
- **Platform**: macOS 10.15+, Windows 10+, Linux (Ubuntu 20.04+)

### Max Framework Setup

Max Framework provides AI persona and command system capabilities:

```bash
# Setup Max Framework dependency
./.windsurf/setup_max.sh
```

The Max Framework includes:

- Command execution with namespace support
- Persona system for domain-specific AI assistance
- Task management and planning tools
- AI code quality analysis
- Workflow automation

## Repository Structure

````
neurato/
├── src/                     # All source code
│   ├── app/                 # Main application (Main.cpp)
│   ├── engine/              # Audio engine (AudioEngine, Transport, Metronome, AudioTrack, SessionRenderer, OfflineRenderer)
│   ├── model/               # Data models (Session, Track, Clip, MidiClip, ProjectSerializer)
│   ├── commands/            # Command pattern (Command, CommandManager, ClipCommands, MidiCommands)
│   ├── ui/                  # User interface (Theme, TransportBar, TrackView, TimelineView, AudioSettingsPanel, AudioFileBrowser, BounceProgressDialog, MixerPanel, PianoRollEditor, AudioClipEditor)
│   └── util/                # Utilities (LockFreeQueue, Types, RecentProjects)
├── .windsurf/               # Neurato-specific configuration
│   ├── project-config.md    # This file
│   ├── max_bridge.py        # Bridge to Max Framework
│   ├── setup_max.sh         # Max Framework setup script
│   ├── neurato_quest.py     # Neurato-specific quest game
│   ├── quest_executor.py    # Quest executor
│   ├── pinned_tasks.json    # Current pinned tasks
│   ├── plan_state.json      # Current plan state
│   ├── context-cache/        # Session context cache
│   └── insights/             # Memory insights
├── ../max/                  # Max Framework (external dependency)
│   └── .windsurf/           # Max Framework core
│   (linked as .windsurf/max-framework)
├── CMakeLists.txt           # Build configuration
├── README.md                # Project documentation
└── assets/                  # Icons, images, etc.

## Milestones

### Current Milestone: Audio Graph & Automation

**Status**: In Progress
**Target Date**: 2025-03-01

**Goals**:
- [ ] Audio processing graph (nodes + edges)
- [ ] Latency compensation (PDC)
- [ ] Automation lanes
- [ ] Built-in gain/EQ/compressor plugins

**In Scope**:
- Audio graph architecture
- Plugin processing
- Automation data model
- Basic built-in effects

**Out of Scope**:
- Third-party plugin support (VST/AU)
- Advanced automation curves
- Plugin UI

### Completed Milestones

#### Milestone 4.5: MIDI Tracks & UI Modernization
- Status: Complete
- Delivered: MIDI data model, polyphonic synth, PianoRollEditor, modern UI theme
- Date: 2025-02-20

#### Milestone 4: Multi-Track & Mixer
- Status: Complete
- Delivered: MixerPanel, ChannelStrip, master bus, track reordering
- Date: 2025-02-20

#### Milestone 3: Project Persistence & Offline Bounce
- Status: Complete
- Delivered: ProjectSerializer, OfflineRenderer, BounceProgressDialog, RecentProjects
- Date: 2025-02-20

#### Milestone 2: Timeline & Non-Destructive Edits
- Status: Complete
- Delivered: Session model, non-destructive clip editing, TimelineView, CommandManager
- Date: 2025-02-20

#### Milestone 1: Audio Host Skeleton
- Status: Complete
- Delivered: Native app window, audio device selection, transport, metronome, single track playback
- Date: 2025-02-20

## Key Architectural Rules

1. **Audio thread sacred** - Zero allocations, zero locks, zero I/O on audio thread
2. **UI thread owns model** - Audio thread reads via atomic pointer swaps
3. **All edits through CommandManager** - Everything undoable/redoable
4. **Cross-platform from day one** - No platform-specific code in core

## Known Issues

- **Plugin support**: No VST/AU support yet - High
- **Audio graph**: Basic routing only, no send/insert effects - Medium
- **Automation**: No automation lanes yet - Medium

## Recent Decisions

- **2025-02-20**: Added MIDI data model with polyphonic sine synth
- **2025-02-20**: Implemented PianoRollEditor with note editing tools
- **2025-02-20**: Created Theme.h for shared UI palette and constants
- **2025-02-20**: Added MixerPanel with per-track gain/pan/mute/solo
- **2025-02-20**: Implemented master bus with gain/pan controls
- **2025-02-20**: Added track reordering and removal commands
- **2025-02-20**: Updated ProjectSerializer v2 for MIDI compatibility
- **2025-02-20**: Fixed audio thread RT safety violations

## Backlog

### High Priority
- [ ] Design audio graph node interface
- [ ] Implement basic gain/EQ/compressor plugins
- [ ] Add automation data model
- [ ] Create automation lanes UI

### Medium Priority
- [ ] Add send/insert effects routing
- [ ] Implement plugin parameter automation
- [ ] Create plugin manager for built-in effects

### Low Priority
- [ ] Add third-party plugin support (VST/AU)
- [ ] Implement advanced automation curves
- [ ] Create plugin UI editor

## Dependencies

### Runtime
- JUCE 8.0.12 (fetched via CMake)
- macOS 10.15+, Windows 10+, Linux (modern)

### Development
- CMake 3.22+
- C++20 compiler
- Xcode 15+ (macOS), Visual Studio 2022 (Windows), GCC 13+ (Linux)

## Environment Variables

```bash
# Optional
JUCE_PATH="/path/to/JUCE"  # If not using FetchContent
````

## Deployment

### Local Build

```bash
cmake --preset default
cmake --build build/debug
```

### Distribution

- GitHub releases with pre-built binaries
- Signed notarized packages (macOS)
- Installer with dependencies (Windows)

## Team

- **Creator**: Ethan Dellaposta
- **Contributors**: Open source community

## Resources

- GitHub: https://github.com/ethandellaposta/neurato
- Documentation: docs/
- JUCE Framework: https://juce.com
