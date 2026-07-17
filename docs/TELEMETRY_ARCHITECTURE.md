<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# DarkSpark Telemetry Architecture

Authoritative design specification for the first local system-monitoring
capability in DarkSpark Desktop.

Status: specification (documentation only). No implementation code accompanies
this document. Implementation is a separate, later batch.

Scope of this document: the architecture, contracts, and behavior for a single
metric — aggregate CPU utilization — delivered end-to-end from a Linux data
source to the existing System-page card. It is intentionally one narrow
vertical slice through the whole telemetry stack, chosen so the architecture
(service to model to interface to UI) is proven before any breadth is added.

---

## 1. Scope and Non-Goals

### In scope

- One model value type: `MetricSample`.
- One metric identity: `MetricId::CpuTotalUtilization`.
- One state enum: `MetricState`.
- One provider contract: `ITelemetryProvider`.
- One service: `CpuTelemetryService`, reading aggregate CPU counters from
  `/proc/stat`.
- The data-flow boundary by which a `MetricSample` reaches the System page and
  updates the existing CPU `DashboardCard`.
- A deterministic test suite for the CPU calculation and state machine.

### Explicit non-goals (deferred; not in the first telemetry batch)

Memory, GPU, storage, network, temperatures, fan, per-core breakdown,
per-process data, history, graphs, charts, persistence, databases, networking,
remote nodes, plugins, AI, DarkSpark Core integration, a settings or
preferences UI, a permission-prompt UI, and worker threads. None of these are
designed here beyond noting where a future seam would attach.

This document does not design a large metric catalog. Only the single metric
above is defined.

---

## 2. Directory and Ownership Boundaries

Three new production directories are introduced, consistent with the dependency
rules in `docs/FOUNDATION.md` (services never depend on UI; models depend on
neither UI nor services; UI depends only on interfaces and models).

```
src/
  models/       (new)  MetricSample.{hpp,cpp} — MetricId, MetricUnit,
                       MetricState, MonotonicTimestamp defined here for
                       the initial slice (extractable later)
  interfaces/   (new)  ITelemetryProvider
  services/     (new)  CpuTelemetryService  (implements ITelemetryProvider)
  deck/
    pages/             DeckPage  (gains an intentional update entry point)
    cards/             DashboardCard  (existing consumer; unchanged contract)
  application/         Application  (composition root: owns and wires service)
```

Ownership rules for this batch:

- `models/` contains plain data types. No Qt widget types, no service types, no
  UI types. `MetricSample` is copyable and self-contained.
- `interfaces/ITelemetryProvider` is an abstract contract. It may use
  `QObject`/signals (this is a Qt application), but it must not reference any
  `deck/` or widget type. No `DashboardCard`, `DeckPage`, `QWidget`, etc.
- `services/CpuTelemetryService` implements the interface and owns the polling
  timer and all `/proc/stat` parsing. It includes nothing from `deck/` and no
  widget headers.
- The **composition root** owns the service and connects it to the UI. See
  Section 8 for the exact identification of the composition root.
- The UI (`DeckPage` / `DashboardCard`) receives `MetricSample` values through
  an intentional page-level entry point and never learns about the service.

No writes to `src/core/` or `src/utilities/` are planned for the telemetry
implementation batch.

---

## 3. Dependency Diagram

Arrows read "may depend on." No arrow points into UI from a service or model.

```
            Application  (composition root)
              /       \
             v         v
   CpuTelemetryService  DeckWindow / DeckPage        (UI)
             |                     |
             v                     v
     ITelemetryProvider  <---------+   (UI depends on the interface)
             |                     |
             v                     v
          MetricSample  <----------+   (both sides use the model)
             |
             v
   MetricId / MetricState / MetricUnit
```

- `CpuTelemetryService` depends on `ITelemetryProvider` (implements it) and on
  the model types.
- The UI depends on `ITelemetryProvider` and `MetricSample` only — never on
  `CpuTelemetryService` concretely.
- `MetricSample` depends only on the small model enums.
- `Application` is the single place where the concrete service and the concrete
  UI meet and are wired together.

---

## 4. Metric Identity

Metric identity is a strongly typed enum, not a bare string.

```cpp
// MetricSample.hpp (MetricId defined here for the initial slice)
namespace darkspark::models {

enum class MetricId {
    CpuTotalUtilization,
};

}  // namespace darkspark::models
```

