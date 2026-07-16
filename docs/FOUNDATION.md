# DarkSpark Desktop — Foundation

This document is the project constitution. Its principles are
non-negotiable. Where a proposed feature or change conflicts with this
document, the document wins until it is deliberately and explicitly amended.

> **Knowledge Belongs to All.**

## Non-Negotiable Principles

### Class design

- **One responsibility per class.** A class does one thing. If a class needs
  the word "and" to describe its job, it is two classes.
- **Class names end with what they are.** Use suffixes such as `Service`,
  `Card`, `Page`, `Window`, `Manager`, `Model`. A reader should know a
  type's role from its name.

### UI

- **UI never collects or owns external data.** UI displays data and emits
  user intent. It does not poll sensors, open sockets, read files, or hold
  authoritative state.
- **Pages are layout containers.** A page arranges cards and manages its own
  layout. It does not contain business logic.
- **Cards are reusable presentation components.** A card renders a defined
  piece of information or control. Cards are composable and do not assume a
  specific page.

### Services

- **Services never depend on UI.** A service must be fully usable and
  testable without any UI type present. Services expose data and signals;
  they do not reach into widgets.

### Models

- **Models contain data and state only.** Models hold values and state
  transitions. They contain no UI and no service dependencies.

### Interfaces

- **Interfaces define dependency boundaries.** Where one layer needs another,
  the contract is expressed as an interface. Concrete cross-layer coupling is
  avoided.

### Themes

- **Themes never change application logic.** A theme controls appearance only
  — color, spacing, glow, borders, typography roles. Changing the theme must
  never change behavior.

### Restraint

- **No speculative abstractions.** Do not add layers, factories, or
  indirection for hypothetical future needs.
- **No class created only because it may be useful later.** Every type must
  earn its place by serving a current, approved need.
- **No silent architectural changes.** Any change to boundaries, dependency
  direction, or responsibilities is proposed and reviewed, never slipped in.
- **Features must fit the architecture before implementation.** If a feature
  does not fit, the architecture is discussed first. Implementation does not
  lead the design.

### Product posture

- **Desktop Command Center first.** The product is a Desktop Command Center.
  Design decisions serve that identity.
- **Touch-first but not hardware-locked.** Interactions are designed for
  touch, and the application still works with mouse and keyboard and on
  displays other than the XENEON EDGE.
- **Graceful failure for unavailable integrations.** When an integration or
  data source is unavailable, the application shows a clear unavailable or
  degraded state and keeps running. It never crashes or hangs because
  something optional is missing.
- **Performance and responsiveness are product requirements.** Smoothness,
  input latency, and frame stability are treated as features, not polish.

### Scope boundary for this stage

- **No mention of or dependency on DarkSpark Core at this stage.** DarkSpark
  Desktop is developed as a self-contained application. It does not reference,
  link, or assume any separate core project at this stage.

## Dependency Direction

Dependencies are **not** a single linear chain. `application/` is a
composition root that connects two sides — UI and services — which meet only
at interfaces and models.

The rules:

- **`application/` is the composition root.** It creates, owns, and connects
  the top-level objects. It is the only place where concrete UI and concrete
  services are assembled together.
- **`desktop/` and `deck/` may depend on approved interfaces and models.**
  UI depends on contracts and plain data, not on service internals.
- **`services/` implement interfaces and may depend on models.** A service
  provides behavior behind an interface and produces or consumes models.
- **UI must not depend on concrete service implementations.** UI reaches
  services only through interfaces, never by including a concrete service
  type.
- **Services must not depend on UI.** No service includes, references, or is
  compiled against any UI type.
- **Models must not depend on UI or services.** Models are the most
  independent layer. They know nothing about who displays or produces them.
- **Interfaces remain focused dependency boundaries**, not a collection of
  abstractions created in advance. See "Interfaces are introduced on demand"
  below.
- **Themes must not contain behavior.** Themes are data describing
  appearance. No control flow, no service calls.
- **Utilities must not become a miscellaneous dumping ground.** A utility
  must have a clear, narrow purpose. "Miscellaneous helper" is not a purpose.
  When in doubt, a piece of logic belongs to a model, service, or UI type —
  not to utilities.

### Conceptual diagram

```
                    Application
                   /           \
                  v             v
          Desktop / Deck     Services
                 |              |
                 v              v
             Interfaces <---- Implementations
                 |
                 v
               Models
```

This diagram is **conceptual**. It shows how the layers relate; it does
**not** require every feature to have an interface. `application/` composes
both sides. UI and service implementations meet at interfaces, and both
ultimately rest on models.

### Interfaces are introduced on demand

An interface is created only when a **real** dependency boundary,
substitution need, or test seam exists. Interfaces are not added
speculatively "because there might be an implementation later." A feature
with a single concrete collaborator and no substitution or test-seam need
does not require an interface until one of those needs is real.

### Why this direction

The direction keeps the two most valuable properties of the codebase intact:

1. **Services are testable in isolation** because they never touch UI.
2. **UI is replaceable** because it depends only on interfaces and plain
   models, not on service internals.

Any dependency that would reverse a rule above (for example, a service
including a card, a UI type including a concrete service, or a model calling a
service) is a violation of this foundation and must be resolved before merge.
