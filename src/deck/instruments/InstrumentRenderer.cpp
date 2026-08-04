// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/InstrumentRenderer.hpp"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRect>

#include <algorithm>
#include <cmath>

#include "deck/instruments/CpuInstrumentLayout.hpp"  // shared instrument geometry
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::instruments {

using themes::LegacyTheme;

namespace {

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
                      const InstrumentRenderModel& rm) {
    const InstrumentSizeMode mode = rm.mode;
    const InstrumentAccents& accents = rm.accents;
    const QString& title = rm.title;
    const bool awaiting = rm.awaitingTelemetry;
    const QString mono = LegacyTheme::monoFontFamily();

    // Title: whatever the render model supplies ("CPU", "GPU", ...). Present
    // even in Small so the instrument stays understandable when resized.
    // Compact and subordinate, but deliberate -- wide tracking makes it read as
    // a machined label rather than
    // a shy caption. Subsystem shells override the title (GPU, Memory, ...).
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
        painter.drawText(box, Qt::AlignHCenter | Qt::AlignVCenter, title);
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
        // When utilization has no value (a shell awaiting telemetry, or a live
        // instrument whose utilization is momentarily unavailable), show a
        // restrained placeholder instead of a fabricated 0%. Presentation only.
        const QString text =
            (rm.utilizationAvailability == ValueAvailability::Absent)
                ? QStringLiteral("--")
                : QString::number(rm.utilizationPercent, 'f', 0)
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

    // Secondary line. A neutral subordinate reading: the instrument supplies the
    // fully-formatted text (temperature, memory GB, ...), and this renderer draws
    // it without knowing what it means. For a shell it is the "Awaiting
    // Telemetry" caption; when the reading is Absent it is a neutral "--"
    // placeholder. All are muted so they never compete with the primary value.
    {
        QFont f(mono);
        const int secPx =
            (mode == InstrumentSizeMode::Small) ? 12 : LegacyTheme::fontCardSubtitle();
        f.setPixelSize(awaiting ? std::max(9, secPx - 2) : secPx);
        f.setWeight(QFont::Medium);
        if (awaiting) {
            // Slight tracking makes the caption read as a deliberate status line.
            f.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        }
        painter.setFont(f);
        // Subordinate: muted text, not an accent, so the secondary line does not
        // compete with utilization for the eye. The accent lives on the ring.
        painter.setPen(LegacyTheme::textSecondary());
        const QRectF box(0, layout.secondaryY - secPx, layout.side, secPx * 1.8);
        QString secondaryText;
        if (awaiting) {
            // A shell awaiting its real instrument: no numbers, just a quiet
            // status caption. The dormant conduits already convey "present but
            // not live".
            secondaryText = QStringLiteral("Awaiting Telemetry");
        } else if (!rm.secondaryText.isEmpty()) {
            // The instrument formatted this, units and all -- including its own
            // absence text (e.g. "--\u00B0C"). The renderer stays unit-neutral.
            secondaryText = rm.secondaryText;
        } else {
            // The instrument left the secondary empty: a restrained, unit-free
            // neutral placeholder rather than a fabricated value.
            secondaryText = QStringLiteral("--");
        }
        painter.drawText(box, Qt::AlignHCenter | Qt::AlignVCenter, secondaryText);
    }
}
/// Draw the recessed center chamber: a soft radial darkening with a faint inner
/// rim, sized to the innermost ring. Part of the STRUCTURE layer -- it gives the
/// center dimensional depth so the information reads as sitting inside the
/// instrument rather than printed on its face.
void paintChamber(QPainter& painter, const CpuInstrumentLayout& layout) {
    const RingGeometry& innermost = layout.hasInnerRing
                                        ? layout.temperatureRing
                                        : layout.utilizationRing;
    const double chamberR = innermost.outerRadius - innermost.thickness;
    const QPointF c(layout.centerX, layout.centerY);
    QRadialGradient chamber(QPointF(c.x(), c.y() - chamberR * 0.04), chamberR);
    chamber.setColorAt(0.0, LegacyTheme::backgroundBase().darker(140));
    chamber.setColorAt(0.72, LegacyTheme::backgroundBase());
    chamber.setColorAt(1.0, LegacyTheme::backgroundRaised());
    painter.setPen(Qt::NoPen);
    painter.setBrush(chamber);
    painter.drawEllipse(c, chamberR, chamberR);
    QColor rim = LegacyTheme::borderSubtle();
    rim.setAlphaF(0.55f);
    QPen rimPen(rim);
    rimPen.setWidthF(1.0);
    painter.setPen(rimPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(c, chamberR, chamberR);
}

// === The three layers of the DarkSpark Instrument Language ==================
//
// The renderer is organized around Structure / Energy / Information so future
// additions stay modular: a new structural element (frame, bracket,
// registration mark) goes in the structure layer; a new energy behavior
// (breathing, pulse, directional flow) goes in the energy layer; a new readout
// (trend, diagnostic) goes in the information layer -- each without disturbing
// the others or the compose sequence. None of these functions knows subsystem
// identity; they render only what the neutral InstrumentRenderModel describes.

/// STRUCTURE -- defines the instrument as engineered equipment. Drawn first, so
/// energy and information sit on top. (Recessed chamber, graduation ticks; in
/// future: frames, brackets, registration marks.)
void renderStructureLayer(QPainter& painter, const CpuInstrumentLayout& layout,
                          const InstrumentRenderModel& model) {
    paintChamber(painter, layout);
    paintGraduationTicks(painter, layout, model.accents.utilization);
}

/// ENERGY -- makes the instrument feel alive. The segmented conduits with their
/// bloom and fiber-optic cores. (In future: breathing, pulses, directional
/// flow.) Outer/primary conduit first, then inner/secondary where present.
void renderEnergyLayer(QPainter& painter, const CpuInstrumentLayout& layout,
                       const InstrumentRenderModel& model) {
    paintSegmentedRing(painter, layout, layout.utilizationRing,
                       model.accents.utilization, model.glowStrength);
    paintSegmentedRing(painter, layout, layout.temperatureRing,
                       model.accents.temperature, model.glowStrength);
}

/// INFORMATION -- communicates state. Title, dominant value, secondary line.
/// (In future: trend graphs, diagnostics.) Drawn last, over structure + energy.
void renderInformationLayer(QPainter& painter, const CpuInstrumentLayout& layout,
                            const InstrumentRenderModel& model) {
    paintCenterStack(painter, layout, model);
}

}  // namespace

namespace InstrumentRenderer {

void paint(QPainter& painter, const QRect& widgetRect,
           const InstrumentRenderModel& model) {
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Own the square: centre a square content box in the widget rectangle; the
    // surrounding negative space is intentional. The composition is three
    // layers drawn back-to-front so they never fight, and reading distance maps
    // onto them: from across the room the eye gets silhouette + energy + primary
    // value; structural craftsmanship resolves only up close. Each layer is a
    // single call so future elements slot into the right layer without touching
    // this sequence.
    const double side = std::min(widgetRect.width(), widgetRect.height());
    const double offsetX = (widgetRect.width() - side) / 2.0;
    const double offsetY = (widgetRect.height() - side) / 2.0;

    painter.save();
    painter.translate(widgetRect.x() + offsetX, widgetRect.y() + offsetY);

    const CpuInstrumentLayout layout = resolveCpuInstrumentLayout(
        model.mode, side, model.utilizationPercent, model.secondaryValue);

    renderStructureLayer(painter, layout, model);    // Layer 1
    renderEnergyLayer(painter, layout, model);       // Layer 2
    renderInformationLayer(painter, layout, model);  // Layer 3

    painter.restore();
}

}  // namespace InstrumentRenderer

}  // namespace darkspark::deck::instruments