Rationale: a strong type prevents typo-based identity bugs, makes exhaustive
switch handling possible, and avoids a stringly-typed catalog. Only the single
value needed now is defined; the enum is expected to grow one entry per future
metric, under its own batch and review. A future string form (for
diagnostics/logging) can be provided by a small `toString(MetricId)` helper
without weakening the type.

---

## 5. Metric Sample Semantics

### Model shape

```cpp
// MetricSample.hpp (MetricUnit defined here for the initial slice)
namespace darkspark::models {

enum class MetricUnit {
    Percent,
};

}  // namespace darkspark::models
```

```cpp
// MetricSample.hpp (MetricState defined here for the initial slice)
namespace darkspark::models {

// Independent of DashboardCard::State by design. The UI maps from this to a
// presentation state; the model never imports a UI enum.
enum class MetricState {
    Unavailable,  // no usable value (never 0 as a stand-in for missing)
    Fresh,        // a valid, current value
    Stale,        // last valid value retained but known to be out of date
};

}  // namespace darkspark::models
```

```cpp
// models/MetricSample.hpp
#include <cstdint>
#include <optional>

namespace darkspark::models {

// For the initial telemetry slice, MetricId, MetricUnit, and MetricState are
// defined in MetricSample.hpp. This is a starting-point choice for the first
// slice, not a permanent architectural rule: these types may be extracted into
// their own headers later without changing the architecture.

// Project-owned timestamp alias so the representation stays abstract at the
// model boundary and can change later without touching every signature.
using MonotonicTimestamp = std::int64_t;  // monotonic milliseconds, fixed origin

// A plain, passive value object. No UI dependency, no service dependency, and
// no knowledge of where a value originated (the service owns the concept of the
// "last valid sample"; the model only represents a stale metric).
class MetricSample {
public:
    // Factory construction is the only construction path, so an invalid
    // (state, value) combination cannot be built by accident. Architectural
    // invariant: a Fresh sample shall never contain a non-finite numeric value
    // (see "Floating-point invariant"). The enforcement mechanism is a T1
    // decision.
    static MetricSample unavailable(MetricId id, MonotonicTimestamp t);
    static MetricSample fresh(MetricId id, double value, MetricUnit unit,
                              MonotonicTimestamp t);
    // Passive stale construction: the caller (the service) supplies the last
    // valid value and unit explicitly. MetricSample does not take a previous
    // sample and does not reach into another sample's fields.
    static MetricSample stale(MetricId id, double last_valid_value,
                              MetricUnit unit, MonotonicTimestamp t);

    [[nodiscard]] MetricId id() const;
    [[nodiscard]] std::optional<double> value() const;
    [[nodiscard]] MetricUnit unit() const;
    [[nodiscard]] MetricState state() const;
    [[nodiscard]] MonotonicTimestamp timestamp() const;

    friend bool operator==(const MetricSample&, const MetricSample&) = default;

private:
    MetricSample() = default;  // used only by the factories

    MetricId id_{MetricId::CpuTotalUtilization};
    std::optional<double> value_{};
    MetricUnit unit_{MetricUnit::Percent};
    MetricState state_{MetricState::Unavailable};
    MonotonicTimestamp timestamp_{0};
};

}  // namespace darkspark::models
```

### Invariants (enforced by the factories and checked in tests)

- **Unavailable has no numeric value.** `state() == Unavailable` implies
  `value() == std::nullopt`.
- **Fresh has a valid, finite numeric value.** `state() == Fresh` implies
  `value().has_value()` and the value is finite.
- **Stale retains a valid value but is explicitly `state() == Stale`.** The
  `stale()` factory receives the last valid value and unit **directly** from
  the caller (the service); `MetricSample` does not take another sample and
  does not read another sample's fields. The model represents a stale metric;
  it does not know where the value came from.
- **Missing data is never 0%.** No path emits `value() == 0.0` to represent
  absence; absence is always `Unavailable` with no value.
- **A Fresh value is never non-finite.** Architectural invariant: a
  `MetricSample` representing a Fresh value shall never contain a non-finite
  numeric value (NaN, +Inf, -Inf). The enforcement mechanism (assertion,
  contract, factory semantics) is left to T1.

