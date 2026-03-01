# Ampl — AI-First DAW: Product Spec

## Vision
Ampl is a native, cross-platform DAW where AI is a first-class workflow — not a chatbot bolted onto a traditional DAW. Every AI action is deterministic, reversible, and previewed before application.

---

## MVP (Milestone 1–3, ~8 weeks)

### What ships
- Native app window on macOS, Windows, Linux
- Audio device selection (CoreAudio, WASAPI, ALSA/JACK)
- Transport: play, stop, pause, loop, BPM, time signature
- Metronome click (synthesized, zero-allocation audio thread)
- Single audio track that plays a WAV/AIFF clip
- Timeline UI: waveform display, playhead, zoom, scroll
- Non-destructive edit model with undo/redo (command pattern)
- Offline bounce (faster-than-realtime)
- Project save/load (JSON + audio asset references)

### What does NOT ship yet
- MIDI, plugins, AI features, multi-track mixing

---

## v1 (Milestone 4–7, ~16 weeks after MVP)

### Core DAW
- Multi-track audio + MIDI
- Graph-based audio engine with latency compensation
- VST3 plugin hosting (sandboxed, separate process)
- AU hosting on macOS
- Sample-accurate automation (breakpoint + LFO)
- Mixer: per-track gain, pan, mute, solo, sends
- Basic built-in effects: EQ, compressor, reverb, delay

### AI-First Workflows
- **Session State API**: structured snapshot of entire session (tracks, clips, tempo, markers, plugin chains, automation) as an internal representation
- **Action DSL**: typed edit operations (AddClip, WarpClip, Quantize, SetParam, InsertPlugin, AutomateParam, etc.)
- **Command Palette**: natural-language input → DSL action plan → preview diff → accept/reject
- **Mix Assistant v1**: gain staging suggestions, basic EQ recommendations — all as reversible Action DSL patches
- **Transient detection + beat grid**: automatic tempo mapping from audio

### AI Infrastructure
- Planner module: NL → DSL with confidence scores
- Local inference runtime (ONNX Runtime or llama.cpp) with plugin-like architecture
- Remote inference opt-in (explicit user action only)
- Preview/diff UI for all AI-proposed edits

---

## Future (v2+)

- Stem separation module (local model)
- Pitch correction assistant
- "Tighten timing like track X" — cross-track timing analysis
- Agentic multi-step editing (AI proposes a chain of edits)
- "Write a pad that supports the chorus" — generative audio via local models
- Collaborative editing (CRDT-based)
- Video sync track
- Custom plugin format (Ampl Native Plugins)
- Marketplace for AI models and presets

---

## Non-Negotiable Principles

1. **Real-time safe audio thread**: zero allocations, zero locks, zero I/O
2. **AI actions are deterministic + reversible**: always preview, never destructive without consent
3. **Local-first**: AI works offline; remote inference is opt-in
4. **Privacy-first**: no telemetry by default; no data leaves the machine without explicit action
5. **Native performance**: no browser runtime, no Electron, no web views
6. **Every milestone ships a runnable app**
