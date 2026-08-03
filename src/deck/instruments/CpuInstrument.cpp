// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/CpuInstrument.hpp"

#include <algorithm>
#include <cmath>

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>
#include <QSizePolicy>
#include <QTimer>
#include <QRectF>
#include <QString>

#include "deck/instruments/CpuInstrumentLayout.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::instruments {

using themes::LegacyTheme;

namespace {

// A resolved accent pair. Held as locals (not hard-coded in paint) so a future
// customization layer can supply monochrome, swapped, or user/theme colors
// without touching the paint routines.
struct AccentPair {
    QColor utilization;  // primary
    QColor temperature;  // secondary
};

[[nodiscard]] AccentPair resolveAccents(InstrumentState /*state*/) {
    // Prototype default: utilization cyan, temperature purple. The purple is a
    // presentation choice, not a health meaning. This is the single seam a
    // customization system would later drive.
    return AccentPair{LegacyTheme::accentCyan(), LegacyTheme::accentPurple()};
}

/// Idle carries the faintest presence; Hover/Touch have somewhere to grow. The
/// prototype only ever passes Idle, but the multipliers are already here so the
/// future states are a data change, not a code change.
[[nodiscard]] double glowStrengthForState(InstrumentState state) {
    switch (state) {
    case InstrumentState::Idle:
        return 1.0;
    case InstrumentState::Hover:
        return 1.6;
    case InstrumentState::Touch:
        return 2.1;
    case InstrumentState::Expanded:
        return 1.8;
    }
    return 1.0;
}

/// Draw a ring of fine graduation ticks just outside the utilization conduit.
///
/// These ticks are STRUCTURAL INSTRUMENTATION -- part of the instrument's
/// structure layer, not decoration and not a data readout. Today they are
/// static, but they are deliberately not classified as throwaway ornament:
/// they are the substrate for future structural roles such as calibration
/// references, interaction anchors, animation guides, overlay alignment, and
/// measurement references. Treating them as structure (rather than decoration)
/// is what keeps that evolution clean.
///
/// Their rendering is tuned to the "multiple reading distances" principle:
///
///   * Across the room: the ticks nearly vanish. Silhouette, conduits, and the
///     center value are all that register. The bold read is untouched.
///   * At arm's length: the graduation ring resolves as calibration.
///   * Up close: minor and major ticks are individually legible, rewarding
///     inspection.
///
/// So they are very dim by design (minor fainter than major), sit outside the
/// outer conduit so they never crowd the center, and derive from the accent so
/// any theme expresses them correctly. Restraint is the point: precision
/// emerges through craftsmanship, not quantity.
void paintGraduationTicks(QPainter& painter, const CpuInstrumentLayout& layout,
                          const QColor& accent) {
    const RingGeometry& ring = layout.utilizationRing;
    if (!ring.present) {
        return;
    }
    const QPointF center(layout.centerX, layout.centerY);
    const double rBase = ring.outerRadius + ring.thickness * 0.55;
    const double minorLen = ring.thickness * 0.22;
    const double majorLen = ring.thickness * 0.42;

    // 60 minor divisions across the sweep; every 5th is a major tick. The count
    // is a calibration aesthetic, unrelated to the value or the segment count.
    constexpr int kDivisions = 60;
    constexpr int kMajorEvery = 5;

    for (int i = 0; i <= kDivisions; ++i) {
        const double deg =
            layout.arcStartDegrees
            + layout.arcSpanDegrees * static_cast<double>(i) / kDivisions;
        const double a = deg * M_PI / 180.0;
        const bool major = (i % kMajorEvery == 0);
        const double len = major ? majorLen : minorLen;
        const double r0 = rBase;
        const double r1 = rBase + len;

        QColor tick = accent;
        // Very faint: major ticks a touch stronger so structure emerges up
        // close without ever asserting itself from across the room.
        tick.setAlphaF(major ? 0.30f : 0.14f);
        QPen pen(tick);
        pen.setWidthF(major ? 1.1 : 0.7);
        pen.setCapStyle(Qt::FlatCap);
        painter.setPen(pen);
        painter.drawLine(
            QPointF(center.x() + r0 * std::cos(a),
                    center.y() - r0 * std::sin(a)),
            QPointF(center.x() + r1 * std::cos(a),
                    center.y() - r1 * std::sin(a)));
    }
}


///
/// The mental model is a conduit, not a progress bar: the lit segments are the
/// ENERGIZED portion of the instrument, and the unlit segments are AVAILABLE
/// CAPACITY -- dormant circuitry, not empty space. So unlit segments are never
/// blank: they are a very dark accent-tinted fill with a faint accent edge for
/// depth, quietly suggesting energy could flow there at any moment. The lit
/// segments own nearly all of the luminance (a soft bloom under a bright core),
/// so across the room there is never any confusion about which are active.
///
/// Every visual here has an obvious animation path: the lit/dormant boundary
/// animates as load changes, the bloom scales with interaction state, and a
/// future idle "breathing" pass can modulate the dormant tint -- none of which
/// needs a redesign, because each is already expressed as a parameter here.
void paintSegmentedRing(QPainter& painter, const CpuInstrumentLayout& layout,
                        const RingGeometry& ring, const QColor& accent,
                        double glowStrength) {
    if (!ring.present || ring.segments <= 0) {
        return;
    }

    const QPointF center(layout.centerX, layout.centerY);
    const double rOuter = ring.outerRadius;
    const double rInner = ring.outerRadius - ring.thickness;
    const double startDeg = layout.arcStartDegrees;
    const double spanDeg = layout.arcSpanDegrees;

    // Small gap between segments so they read as discrete ticks, not a bar.
    const double perSegmentDeg = spanDeg / static_cast<double>(ring.segments);
    const double gapDeg = std::min(perSegmentDeg * 0.22, 2.2);
    const int litCount = static_cast<int>(
        std::lround(ring.fillFraction * static_cast<double>(ring.segments)));

    // Dormant circuitry colors, derived from the accent so any theme's accent
    // (or a calm, flat theme) expresses correctly. Pushed one step fainter than
    // before: the dormant body is a whisper of the accent over near-black, and
    // the edge is barely there -- enough that the instrument never feels off at
    // night, never enough to be confused with an energized segment.
    QColor dormantCore = accent;
    dormantCore.setRedF(accent.redF() * 0.075f);
    dormantCore.setGreenF(accent.greenF() * 0.075f);
    dormantCore.setBlueF(accent.blueF() * 0.075f);
    QColor dormantEdge = accent;
    dormantEdge.setAlphaF(0.09f);

    for (int i = 0; i < ring.segments; ++i) {
        const double segStart = startDeg + perSegmentDeg * i + gapDeg / 2.0;
        const double segSpan = perSegmentDeg - gapDeg;
        const bool lit = (i < litCount);

        // Build the segment as an annular wedge with chamfered ends: the outer
        // corners are cut back by a small angle so the ends read as engineered
        // cuts rather than blunt stops. TRON geometry rarely uses perfectly
        // blunt ends; the chamfer gives each segment a machined edge.
        const double chamferDeg = std::min(segSpan * 0.16, 3.0);
        const double a0 = segStart * M_PI / 180.0;
        const double a0c = (segStart + chamferDeg) * M_PI / 180.0;
        const double a1 = (segStart + segSpan) * M_PI / 180.0;
        const double rChamfer = rOuter - ring.thickness * 0.30;

        const QRectF outerRect(center.x() - rOuter, center.y() - rOuter,
                               2 * rOuter, 2 * rOuter);
        const QRectF innerRect(center.x() - rInner, center.y() - rInner,
                               2 * rInner, 2 * rInner);

        QPainterPath path;
        // Start at the chamfered outer-leading corner (stepped in radially).
        path.moveTo(center.x() + rChamfer * std::cos(a0),
                    center.y() - rChamfer * std::sin(a0));
        // Up to full outer radius after the leading chamfer.
        path.lineTo(center.x() + rOuter * std::cos(a0c),
                    center.y() - rOuter * std::sin(a0c));
        // Along the outer arc to the trailing chamfer.
        path.arcTo(outerRect, segStart + chamferDeg, segSpan - 2.0 * chamferDeg);
        // Down the trailing chamfer to stepped-in radius.
        path.lineTo(center.x() + rChamfer * std::cos(a1),
                    center.y() - rChamfer * std::sin(a1));
        // Across to the inner arc trailing end.
        path.lineTo(center.x() + rInner * std::cos(a1),
                    center.y() - rInner * std::sin(a1));
        // Back along the inner arc.
        path.arcTo(innerRect, segStart + segSpan, -segSpan);
        path.closeSubpath();

        if (lit) {
            // Energized fiber-optic segment: a soft bloom underlay, then a body
            // filled with a radial gradient (bright core fading to darker edges)
            // so each segment reads like illuminated glass rather than a flat
            // fill. The bloom scales with interaction state.
            const double bloomAlpha =
                std::clamp(0.28 * glowStrength, 0.0, 0.7);
            QColor bloom = accent;
            bloom.setAlphaF(static_cast<float>(bloomAlpha));

            const double bloomGrow = ring.thickness * 0.35;
            const double bO = rOuter + bloomGrow;
            const double bI = rInner - bloomGrow;
            QPainterPath bloomPath;
            const QRectF bOuter(center.x() - bO, center.y() - bO, 2 * bO, 2 * bO);
            const QRectF bInner(center.x() - bI, center.y() - bI, 2 * bI, 2 * bI);
            bloomPath.moveTo(center.x() + bO * std::cos(a0),
                             center.y() - bO * std::sin(a0));
            bloomPath.arcTo(bOuter, segStart, segSpan);
            bloomPath.lineTo(center.x() + bI * std::cos(a1),
                             center.y() - bI * std::sin(a1));
            bloomPath.arcTo(bInner, segStart + segSpan, -segSpan);
            bloomPath.closeSubpath();

            painter.setPen(Qt::NoPen);
            painter.setBrush(bloom);
            painter.drawPath(bloomPath);

            // Fiber-optic core: brighter than the accent at the segment's mid
            // line, darker at the edges. Centered on the segment's midpoint.
            const double midDeg = segStart + segSpan / 2.0;
            const double midRad = (rOuter + rInner) / 2.0;
            const double mA = midDeg * M_PI / 180.0;
            const QPointF segMid(center.x() + midRad * std::cos(mA),
                                 center.y() - midRad * std::sin(mA));
            QColor coreBright = accent.lighter(155);
            QColor coreEdge = accent.darker(210);
            QRadialGradient grad(segMid, ring.thickness * 0.9);
            grad.setColorAt(0.0, coreBright);
            grad.setColorAt(0.5, accent);
            grad.setColorAt(1.0, coreEdge);
            painter.setBrush(grad);
            QPen rim(accent.darker(260));
            rim.setWidthF(std::max(0.5, ring.thickness * 0.06));
            painter.setPen(rim);
            painter.drawPath(path);
        } else {
            // Dormant circuitry that whispers "ready": very dark accent-tinted
            // body with a barely-there accent edge. Fainter than active by a
            // wide margin, but never fully off -- at night the instrument still
            // has quiet presence.
            painter.setBrush(dormantCore);
            QPen edge(dormantEdge);
            edge.setWidthF(std::max(0.5, ring.thickness * 0.05));
            painter.setPen(edge);
            painter.drawPath(path);
        }
    }
}

void paintCenterStack(QPainter& painter, const CpuInstrumentLayout& layout,
                      const CpuInstrumentModel& model, InstrumentSizeMode mode,
                      const AccentPair& accents) {
    const QString mono = LegacyTheme::monoFontFamily();

    // Title "CPU": present even in Small, so the instrument stays understandable
    // when moved or resized. Compact and subordinate, but deliberate -- wide
    // tracking makes it read as a machined label rather than a shy caption.
    {
        QFont f(mono);
        const int titlePx =
            (mode == InstrumentSizeMode::Small) ? 12 : LegacyTheme::fontCardTitle();
        f.setPixelSize(titlePx);
        f.setWeight(QFont::DemiBold);
        // Wide letter spacing: the label reads as engineered, not typed.
        f.setLetterSpacing(QFont::AbsoluteSpacing, titlePx * 0.30);
        painter.setFont(f);
        // A touch brighter than plain secondary, still clearly subordinate.
        QColor titleColor = LegacyTheme::textSecondary().lighter(115);
        painter.setPen(titleColor);
        const QRectF box(0, layout.titleY - titlePx, layout.side, titlePx * 1.6);
        painter.drawText(box, Qt::AlignHCenter | Qt::AlignVCenter,
                         QStringLiteral("CPU"));
    }

    // The dominant utilization value: the one number read across the room. Mono,
    // large, primary. A faint accent glow sits under it so it reads as carved
    // into the instrument and lit from within, not merely printed on top.
    // Authority comes from weight and tracking, not just size.
    {
        QFont f(mono);
        f.setPixelSize(static_cast<int>(std::lround(layout.primaryValuePx)));
        f.setWeight(QFont::Black);
        f.setLetterSpacing(QFont::AbsoluteSpacing, layout.primaryValuePx * 0.02);
        painter.setFont(f);
        const double h = layout.primaryValuePx * 1.4;
        const QRectF box(0, layout.valueY - h / 2.0, layout.side, h);
        const QString text = QString::number(model.utilizationPercent, 'f', 0)
                             + QStringLiteral("%");

        // Glow underlay in the utilization accent, low alpha.
        QColor valueGlow = accents.utilization;
        valueGlow.setAlphaF(0.25f);
        painter.setPen(valueGlow);
        painter.drawText(box, Qt::AlignHCenter | Qt::AlignVCenter, text);

        // Bright core.
        painter.setPen(LegacyTheme::textPrimary());
        painter.drawText(box, Qt::AlignHCenter | Qt::AlignVCenter, text);
    }

    // Secondary temperature: easily readable but clearly subordinate.
    {
        QFont f(mono);
        const int tempPx =
            (mode == InstrumentSizeMode::Small) ? 12 : LegacyTheme::fontCardSubtitle();
        f.setPixelSize(tempPx);
        f.setWeight(QFont::Medium);
        painter.setFont(f);
        // Subordinate: muted text, not the temperature accent, so temperature
        // does not compete with utilization for the eye. The accent lives on the
        // ring, where it separates the two readings without shouting.
        painter.setPen(LegacyTheme::textSecondary());
        const QRectF box(0, layout.secondaryY - tempPx, layout.side, tempPx * 1.8);
        // When package temperature is unavailable, show a restrained neutral
        // placeholder rather than a fabricated number. This is presentation
        // only: it carries no health meaning, it simply says "no reading".
        const QString tempText =
            (model.temperatureAvailability == ValueAvailability::Absent)
                ? QStringLiteral("--\u00B0C")
                : QString::number(model.temperatureCelsius, 'f', 0)
                      + QStringLiteral("\u00B0C");
        painter.drawText(box, Qt::AlignHCenter | Qt::AlignVCenter, tempText);
    }
}

}  // namespace