### Floating-point invariant

Architectural invariant: **a `MetricSample` representing a Fresh value shall
never contain a non-finite numeric value** (NaN, +Inf, -Inf). This guarantees
no invalid floating-point value flows downstream through a `MetricSample`. The
enforcement mechanism — assertion, contract/precondition, or factory semantics
such as degrading a non-finite input to Unavailable — is deliberately left to
the T1 implementation batch, which will fix and test it explicitly.

### Timestamp representation

The model exposes a **single monotonic timestamp** through the project-owned
alias `MonotonicTimestamp` (currently `std::int64_t` milliseconds from a fixed
origin), not both wall-clock and monotonic, and not a raw integer at the public
boundary.

Justification for the minimum representation: the only current consumers of the
timestamp are (a) freshness reasoning — "is this sample newer than that one, has
an expected tick elapsed" — and (b) diagnostics ordering. Both are correctly and
robustly served by a monotonic clock, which is immune to wall-clock jumps (NTP
steps, DST, manual changes). No current feature displays an absolute wall-clock
time for a sample, correlates it with external wall-clock logs, or persists it,
so a wall-clock field would be unused weight. If a later feature needs absolute
time (a history view, an exported diagnostic bundle), a wall-clock field will be
added at that point with that need as its justification. Using an alias means
that later change need not touch every signature.

Source (later stages): `QElapsedTimer` (monotonic) owned by the service, or
`std::chrono::steady_clock`. The service stamps each sample at creation. In T1
the timestamp is supplied to the factories as a plain value; the model does not
read a clock.

---

## 6. Provider Contract

### Signatures

```cpp
// interfaces/ITelemetryProvider.hpp
#include <optional>

#include <QObject>

#include "models/MetricSample.hpp"

namespace darkspark::interfaces {

// Abstract telemetry provider contract. QObject-based so implementations can
// emit Qt signals, which is idiomatic in this Qt application. Qt WIDGET types
// must never appear in this contract; only model values cross the boundary.
class ITelemetryProvider : public QObject {
    Q_OBJECT

public:
    explicit ITelemetryProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~ITelemetryProvider() override = default;

    // Begin (or resume) sampling. Idempotent: calling start() while already
    // running is a no-op. Safe to call after stop().
    virtual void start() = 0;

    // Stop sampling. Idempotent: calling stop() while already stopped is a
    // no-op. Does not delete the provider.
    virtual void stop() = 0;

    // The most recent sample. Before the first start(), and after stop(), this
    // returns an Unavailable sample (never a fabricated value). See the
    // current-sample behavior table below.
    [[nodiscard]] virtual models::MetricSample currentSample() const = 0;

signals:
    // Emitted on every sampling tick that produces a new sample, including
    // transitions into Unavailable/Stale. The UI connects to this.
    void readingChanged(const darkspark::models::MetricSample& sample);
};

}  // namespace darkspark::interfaces
```

### QObject/signals versus a pure C++ callback

Decision: **use `QObject` and Qt signals.** DarkSpark Desktop is a Qt
application with a running event loop; signal/slot is the idiomatic, well-understood
delivery mechanism, integrates with the timer that drives polling, and gives
automatic disconnection when either end is destroyed (see connection lifetime).
A pure `std::function` callback contract was considered and rejected for this
batch because it would require hand-rolling connection lifetime management that
Qt already provides correctly, for no benefit in a Qt process. The constraint
that matters — no widget types crossing the boundary — is independent of this
choice and is upheld: only `MetricSample` (a model value) is emitted.

### Ownership

- The provider is a `QObject` owned by the composition root via Qt parent
  ownership (or an equivalent explicitly-owned smart pointer held by the
  composition root). It is not owned by any UI widget and does not own any UI
  widget.
- The service owns its internal timer (Qt parent ownership) and its counter
  baseline state.

### Connection lifetime

- The UI side connects to `readingChanged` using the standard
  `QObject::connect` with a receiver `QObject*` context, so the connection is
  automatically removed when either the provider or the receiver is destroyed.
  No manual disconnect is required for correctness, and no dangling-slot call
  can occur across destruction.

### start()/stop() idempotence

- `start()` while stopped: begins the timer, resets the baseline to "not yet
  established" (the next successful read is a baseline, producing Unavailable).
