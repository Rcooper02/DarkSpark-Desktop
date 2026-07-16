# DarkSpark Desktop — Architecture

This document describes the responsibilities of each source directory, the
initial runtime architecture, and the concepts the application is built
around. It records only decisions that have been approved. It does not
invent implementation details.

Read `docs/FOUNDATION.md` first; the dependency rules there govern
everything below.

## Source Directory Responsibilities

### `src/application`

Application entry and composition. Responsible for process startup, choosing
the startup mode (standard window or Deck Mode), constructing top-level
objects, and wiring them together. This is the only place where the concrete
layers are assembled. It owns lifetime of the top-level window and the page
management. It contains no card-level logic and no service logic.

### `src/core`

Essential application-wide infrastructure: components with lifecycle,
coordination, or application-policy implications. `core` must not become a
home for speculative frameworks. Infrastructure such as event buses, logging
façades, configuration systems, and global state is **not** created here
until an approved requirement needs it. When something has no lifecycle,
coordination, or policy role, it does not belong in `core`. See
"`core/` versus `utilities/`" below for the exact boundary.

### `src/desktop`

The standard desktop window presentation: hosting the application inside a
conventional resizable window suitable for development and everyday desktop
use. The standard desktop window may host or launch Deck Mode. It is **not**
assumed to use the same page and card framework as Deck Mode; `desktop` and
`deck` are distinct presentations at this stage.

### `src/deck`

The Deck Mode presentation: the full-surface, touch-first Desktop Command
Center experience intended for the Corsair XENEON EDGE and similar displays.
`deck` is the home of the page experience and its sub-parts.

#### `src/deck/pages`

Deck page types. A page is a layout container that arranges cards. Pages own
their layout and composition of cards. Pages contain no business logic and
do not own external data. **`DeckPage` is currently a Deck presentation
concept**, not a shared UI abstraction.

#### `src/deck/cards`

Reusable Deck presentation components. A card renders one defined piece of
information or one control. Cards are composable within Deck Mode, do not
assume a specific page, and receive their data through models and interfaces
rather than collecting it themselves. **`DashboardCard` is currently a Deck
presentation concept**, not a shared UI abstraction.

#### `src/deck/navigation`

Movement between pages: touch navigation (swipe or equivalent) and the page
indicator. Navigation translates user gestures into page changes and
reflects current position. It does not own page content.

#### `src/deck/animations`

Transition and motion definitions used by the Deck Mode presentation — page
transitions, feedback motion, and similar. Animations communicate state,
navigation, or feedback. They are presentation only and carry no logic that
changes application behavior.

### `src/interfaces`

Interface (contract) definitions that express dependency boundaries between
layers. Interfaces let UI depend on a contract rather than a concrete
service, and let services be substituted or tested. Interfaces contain no
implementation.

### `src/models`

Data and state types. Models hold values and represent state. They depend on
nothing in UI or services. They are the shared vocabulary that services
produce and UI consumes.

### `src/services`

Providers of data and behavior that is not UI. Services gather or compute
information, expose it as models, and communicate changes through interfaces
and signals. Services never depend on UI and are usable and testable
headlessly.

### `src/themes`

Appearance definitions — the Legacy theme and any future themes. Themes are
data describing color, spacing, border, glow, and typography roles. Themes
contain no behavior and never alter application logic.

### `src/utilities`

Small, stateless, narrowly scoped helpers, each independently testable.
Utilities own no application lifecycle and carry no domain policy. They must
not accumulate unrelated functionality. Anything with lifecycle,
coordination, policy, or domain meaning belongs in `core`, a model, a
service, or a UI type instead. See "`core/` versus `utilities/`" below.

## `core/` versus `utilities/`

These two directories are easy to confuse, so the boundary is recorded
explicitly.

**`core/`**

- Essential application-wide infrastructure.
- Components with lifecycle, coordination, or application-policy
  implications.
- Must not become a home for speculative frameworks.
- Event buses, logging façades, configuration systems, and global state are
  not created until an approved requirement needs them.