CpuInstrument::CpuInstrument(InstrumentSizeMode mode, QWidget* parent)
    : QWidget(parent), mode_(mode), transitionTimer_(new QTimer(this)) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    applySizePolicyForMode();
    // ~60fps ticks while a transition is in progress; the timer is stopped
    // whenever the displayed model has reached the target, so there is no idle
    // animation -- motion happens only between telemetry values.
    transitionTimer_->setInterval(16);
    connect(transitionTimer_, &QTimer::timeout, this,
            &CpuInstrument::advanceInterpolation);
}

void CpuInstrument::applySizePolicyForMode() {
    // The instrument paints at side = min(width, height) of whatever container
    // it is given, so a stretching layout can inflate it far past its intended
    // diameter. Small is a supporting quick-glance instrument and must NOT grow
    // to fill a large column.
    //
    // Rather than pinning an immutable pixel size, Small declares an INTENDED
    // MAXIMUM visual footprint: a maximum size at its size hint, plus a
    // non-greedy size policy. This prevents the unwanted expansion (the actual
    // problem) while preserving flexibility the Command Deck will later need --
    // the instrument may still shrink on smaller displays or in denser layouts,
    // and a future layout could raise the cap deliberately. It simply will not
    // expand past its intended footprint on its own.
    //
    // Large (and the other larger modes) keep the default, expanding policy so
    // their sizing behaviour is unchanged.
    if (mode_ == InstrumentSizeMode::Small) {
        // Preferred (not Expanding): the widget requests its hint and does not
        // greedily claim extra space, but remains free to be given less.
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setMaximumSize(sizeHint());
    } else {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        // Clear any maximum a prior Small mode may have set, so the larger
        // modes are free to occupy their region as before.
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }
}

