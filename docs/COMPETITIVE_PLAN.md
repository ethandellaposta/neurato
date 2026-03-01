# Ampl Competitive Plan (Human-Centered AI DAW)

## Intent

Build Ampl into a modern, professional DAW that competes with ACE Studio and Studio One-class workflows while keeping the musician in control.

## Product Direction

Ampl should be:

- **Human-centered**: AI assists, never auto-publishes or silently rewrites creative decisions.
- **Pro workflow first**: recording, editing, mixing, arrangement, reliability, speed.
- **AI-augmented**: optional tools for ideation, cleanup, and repetitive tasks.

---

## Competitive Snapshot (Publicly Advertised Features)

> Note: This matrix is based on publicly accessible product pages and may evolve over time.

### ACE Studio (AI production suite)

- AI singing voice generation from MIDI + lyrics
- Voice cloning (custom voice models)
- Choir mode / multi-voice stacking
- AI instruments (including ensemble workflows)
- Stem splitter (vocals/drums/bass/instruments)
- Audio-to-MIDI / vocal-to-MIDI workflows
- Voice changer
- Generative kits (idea generation / layer generation / enhancer-style tools)
- DAW integration claims via bridge plugin (VST3/AU/AAX) and ARA/tempo sync messaging
- Artist-rights positioning: licensed/trained voices and “human in the loop” framing

### Studio One / Fender Studio Pro positioning

- Full DAW scope: record, produce, mix, master, perform
- Modernized UI and workflow overviews (arrangement/channel overview)
- AI-assisted audio-to-note conversion and chord-assistant style composition support
- Deep built-in effects/instruments ecosystem and strong content bundle
- Video support in performance workflows
- Mobile/desktop workflow continuity messaging
- Commercial ecosystem strength (content, collaboration tiers, cloud extras)

### Ampl (current state from repo docs)

- Strong architectural foundation: native app, lock-free UI↔audio queues, RT-safe principles
- Core transport + timeline + clips + save/load + offline bounce
- Multi-track audio/MIDI direction defined in milestones
- AI architecture blueprint exists (Session State API + Action DSL + preview/diff)
- Missing from competitive parity: polished UI system, mature editing depth, plugin ecosystem depth, collaboration/content moat

---

## Gap Analysis (What Actually Wins Users)

### 1) Product Surface Gap

- **Gap**: Current visual/UI polish and information density are below pro-DAW expectations.
- **Impact**: Users perceive “toy” quality before hearing audio quality.
- **Priority**: Critical.

### 2) Workflow Completeness Gap

- **Gap**: Editing depth and mixer workflows are not yet at daily-driver level.
- **Impact**: Power users churn before adopting AI features.
- **Priority**: Critical.

### 3) Trust & Control Gap (for AI)

- **Gap**: AI needs explicit guardrails in UX language and behavior, not just architecture docs.
- **Impact**: Users fear loss of creative agency.
- **Priority**: Critical.

### 4) Ecosystem Gap

- **Gap**: No clear moat around presets/content/templates/compatibility/community.
- **Impact**: Harder to retain users after trial.
- **Priority**: High.

---

## Human-Centered AI Principles for Ampl

1. **Ask before acting**
    - Every AI action uses preview + confidence + reversible patch.

2. **Intent over generation**
    - Default AI tools should clarify, organize, and enhance user material before generating net-new content.

3. **Editable output always**
    - Any AI result becomes normal clips/automation/notes users can edit by hand.

4. **A/B and diff by default**
    - Users can compare before/after and recover original state instantly.

5. **Local-first privacy**
    - Local inference default; cloud opt-in per action.

6. **Creative attribution clarity**
    - Surface source/origin metadata for AI-assisted assets and edits.

---

## UX Modernization Plan (Logic-like professionalism, Ampl identity)

### Visual Language

- Layered dark surfaces with subtle elevation contrast
- Consistent control sizes/spacing/radii
- Typographic hierarchy (primary/secondary/timecode/track metadata)
- Stronger state signaling (mute/solo/record/armed/selected)

### High-Value UX Targets

1. Transport as “control center” (clear status, meters, timeline context)
2. Toolbar grouping by task (project, edit, timeline, mix, utility)
3. Track headers with dense but legible metadata and quick actions
4. Clip readability improvements (name, type, selection, handles)
5. Mixer parity with timeline state (selection, color, mute/solo consistency)