- `start()` while already running: no-op (does not reset the baseline, does not
  restart the timer).
- `stop()` while running: stops the timer. The last sample remains queryable
  but the provider is considered stopped.
- `stop()` while already stopped: no-op.
- Repeated `start()`/`stop()` cycles are safe and are covered by tests.

### Current-sample behavior

| Situation                         | `currentSample()` returns                          |
| --------------------------------- | -------------------------------------------------- |
| Before the first `start()`        | Unavailable (no value)                             |
| After `start()`, before 1st tick  | Unavailable (no value)                             |
| After baseline read (1st valid)   | Unavailable (baseline established, no delta yet)   |
| After 2nd valid read              | Fresh (computed utilization)                       |
| After a failed/malformed read     | Stale (if a prior valid value exists) else Unavailable |
| After `stop()`                    | The last emitted sample remains queryable          |

---

## 7. CPU Calculation Correctness

### Source

The first line of `/proc/stat`, the aggregate `cpu` line, for example:

```
cpu  user nice system idle iowait irq softirq steal guest guest_nice
```

All fields are cumulative counters in USER_HZ jiffies since boot.

### Fields and the double-counting rule

The Linux kernel already includes `guest` within `user` and `guest_nice`
within `nice`. Therefore `guest` and `guest_nice` **must not be added again**;
doing so double-counts guest time. The calculation below uses only the first
eight fields and never adds `guest`/`guest_nice` separately.

Fields used (indices on the `cpu` line after the label):

```
user    nice    system    idle    iowait    irq    softirq    steal
```

### Definitions

Using unsigned 64-bit arithmetic for all counters:

```
idle_all = idle + iowait
busy     = user + nice + system + irq + softirq + steal
total    = idle_all + busy
```

`iowait` is counted as idle for utilization purposes (the CPU is not doing work
during iowait). `steal` is counted as busy.

### Delta and utilization

Between a previous snapshot `p` and a current snapshot `c`:

```
d_total = total_c - total_p        (uint64)
d_idle  = idle_all_c - idle_all_p  (uint64)

utilization_percent = 100.0 * (d_total - d_idle) / d_total
```

### Validity rules (reject, do not fabricate)

- **Counter regression:** if any used field in `c` is less than the
  corresponding field in `p` (unsigned compare), the snapshot pair is invalid.
  Reject — do not wrap around, do not guess. This yields Stale-or-Unavailable
  per the failure rules and invalidates the baseline.
- **Zero total delta:** if `d_total == 0`, reject (no time elapsed / no usable
  data). Do not divide by zero, do not emit 0%.
- **Unsigned arithmetic only:** all counter math is `uint64`. Deltas are
  computed only after confirming `c >= p` field-wise, so no unsigned underflow
  occurs.
- **Clamping:** the final floating-point percentage may be clamped **only** to
  correct negligible boundary error — i.e. clamp a result of, say, `-0.0000001`
  or `100.0000001` into `[0, 100]`. This clamp applies solely to tiny
  floating-point rounding at the boundary. It must never be used to mask
  invalid input; invalid input is rejected before any percentage is computed.

### Baseline lifecycle

- The **first successful read establishes a baseline** and produces
  **Unavailable** (there is no prior snapshot to delta against).
- The **next valid read** produces **Fresh**.
- **After any failed or malformed read:**
  - emit **Stale** with the last valid value if one exists;
  - otherwise emit **Unavailable**;
  - **discard/invalidate the counter baseline.**
- **After recovery**, the first valid read **re-establishes the baseline**
  (producing Unavailable) and does **not** compute a delta across the failed
  interval. This prevents a misleadingly large utilization spike spanning the
  outage.

---

## 8. Polling, Threading, and the Composition Root

### Timer and thread

- Polling uses a **service-owned `QTimer`** running on the **application
  event-loop (GUI) thread**. No worker thread is introduced in this batch.
- Default cadence: **1000 ms**, expressed as a **private named constant inside
  `CpuTelemetryService`** (authoritative ruling). No settings, no configuration
  files, and no global telemetry-configuration object are introduced.

### Why a synchronous `/proc/stat` read is acceptable here

`/proc/stat` is a kernel-generated pseudo-file. Reading its first line is a
bounded, non-blocking operation on the order of microseconds; it performs no
disk I/O and cannot block on external resources. At a 1000 ms cadence the read
occupies a negligible fraction of one frame interval, so performing it on the
event-loop thread does not risk UI stutter.

