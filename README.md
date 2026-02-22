# Neurato — AI-First DAW

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

- Project file format (.neurato JSON + relative audio asset paths)
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

### Run

#### macOS

```bash
open build/debug/Neurato_artefacts/Debug/Neurato.app
```

#### Linux

```bash
./build/debug/Neurato_artefacts/Debug/Neurato
```

#### Windows

```bash
build\debug\Neurato_artefacts\Debug\Neurato.exe
```

## AI Features

Neurato includes a comprehensive AI layer that enhances the audio production workflow:

### Command Palette

Press `Cmd+Shift+P` (or `Ctrl+Shift+P` on Windows/Linux) to open the command palette:

- **Natural Language Commands**: Type "create a new track" or "add reverb to vocals"
- **AI Suggestions**: Get context-aware suggestions based on your session
- **Traditional Commands**: Access all menu items and keyboard shortcuts

### Mix Assistant

The AI-powered mix assistant helps you achieve professional-sounding mixes:

- **Automatic Gain Staging**: Suggests optimal track levels for target LUFS
- **EQ Recommendations**: Analyzes frequency content and suggests EQ adjustments
- **Dynamic Range Control**: Recommends compression settings based on audio analysis
- **Learning System**: Adapts to your mixing preferences over time

### Automation & Analysis

- **Smart Automation**: Generate automation curves from audio analysis
- **Transient Detection**: Automatically detect transients for beat grid alignment
- **Audio Analysis**: RMS, peak, spectral analysis, and more
- **Preview System**: See AI suggestions before applying them

### Local Inference

All AI processing runs locally on your machine:

- **Privacy**: No audio data leaves your computer
- **Offline Capability**: Works without internet connection
- **Custom Models**: Support for ONNX, llama.cpp, and custom models
- **Performance**: Optimized for real-time audio processing

## Keyboard Shortcuts

| Key           | Action        |
| ------------- | ------------- |
| Space         | Play / Stop   |
| Cmd+Z         | Undo          |
| Cmd+Shift+Z   | Redo          |
| Cmd+N         | New Project   |
| Cmd+O         | Open Project  |
| Cmd+S         | Save Project  |
| Cmd+E         | Export Audio  |
| Cmd+M         | Toggle Mixer  |
| Cmd+= / Cmd+- | Zoom In / Out |
| Cmd+0         | Zoom to Fit   |
| Escape        | Close Editor  |

## Usage Instructions

### Getting Started (Milestone 1)

1. **Launch** the app (see Build/Run above).
2. **Audio Settings** — click the "Audio Settings" button (or File → Audio Settings) to select your audio output device and sample rate.
3. **Transport** — press **Space** to play/stop. Adjust BPM in the transport bar. Toggle the metronome on/off and set its gain.
4. **Peak Meters** — observe real-time L/R peak levels in the transport bar during playback.

### Timeline & Editing (Milestone 2)

1. **Add Tracks** — click **+ Track** to create new audio tracks.
2. **Import Audio** — click **Load Audio** or use the audio file browser sidebar to import WAV, AIFF, MP3, FLAC, or OGG files. Audio is placed as a clip on the first track.
3. **Timeline Navigation**
    - **Zoom**: Cmd+= / Cmd+- or scroll wheel with Cmd held. Cmd+0 zooms to fit all content.
    - **Scroll**: horizontal scroll to navigate the timeline.
    - **Seek**: click on the timeline ruler to move the playhead.
4. **Clip Editing** (non-destructive)
    - **Move clips**: drag a clip left/right on the timeline to reposition it.
    - **Trim**: drag clip edges to adjust start/end points.
    - **Gain/Fade**: adjust per-clip gain, fade-in, and fade-out via the clip properties.
5. **Undo/Redo** — Cmd+Z / Cmd+Shift+Z, or use the Undo/Redo toolbar buttons. All clip and track edits are undoable.

### Project Persistence & Export (Milestone 3)