**`utilities/`**

- Small, stateless, narrowly scoped helpers.
- Independently testable.
- No application lifecycle ownership.
- No domain policy.
- Must not become a miscellaneous dumping ground.

If a component does not fit clearly in either directory, its ownership must
be reconsidered rather than placed arbitrarily. The question "does this have
lifecycle, coordination, or policy?" separates the two: yes points to
`core`, no points to `utilities` — and if it has domain meaning, it belongs
to a model, service, or UI type instead.

## Initial Runtime Architecture

The approved initial object relationship is:

```
Application  (composition root)
  -> DesktopWindow   (standard desktop window)
  -> DeckWindow      (Deck Mode)
       -> PageManager
            -> DeckPage
                 -> DashboardCard
```

- **Application** is the composition root. It selects the startup mode and
  constructs the appropriate top-level window.
- **DesktopWindow** hosts the standard desktop window. It may host or launch
  Deck Mode. It is not assumed to use the `PageManager` / `DeckPage` /
  `DashboardCard` framework; that framework currently belongs to Deck Mode.
- **DeckWindow** hosts the Deck Mode experience.
- **PageManager** owns the set of Deck pages and the current-page concept,
  and cooperates with navigation.
- **DeckPage** is a Deck page: a layout container arranging cards.
- **DashboardCard** is a reusable Deck card rendering one unit of information
  or control.

The `PageManager → DeckPage → DashboardCard` path is a **Deck Mode** concept.
Any shared page/card abstraction between Desktop and Deck is extracted later
only if real Desktop requirements demonstrate common behavior; it is not
assumed now. These names describe the initial structure. Concrete class
contents are defined when the corresponding roadmap stage begins.

## Later Data Flow

Once services exist, data reaches the UI in one direction:

```
Service
  -> Model / state
  -> Interface / signal
  -> DashboardCard
```

A service produces or updates a model, publishes the change across an
interface or signal, and a card renders the result. Cards never call
services directly and never collect data on their own. This preserves the
dependency direction defined in the foundation.

## Key Concepts

### Application startup modes

The approved requirements for startup are:

- **A normal development mode must exist** — a standard, resizable desktop
  window for development and general desktop use.
- **A Deck fullscreen mode must exist** — the touch-first, full-surface Deck
  Mode experience.
- **Manual display selection must be supported** — the user can direct Deck
  Mode to a chosen display.
- **Invalid selections must fall back safely** — an invalid or unavailable
  display selection results in a safe, defined fallback rather than a failure.

The precise mechanism for selecting the mode and target display — the exact
command-line syntax and any future persistence of the choice — is **not**
decided here. It will be decided in the Deck-0 implementation plan. Only the
requirements above are approved at this stage.

### Display enumeration

The application is aware of available displays so Deck Mode can target the
intended surface and so layouts can adapt. Display enumeration and selection
are part of the Deck-0 scope, subject to the "manual display selection" and
"safe fallback" requirements above; the mechanism is defined when that stage
begins.

### Page navigation

The user moves between pages using touch navigation (swipe or an equivalent
gesture), and a page indicator communicates position within the page set.
Navigation is touch-first and must also work with non-touch input.

### Card composition

Within Deck Mode, pages are composed of cards. A card is self-contained and
reusable across Deck pages. Composition is a layout concern owned by pages;
cards do not decide where they live.

### Future layout persistence

Customizable and persistent layouts are a **future** capability
(see the roadmap). The architecture anticipates that layouts will be
describable as data so they can be saved and restored, but no persistence
mechanism is defined or implied at this stage. No feature depends on it yet.

### Future plugin boundaries

A plugin and integration framework is a **future** capability. The intent is
that interfaces and the card model form the seam along which future plugins
and integrations attach, so that adding one does not require changing the
core page/card architecture. No plugin mechanism, format, or loading model
is defined at this stage.

## Out of Scope for This Document

This document does not define concrete class members, threading models,
build configuration, or integration protocols. Those are established in the
roadmap stage that introduces them, under review, and in accordance with the
foundation.
