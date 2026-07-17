# DarkSpark Visual Language

## Purpose

This document defines the visual language of DarkSpark Desktop.

It establishes the rules used to create a consistent, recognizable, and
high-quality interface across Desktop mode, Deck Mode, dashboard cards,
navigation, dialogs, settings, and future modules.

DarkSpark should feel like a modern digital command center.

The interface should be:

- deliberate
- readable
- restrained
- responsive
- touch friendly
- technically precise
- visually distinctive

The visual direction may draw subtle inspiration from science-fiction control
systems, illuminated glass cockpits, mission-control displays, and TRON-style
light architecture.

DarkSpark must not become visually noisy, excessively neon, difficult to read,
or dependent on novelty typography.

---

## Core Principle

Every visual element must communicate one or more of the following:

- hierarchy
- state
- interaction
- grouping
- navigation
- importance

Decoration without purpose should be avoided.

The interface should look advanced because it is consistent and intentional,
not because every surface glows.

---

## Design Character

DarkSpark uses:

- deep neutral backgrounds
- restrained cyan illumination
- secondary purple accents
- high-contrast primary text
- muted supporting text
- thin structural borders
- controlled glow
- generous spacing
- smooth, brief motion
- clear information hierarchy

The experience should feel calm when idle and responsive when used.

---

## Color Roles

Colors must be used by semantic role rather than by arbitrary widget choice.

### Background roles

- Application background: deepest neutral surface
- Page background: nearly identical to the application background
- Card background: slightly elevated neutral surface
- Interactive overlay: subtle illuminated tint
- Disabled surface: reduced-contrast neutral surface

### Accent roles

- Cyan: primary DarkSpark accent
- Purple: secondary accent and alternate grouping
- White: primary information and important labels
- Muted blue-grey: secondary information
- Amber: warning
- Red: error or destructive state
- Green: success or confirmed healthy state

### Accent discipline

Cyan is the default accent.

Purple should be used selectively to:

- distinguish a secondary group
- identify an alternate category
- provide rhythm across a page
- highlight a selected secondary element

Warning, error, and success colors override decorative accent colors.

State must always take precedence over accent.

Color must never be the only method used to communicate state.

---

## Typography

DarkSpark uses system fonts supplied by the operating system.

Do not add external font dependencies during the foundation phases.

Typography should prioritize:

- readability
- clarity
- consistent hierarchy
- good rendering on Fedora and Linux desktops
- readability on the XENEON EDGE display

### Typography hierarchy

#### Application title

Used for major application identity.

- strongest visual weight
- used sparingly
- never repeated unnecessarily

#### Page title

Used once per page.

- prominent
- immediately recognizable
- visually separated from card content

#### Page subtitle

Describes the page's purpose.

Examples:

- Hardware & Performance
- Media & Playback
- Messages & Notifications
- Your Digital Workspace

The subtitle should remain quieter than the page title.

#### Card title

The strongest text inside a card.

It should be recognizable immediately when scanning the display.

#### Card subtitle

Describes the subject, measurement, or purpose of the card.

Examples:

- Processor utilization
- Physical memory
- Active output
- Recent notifications

#### Primary value

The largest information inside a data card.

Examples:

- 12%
- 48 GB
- Online
- Completed

#### Supporting value

Used for units, labels, and supplementary information.

#### Status text

Placed near the bottom of a card.

Status text must be concise and visually subdued unless it communicates a
warning or error.

---

## Typography Scale

The implementation may adapt exact pixel sizes for display scaling, but relative
hierarchy must remain consistent.

Recommended starting scale:

- Application title: 28 px
- Page title: 26 px
- Page subtitle: 15 px
- Card title: 20 px
- Card subtitle: 14 px
- Primary value: 34 px
- Supporting text: 13 px
- Status text: 12 px
- Small annotation: 11 px

Font weight should provide hierarchy before color is added.

Avoid excessive use of bold text.

---

## Spacing System

DarkSpark uses a consistent spacing scale.

Recommended tokens:

- space.xs: 4 px
- space.sm: 8 px
- space.md: 12 px
- space.lg: 16 px
- space.xl: 24 px
- space.2xl: 32 px
- space.3xl: 48 px

All margins, padding, gutters, and internal gaps should use these tokens where
practical.

Avoid isolated one-off spacing values.

---

## Page Anatomy

A Deck page should use the following structure:

1. page header
2. page subtitle
3. card grid
4. page navigation indicator

The page header should not compete with card information.

Recommended structure:

```text
SYSTEM
Hardware & Performance

┌───────────────┐  ┌───────────────┐
│ Card          │  │ Card          │
└───────────────┘  └───────────────┘