**Condition that would require revisiting this decision:** if a future metric
reads a source that can block or is materially expensive — for example a sysfs
node that stalls, a device query with latency, a source requiring many files
per tick, or a cadence high enough that cumulative read time approaches a frame
budget — then that metric's provider (not this one) must move its read to a
worker thread or asynchronous I/O, as a deliberate, separately reviewed
decision. This CPU provider does not meet that condition.

### Composition root

The **composition root is `darkspark::application::Application`**. It already
constructs and owns the top-level windows and wires objects together (it is the
only place concrete UI and concrete services are assembled). For this batch,
`Application` will additionally:

1. construct and own the `CpuTelemetryService`,
2. own the `DeckWindow`,
3. obtain the service as an `ITelemetryProvider`,
4. connect the provider's `readingChanged` signal to
   `DeckWindow::applyMetricSample(const MetricSample&)`,
5. call `start()`/`stop()` in step with the Deck lifecycle.

Fixed wiring ruling (authoritative):

- `Application` owns both `CpuTelemetryService` and `DeckWindow`, and connects
  `ITelemetryProvider::readingChanged` to
  `DeckWindow::applyMetricSample(const MetricSample&)`.
- `DeckWindow` routes the sample internally to the System `DeckPage`.
- `Application` must not retrieve or manipulate a `DeckPage` or `DashboardCard`
  directly.
- `DeckWindow` and `DeckPage` must not expose `DashboardCard` pointers.
- An unknown or currently unrepresented `MetricId` must be ignored safely at
  the routing boundary (no crash, no fabricated card, no error state).

This keeps the service UI-agnostic and confines all card knowledge to the page
that owns the cards.

---

## 9. UI Data-Flow Boundary

The service must not know about cards. `DeckWindow` and `DeckPage` must not
expose `DashboardCard` pointers. A `MetricSample` reaches the correct card
through an **intentional window-level entry point that forwards to the page**.

### Entry point

`DeckWindow` gains the public entry point the composition root connects to;
`DeckWindow` forwards to the System `DeckPage`, which owns the card mapping:

```cpp
// deck/DeckWindow.hpp  (added in the implementation batch)
public:
    // Receive a telemetry sample and route it internally to the correct page.
    // DeckWindow does not expose pages or cards to the caller.
    void applyMetricSample(const darkspark::models::MetricSample& sample);
```

```cpp
// deck/pages/DeckPage.hpp  (added in the implementation batch)
public:
    // Apply a telemetry sample to this page. The page maps the sample's
    // MetricId to the specific card it owns and updates that card's
    // presentation. Cards are not exposed; the mapping stays inside the page.
    void applyMetricSample(const darkspark::models::MetricSample& sample);
```

- `DeckWindow` routes the sample to the System `DeckPage`. `Application` never
  touches a `DeckPage` or `DashboardCard` directly.
- The page owns the private mapping from `MetricId` to the specific
  `DashboardCard` it constructed (the System page maps `CpuTotalUtilization` to
  its "CPU" card).
- An unknown or currently unrepresented `MetricId` is ignored safely at both
  the window and page routing boundaries — no crash, no fabricated card, no
  error state.

### Fixed UI state mapping (authoritative, first CPU slice)

The page translates `MetricState` into the card's existing Batch-2 presentation
states. No new `DashboardCard` state is added in the telemetry batch. The
mapping is:

| `MetricState`          | `DashboardCard::State`      | Card content                              |
| ---------------------- | --------------------------- | ----------------------------------------- |
| `Fresh`                | `Normal`                    | current utilization percentage            |
| `Unavailable`          | `Unavailable`               | no value (never 0%)                        |
| `Stale`                | `Warning` (temporary)       | last valid percentage + status text "Stale" |

- A `Stale` sample **retains and displays the last valid percentage** and must
  show explicit status text such as "Stale".
- The `Stale` to `Warning` mapping is a temporary presentation choice for this
  slice, chosen to reuse an existing accepted state rather than introduce a new
  one. It may be revisited when a dedicated stale/degraded presentation is
  designed.
- This mapping lives **only** in the UI layer (the page). `MetricState`
  semantics are not changed to match `DashboardCard::State`; the model never
  imports a UI enum.
