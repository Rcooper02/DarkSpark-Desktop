# DarkSpark Desktop — Coding Standards

These standards apply to all code in the repository. They exist to keep the
codebase consistent, readable, and aligned with the foundation. Read
`docs/FOUNDATION.md` first; where a standard here touches architecture, the
foundation governs.

## Platform and Toolchain

- **Language:** C++23.
- **UI / framework:** Qt 6.
- **Build system:** CMake.
- **Generator:** Ninja.
- **Primary validation platform:** Fedora. Code is expected to build and be
  validated on Fedora first. It should not depend unnecessarily on anything
  unavailable there.

The build system, generator, and dependencies are introduced at the Deck-0
stage. They are named here so conventions are known in advance; no build
configuration is committed yet.

## Naming

- **Classes use PascalCase.** For example, `PageManager`, `DashboardCard`.
- **Functions and local variables use camelCase.** For example,
  `currentPage`, `updateLayout()`.
- **Constants use a single agreed convention.** True compile-time constants
  use `UPPER_SNAKE_CASE`. The following qualifications apply:
  - Qt-required names retain Qt conventions.
  - External API identifiers retain their defined spelling.
  - Enum naming will follow the project's chosen C++/Qt convention when enums
    are first introduced; it is not fixed here.
  - `UPPER_SNAKE_CASE` is **not** forced onto values that are not true
    constants (for example, `const` locals, configuration values, or
    computed read-only members).
- **No unexplained abbreviations.** Names are spelled out. If an abbreviation
  is genuinely standard and unambiguous in context, it may be used, but
  invented or obscure shortenings are not.
- **No `Mgr` suffix.** Use the full word `Manager`.
- **Class names end with what they are.** Use role suffixes such as
  `Service`, `Card`, `Page`, `Window`, `Manager`, `Model`. A reader should
  know a type's role from its name alone.

## File Organization

- **One public class per matching header/source pair** (for example,
  `PageManager.h` / `PageManager.cpp`), unless there is a strong, documented
  reason to do otherwise. Small tightly-coupled helper types local to a
  single class may live alongside it.

## Memory and Ownership

- **RAII.** Resources are owned by objects and released deterministically in
  destructors. No manual cleanup paths that can be skipped.
- **No raw owning pointers.** Ownership is expressed through smart pointers or
  Qt's parent-child ownership. A raw pointer never owns.
- **Qt parent ownership where appropriate.** For `QObject`-derived types,
  parent-child ownership is the preferred mechanism and is used consistently.
- **Explicit ownership documentation.** Where ownership is not obvious from
  the type, it is documented at the declaration. Every non-trivial pointer or
  reference has clear, stated ownership semantics.

## Correctness

- **Const correctness.** Methods that do not modify state are `const`.
  Parameters that are not modified are passed by const reference or value as
  appropriate. Const is the default; mutability is deliberate.
- **Warnings enabled.** The build enables a strong warning set.
- **No warnings ignored without explanation.** A suppressed or worked-around
  warning carries a comment explaining why. Silent suppression is not
  allowed.

## Testing

- **Tests are required for non-trivial logic.** Any logic with branching,
  state transitions, or computation has tests. Trivial accessors do not
  require tests. Services, being UI-independent, are expected to be tested in
  isolation.

## Documentation and Comments

- **Comments explain why, not what.** The code says what it does; comments
  capture intent, constraints, and non-obvious reasoning.
- **Public interfaces are documented.** Every public interface has a
  description of its contract, including ownership and, where relevant,
  threading expectations.

## Public API Hygiene

- **No third-party types exposed through public DarkSpark interfaces without
  approval.** Public DarkSpark interfaces are expressed in terms of DarkSpark
  and standard-library types. Exposing a third-party type (including through
  a public interface signature) requires explicit approval, so that
  dependencies do not leak across boundaries unnoticed.

## Relationship to the Foundation

These standards are subordinate to `docs/FOUNDATION.md`. In particular, the
dependency direction, the "no speculative abstractions" rule, and the "no
silent architectural changes" rule apply to all code regardless of anything
here. A change that satisfies these standards but violates the foundation is
not acceptable.
