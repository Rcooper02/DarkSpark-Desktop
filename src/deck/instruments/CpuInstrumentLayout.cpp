// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/CpuInstrumentLayout.hpp"

#include <algorithm>

namespace darkspark::deck::instruments {

double positionalFraction(double value, double lo, double hi) {
    if (hi <= lo) {
        return 0.0;
    }
    const double f = (value - lo) / (hi - lo);
    return std::clamp(f, 0.0, 1.0);
}

namespace {

using namespace layout_constants;

CpuInstrumentLayout resolveLarge(double side, double util,
                                 double secondaryFraction) {
    CpuInstrumentLayout l;
    l.side = side;
    l.centerX = side / 2.0;
    l.centerY = side / 2.0;
    l.arcStartDegrees = kArcStartDegrees;
    l.arcSpanDegrees = kArcSpanDegrees;

    l.utilizationRing.outerRadius = side * kLargeOuterRadiusFrac;
    l.utilizationRing.thickness = side * kLargeOuterThicknessFrac;
    l.utilizationRing.segments = kLargeOuterSegments;
    l.utilizationRing.fillFraction = positionalFraction(util, 0.0, 100.0);
    l.utilizationRing.present = true;

    l.temperatureRing.outerRadius = side * kLargeInnerRadiusFrac;
    l.temperatureRing.thickness = side * kLargeInnerThicknessFrac;
    l.temperatureRing.segments = kLargeInnerSegments;
    // The inner ring is filled by a pre-computed [0, 1] fraction supplied by
    // the instrument, so this shared geometry stays subsystem-agnostic (no
    // temperature bounds or units here).
    l.temperatureRing.fillFraction = secondaryFraction;
    l.temperatureRing.present = true;

    // Responsive primary value: dominant but never overflowing the square.
    l.primaryValuePx =
        std::min(kLargePrimaryValueTargetPx, side * kLargePrimaryValueFrac);

    // Center stack, measured around the optical center.
    l.titleY = l.centerY - side * 0.205;
    l.valueY = l.centerY - side * 0.005;  // the carved-in percentage owns center
    l.secondaryY = l.centerY + side * 0.135;

    l.trendBandTop = l.centerY + side * 0.055;
    l.trendBandBottom = l.centerY + side * 0.18;
    l.hasTrendBand = true;
    l.hasInnerRing = true;
    l.hasStatusGap = true;
    return l;
}

CpuInstrumentLayout resolveSmall(double side, double util) {
    CpuInstrumentLayout l;
    l.side = side;
    l.centerX = side / 2.0;
    l.centerY = side / 2.0;
    l.arcStartDegrees = kArcStartDegrees;
    l.arcSpanDegrees = kArcSpanDegrees;

    // Single arc: utilization only, fewer/wider segments to keep the segmented
    // identity legible at small size.
    l.utilizationRing.outerRadius = side * kSmallOuterRadiusFrac;
    l.utilizationRing.thickness = side * kSmallOuterThicknessFrac;
    l.utilizationRing.segments = kSmallSegments;
    l.utilizationRing.fillFraction = positionalFraction(util, 0.0, 100.0);
    l.utilizationRing.present = true;

    // No inner ring in Small: temperature becomes a compact text line.
    l.temperatureRing.present = false;

    l.primaryValuePx =
        std::min(kSmallPrimaryValueTargetPx, side * kSmallPrimaryValueFrac);

    l.titleY = l.centerY - side * 0.19;
    l.valueY = l.centerY - side * 0.01;
    l.secondaryY = l.centerY + side * 0.16;

    l.hasTrendBand = false;
    l.hasInnerRing = false;
    l.hasStatusGap = true;
    return l;
}

}  // namespace

CpuInstrumentLayout resolveCpuInstrumentLayout(InstrumentSizeMode mode,
                                               double side,
                                               double utilizationPercent,
                                               double secondaryFraction) {
    switch (mode) {
    case InstrumentSizeMode::Small:
        return resolveSmall(side, utilizationPercent);
    case InstrumentSizeMode::Large:
    case InstrumentSizeMode::Medium:  // fall back to Large until designed
    case InstrumentSizeMode::Wide:    // fall back to Large until designed
        return resolveLarge(side, utilizationPercent, secondaryFraction);
    }
    return resolveLarge(side, utilizationPercent, secondaryFraction);
}

}  // namespace darkspark::deck::instruments