- The card continues to expose no data-collection or service behavior; it only
  renders what the page hands it.

### Data-flow diagram

```
/proc/stat
   |  (read + parse + delta, service-internal)
   v
CpuTelemetryService  --emits-->  ITelemetryProvider::readingChanged(MetricSample)
   |                                              |
   |  (owned + connected by Application)          |
   v                                              v
Application  --connects signal to-->  DeckWindow::applyMetricSample()
                                          |
                                          |  (routes by MetricId; unknown ids ignored)
                                          v
                                       DeckPage::applyMetricSample()
                                          |
                                          |  (page maps MetricId -> its card,
                                          |   maps MetricState -> DashboardCard::State)
                                          v
                                       DashboardCard  (renders value / stale / unavailable)
```

No arrow runs from the service to a card, and no arrow runs from `Application`
to a page or card. The only UI-facing surface of the service is the interface
signal carrying a model value.

---

## 10. Logging and Errors

- Normal missing-data behavior is represented through `MetricState`
  (Unavailable/Stale), **never through exceptions**. Parsing and reading return
  a result the service converts into a sample; no exception crosses the service
  boundary.
- **Log on state transitions only.** The service logs when the emitted
  `MetricState` changes (for example Fresh to Stale, or Unavailable to Fresh),
  not on every tick. A `/proc/stat` that is unreadable for a minute therefore
  produces a single transition log entry, not sixty identical lines. This is
  the specified anti-spam rule for this batch (chosen over rate-limited
  duplicate logging because state-transition logging is simpler and sufficient
  here).
- Log content is limited to the metric id, the new state, and a short reason
  (for example "read failed", "counter regression", "zero delta"). No secrets,
  no environment data, no per-process information (none is collected).

---

## 11. State-Transition Table

Let "valid read" mean a `/proc/stat` snapshot that parses and passes all
validity rules; "bad read" means unreadable, malformed, missing fields,
non-numeric, counter regression, or zero total delta.

| Current internal state        | Event       | Next emitted sample            | Baseline after            |
| ----------------------------- | ----------- | ------------------------------ | ------------------------- |
| No baseline (start/after fail)| valid read  | Unavailable (baseline set)     | established               |
| No baseline                   | bad read    | Unavailable                    | none                      |
| Baseline set, no prior value  | valid read  | Fresh (computed)               | updated to current        |
| Baseline set, no prior value  | bad read    | Unavailable                    | invalidated               |
| Have last valid value (Fresh) | valid read  | Fresh (computed)               | updated to current        |
| Have last valid value (Fresh) | bad read    | Stale (last value retained)    | invalidated               |
| Stale (last value retained)   | valid read  | Unavailable (re-baseline)      | re-established            |
| Stale (last value retained)   | bad read    | Stale (last value retained)    | none                      |

Notes:
- A `bad read` always invalidates the baseline so the subsequent recovery does
  not delta across the gap.
- Recovery always passes through an Unavailable re-baseline before the next
  Fresh; utilization is never computed across a failed interval.
- Logging fires only on the rows where the emitted state differs from the
  previously emitted state.

---

## 12. Test Strategy and Matrix

### Principles

- Deterministic; no dependence on live CPU load or the CI machine's real
  `/proc/stat`.
- Parsing and calculation are driven by **injected fixture strings** through an
  internal seam (an internal parse/compute function or an injectable
  read-source), kept private/internal so the public production interface is not
  widened merely for testing.
- No root, no GUI, no network; runs on generic Fedora-compatible CI and in the
  sandbox.

### Test matrix

| # | Fixture / scenario                                  | Expected result                                  |
| - | --------------------------------------------------- | ------------------------------------------------ |
| 1 | First valid snapshot                                | Unavailable (baseline established)               |
| 2 | Two snapshots, known counters                       | Fresh, utilization equals the hand-computed %    |
| 3 | Idle-only delta (only idle/iowait advance)          | Fresh, ~0% utilization                           |
| 4 | Busy-only delta (no idle advance)                   | Fresh, ~100% utilization                         |
| 5 | Malformed `cpu` line (garbage)                      | bad read: Unavailable or Stale per prior state   |
| 6 | Missing fields (fewer than 8 counters)              | bad read                                          |
| 7 | Non-numeric field                                   | bad read                                          |
| 8 | Counter regression (a field decreases)              | bad read; baseline invalidated                   |
| 9 | Zero total delta (identical snapshots)              | bad read; no divide-by-zero, no 0% fabrication   |
| 10| Failed read after a valid value                     | Stale, retaining the last valid value            |
| 11| Recovery after failure (bad, then two valid)        | Unavailable (re-baseline) then Fresh; no cross-gap delta |
| 12| guest/guest_nice present                            | not double-counted; result matches user/nice-only math |
| 13| Repeated start()/stop() calls                       | idempotent; no crash; baseline reset semantics hold |

