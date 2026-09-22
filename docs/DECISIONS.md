# DarkSpark Desktop Architecture Decisions

This document records significant architectural and product decisions that shape the evolution of DarkSpark Desktop.

Each decision should include:

- Identifier
- Date
- Status
- Decision
- Rationale
- Consequences

---

## ADR-001

**Date:** 2026-07-15

**Status:** Accepted

### Decision

DarkSpark Desktop is an independent project.

### Rationale

The Desktop is responsible for the user experience and workspace.
It must remain modular and capable of evolving independently.

### Consequences

The repository focuses solely on the native desktop application.

---

## ADR-002

**Date:** 2026-07-15

**Status:** Accepted

### Decision

The primary product concept is a Desktop Command Center.

### Rationale

The application unifies monitoring, media, communication, navigation, and productivity into a single touch-friendly workspace.

### Consequences

Future features should support the Desktop Command Center vision.

---

## ADR-003

**Date:** 2026-07-15

**Status:** Accepted

### Decision

Pages and DashboardCards remain under `src/deck`.

### Rationale

Deck Mode is currently the only presentation model using this architecture.

### Consequences

Shared abstractions will only be extracted if future Desktop requirements demonstrate a genuine need.

---

## ADR-004

**Date:** 2026-07-15

**Status:** Accepted

### Decision

Services never depend on the UI.

### Rationale

Maintains clean separation between presentation and data.

### Consequences

DashboardCards consume interfaces and models, not concrete service implementations.

---

## ADR-005

**Date:** 2026-07-15

**Status:** Accepted

### Decision

Architecture precedes implementation.

### Rationale

The project should grow through deliberate architectural decisions rather than incremental feature additions.

### Consequences

Major architectural changes require documentation before implementation.

---

## ADR-006

**Date:** 2026-09-18

**Status:** Accepted

### Decision

DarkSpark Desktop will include a Companion subsystem. Companion state is a
UI-independent model; Deck renders that state; future camera, audio, speech,
AI, and automation services publish through explicit interfaces assembled by
the Application.

The primary visual identity is a single red optical lens embedded in the
Command dashboard. The lens accepts normalized subject coordinates separately
from the camera's physical PTZ controls, allowing the on-screen gaze and PIXY
tracking to respond to the same locally detected subject.

Companion-0 is hardware-independent and includes only the state vocabulary,
animated presentation, and manual development controls. It does not access a
camera or microphone, perform recognition, contact an AI service, or execute
system commands.

### Rationale

The Companion is now an approved part of the Desktop Command Center product.
Beginning with a hardware-independent presentation establishes the interaction
language and a safe dependency seam before the EMEET PIXY arrives.

### Consequences

- Companion UI remains usable when optional hardware or services are absent.
- Camera/audio/AI implementations cannot be placed in Deck widgets.
- Computer actions will require an explicit allow-listed action boundary.
- Privacy state must be visible whenever sensing features are introduced.

---

## ADR-007

**Date:** 2026-09-18

**Status:** Accepted

### Decision

The permanent first three Deck pages and reserved fourth slot are ordered as
follows:

1. Command — the HAL-inspired Companion eye and command summary; always active
   at startup.
2. System — monitoring plus audio transport and volume controls.
3. Control Deck — a Stream Deck-style Fedora launcher grid plus audio controls.
4. Expansion — an intentionally unconfigured slot whose purpose will be chosen
   from a real future requirement.

Existing swipe, keyboard, and page-indicator navigation remain the only owners
of page movement. Desktop controls cross an allow-listed service boundary; UI
widgets never construct or execute shell commands.

### Rationale

The ordering matches the XENEON EDGE's daily purpose: Companion presence first,
health information one swipe away, and high-frequency controls one additional
swipe away.

### Consequences

- Page zero must remain Command unless the decision is deliberately revised.
- The Expansion page must not gain speculative features merely to fill space.
- Control actions are enum values, not arbitrary command strings.
- Missing optional Fedora utilities produce visible degraded feedback.
- Future HAL voice/audio intents reuse the same ControlAction allow list and
  DesktopControlService as touch controls.
