# DarkSpark Desktop — Roadmap

Staged plan for DarkSpark Desktop. Each stage builds on the previous one and
is implemented only after it fits the architecture. Nothing in a later stage
is implemented early. Listing a capability here does **not** mean it exists.

## Stages

### Foundation Documentation (current)

Establish the project constitution, architecture, design system, coding
standards, style guide, and this roadmap. Documentation only. No code, no
build system, no dependencies.

### Deck-0: native application shell

The first implementation stage. Produces a runnable native shell with
navigation and placeholders, and nothing more. Scope is fixed below.

### Deck-1: system monitoring

Introduce real system monitoring cards, fed by services through models and
interfaces. First use of the service → model → interface → card data flow.

### Companion-0: animated local presence (current)

Establish the DarkSpark Companion without requiring camera or audio hardware:

- data-only Dormant, Idle, Listening, Thinking, Speaking, and Alert states
- animated single red optical lens embedded in the Command dashboard
- normalized gaze-target input with a hardware-free tracking simulation
- touch-friendly manual state controls for development and demonstration
- a presentation seam for future services

Explicitly excluded from Companion-0: camera/microphone access, face or presence
recognition, wake-word detection, speech-to-text, text-to-speech, network AI,
and computer automation.

The core Deck order is Command, System, Control Deck, then Expansion. Command
holds the Companion eye and is always page zero at boot. System combines
monitoring with audio transport/volume controls. Control Deck provides an
allow-listed Fedora launcher grid and a second audio-control surface while
preserving swipe navigation. Expansion is a deliberately empty fourth slot;
its purpose remains undecided.

When local voice reaches Companion-2, HAL audio commands reuse the existing
ControlAction allow list and DesktopControlService. Voice input does not create
a parallel command executor and cannot supply arbitrary shell text.

### Companion-1: PIXY eyes and ears

After the device interfaces are verified on Fedora, add optional camera,
microphone, PTZ, privacy, and presence services. Hardware failure must leave the
Command dashboard functional in an explicit unavailable state.

### Companion-2: local voice loop

Add local wake-word detection, bounded speech capture, speech-to-text, and
text-to-speech. Listening state must be visible and audio capture must be
session-bounded rather than continuous cloud transcription.

### Companion-3: conversational command center

Add an AI conversation service plus allow-listed DarkSpark actions. Arbitrary
model-generated shell execution is outside the approved design.

### Deck-2: customizable and persistent layouts

Allow arranging cards and pages, and saving and restoring that arrangement.
Introduces layout persistence.

### Deck-3: music and media

Media presentation and controls.

### Deck-4: chat and communications

Chat and communication surfaces.

### Deck-5: embedded homepage and web panels

Embedded homepage and web content panels.

### Deck-6: plugin and integration framework

A framework for plugins and integrations, attaching along the interface and
card seams anticipated in the architecture.

### Desktop expansion beyond the XENEON EDGE

Broaden first-class support to additional displays and window configurations
beyond the primary XENEON EDGE target.

## Deck-0 Scope (Fixed)

Deck-0 is limited to exactly the following:

- CMake foundation (build system and structure).
- Application startup.
- Standard development window.
- DeckWindow.
- Display enumeration and selection.
- Five placeholder pages.
- Swipe or equivalent touch navigation.
- Page indicator.
- Reusable DashboardCard placeholder.
- Centralized Legacy theme.
- Safe exit from fullscreen.

The intent of Deck-0 is a navigable, themed shell: real pages and cards as
placeholders, real navigation, real theming, running in both a standard
window and a Deck window — with no real data behind anything.

### Explicitly Excluded from Deck-0

The following are **not** part of Deck-0 and must not be implemented in it:

- Real monitoring.
- Media APIs.
- Chat integrations.
- WebEngine / embedded web content.
- Databases.
- Plugins.
- Layout persistence.
- Network access.
- AI features.

## Rules Across All Stages

- A stage is implemented only after it fits the documented architecture.
- No capability from a later stage is implemented early.
- Documentation and implementation stay mutually consistent.
- Unimplemented capabilities are never described as if they already work.
