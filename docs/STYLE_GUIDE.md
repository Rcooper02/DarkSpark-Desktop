# DarkSpark Desktop — Style Guide

Conventions for user-facing text, interaction, and presentation. This guide
keeps the product legible, touch-first, and consistent. It complements the
Legacy theme in `docs/design-system.md`.

## Language and Labels

- **Concise labels.** Labels are short and direct. Prefer a noun or a short
  verb phrase over a sentence.
- **Readable status text.** Status is stated plainly. A user should
  understand a state without decoding it.
- **No unnecessary jargon.** Use plain terms. Technical vocabulary appears
  only where it is the clearest option for the intended user.

## Interaction

- **No hover-only interactions.** Every action reachable by hover is also
  reachable by touch. Hover may enhance, but never gates functionality.
- **Touch targets.** Interactive elements meet the minimum touch target
  defined in the design system (`touch.target.min`, 44×44), including spacing
  so adjacent targets are not easily mis-tapped.

## Naming in the Product

Consistent terminology across UI elements:

- **Buttons** are named by the action they perform, as a short verb or verb
  phrase (for example, "Refresh", "Open Settings").
- **Cards** are named by the information or control they present (for
  example, "System Load", "Now Playing").
- **Pages** are named by their theme or grouping (for example, "Overview",
  "Media").
- **Navigation** elements are named by destination or direction, and the page
  indicator reflects position rather than naming each page redundantly.

## State Presentation

Every data-bearing element can present these states clearly and distinctly:

- **Loading.** A neutral in-progress indication. Loading is visibly
  different from having no data and from an error.
- **Unavailable.** The source or integration is not present or not reachable.
  Stated plainly (for example, "Unavailable"), not shown as a zero or blank.
- **Degraded.** Data is present but partial, stale, or reduced in confidence.
  The element communicates the reduced quality rather than presenting
  degraded data as fully reliable.
- **Error.** A failure occurred. The element shows an error state with a
  short, honest description and, where possible, a next step.

Missing or unavailable data is never presented as a false zero or an empty
value that could be mistaken for real data.

## Capitalization

- Use **sentence case** for labels, buttons, and status text (capitalize the
  first word and proper nouns only).
- Use **Title Case** only for formal proper names where it is expected.
- Avoid all-caps except for short, intentional indicators; never use all-caps
  for running text.

## Numbers and Units

- Always pair a number with its unit where a unit applies (for example,
  "42 %", "3.2 GB", "60 fps").
- Use a consistent number of decimal places per measurement so values do not
  visually jitter as they update.
- Use monospaced numeric readouts (the `type.mono` role) for changing values
  so digits do not shift layout.
- Use locale-appropriate separators once localization is considered; do not
  hard-code assumptions that will not localize.

## Timestamps

- Present timestamps in a clear, unambiguous form appropriate to context
  (relative time such as "2 min ago" for recency, absolute time for records).
- Be explicit about time zone where an absolute time could be
  misinterpreted.
- Keep timestamp formatting consistent across the application.

## Color and Status

- **Color must not be the only status indicator.** Every status conveyed by
  color is also conveyed by a shape, icon, label, or text. This is required
  for accessibility and for legibility on varied backgrounds.
- Status colors from the design system are reserved for status and are not
  used decoratively.

## Animation

- **Animations must communicate state, navigation, or feedback.** Motion
  earns its place by showing a change of state, movement between pages, or
  response to input.
- Animation is subtle and uses the design system's motion durations.
- Motion never blocks interaction and never obscures information the user
  needs.

## Accessibility

- Maintain strong text contrast against all supported backgrounds (live,
  static, transparent).
- Do not rely on color alone (restated here because it is central).
- Ensure touch targets and spacing accommodate imprecise input.
- Keep motion subtle; avoid large, rapid, or flashing motion that could be
  distracting or uncomfortable.
- Prefer clear text alternatives for any icon whose meaning is not obvious.

## Consistency

This guide, the design system, the coding standards, and the foundation are
intended to be mutually consistent. Where product text and visuals meet
architecture (for example, showing an unavailable state for a missing
integration), the foundation's "graceful failure" principle governs.