1. **Save Project** — Cmd+S (or File → Save). Projects are saved as `.neurato` JSON files with relative audio asset paths.
2. **Save As** — File → Save As to save to a new location.
3. **Open Project** — Cmd+O (or File → Open) to load a `.neurato` project file.
4. **New Project** — Cmd+N (or File → New Project). If you have unsaved changes, you'll be prompted to discard or cancel.
5. **Recent Projects** — File → Recent Projects shows your recently opened projects.
6. **Export Audio** — Cmd+E (or File → Export Audio) to bounce the session to a WAV file. A progress dialog shows rendering status with a cancel button. Rendering is faster-than-realtime.
7. **Audio File Browser** — click the **Files** toolbar button (or View → Toggle File Browser) to open a sidebar for browsing and importing audio files. Double-click a file to import it.
8. **Window Title** — shows the project name and a `*` prefix when there are unsaved changes.

### Multi-Track & Mixer (Milestone 4)

1. **Multiple Tracks** — click **+ Track** to add as many audio tracks as needed. Each track can hold multiple clips on the timeline.
2. **Toggle Mixer** — press **Cmd+M** or use View → Toggle Mixer to show/hide the mixer panel at the bottom of the window.
3. **Channel Strips** — each track gets a channel strip in the mixer with:
    - **Gain slider** (vertical, -60 dB to +12 dB)
    - **Pan knob** (rotary, full left to full right)
    - **M button** — mute the track (red when active)
    - **S button** — solo the track (yellow when active). When any track is soloed, only soloed tracks are heard.
    - **X button** — remove the track (cannot remove the last track)
4. **Master Bus** — the rightmost strip in the mixer controls the master output gain and pan. All tracks are summed through the master bus before output.
5. **All mixer changes are undoable** — gain, pan, mute, solo, and track removal all go through the command system and can be undone with Cmd+Z.
6. **Export respects mixer state** — bouncing to WAV applies all track gain/pan/mute/solo settings and the master bus, so the exported file matches what you hear.
7. **Project save includes mixer state** — master gain and pan are persisted in the `.neurato` project file.

### MIDI Tracks & Editors (Milestone 4.5)

1. **Add MIDI Track** — click **+ MIDI Track** in the toolbar to create a MIDI track. MIDI tracks are visually distinct from audio tracks on the timeline.
2. **Piano Roll Editor** — double-click a MIDI clip on the timeline to open the Piano Roll Editor in the bottom panel:
    - **Keyboard sidebar** — shows note names (C, C#, D, etc.) with octave labels
    - **Note grid** — displays MIDI notes as colored rectangles; snap-to-grid for precise placement
    - **Velocity lane** — bottom strip shows per-note velocity bars; drag to adjust
    - **Tools**: Draw (pencil), Select (pointer), Move, Resize, Erase — click the tool buttons or use the grid directly
    - **All edits are undoable** — adding, moving, resizing, deleting notes all go through the command system
3. **Audio Clip Editor** — double-click an audio clip to open the Audio Clip Editor:
    - **Zoomed waveform** — detailed view of the clip's audio
    - **Gain handle** — drag to adjust clip gain
    - **Fade handles** — drag fade-in/fade-out markers
    - **Trim** — adjust clip boundaries
4. **Close Editor** — press **Escape** to close the editor panel.
5. **MIDI Playback** — MIDI tracks are synthesized in real-time using a built-in polyphonic sine synth with attack/release envelope. MIDI notes play back during transport playback.
6. **MIDI in Project Files** — MIDI tracks, clips, and notes are fully serialized in the `.neurato` project file (format v2, backward-compatible with v1 audio-only projects).

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full system architecture.

## Thread Model

| Thread       | RT-Safe? | Allocations? |
| ------------ | -------- | ------------ |
| Audio Thread | YES      | NONE         |
| UI Thread    | No       | Yes          |

Communication between threads uses lock-free SPSC queues only. No mutexes on the audio path.

## Roadmap

See [docs/MILESTONES.md](docs/MILESTONES.md) for the full implementation plan.
