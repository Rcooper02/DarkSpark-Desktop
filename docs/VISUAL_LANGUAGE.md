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
```

Page titles may be displayed in uppercase visually, but source strings should
remain normally capitalized unless uppercase is intentionally part of the text.

---

## Dashboard Card Anatomy

DashboardCard is the visual foundation of Deck Mode.

A card should contain four logical regions:

```text
┌──────────────────────────────────────┐
│ Header                               │
│ Title                         State  │
│ Subtitle                             │
│                                      │
│ Content                              │
│ Primary information or placeholder   │
│                                      │
│ Footer                               │
│ Status                               │
└──────────────────────────────────────┘
```

### Header

Contains:

- card title
- optional indicator
- optional future action affordance

### Subtitle

Provides context without repeating the title.

### Content Region

Contains the primary information.

In placeholder cards, this region should still occupy useful visual space so
the card feels complete rather than collapsed.

### Footer

Contains:

- state
- update information
- concise supporting context

The footer should be visually separated from the main content with spacing or a
subtle divider.

---

## Card Size Roles

Supported roles:

### Small

Used for compact status and utility information.

### Medium

Default general-purpose card.

### Large

Used when information deserves greater emphasis or needs additional vertical
space.

### Wide

Used for timelines, summaries, media, charts, or multi-column information.

Size roles are layout hints.

They must not contain business logic or persistence behavior.---

## Card Borders

Borders should establish structure without dominating the interface.

Default cards should use:

- a thin low-contrast border
- subtle corner rounding
- limited illumination

Focused or selected cards may increase border brightness.

Warning and error states override accent borders.

Avoid bright borders around every card simultaneously.

---

## Corners

Corner radius should feel modern but technical.

Recommended starting values:

- small controls: 6 px
- cards: 10 px
- large surfaces: 12 px

Avoid exaggerated pill-shaped cards or controls unless the element is
specifically a compact status indicator.

---

## Glow

Glow is an accent, not a background effect.

Glow may be used for:

- keyboard focus
- active navigation
- selected cards
- primary call-to-action controls
- brief interaction feedback

Glow should not:

- surround all cards at full intensity
- reduce text clarity
- obscure borders
- become a permanent large-radius blur

Default card glow should be effectively absent or extremely subtle.

---

## Dividers

Dividers may be used between:

- card header and content
- content and footer
- major page groups

Dividers should be lower contrast than borders.

They should improve scanning without creating a table-like appearance.

---

## Status Indicators

Status indicators must communicate using more than color.

A status indicator may combine:

- shape
- icon or glyph
- short text
- color
- opacity

Preferred long-term implementation:

- custom-painted Qt indicator
- scalable at different display densities
- no reliance on uncertain Unicode glyph coverage

State examples:

- normal: filled circle or steady mark
- loading: animated ring or pulse
- empty: hollow circle
- unavailable: horizontal mark
- warning: triangle
- error: cross
- disabled: muted ring or line---

## State Priority

State styling follows this priority:

1. error
2. warning
3. unavailable
4. disabled
5. loading
6. focused or pressed interaction
7. decorative accent
8. normal

A card's semantic state must remain unmistakable even when an accent role is
also assigned.

---

## Interaction States

All interactive cards and controls should support:

- normal
- keyboard focus
- pressed
- disabled

Hover may enhance desktop use, but no essential interaction may depend on hover.

### Focus

Keyboard focus must be clearly visible.

Focus should use:

- brighter structural border
- controlled cyan illumination
- no large layout shift

### Pressed

Pressed state should feel immediate.

Recommended behavior:

- subtle background illumination
- small border intensity change
- no exaggerated movement
- no delayed response

### Disabled

Disabled elements should remain readable but clearly inactive.

Do not simply lower opacity until text becomes difficult to read.

---

## Motion

Motion should explain change and reinforce interaction.

It must not delay the user.

Recommended durations:

- immediate feedback: 80–120 ms
- standard transition: 180–240 ms
- page transition: 220–320 ms
- ambient loading animation: continuous but restrained

Recommended easing:

- OutCubic for entry
- InOutCubic for movement
- OutQuad for brief interaction feedback

Avoid:

- bouncing
- elastic motion
- excessive overshoot
- large zoom effects
- simultaneous animation of every element---

## Page Transitions

Page transitions should preserve spatial understanding.

Recommended direction:

- next page moves left
- previous page moves right
- content fades slightly during motion
- page indicator updates in synchronization

Animations must remain smooth on the target Fedora system and XENEON EDGE
display.

The user must be able to navigate quickly without waiting for an animation to
finish.

---

## Page Indicator

The page indicator is a signature component of Deck Mode.

It should communicate:

- total number of pages
- current page
- movement direction
- interaction progress where appropriate

The current page marker may be:

- larger
- brighter
- softly illuminated

Inactive markers should remain visible but quiet.

Avoid excessive size or distracting animation.

---

## Touch Targets

Deck Mode is touch first.

Recommended minimum interactive target:

- 44 × 44 px

Preferred for primary controls:

- 52 × 52 px or larger

Spacing must reduce accidental activation.

No important action should require precision clicking.

---

## Accessibility

DarkSpark must remain usable without relying exclusively on:

- color
- hover
- animation
- extremely small text
- low-contrast decoration

Keyboard focus must remain visible.

Animations should eventually respect reduced-motion preferences when settings
support is introduced.

Text should remain readable against every supported surface.---

## Icons

During early Deck phases:

- use no external icon library
- avoid inconsistent Unicode symbols
- prefer custom-painted geometric indicators where practical
- use text placeholders only when no meaningful visual exists

Future icon work should use one consistent icon system.

Do not mix multiple visual icon styles.

---

## Desktop Mode

Desktop mode and Deck Mode should share:

- colors
- typography hierarchy
- corner rules
- control states
- spacing tokens

Desktop mode may be denser than Deck Mode because it is primarily mouse and
keyboard driven.

Deck Mode must remain touch optimized.

---

## Visual Restraint

DarkSpark should never become:

- a wall of neon
- an imitation movie interface
- a collection of unrelated widget styles
- unreadable science-fiction typography
- a permanently animated display
- a dashboard where all information has equal visual importance

A small number of strong visual decisions should define the product.

---

## Validation Questions

Every visual batch should be reviewed using these questions:

1. Can the page purpose be understood immediately?
2. Is the most important information visually dominant?
3. Are state and interaction clear without relying only on color?
4. Is spacing consistent?
5. Does the interface remain readable at 2560 × 720?
6. Does it still work in a resizable development window?
7. Are touch targets large enough?
8. Is motion useful rather than decorative?
9. Does the page feel calm when idle?
10. Does it look and feel recognizably like DarkSpark?

---

## Current Deck-1 Priorities

The current visual priorities are:

1. improve card anatomy
2. improve typography hierarchy
3. establish page title and subtitle treatment
4. reduce default border intensity
5. improve focus and pressed feedback
6. replace fragile status glyphs with painted indicators
7. refine the page indicator
8. add restrained page-transition polish

No live system data or external integrations should be introduced until the
visual foundation is accepted.