Additional model-level tests:
- `MetricSample` invariants: Unavailable has no value; Fresh has a value; Stale
  retains the prior value and is marked Stale; no path yields 0.0-for-missing.

### Sandbox versus Fedora

- **Sandbox-testable:** all of the above — parsing, the CPU formula, the state
  machine, and the model invariants are pure logic and run without Qt or real
  hardware.
- **Fedora-only:** the live `QTimer` firing against the real `/proc/stat`, the
  signal reaching `DeckWindow::applyMetricSample` and forwarding to the System
  `DeckPage`, and the CPU card visibly updating about once per second.

---

## 13. Implementation Batch Breakdown

The implementation (separate from this specification) proceeds as four small,
independently reviewable and independently reversible stages, each stopping at a
clean boundary:

- **T1 — Models only.** `MetricId`, `MetricUnit`, `MetricState`, and
  `MetricSample` (with factory construction and invariants) plus deterministic
  model unit tests. No interface, no service, no `/proc`, no timers, no UI.
- **T2 — `ITelemetryProvider`.** The abstract provider contract only
  (`start`/`stop`/`currentSample`/`readingChanged`). No concrete service, no
  `/proc` parsing, no timers, no UI wiring.
- **T3 — `CpuTelemetryService`.** The concrete service implementing
  `ITelemetryProvider`: `/proc/stat` parsing, the CPU formula, the state
  machine, and the service-owned `QTimer` with the private 1000 ms cadence
  constant, all driven and tested behind an internal seam with the full test
  matrix (Section 12) green. No UI wiring.
- **T4 — UI wiring.** `Application` owns the service and `DeckWindow` and
  connects `readingChanged` to `DeckWindow::applyMetricSample`; `DeckWindow`
  routes to the System `DeckPage`, which gains `applyMetricSample` and the
  private `MetricId`-to-card mapping; the System CPU card renders
  live / stale / unavailable. Fedora validation of the live path.

Each stage is delivered as its own patch against the then-current committed
head, with `git apply --check` and a checksum, following the established
workflow.

---

## 14. Resolved Rulings

The three questions previously open in this section have been ruled on and are
now authoritative. They are recorded here and reflected in Sections 8 and 9.

1. **T3 wiring (resolved).** `Application` owns both `CpuTelemetryService` and
   `DeckWindow`, and connects `ITelemetryProvider::readingChanged` to
   `DeckWindow::applyMetricSample(const MetricSample&)`. `DeckWindow` routes the
   sample internally to the System `DeckPage`. `Application` must not retrieve
   or manipulate a `DeckPage` or `DashboardCard` directly. `DeckWindow` and
   `DeckPage` must not expose `DashboardCard` pointers. Unknown or currently
   unrepresented `MetricId` values are ignored safely. See Sections 8 and 9.
2. **UI state mapping (resolved).** For the first CPU slice:
   `MetricState::Fresh` to `DashboardCard::State::Normal`,
   `MetricState::Unavailable` to `DashboardCard::State::Unavailable`, and
   `MetricState::Stale` to `DashboardCard::State::Warning` (temporary). A stale
   sample retains and displays the last valid percentage and shows explicit
   status text such as "Stale". No new `DashboardCard` state is added in the
   telemetry batch. The mapping stays isolated inside the UI layer;
   `MetricState` semantics are not changed to match `DashboardCard::State`. See
   Section 9.
3. **Cadence (resolved).** The 1000 ms cadence is a private named constant
   inside `CpuTelemetryService`. No settings, configuration files, or global
   telemetry-configuration object are introduced. See Section 8.

No open question blocks starting the T1 implementation batch.

---

*Knowledge Belongs to All.*
