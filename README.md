# Ampl — AI-First DAW

A native, cross-platform, ultra-performant DAW with AI as a first-class workflow.

**Not Electron. Not a web app. Native C++20 with JUCE.**

## Current Status: Milestone 7 Complete + Logic Pro X Features

### Milestone 1 ✅ Audio Host Skeleton

- Native app window, audio device selection, transport, metronome, single audio track, lock-free SPSC queues, peak level meters, clean shutdown

### Milestone 2 ✅ Timeline & Non-Destructive Edits

- Timeline UI with waveform rendering, playhead, beat grid, zoom, scroll
- Non-destructive clip model: gain, fade in/out, trim, timeline position
- Command pattern undo/redo, keyboard shortcuts

### Milestone 3 ✅ Project Persistence & Offline Bounce

- Project file format (.ampl JSON + relative audio asset paths)
- Save / Save As / Open with versioning
- Offline bounce (faster-than-realtime) WAV export with progress dialog
- Recent projects list, audio file browser, file/view menus
- Window title with project name + unsaved changes indicator

### Milestone 4 ✅ Multi-Track & Mixer

- ✅ Multi-track audio playback with summing (SessionRenderer)
- ✅ Mixer panel with per-track gain, pan, mute, solo (toggle with Cmd+M)
- ✅ Master bus with gain and pan controls
- ✅ Track add/remove/reorder/rename via undoable commands
- ✅ View menu (Mixer / File Browser toggles)
- ✅ Offline renderer + project serializer updated for mixer state

### Milestone 4.5 ✅ MIDI Tracks & UI Modernization

- ✅ MIDI tracks with TrackType enum (Audio/MIDI)
- ✅ Piano Roll Editor: note grid, keyboard sidebar, velocity lane, draw/select/move/resize/erase tools, snap-to-grid
- ✅ Audio Clip Editor: zoomed waveform, gain/fade handles, trim
- ✅ MIDI synth engine: polyphonic sine synth with ADSR envelope in SessionRenderer
- ✅ MIDI commands: all note/clip operations undoable via CommandManager
- ✅ Modern dark UI theme applied across TransportBar, TimelineView, MixerPanel
- ✅ Timeline distinguishes audio vs MIDI clips with note previews
- ✅ Double-click clips to open editors, Escape to close
- ✅ Project serialization v2: MIDI tracks/clips/notes saved and loaded (backward-compatible)

### Milestone 5 ✅ Audio Graph & Automation

- ✅ Audio processing graph with nodes + edges
- ✅ Latency compensation (PDC) with automatic delay line insertion
- ✅ Automation lanes with sample-accurate breakpoint editing
- ✅ Built-in processors: gain, 4-band parametric EQ, compressor
- ✅ Real-time parameter smoothing and automation interpolation
- ✅ Graph validation and topological sorting for processing order

### Milestone 6 ✅ Plugin Hosting

- ✅ VST3 scanner + loader with plugin database management
- ✅ AU scanner + loader (macOS) with component validation
- ✅ Sandboxed plugin hosting with crash isolation and IPC communication
- ✅ Plugin state serialization (chunk and parameter formats)
- ✅ Plugin parameter automation with lane mapping
- ✅ Real-time plugin processing with bypass and parameter control

### Milestone 7 ✅ AI Layer v1 (the flagship feature)

- ✅ Session State API: structured snapshot of entire session with audio analysis
- ✅ Action DSL: typed edit operations with automatic inverse generation
- ✅ Command palette UI with natural language input and AI-powered suggestions
- ✅ Planner module: NL → DSL actions with confidence scoring
- ✅ Preview/diff UI for AI-proposed edits with before/after comparison
- ✅ Local inference runtime with support for multiple model architectures
- ✅ Mix assistant v1: gain staging + EQ suggestions with learning system
- ✅ Transient detection + beat grid with tempo estimation
- ✅ End-to-end test suite with performance benchmarks and stress testing

### 🎛️ Logic Pro X-Style Features

#### Professional Mixer

- **Advanced Channel Strips**: 15 plugin slots, 8 sends per channel, VCA assignment, phase inversion, trim gain
- **Smart Controls**: Map multiple parameters to single controls with custom curves and ranges
- **Bus Routing**: Create unlimited buses with flexible routing and send/return options
- **VCA Groups**: Group multiple tracks under VCA faders for unified control
- **Environment**: Advanced routing matrix with buses, VCAs, and track assignments

#### Track Types & Features

- **Multiple Track Types**: Audio, Instrument, Drum Machine, External, Output, Master, Bus, Input, Aux, VCA, Folder Stack
- **Track Alternatives**: Multiple takes and comping with edit points
- **Flex Time**: Advanced time stretching with multiple modes (monophonic, polyphonic, rhythmic, slicing, speed)
- **Step Sequencer**: Built-in drum machine with pattern editing and real-time recording
- **Score Editor**: MIDI notation with quantization, transposition, and import/export

#### Advanced Automation

- **Sample-Accurate Automation**: Breakpoint editing with curve interpolation
- **Parameter Mapping**: Automate any plugin parameter, send level, or mixer control
- **Smart Automation**: AI-assisted automation generation from audio analysis
- **Automation Lanes**: Unlimited lanes per track with independent curves

#### Professional Workflow

- **Command Palette**: Natural language commands with AI-powered suggestions
- **Channel Strip Editor**: Detailed parameter editing with plugin chain management
- **Mixer Toolbar**: Quick access to view options, track creation, and zoom controls
- **Real-Time Meters**: Peak, RMS, and LUFS metering with customizable ballistics

## Repository Structure

