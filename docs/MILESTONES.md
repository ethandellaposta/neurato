# Neurato — Implementation Milestones

Each milestone ends in a **runnable application**.

---

## Milestone 1: Audio Host Skeleton (Weeks 1–2)

**Deliverable:** Native window, audio device I/O, transport, metronome, single audio track

- [x] CMake project with JUCE, compiles on macOS/Windows/Linux
- [x] App window with minimal UI (transport bar, track area)
- [x] Audio device manager (select input/output/sample rate/buffer size)
- [x] Transport: play, stop, BPM control
- [x] Metronome click (synthesized sine ping, RT-safe)
- [x] Single audio track that plays a loaded WAV file
- [x] Lock-free SPSC queue for UI↔Audio communication
- [x] Clean shutdown (no crashes, no leaked threads)

## Milestone 2: Timeline & Non-Destructive Edits (Weeks 3–4)

**Deliverable:** Visual timeline, waveform display, undo/redo

- [x] Timeline UI with waveform rendering, playhead, zoom, scroll
- [x] Clip model: gain, fade in/out, trim
- [x] Command pattern: undo/redo stack
- [x] Loop region with visual markers
- [x] Keyboard shortcuts for transport and editing

## Milestone 3: Project Persistence & Offline Bounce (Weeks 5–6)

**Deliverable:** Save/load projects, export audio

- [x] Project file format (JSON metadata + audio asset references)
- [x] Save/load with versioning
- [x] Offline bounce (faster-than-realtime) to WAV with progress dialog
- [x] Recent projects list (persisted to ~/Library/Application Support/Neurato)
- [x] Audio file browser panel (toggle sidebar)
- [x] File menu bar (New/Open/Recent/Save/Save As/Export/Audio Settings)
- [x] Window title with project name + dirty state indicator
- [x] New Project command (Cmd+N) with unsaved changes confirmation
- [x] Keyboard shortcuts: Cmd+S (save), Cmd+O (open), Cmd+E (export), Cmd+N (new)

## Milestone 4: Multi-Track & Mixer (Weeks 7–9)

**Deliverable:** Multiple audio + MIDI tracks, mixer view

- [x] Multi-track audio playback with summing (SessionRenderer)
- [x] MIDI track with piano roll (see Milestone 4.5)
- [x] Mixer panel: gain, pan, mute, solo per track (MixerPanel + ChannelStrip)
- [x] Master bus with gain and pan
- [x] Track add/remove via commands (RemoveTrackCommand)
- [x] Track reordering via commands (ReorderTrackCommand)
- [x] Track rename via commands (RenameTrackCommand)
- [x] View menu with Mixer / File Browser toggles
- [x] Keyboard shortcut: Cmd+M (toggle mixer)
- [x] Offline renderer updated with solo, pan, and master bus
- [x] Project serialization includes master gain/pan

## Milestone 4.5: MIDI Tracks & UI Modernization

**Deliverable:** MIDI tracks with piano roll editor, audio clip editor, modern themed UI

- [x] Shared UI Theme (Theme.h): modern dark palette, font helpers, layout constants
- [x] MIDI data model: MidiNote, MidiClip structs, TrackType enum (Audio/MIDI)
- [x] MIDI synth engine: polyphonic sine synth with ADSR in SessionRenderer
- [x] MIDI commands: AddMidiClip, RemoveMidiClip, AddNote, RemoveNote, MoveNote, ResizeNote, SetNoteVelocity (all undoable)
- [x] Piano Roll Editor: note grid, keyboard sidebar, velocity lane, draw/select/move/resize/erase tools, snap-to-grid
- [x] Audio Clip Editor: zoomed waveform, gain/fade handles, trim
- [x] Editor Panel: bottom panel hosting PianoRoll or AudioClipEditor, opened by double-click or Escape to close
- [x] Timeline updated: MIDI clips rendered with note previews, audio/MIDI visual distinction
- [x] TransportBar, MixerPanel, TimelineView modernized with Theme colors
- [x] Project serialization updated: MIDI tracks, clips, and notes saved/loaded (format v2, backward-compatible with v1)
- [x] Sample rate forwarded to SessionRenderer for accurate MIDI synthesis
- [x] Keyboard shortcut: Escape closes editor panel

## Milestone 5: Audio Graph & Automation (Weeks 10–12)

**Deliverable:** Graph-based processing, sample-accurate automation

- Audio processing graph (nodes + edges)
- Latency compensation (PDC)
- Automation lanes: breakpoint editing, sample-accurate interpolation
- Built-in gain, EQ (parametric 4-band), compressor

## Milestone 6: Plugin Hosting (Weeks 13–16)

**Deliverable:** VST3 + AU hosting with crash isolation

- VST3 scanner + loader
- AU scanner + loader (macOS)
- Sandboxed plugin hosting (separate process)
- Crash recovery: plugin crash doesn't kill host
- Plugin state serialization (save/load with project)
- Plugin parameter automation

## Milestone 7: AI Layer v1 (Weeks 17–22)

**Deliverable:** Command palette, Action DSL, session snapshots, mix assistant

- Session State API: structured snapshot of entire session
- Action DSL: typed edit operations with inverse generation
- Command palette UI with NL input
- Planner module: NL → DSL actions + confidence
- Preview/diff UI for AI-proposed edits
- Local inference runtime (ONNX or llama.cpp)
- Mix assistant v1: gain staging + EQ suggestions
- Transient detection + beat grid

---

## Future Milestones

- Stem separation module
- Pitch correction assistant
- Cross-track timing analysis
- Generative audio (local models)
- Collaborative editing
- Video sync