void CpuInstrument::setModel(const CpuInstrumentModel& model) {
    target_ = model;
    // Availability changes apply immediately (a value becoming Absent should not
    // be "eased" -- the placeholder is a discrete state). Only the numeric
    // values interpolate. If this is the first model (displayed still Absent for
    // a metric that is now present), snap that metric so it does not sweep up
    // from zero on first appearance.
    if (displayed_.utilizationAvailability == ValueAvailability::Absent
        && target_.utilizationAvailability != ValueAvailability::Absent) {
        displayed_.utilizationPercent = target_.utilizationPercent;
    }
    if (displayed_.temperatureAvailability == ValueAvailability::Absent
        && target_.temperatureAvailability != ValueAvailability::Absent) {
        displayed_.temperatureCelsius = target_.temperatureCelsius;
    }
    displayed_.utilizationAvailability = target_.utilizationAvailability;
    displayed_.temperatureAvailability = target_.temperatureAvailability;

    if (interpolationSettled()) {
        displayed_.utilizationPercent = target_.utilizationPercent;
        displayed_.temperatureCelsius = target_.temperatureCelsius;
        update();
    } else if (!transitionTimer_->isActive()) {
        transitionTimer_->start();
    }
}

bool CpuInstrument::interpolationSettled() const {
    const double du =
        std::fabs(displayed_.utilizationPercent - target_.utilizationPercent);
    const double dt =
        std::fabs(displayed_.temperatureCelsius - target_.temperatureCelsius);
    return du < 0.1 && dt < 0.1;
}