```
ampl/
├── src/                          # Main source code
│   ├── app/                      # Application entry point
│   │   └── Main.cpp              # Main application class
│   ├── engine/                   # Audio engine core
│   │   ├── AudioEngine.cpp       # Main audio engine
│   │   ├── Transport.cpp         # Play/stop/loop control
│   │   ├── Metronome.cpp         # Metronome implementation
│   │   ├── AudioTrack.cpp        # Track audio processing
│   │   ├── SessionRenderer.cpp   # Real-time rendering
│   │   ├── OfflineRenderer.cpp   # Export rendering
│   │   ├── PluginManager.cpp     # Plugin management
│   │   ├── PianoSynth.cpp        # MIDI synthesis
│   │   ├── AudioGraph.cpp        # Processing graph
│   │   ├── Automation.cpp        # Parameter automation
│   │   ├── AudioProcessors.cpp   # Built-in effects
│   │   ├── PluginHost.cpp        # Plugin hosting
│   │   ├── SandboxHost.cpp       # Plugin sandboxing
│   │   └── LogicFeatures.cpp     # Logic Pro X features
│   ├── ai/                       # AI/ML components
│   │   ├── AIComponents.cpp      # AI core components
│   │   ├── AIImplementation.cpp  # AI implementation details
│   │   └── MixAssistant.cpp      # AI mixing assistant
│   ├── model/                    # Data models
│   │   ├── Session.cpp           # Project session model
│   │   └── ProjectSerializer.cpp # Save/load functionality
│   ├── commands/                 # Command pattern
│   │   └── CommandManager.cpp    # Undo/redo system
│   ├── ui/                       # User interface
│   │   ├── TransportBar.cpp      # Transport controls
│   │   ├── TrackView.cpp         # Track display
│   │   ├── TimelineView.cpp      # Timeline interaction
│   │   ├── AudioSettingsPanel.cpp # Audio device settings
│   │   ├── AudioFileBrowser.cpp  # File browser
│   │   ├── BounceProgressDialog.cpp # Export progress
│   │   ├── MixerPanel.cpp        # Mixer interface
│   │   ├── PianoRollEditor.cpp   # MIDI editing
│   │   ├── AudioClipEditor.cpp   # Audio editing
│   │   └── Theme.cpp             # UI theming
│   └── util/                     # Utilities
│       └── RecentProjects.cpp    # Recent projects list
├── tests/                        # Test suite
│   ├── unit/                     # Unit tests
│   ├── integration/              # Integration tests
│   └── generate_test_data.py     # Test data generator
├── docs/                         # Documentation
│   ├── ARCHITECTURE.md           # System architecture
│   ├── MILESTONES.md             # Development roadmap
│   ├── API.md                    # API documentation
│   └── AI.md                     # AI features guide
├── assets/                       # Application assets
│   └── Ampl.icns                 # Application icon
├── CMakeLists.txt                # Build configuration
└── README.md                     # This file
```

## Key Module Documentation

- **[Usage Guide](docs/usage/README.md)** - Build, launch, and common day-to-day workflows
- **[Audio Engine](docs/ENGINE.md)** - Core audio processing architecture
- **[AI Layer](docs/AI.md)** - Machine learning integration and APIs
- **[Plugin System](docs/PLUGINS.md)** - VST3/AU hosting and sandboxing
- **[UI Architecture](docs/UI.md)** - Component-based UI design
- **[Command System](docs/COMMANDS.md)** - Undo/redo and command pattern

## Build Instructions

### Prerequisites

- **CMake** ≥ 3.22
- **C++20 compiler**: Clang 14+, GCC 12+, or MSVC 2022+
- **Git** (for fetching JUCE)
- **Optional AI Dependencies**: ONNX Runtime, llama.cpp (for local inference)

#### macOS

```bash
xcode-select --install   # if not already installed
brew install cmake       # if not already installed
```

#### Linux (Ubuntu/Debian)

```bash
sudo apt install build-essential cmake git
sudo apt install libasound2-dev libjack-jackd2-dev \
    libfreetype6-dev libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libgl1-mesa-dev libwebkit2gtk-4.0-dev
```

#### Windows

- Install Visual Studio 2022 with C++ workload
- Install CMake (or use the one bundled with VS)

### Build

```bash
# Configure (Debug)
cmake --preset default

# Build
cmake --build build/debug

# Or Release:
cmake --preset release
cmake --build build/release

# Run tests (if enabled)
ctest --test-dir build/debug --output-on-failure
```

### VS Code Actions (Tasks)

All CMake build commands are available as VS Code tasks.

- Open Command Palette → `Tasks: Run Task`
- Run any of these actions:
    - `Configure Debug`, `Configure Release`, `Configure RelWithDebInfo`, `Configure CI`
    - `Build Debug`, `Build Release`, `Build RelWithDebInfo`, `Build CI`, `Build Verbose Debug`
    - `Build + Open Ampl (Debug)`, `Build + Open Ampl (Release)`
    - `Run Ampl (Debug)`, `Run Ampl (Release)`
    - `Open Ampl.app (Debug)`, `Open Ampl.app (Release)`
    - `Clean Debug`, `Clean Release`, `Clean RelWithDebInfo`
    - `Install Debug`, `Install Release`, `Install RelWithDebInfo`
    - `Run Tests (Debug)`, `Run Tests (Release)`
    - `Full Clean and Rebuild (Debug)`, `Reset Debug CMake Cache`

### Run

#### macOS

```bash
open build/debug/Ampl_artefacts/Debug/Ampl.app
```

#### Linux

```bash
./build/debug/Ampl_artefacts/Debug/Ampl
```

#### Windows

```bash
build\debug\Ampl_artefacts\Debug\Ampl.exe
```
