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