void CpuInstrument::advanceInterpolation() {
    // Exponential ease toward the target: a fixed fraction of the remaining
    // distance each tick gives a smooth, framerate-tolerant approach with an
    // obvious future animation path. No overshoot, no idle motion.
    constexpr double kEase = 0.22;
    displayed_.utilizationPercent +=
        (target_.utilizationPercent - displayed_.utilizationPercent) * kEase;
    displayed_.temperatureCelsius +=
        (target_.temperatureCelsius - displayed_.temperatureCelsius) * kEase;

    if (interpolationSettled()) {
        displayed_.utilizationPercent = target_.utilizationPercent;
        displayed_.temperatureCelsius = target_.temperatureCelsius;
        transitionTimer_->stop();
    }
    update();
}

void CpuInstrument::setSizeMode(InstrumentSizeMode mode) {
    mode_ = mode;
    applySizePolicyForMode();
    updateGeometry();
    update();
}

void CpuInstrument::setInstrumentState(InstrumentState state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    update();
}

QSize CpuInstrument::sizeHint() const {
    switch (mode_) {
    case InstrumentSizeMode::Small:
        return QSize(220, 220);
    case InstrumentSizeMode::Medium:
        return QSize(320, 320);
    case InstrumentSizeMode::Large:
    case InstrumentSizeMode::Wide:
        return QSize(440, 440);
    }
    return QSize(440, 440);
}

