# DarkSpark Desktop

A native, modular, touch-friendly Desktop Command Center.

> **Knowledge Belongs to All.**

## Overview

DarkSpark Desktop is a native desktop application that presents system
information and controls as a set of swipeable, touch-first pages. It is
built to feel like a dedicated control surface rather than a conventional
windowed utility, while still running correctly inside an ordinary desktop
window.

The **Corsair XENEON EDGE** at its native **2560×720** resolution on a
14.5-inch panel is the primary design and validation target. The application
must remain fully usable on other displays and in a standard resizable
desktop window. The XENEON EDGE is a design and validation target, not a
hardware dependency.

## Mission

Provide a fast, legible, touch-first Desktop Command Center that surfaces the
information and controls a user cares about, degrades gracefully when an
integration is unavailable, and stays maintainable through clear
architectural boundaries.

## The Desktop Command Center Concept

DarkSpark Desktop is a **Desktop Command Center**: it organizes functionality
into **pages**, where each page is a layout container that arranges reusable
**cards**. The user moves between pages with touch navigation (swipe or an
equivalent gesture) and a page indicator shows position within the set.

Two presentation experiences are planned:

- **Standard desktop window** — a normal resizable window for development
  and everyday desktop use.
- **Deck Mode** — the touch-first, full-surface experience intended for the
  Corsair XENEON EDGE or a similarly proportioned display.

The standard desktop window may host or launch Deck Mode. It is not assumed
that the standard desktop window uses the same page and card framework as
Deck Mode; see `docs/architecture.md` for the current boundary.

## Primary Target: Corsair XENEON EDGE

- Native resolution: **2560×720**
- Panel size: 14.5-inch
- Interaction: touch-first

Layouts, typography, spacing, and touch targets are validated first at
2560×720, then checked for graceful behavior on standard displays.

## Current Status

Documentation foundation in progress.

- Repository structure created and committed.
- No C++ source files exist yet.
- No build system is configured yet.
- No dependencies are declared yet.

This repository currently contains **documentation only**. Nothing in the
application is implemented.

## High-Level Feature Vision

These are directional goals for future stages, **not** current
capabilities:

- System monitoring cards
- Music and media presentation and controls
- Quick actions
- Weather and ambient information
- Embedded homepage and web panels
- Animated, static, and transparent backgrounds
- Customizable, persistent layouts
- A plugin and integration framework

Each of these is staged in `docs/roadmap.md`. None of them exist yet.

## Build Status

**Not yet implemented.**

There is no build system, no compilable source, and no runnable
application at this stage. Build and run instructions will be added when the
Deck-0 application shell begins.

## Relationship to EdgeDisplayWidgets

An existing browser-based project named **EdgeDisplayWidgets** is used
**only** as a product and interaction reference. It demonstrates swipeable
pages, system monitoring, music/media, quick actions, weather, embedded web
content, animated backgrounds, and customizable layouts.

DarkSpark Desktop is **not** a code port of EdgeDisplayWidgets. It is a
native application with its own architecture. EdgeDisplayWidgets informs
product decisions and interaction expectations; it does not constrain
implementation.

## Initial Development Workflow

1. Read `docs/FOUNDATION.md` first. It is the project constitution and its
   principles are non-negotiable.
2. Review `docs/architecture.md` to understand directory responsibilities
   and dependency direction.
3. Review `docs/CODING_STANDARDS.md` and `docs/STYLE_GUIDE.md` before
   writing any code or user-facing text.
4. Consult `docs/design-system.md` for the Legacy theme tokens.
5. Work against the staged plan in `docs/roadmap.md`. The current active
   scope is the Foundation Documentation stage; the next stage is Deck-0.
6. No feature is implemented before it fits the documented architecture.

## License

The intended project license is **GPL-3.0-or-later**.

The `LICENSE` file in the repository is currently empty. Populating it with
the full GPL-3.0-or-later text is a repository action to be performed before
the first code batch; it is deliberately not done as part of this
documentation batch.
