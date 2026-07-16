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