QSize CpuInstrument::minimumSizeHint() const { return QSize(160, 160); }

void CpuInstrument::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Own the square. The instrument centers a square content box in whatever
    // rectangle it is given; the surrounding negative space is intentional.
    //
    // Composition is organized as three independent visual layers, drawn
    // back-to-front so they never fight:
    //
    //   1. STRUCTURE     -- recessed chamber, graduation ticks (and, in future,
    //                       frames, brackets, registration marks). Defines the
    //                       instrument as engineered equipment.
    //   2. ENERGY        -- the segmented conduits, their bloom and value glow
    //                       (and, in future, breathing, pulses, transitions).
    //                       Makes the instrument feel alive.
    //   3. INFORMATION   -- title, utilization value, temperature (and, in
    //                       future, graphs, diagnostics). Communicates state.
    //
    // Reading distance maps onto these layers: from across the room the user
    // perceives mainly silhouette, energy, and primary information; structural
    // craftsmanship reveals itself only as they move closer. Keeping the three
    // responsibilities separate is what will keep future evolution clean.
    const double side = std::min(width(), height());
    const double offsetX = (width() - side) / 2.0;
    const double offsetY = (height() - side) / 2.0;
    painter.translate(offsetX, offsetY);

    const CpuInstrumentLayout layout = resolveCpuInstrumentLayout(
        mode_, side, displayed_.utilizationPercent, displayed_.temperatureCelsius);
    const AccentPair accents = resolveAccents(state_);
    const double glow = glowStrengthForState(state_);

    // Dark negative space is the ground the instrument sits in. No opaque fill:
    // the instrument is composed to work over transparent or animated
    // backgrounds later, so it paints only its own marks.

    // --- Layer 1: STRUCTURE --------------------------------------------------
    // Recessed center chamber: a soft radial darkening with a faint inner rim,
    // drawn under the typography so the numbers read as floating inside a
    // recessed chamber -- looking INTO the instrument, not printed on its face.
    // Not 3D; just dimensional. Its radius follows the innermost ring so the
    // chamber always sits just inside the conduits.
    {
        const RingGeometry& innermost = layout.hasInnerRing
                                            ? layout.temperatureRing
                                            : layout.utilizationRing;
        const double chamberR = innermost.outerRadius - innermost.thickness;
        const QPointF c(layout.centerX, layout.centerY);

        QRadialGradient chamber(QPointF(c.x(), c.y() - chamberR * 0.04),
                                chamberR);
        QColor deep = LegacyTheme::backgroundBase().darker(140);
        QColor lip = LegacyTheme::backgroundRaised();
        chamber.setColorAt(0.0, deep);
        chamber.setColorAt(0.72, LegacyTheme::backgroundBase());
        chamber.setColorAt(1.0, lip);
        painter.setPen(Qt::NoPen);
        painter.setBrush(chamber);
        painter.drawEllipse(c, chamberR, chamberR);

        // Faint inner rim catches an edge of light, selling the recess.
        QColor rim = LegacyTheme::borderSubtle();
        rim.setAlphaF(0.55f);
        QPen rimPen(rim);
        rimPen.setWidthF(1.0);
        painter.setPen(rimPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(c, chamberR, chamberR);
    }

    // Fine graduation ticks: a structural calibration layer outside the outer
    // conduit. Drawn before the conduits so the energized segments always sit
    // visually on top. Tuned to vanish at distance and reward inspection up
    // close (the "multiple reading distances" principle).
    paintGraduationTicks(painter, layout, accents.utilization);

    // --- Layer 2: ENERGY -----------------------------------------------------
    // Rings, outer (primary) first, then inner (secondary) where present.
    paintSegmentedRing(painter, layout, layout.utilizationRing,
                       accents.utilization, glow);
    paintSegmentedRing(painter, layout, layout.temperatureRing,
                       accents.temperature, glow);

    // --- Layer 3: INFORMATION ------------------------------------------------
    paintCenterStack(painter, layout, displayed_, mode_, accents);

    // Reserved regions (trend band, status gap, per-core annulus) are accounted
    // for in the layout but intentionally not drawn: the composition already
    // holds their space so those features land later without recomposition.
}

}  // namespace darkspark::deck::instruments