### Interaction Quality Bar

- Immediate input feedback (< 16ms perception target)
- Predictable keyboard-first workflow (transport/edit/nav)
- Reduced accidental destructive actions

---

## 4-Phase Roadmap to Competitiveness

## Phase 1 (0–6 weeks): “Looks Pro, Feels Stable”

**Goal**: First impression and daily usability parity with modern DAWs.

- Finish theme system rollout to all visible controls and panels
- Establish spacing, alignment, iconography, and state consistency rules
- Improve timeline readability (grid, labels, clip contrast, playhead legibility)
- Harden core UX reliability (no visual jitter, stable redraw, predictable focus)
- Ship keyboard navigation baseline + command discoverability

**Success metrics**

- New users can complete: create project, import audio, add MIDI track, bounce
- Reduced UI-related bug reports and less visual inconsistency per screen

## Phase 2 (6–14 weeks): “Daily Driver Core DAW”

**Goal**: Core workflows that keep users inside Ampl for real sessions.

- Mature multi-track editing
- Mixer ergonomics (grouping/sends/basic buses/meter clarity)
- Plugin hosting stability and browser usability
- Strong undo/redo semantics across timeline + mixer + editors
- Project templates and starter session presets

**Success metrics**

- End-to-end song draft possible without leaving Ampl
- Session stability for longer projects

## Phase 3 (14–24 weeks): “Human-Centered AI Assistants”

**Goal**: AI helps speed and quality without taking ownership.

- Command palette with explicit preview/diff and confidence
- Mix Assistant v1: gain staging, cleanup, EQ hints (advisory + apply buttons)
- Timing helpers: transient detection + quantize suggestions
- Audio-to-MIDI helper for quick composition transfer
- “Explain this suggestion” transparency panel

**Success metrics**

- High acceptance rate of suggested actions
- Low undo-backout rate for AI-applied patches

## Phase 4 (24+ weeks): “Ecosystem & Retention Moat”

**Goal**: Keep users and teams in Ampl long-term.

- Shared templates, presets, and session packs
- Collaboration primitives (commenting/versioning, then real-time later)
- Hybrid local/cloud model hosting with explicit governance
- Creator marketplace for presets and AI helper modules

**Success metrics**

- Session return rate, template reuse, collaboration adoption

---

## Feature Strategy: Where to Match vs Differentiate

### Match Competitors

- Fast, dependable core DAW operations
- Pro-grade editing + mix ergonomics
- Audio-to-MIDI and stem workflows as practical utilities

### Differentiate Ampl

- Better **human-in-control** AI UX than “one-click magic” competitors
- Deterministic Action DSL with reversible edits as product trust advantage
- Transparent AI provenance + editable outcomes
- Privacy-first local-first creative workflow

---

## 90-Day Execution Backlog (Suggested)

### Sprint Block A: Visual System

- Complete full UI token audit (Theme coverage map)
- Unify button, slider, panel, meter, and selected-state rendering
- Introduce consistent spacing scale and toolbar grouping rules

### Sprint Block B: Timeline/Mixer Professionalism

- Track header action strip (mute/solo/arm/monitor)
- Clip states: selected, hovered, muted, locked
- Mixer-to-track linking, channel color consistency, improved meters

### Sprint Block C: AI Guardrails + First Assistants

- AI action preview cards with “why” + confidence
- Reversible batch apply and per-step accept/reject
- Mix Assistant recommendations limited to non-destructive suggestions first

### Sprint Block D: Validation Loop

- Task-based UX tests with producers
- Instrumented telemetry (local anonymized opt-in) for command completion and undo patterns
- Iterate highest-friction workflows

---

## Risks and Mitigations

- **Risk**: AI complexity delays core DAW quality.
    - **Mitigation**: Core DAW quality gates before each AI milestone.

- **Risk**: Feature sprawl without UX coherence.
    - **Mitigation**: Single design system owner and UX acceptance checklist.

- **Risk**: Competitor feature chasing.
    - **Mitigation**: Prioritize user outcomes (speed, control, trust), not checkbox parity.

---

## North-Star Statement

Ampl should feel like a **serious professional instrument** first and an **intelligent assistant** second: always fast, always editable, always under the musician’s control.
