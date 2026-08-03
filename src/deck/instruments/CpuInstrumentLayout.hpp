// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTLAYOUT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTLAYOUT_HPP

#include "deck/instruments/InstrumentSizeMode.hpp"

namespace darkspark::deck::instruments {

/// One concentric ring's geometry, as resolved for a given square side.
///
/// Radii and thickness are absolute pixels derived from the content square, so
/// the composition scales with the instrument without any layout code caring
/// about screen size. `fillFraction` is the [0,1] portion of the arc sweep that
/// is "lit" -- a pure positional mapping, never a health judgement.
struct RingGeometry {
    double outerRadius = 0.0;
    double thickness = 0.0;
    int segments = 0;
    double fillFraction = 0.0;  ///< [0,1] portion of the sweep that is lit
    bool present = false;       ///< Small mode omits the inner (temperature) ring
};

/// The resolved geometry for a CpuInstrument at a given size and content square.
///
/// This is pure data: no Qt, no painting. Separating it lets the composition be
/// reasoned about and unit-tested without a display. The paint layer consumes
/// this and draws; it makes no geometric decisions of its own.
///
/// Every field here has an obvious future animation path -- fill fractions
/// animate toward new values, the center value cross-fades, radii can breathe --
/// so no element's eventual motion requires re-deriving the layout.
struct CpuInstrumentLayout {
    double side = 0.0;        ///< the square content side actually used
    double centerX = 0.0;
    double centerY = 0.0;

    // Arc sweep shared by both rings (degrees). A 270-degree sweep with the
    // opening at bottom-dead-center; the gap houses the reserved status region,
    // which is why the sweep is not a full circle.
    double arcStartDegrees = 0.0;
    double arcSpanDegrees = 0.0;

    RingGeometry utilizationRing;  ///< outer, thick, primary
    RingGeometry temperatureRing;  ///< inner, thin, secondary (Large only)

    // Center stack anchor points (y as absolute pixels), all measured so the
    // primary value sits at the optical center of the square.
    double titleY = 0.0;         ///< "CPU"
    double valueY = 0.0;         ///< dominant utilization value (optical center)
    double secondaryY = 0.0;     ///< subordinate temperature value
    double primaryValuePx = 0.0; ///< resolved, responsive primary font size

    // Reserved regions (accounted for so features land without recomposition).
    double trendBandTop = 0.0;     ///< reserved 60s trend region (Large)
    double trendBandBottom = 0.0;
    bool hasTrendBand = false;
    bool hasInnerRing = false;
    bool hasStatusGap = true;      ///< the bottom opening; always present
};

/// Map a value within [lo, hi] to a [0,1] fraction, clamped to the ends.
/// Used for positional ring fill (utilization 0..100, temperature 20..100).
/// Purely visual placement; carries no threshold or health meaning.
[[nodiscard]] double positionalFraction(double value, double lo, double hi);

/// Resolve the full layout for a size mode within a square of the given side.
/// `utilizationPercent` and `temperatureCelsius` set the ring fill fractions.
[[nodiscard]] CpuInstrumentLayout resolveCpuInstrumentLayout(
    InstrumentSizeMode mode, double side, double utilizationPercent,
    double temperatureCelsius);

// --- Named composition constants (future theme-token candidates) ------------
//
// These geometric quantities are CPU-instrument constants for the prototype.
// They are deliberately NOT in LegacyTheme yet: ring geometry has no theme
// token today, and we promote these to tokens only once the design is approved
// on the panel. They are the CPU instrument's PERSONALITY, not DarkSpark's:
// concentric segmented rings are how the CPU instrument expresses itself, and
// future subsystem instruments (cooling as a living fan, network as flow paths,
// storage as capacity bands) must discover their own metaphor rather than
// inherit rings.
namespace layout_constants {

// Arc: 270-degree sweep, opening at bottom-dead-center.
inline constexpr double kArcStartDegrees = 135.0;
inline constexpr double kArcSpanDegrees = 270.0;

// Temperature positional display span (visual only; NOT thresholds).
inline constexpr double kTempSpanLowC = 20.0;
inline constexpr double kTempSpanHighC = 100.0;

// Large mode (fractions of side S).
inline constexpr double kLargeOuterRadiusFrac = 0.47;
inline constexpr double kLargeOuterThicknessFrac = 0.060;
inline constexpr int kLargeOuterSegments = 38;
inline constexpr double kLargeInnerRadiusFrac = 0.37;
inline constexpr double kLargeInnerThicknessFrac = 0.035;
inline constexpr int kLargeInnerSegments = 38;
inline constexpr double kLargePrimaryValueTargetPx = 64.0;
inline constexpr double kLargePrimaryValueFrac = 0.155;  // responsive: k * S

// Small mode.
inline constexpr double kSmallOuterRadiusFrac = 0.46;
inline constexpr double kSmallOuterThicknessFrac = 0.075;
inline constexpr int kSmallSegments = 18;
inline constexpr double kSmallPrimaryValueTargetPx = 28.0;
inline constexpr double kSmallPrimaryValueFrac = 0.20;

}  // namespace layout_constants

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTLAYOUT_HPP
