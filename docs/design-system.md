# DarkSpark Desktop — Design System

Initial visual system for DarkSpark Desktop.

Theme working name: **Legacy**

The token values below are the initial reference for the Legacy theme. They
are a documented starting point, defined here so implementation has a single
source of truth. They may be refined during design review. No external fonts
or icon libraries are required or assumed at this stage.

## Design Goals

- **TRON-inspired, original.** Evokes a luminous dark control surface without
  copying any existing property.
- **High contrast.** Text and key elements read clearly against dark
  backgrounds.
- **Restrained glow.** Illumination is an accent, not the dominant effect.
- **Thin illuminated borders.** Edges are defined by fine light lines rather
  than heavy fills.
- **Readable at 2560×720 on a 14.5-inch display.** Sizing and hierarchy are
  validated first at the XENEON EDGE target.
- **Suitable for touch.** Interactive elements meet touch sizing and spacing.
- **Supports live, static, and transparent backgrounds.** The system works
  over animated, solid, and transparent surfaces.
- **Smooth but subtle animation.** Motion supports understanding; it does not
  distract.
- **Strong information hierarchy.** The eye is guided from most to least
  important information.

## Design Tokens

Token names are stable; values are initial and subject to design review.

### Color

Dark base with restrained cyan and purple accents.

| Token                  | Role                                             | Initial value |
| ---------------------- | ------------------------------------------------ | ------------- |
| `color.background.base`    | Primary background                           | `#05070A`     |
| `color.background.raised`  | Card / panel surface                         | `#0B0F14`     |
| `color.background.overlay` | Elevated overlay surface                     | `#111721`     |
| `color.border.subtle`      | Thin illuminated border, resting             | `#1C2A33`     |
| `color.border.active`      | Thin illuminated border, active / focus      | `#3FE0FF`     |
| `color.accent.cyan`        | Primary accent                               | `#3FE0FF`     |
| `color.accent.purple`      | Secondary accent                             | `#9B7BFF`     |
| `color.text.primary`       | Primary text                                 | `#E6F2F5`     |
| `color.text.secondary`     | Secondary text                               | `#9BB0B8`     |
| `color.text.disabled`      | Disabled text                                | `#4A5A62`     |
| `color.status.warning`     | Warning state                                | `#FFC24B`     |
| `color.status.error`       | Error state                                  | `#FF5C6C`     |
| `color.status.good`        | Nominal / healthy state                      | `#4BE38A`     |

Cyan is the primary accent; purple is secondary and used sparingly. Status
colors are reserved for status and must always be paired with a
non-color indicator (see `STYLE_GUIDE.md`).

### Spacing

A consistent scale, in device-independent pixels.

| Token         | Value |
| ------------- | ----- |
| `space.xxs`   | 2     |
| `space.xs`    | 4     |
| `space.sm`    | 8     |
| `space.md`    | 12    |
| `space.lg`    | 16    |
| `space.xl`    | 24    |
| `space.xxl`   | 32    |

### Border widths

| Token             | Value | Use                          |
| ----------------- | ----- | ---------------------------- |
| `border.hairline` | 1     | Default illuminated edge     |
| `border.thin`     | 2     | Emphasis / active edge       |

Borders stay thin by design. Heavier borders are avoided.

### Corner treatments

| Token          | Value | Use                     |
| -------------- | ----- | ----------------------- |
| `radius.sm`    | 4     | Small controls          |
| `radius.md`    | 8     | Cards and panels        |
| `radius.lg`    | 12    | Large surfaces          |
| `radius.pill`  | 999   | Pill-shaped indicators  |

### Typography roles

Roles are defined; a concrete font family is **not** required yet. The
system uses the platform default sans-serif until a font decision is made.
No decorative or hard-to-read science-fiction font is used for body or data.

| Role              | Purpose                          | Initial size / weight |
| ----------------- | -------------------------------- | --------------------- |
| `type.display`    | Large headline / hero readouts   | 32 / semibold         |
| `type.title`      | Page and card titles             | 22 / semibold         |
| `type.subtitle`   | Secondary headings               | 18 / medium           |
| `type.body`       | Standard text                    | 15 / regular          |
| `type.caption`    | Labels, secondary detail         | 13 / regular          |
| `type.mono`       | Numeric / data readouts          | 15 / regular mono     |

Numeric readouts use a monospaced role so changing digits do not shift
layout.

### Glow levels

Glow is a restrained accent applied to borders, key text, and active
elements. Levels describe intensity, not a specific rendering technique.

| Token         | Intensity | Use                                  |
| ------------- | --------- | ------------------------------------ |
| `glow.none`   | 0         | Default; most surfaces               |
| `glow.subtle` | low       | Resting active edges, key readouts   |
| `glow.strong` | medium    | Focus and pressed emphasis only      |

`glow.strong` is reserved for clear interactive feedback. Glow is never so
strong that it reduces legibility.

### Animation durations

| Token              | Value  | Use                                 |
| ------------------ | ------ | ----------------------------------- |
| `motion.instant`   | 0 ms   | State that must not appear animated |
| `motion.fast`      | 120 ms | Pressed / focus feedback            |
| `motion.standard`  | 220 ms | Page transitions, card changes      |
| `motion.slow`      | 400 ms | Ambient / background transitions    |

Motion is subtle and purposeful. Durations above `motion.slow` are avoided
for interactive transitions.

### Minimum touch target

| Token               | Value |
| ------------------- | ----- |
| `touch.target.min`  | 44    |

All interactive elements present at least a 44×44 device-independent-pixel
touch target, regardless of their visual size.

### Interaction and status states

Every interactive element defines these states. Each state is visually
distinct without relying on color alone.

| State      | Intent                                             |
| ---------- | -------------------------------------------------- |
| `focus`    | Element has input focus; uses `border.active` and `glow.strong`. |
| `pressed`  | Element is being activated; uses `motion.fast` feedback. |
| `disabled` | Element is not interactive; uses `color.text.disabled` and no glow. |
| `warning`  | Attention needed; uses `color.status.warning` plus an icon or label. |
| `error`    | Failure state; uses `color.status.error` plus an icon or label. |

Color is never the sole signal for `warning` or `error`; a shape, icon, or
text label always accompanies it.

## Constraints

- No external font dependency at this stage.
- No external icon library dependency at this stage.
- No token here implies a rendering technology or a specific widget.
- Values marked initial may change under design review; **token names are
  intended to be stable** so implementation can reference them safely.
