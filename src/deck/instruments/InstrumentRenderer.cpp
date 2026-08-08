// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/InstrumentRenderer.hpp"

#include <QFont>
#include <QFontMetricsF>
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

// Warm-shift an accent toward a warmer hue by `warmth` in [0,1]. At warmth 0 the
// color is returned unchanged (neutral-safe). Subsystem-agnostic: the renderer
// applies whatever warmth the model carries, without knowing its source.
QColor applyWarmth(const QColor& accent, double warmth) {
    if (warmth <= 0.0) {
        return accent;
    }
    const double w = warmth > 1.0 ? 1.0 : warmth;
    // Shift toward a warm amber: raise red, ease green slightly, lower blue.
    QColor out = accent;
    out.setRedF(static_cast<float>(std::clamp(accent.redF() + 0.45 * w, 0.0, 1.0)));
    out.setGreenF(static_cast<float>(
        std::clamp(accent.greenF() + 0.12 * w, 0.0, 1.0)));
    out.setBlueF(static_cast<float>(
        std::clamp(accent.blueF() - 0.30 * w, 0.0, 1.0)));
    return out;
}

constexpr double kTwoPi = 6.283185307179586;

// Reactor thermal color: tells the temperature STORY as a plasma would.
//   cold  -> cyan (the cool accent)
//   normal-> blue-white (hotter, energetic)
//   hot   -> amber
//   extreme-> orange -> red
// `t` is normalized thermal [0,1] (personality warmth). Continuous/piecewise.
QColor reactorPlasmaColor(double t, bool outer) {
    const double x = std::clamp(t, 0.0, 1.0);
    // Saturated blue identity held far up the range.
    const QColor cobalt(20, 70, 200);
    const QColor electric(30, 140, 255);
    const QColor litBlue(120, 205, 255);
    const QColor blueWhite(205, 234, 255);
    const QColor darkBlue(28, 88, 188);
    // Molten-metal hot end (never bright yellow).
    const QColor amber(196, 120, 44);
    const QColor orange(200, 84, 28);
    const QColor deepOrange(176, 58, 20);
    const QColor red(150, 34, 18);
    auto mix = [](const QColor& a, const QColor& b, double f) {
        QColor o;
        o.setRedF(static_cast<float>(
            std::clamp(a.redF() + (b.redF() - a.redF()) * f, 0.0, 1.0)));
        o.setGreenF(static_cast<float>(
            std::clamp(a.greenF() + (b.greenF() - a.greenF()) * f, 0.0, 1.0)));
        o.setBlueF(static_cast<float>(
            std::clamp(a.blueF() + (b.blueF() - a.blueF()) * f, 0.0, 1.0)));
        return o;
    };
    if (outer) {
        // Predominantly cobalt/electric blue; molten only in the top ~10%.
        if (x <= 0.35) {
            return mix(cobalt, electric, x / 0.35);
        }
        if (x <= 0.72) {
            return mix(electric, litBlue, (x - 0.35) / 0.37);
        }
        if (x <= 0.82) {
            return mix(litBlue, blueWhite, (x - 0.72) / 0.10);
        }
        if (x <= 0.90) {
            const QColor deep = mix(blueWhite, darkBlue, (x - 0.82) / 0.08);
            return mix(deep, amber, (x - 0.82) / 0.08 * 0.20);
        }
        if (x <= 0.95) {
            return mix(amber, orange, (x - 0.90) / 0.05);
        }
        if (x <= 0.98) {
            return mix(orange, deepOrange, (x - 0.95) / 0.03);
        }
        return mix(deepOrange, red, (x - 0.98) / 0.02);
    }
    // Centre stays cobalt -> white-blue; hottest POINT is white-blue.
    if (x <= 0.85) {
        return mix(cobalt, blueWhite, x / 0.85);
    }
    return mix(blueWhite, amber, (x - 0.85) / 0.15 * 0.14);
}

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
                        double glowStrength, double flowPhase = 0.0,
                        double flowStrength = 0.0) {
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
            // Neutral glow (1.0) still yields the original 0.28; personality
            // lifts it modestly. Restrained so motion leads, not brightness.
            const double bloomAlpha =
                std::clamp(0.28 * glowStrength, 0.0, 0.55);
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
            // Traveling energy flow: a brightness wave sweeps along the lit
            // conduit. Each segment's phase along the arc is offset so the peak
            // moves with flowPhase. At flowStrength 0 the highlight vanishes and
            // the segment is exactly its original appearance (neutral-safe).
            const double segFrac =
                static_cast<double>(i) / static_cast<double>(ring.segments);
            const double wave =
                0.5 * (std::sin(flowPhase - segFrac * 6.283185307179586 * 2.0)
                       + 1.0);
            const double flowLift = flowStrength * wave;  // [0, flowStrength]
            const int brighten = 155 + static_cast<int>(std::lround(120.0 * flowLift));
            QColor coreBright = accent.lighter(brighten);
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
        // The instrument supplies the fully-formatted primary value and its unit
        // suffix; the renderer paints them and never interprets the unit. When
        // the primary has no value (a shell awaiting telemetry, or a live
        // instrument whose primary is momentarily unavailable), show a restrained
        // placeholder instead of a fabricated value. Presentation only.
        const QString text =
            (rm.primary.availability == ValueAvailability::Absent)
                ? QStringLiteral("--")
                : rm.primary.text + rm.primary.suffix;

        // Reference text treatment: cyan OUTER GLOW -> DARK OUTLINE -> WHITE
        // FILL, as a stroked glyph path (renderer-based), so the percentage
        // stays instantly readable over any reactor brightness.
        QPainterPath glyphs;
        {
            QFontMetricsF fm(f);
            const double tw = fm.horizontalAdvance(text);
            const double bx = (layout.side - tw) / 2.0;
            const double by =
                box.center().y() + fm.ascent() / 2.0 - fm.descent() / 2.0;
            glyphs.addText(QPointF(bx, by), f, text);
        }
        const QColor cyanGlow(110, 230, 255);
        for (int pass = 3; pass >= 1; --pass) {
            QColor gg = cyanGlow;
            gg.setAlphaF(static_cast<float>(0.11 * pass));
            QPen gp(gg);
            gp.setWidthF(layout.primaryValuePx * (0.06 * pass + 0.05));
            gp.setJoinStyle(Qt::RoundJoin);
            painter.setPen(gp);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(glyphs);
        }
        QColor outline(6, 12, 20);
        outline.setAlphaF(0.95f);
        QPen op(outline);
        op.setWidthF(std::max(2.0, layout.primaryValuePx * 0.06));
        op.setJoinStyle(Qt::RoundJoin);
        painter.setPen(op);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(glyphs);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255));
        painter.drawPath(glyphs);
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
        } else if (rm.secondary.availability == ValueAvailability::Absent
                   || rm.secondary.text.isEmpty()) {
            // No current secondary reading: a restrained, unit-free neutral
            // placeholder rather than a fabricated value.
            secondaryText = QStringLiteral("--");
        } else {
            // The instrument supplies value text + unit suffix; the renderer
            // composes them exactly as it does the primary and never interprets
            // the unit.
            secondaryText = rm.secondary.text + rm.secondary.suffix;
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
    // STRUCTURE stays mostly static; personality touches it only as a subtle
    // warm-shift of the graduation ticks. At warmth 0 this is a no-op, so the
    // structure is byte-identical to before.
    paintChamber(painter, layout);
    paintGraduationTicks(painter, layout,
                         applyWarmth(model.accents.utilization,
                                     model.personality.warmth));
}

/// ENERGY -- makes the instrument feel alive. The segmented conduits with their
/// bloom and fiber-optic cores. (In future: breathing, pulses, directional
/// flow.) Outer/primary conduit first, then inner/secondary where present.

// Reactor core: the CENTER pulse that leads The Core's personality. A radial
// glow at the instrument center whose radius and intensity breathe with the
// pulse + ambient params. Drawn in the Energy layer (behind the Information
// text). Subsystem-agnostic: it reacts only to neutral scalar params and to the
// accent color, never to subsystem identity. At pulse 0 and ambient 0 it draws
// nothing, so neutral instruments are unaffected.
void paintReactorCore(QPainter& painter, const CpuInstrumentLayout& layout,
                      double pulse, double ambient,
                      double warmth, double flowPhase) {
    // =====================================================================
    // THE CORE -- a SEALED REACTOR MODULE. The viewer is looking through a
    // thick armored viewport into a dangerous piece of hardware. The MACHINE
    // is the hero; the star exists to justify the machine.
    //
    // Read order (what the eye should catch, in sequence):
    //   1. a heavy engineered machine (titanium housing, bolts, seams)
    //   2. a reinforced containment chamber (machined stepped bezel)
    //   3. thick armored glass (depth, bevel, one specular catch)
    //   4. a brilliant blue star trapped inside (smaller, secondary)
    //   5. subtle plasma motion
    //
    // Designed OUTSIDE-IN. Every element must answer "would an engineer have
    // built this?" -- if not, it is not drawn.
    //
    // ONE LIGHTING RULE: the star is the only light source. Metal, glass,
    // clamps and bezel do NOT glow; they are visible only where the star
    // lights their inward faces (bright toward centre, black outward). Only
    // plasma is self-luminous (additive).
    // =====================================================================
    const double thermal = std::clamp(warmth, 0.0, 1.0);
    const double ambientC = std::clamp(ambient, 0.0, 1.0);
    const double pulseC = std::clamp(pulse, 0.0, 1.0);

    const double signal = thermal + ambientC + pulseC;
    if (signal <= 0.001) {
        return;  // neutral instruments draw nothing
    }

    const RingGeometry& innermost = layout.hasInnerRing
                                        ? layout.temperatureRing
                                        : layout.utilizationRing;
    const double R = innermost.outerRadius - innermost.thickness;  // chamber rad
    const QPointF c(layout.centerX, layout.centerY);
    const double clr = std::max(0.0, layout.primaryValuePx * 0.70 * 0.92);

    struct ReactorTuning {
        // --- Machine housing (structural, dominant; fractions of R) ---
        double housingOuter = 1.00;    // fills the chamber bounding rect
        double housingInner = 0.72;    // where the machined bezel begins
        int panels = 8;                // beveled panel segments (seams between)
        double seamGap = 3.0;          // degrees of dark seam between panels
        double boltInset = 0.90;       // bolt-head ring radius
        int bolts = 8;
        // --- Clamp modules (4 cardinal containment actuators) ---
        int clamps = 4;
        double clampSpan = 30.0;       // degrees each clamp subtends
        double clampOuter = 0.98;
        double clampInner = 0.60;      // clamps bridge housing -> chamber
        double clampBarInset = 0.70;   // cyan field-indicator bar position
        // --- Containment chamber / bezel (machined depth) ---
        double bezelOuter = 0.72;
        double bezelInner = 0.60;      // stepped: outer ring + recessed lip
        // --- Armored glass (thick viewport) ---
        double glassOuter = 0.60;
        double glassInner = 0.54;      // glass thickness band
        // --- Electrical core (living reaction inside the cavity) ---
        double cavity = 0.50;          // core cavity radius (fraction of R)
        int primaryArcs = 6;           // FIXED pool; utilization ACTIVATES them
        int arcSegments = 9;           // segments per major arc (irregular path)
        double arcStep = 0.5;          // segment length as fraction of cavity/seg
        double arcJitter = 0.42;       // heading jitter (radians) -> electrical
        double inwardPull = 0.5;       // containment forcing arcs back to centre
        int secondaryMax = 2;          // secondary branches per primary (activated)
        double nucleusRadius = 0.06;   // 6% of cavity: tiny dense point
        // Depth-pass presence (rear/middle/front hierarchy).
        double rearAlpha = 0.28;
        double midAlpha = 0.6;
        double frontAlpha = 0.92;
        double haloAlpha = 0.1;        // extremely restrained; removable
        double coreLineAlpha = 0.85;   // thin white-blue centre on front arcs
        // Behaviour.
        double activationBase = 0.18;  // lowest-load activation floor
        double reconnectChance = 0.5;  // how often arcs reconnect (phase-driven)
        double stressTempThreshold = 0.80;  // amber stress only above this
        double metalReflect = 0.35;    // faint cold-blue cast on nearby metal
        // --- Behaviour mapping (utilization drives INDEPENDENT properties, not
        //     one global speed; each arc also has its own deterministic
        //     personality so the six never read as clones) ---
        double evolSpeedBase = 0.12;   // arc topology evolution rate at idle
        double evolSpeedLoad = 0.80;   // added evolution rate at full load (curve)
        double speedVariance = 0.4;    // +/- per-arc speed personality
        double thicknessLoad = 0.6;    // arcs thicken this much toward full load
        double thicknessVariance = 0.3;  // +/- per-arc thickness personality
        double reachLoad = 0.32;       // arcs reach farther out under load
        double curvatureVariance = 0.5;  // +/- per-arc jitter/curvature character
        double lifetimeBase = 0.05;    // envelope base rate (slow heartbeat)
        double lifetimeVariance = 0.6; // per-arc lifetime-period spread
        double lifetimeOverlap = 0.85; // load raises envelope floor -> arcs
                                       // overlap (dense) instead of speeding up
        double nucleusPulseAmpIdle = 0.06;   // subtle size/bright pulse at idle
        double nucleusPulseAmpLoad = 0.14;   // stronger (never large) at load
        double nucleusPulseSpeedIdle = 0.5;  // slow heartbeat
        double nucleusPulseSpeedLoad = 2.0;  // energised, never flashing
        // Load response CURVE: ease-in so low CPU changes little and activity
        // ramps sharply near the top. Every load-driven term routes through
        // pow(pulseC, loadCurveExp). 3.0 -> 10% and 30% look alike; 70%->100%
        // diverge hard.
        double loadCurveExp = 3.0;
        // --- Material reflectivities (base gray the star light multiplies) ---
        double titaniumHi = 0.72;      // machined edge catching light
        double titaniumLo = 0.14;      // panel body in shadow
        double copperHi = 0.66;        // coil-spring highlight (warm-ish metal)
        double boltGray = 0.5;
    };
    constexpr ReactorTuning T{};

    const double intensity =
        std::clamp(0.36 + 0.30 * pulseC + 0.22 * thermal + 0.12 * ambientC,
                   0.0, 1.0);
    // The electrical reaction fills a fixed cavity (the machine doesn't move);
    // its ACTIVITY, not its size, responds to load. `activation` [0,1] smoothly
    // fades electrical structures in/out -- no abrupt branch-count switching.
    const double cavityR = R * T.cavity;
    const double activation =
        std::clamp(T.activationBase + 0.82 * pulseC, 0.0, 1.0);
    // Ease-in load response: low CPU changes little; activity ramps sharply near
    // the top. Every load-driven electrical term routes through this.
    const double loadCurve = std::pow(pulseC, T.loadCurveExp);

    const QColor starLight = reactorPlasmaColor(thermal, true);
    const QColor coreLight = reactorPlasmaColor(thermal, false);
    // The single-light-source rule in one function: tint a surface as if lit by
    // the star. `reach` is how much star light gets here (falls with distance /
    // facing away); `mat` is the surface's own reflectivity. reach 0 -> black.
    auto litBy = [&](double reach, double mat) {
        const double g = std::clamp(reach, 0.0, 1.0) * std::clamp(mat, 0.0, 1.0);
        QColor o;
        o.setRedF(static_cast<float>(std::clamp(starLight.redF() * g, 0.0, 1.0)));
        o.setGreenF(static_cast<float>(std::clamp(starLight.greenF() * g, 0.0,
                                                  1.0)));
        o.setBlueF(static_cast<float>(std::clamp(starLight.blueF() * g, 0.0,
                                                 1.0)));
        return o;
    };
    // Copper is a warm metal: bias the star light toward amber for coil springs.
    auto litCopper = [&](double reach) {
        QColor sc = litBy(reach, T.copperHi);
        QColor o;
        o.setRedF(static_cast<float>(std::clamp(sc.redF() * 1.15 + 0.10, 0.0,
                                                1.0)));
        o.setGreenF(static_cast<float>(std::clamp(sc.greenF() * 0.85 + 0.04, 0.0,
                                                  1.0)));
        o.setBlueF(static_cast<float>(std::clamp(sc.blueF() * 0.55, 0.0, 1.0)));
        return o;
    };

    const QPainter::CompositionMode prevMode = painter.compositionMode();
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(Qt::NoPen);

    auto deg2 = [](double d) { return static_cast<int>(d * 16.0); };

    // ============ (1) HEAVY MACHINE HOUSING ===============================
    // Beveled titanium panel segments filling the chamber, with dark seams
    // between them and a ring of bolt heads. Lit on inner edges by the star.
    // An engineer built this to shield and mount the vessel.
    {
        const double rOut = R * T.housingOuter;
        const double rIn = R * T.housingInner;
        const double step = 360.0 / T.panels;
        for (int i = 0; i < T.panels; ++i) {
            const double a0 = i * step + T.seamGap * 0.5;
            const double a1 = (i + 1) * step - T.seamGap * 0.5;
            const double span = a1 - a0;
            QPainterPath panel;
            panel.arcMoveTo(-rOut, -rOut, 2 * rOut, 2 * rOut, a0);
            panel.arcTo(-rOut, -rOut, 2 * rOut, 2 * rOut, a0, span);
            panel.arcTo(-rIn, -rIn, 2 * rIn, 2 * rIn, a1, -span);
            panel.closeSubpath();
            // Titanium: bright machined INNER edge (faces the star), dark body
            // outward. Radial gradient (origin-centred, we translate to c).
            QRadialGradient pg(QPointF(0, 0), rOut);
            QColor hi = litBy(0.62, T.titaniumHi);
            hi.setAlphaF(0.98f);
            QColor lo = litBy(0.12, T.titaniumLo);
            lo.setAlphaF(1.0f);
            QColor edge(3, 5, 8);
            edge.setAlphaF(1.0f);
            pg.setColorAt(std::clamp(rIn / rOut, 0.0, 0.99), hi);
            pg.setColorAt(std::clamp((rIn / rOut + 1.0) * 0.5, 0.0, 0.995), lo);
            pg.setColorAt(1.0, edge);
            painter.save();
            painter.translate(c);
            painter.setBrush(pg);
            painter.setPen(Qt::NoPen);
            painter.drawPath(panel);
            painter.restore();
        }
        // Bolt heads: small machined studs around the housing.
        for (int b = 0; b < T.bolts; ++b) {
            const double a = (b + 0.5) * (360.0 / T.bolts) * M_PI / 180.0;
            const double br = R * T.boltInset;
            const QPointF bc(c.x() + br * std::cos(a), c.y() - br * std::sin(a));
            const double bs = std::max(2.0, R * 0.022);
            QRadialGradient bg(QPointF(bc.x() - bs * 0.3, bc.y() - bs * 0.3), bs);
            QColor bh = litBy(0.7, T.boltGray + 0.2);
            bh.setAlphaF(1.0f);
            QColor bl(5, 7, 10);
            bl.setAlphaF(1.0f);
            bg.setColorAt(0.0, bh);
            bg.setColorAt(1.0, bl);
            painter.setBrush(bg);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(bc, bs, bs);
            QColor slot(2, 3, 5);
            slot.setAlphaF(0.8f);
            painter.setBrush(slot);
            painter.drawEllipse(bc, bs * 0.4, bs * 0.4);
        }
    }

    // ============ (2) CONTAINMENT CHAMBER -- machined stepped bezel ========
    // A structural flange with depth: an outer ring, then a recessed inner lip.
    // Titanium, lit from the star; the step reads via a lit face + a shadow.
    {
        QRadialGradient bz(c, R * T.bezelOuter);
        QColor face = litBy(0.5, T.titaniumHi);
        face.setAlphaF(1.0f);
        QColor body = litBy(0.16, T.titaniumLo);
        body.setAlphaF(1.0f);
        bz.setColorAt(0.0, body);
        bz.setColorAt(0.9, body);
        bz.setColorAt(0.965, face);   // machined outer lip catches light
        bz.setColorAt(1.0, QColor(4, 6, 9));
        painter.setBrush(bz);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(c, R * T.bezelOuter, R * T.bezelOuter);
        QRadialGradient lip(c, R * T.bezelInner + R * 0.02);
        QColor shadow(3, 5, 8);
        shadow.setAlphaF(1.0f);
        QColor litEdge = litBy(0.65, T.titaniumHi);
        litEdge.setAlphaF(1.0f);
        lip.setColorAt(0.0, shadow);
        lip.setColorAt(0.86, shadow);
        lip.setColorAt(0.95, litEdge);  // inner lip grazed by the star
        lip.setColorAt(1.0, QColor(2, 3, 5));
        painter.setBrush(lip);
        painter.drawEllipse(c, R * T.bezelInner + R * 0.02,
                            R * T.bezelInner + R * 0.02);
    }

    // ============ (3) FOUR CLAMP MODULES -- containment actuators ==========
    // At 12/3/6/9. Machined body bridging housing->chamber, a beveled cap, a
    // recessed cyan field-indicator bar (lit, not glowing), and copper
    // superconducting-coil hatching. They read as physically clamping the ring.
    for (int k = 0; k < T.clamps; ++k) {
        const double ang = k * 90.0;   // degrees, cardinal
        painter.save();
        painter.translate(c);
        painter.rotate(-ang);
        const double rO = R * T.clampOuter;
        const double rI = R * T.clampInner;
        const double half = T.clampSpan / 2.0;
        QPainterPath bodyP;
        bodyP.arcMoveTo(-rO, -rO, 2 * rO, 2 * rO, -half);
        bodyP.arcTo(-rO, -rO, 2 * rO, 2 * rO, -half, T.clampSpan);
        bodyP.arcTo(-rI, -rI, 2 * rI, 2 * rI, half, -T.clampSpan);
        bodyP.closeSubpath();
        QLinearGradient bg(rI, 0, rO, 0);
        QColor bHi = litBy(0.62, T.titaniumHi);
        bHi.setAlphaF(1.0f);
        QColor bLo(7, 10, 14);
        bLo.setAlphaF(1.0f);
        bg.setColorAt(0.0, bHi);       // inner face toward star lit
        bg.setColorAt(0.55, bLo);
        bg.setColorAt(1.0, QColor(3, 5, 8));
        painter.setBrush(bg);
        painter.setPen(Qt::NoPen);
        painter.drawPath(bodyP);
        QColor capLit = litBy(0.8, T.titaniumHi + 0.15);
        capLit.setAlphaF(static_cast<float>(std::clamp(0.6 + 0.3 * intensity,
                                                       0.0, 0.95)));
        QPen capPen(capLit);
        capPen.setWidthF(std::max(1.0, R * 0.006));
        painter.setPen(capPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(QRectF(-rI, -rI, 2 * rI, 2 * rI), deg2(-half),
                        deg2(T.clampSpan));
        // Copper superconducting-coil hatching across the clamp body.
        const int ribs = 5;
        for (int rb = 0; rb <= ribs; ++rb) {
            const double tt = static_cast<double>(rb) / ribs;
            const double aa = (-half + T.clampSpan * tt) * M_PI / 180.0;
            const double r1 = R * (T.clampInner + 0.03);
            const double r2 = R * (T.clampBarInset - 0.02);
            QColor cu = litCopper(0.55 + 0.35 * std::cos(aa * 3.0));
            cu.setAlphaF(0.9f);
            QPen cp(cu);
            cp.setWidthF(std::max(1.0, R * 0.008));
            painter.setPen(cp);
            painter.drawLine(QPointF(r1 * std::cos(aa), -r1 * std::sin(aa)),
                             QPointF(r2 * std::cos(aa), -r2 * std::sin(aa)));
        }
        // Recessed cyan field-indicator bar (lit by star; a status readout).
        const double barR = R * T.clampBarInset;
        QColor bar = litBy(0.9, 0.8);
        bar.setBlueF(static_cast<float>(std::clamp(bar.blueF() * 1.1 + 0.05, 0.0,
                                                   1.0)));
        bar.setAlphaF(static_cast<float>(std::clamp(0.55 + 0.4 * intensity, 0.0,
                                                    0.95)));
        QPen barPen(bar);
        barPen.setWidthF(std::max(1.4, R * 0.02));
        barPen.setCapStyle(Qt::RoundCap);
        painter.setPen(barPen);
        painter.drawArc(QRectF(-barR, -barR, 2 * barR, 2 * barR),
                        deg2(-half * 0.5), deg2(T.clampSpan * 0.5));
        painter.restore();
    }

    // ============ (4) ARMORED GLASS -- thick viewport =====================
    // A dark glass band with thickness: a shadowed outer edge, a faint blue
    // inner tint (the star seen through glass), and ONE curved specular catch.
    {
        QRadialGradient gl(c, R * T.glassOuter);
        QColor gOuter(6, 10, 16);
        gOuter.setAlphaF(1.0f);
        QColor gTint = litBy(0.4, 0.5);
        gTint.setAlphaF(0.7f);
        gl.setColorAt(0.0, gTint);
        gl.setColorAt(0.85, gTint);
        gl.setColorAt(0.93, gOuter);      // thickness shadow
        gl.setColorAt(1.0, QColor(2, 4, 7));
        painter.setBrush(gl);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(c, R * T.glassOuter, R * T.glassOuter);
    }

    // NOTE: the glass clip is established inline below with an explicit
    // save()/setClipPath()/restore() block (no generic lambda) to avoid a GCC 16
    // template-instantiation ICE in tsubst_expr while substituting a local
    // `auto&&` callback. Behaviour is identical; artwork is unchanged.

    painter.save();
    {
        QPainterPath glassClip;
        glassClip.addEllipse(c, R * T.glassInner, R * T.glassInner);
        painter.setClipPath(glassClip);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QRadialGradient well(c, R * T.glassInner);
        QColor w0 = litBy(0.45, 0.5);
        w0.setAlphaF(0.8f);
        QColor w1(2, 4, 7);
        w1.setAlphaF(1.0f);
        well.setColorAt(0.0, w0);
        well.setColorAt(0.5, w1);
        well.setColorAt(1.0, w1);
        painter.setBrush(well);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(c, R * T.glassInner, R * T.glassInner);

        // =============================================================
        // LIVING ELECTRICAL CORE. No sphere is drawn. The volume is IMPLIED by
        // the spatial distribution of irregular electrical arcs branching from a
        // tiny central nucleus. Deterministic in flowPhase (same phase -> same
        // geometry; smooth evolution, no per-frame randomness). A FIXED pool of
        // arcs is ACTIVATED by utilization (fade in/out), never switched on.
        //
        // Depth by hierarchy: rear (deep cobalt, thin, soft) -> middle (electric
        // blue) -> front (crisp cyan, white-blue cores). Per arc: faint halo ->
        // saturated body -> thin white-blue core (halo removable, still crisp).
        // Temperature = localized stress: blue identity dominant; amber/orange
        // only on stressed outer/contact segments. Glow minimal.
        // =============================================================
        const double coreClear = std::max(2.0, clr * 0.42);  // text-safe centre

        // Deterministic pseudo-noise in [-1,1] from integer-ish seeds + phase.
        auto wob = [](double a, double b, double ph) {
            const double v = std::sin(a * 12.9898 + b * 78.233 + ph)
                             * 43758.5453;
            return 2.0 * (v - std::floor(v)) - 1.0;
        };

        // One arc: an irregular segmented polyline from the nucleus outward,
        // bending back toward centre (containment). Drawn as halo/body/core in a
        // colour set by its depth pass and any thermal stress near its far end.
        // pass: 0 rear, 1 middle, 2 front. `act` scales visibility (activation).
        auto drawArc = [&](int seed, double ang0, int pass, double act,
                           bool secondary, double evolRate, double spdMul,
                           double curveMul, double thickMul) {
            if (act <= 0.02) {
                return;
            }
            // Outward reach grows with load (arcs push toward containment).
            const double loadReach = 1.0 + T.reachLoad * loadCurve;
            const double reach =
                cavityR * (secondary ? 0.55 : 0.94) * loadReach;
            const int segs = secondary ? T.arcSegments / 2 : T.arcSegments;
            const double segLen = reach * T.arcStep / segs * 2.0;
            // Build points.
            QPointF pts[16];
            int n = 0;
            double px = c.x();
            double py = c.y();
            double heading = ang0;
            double rNow = coreClear;
            pts[n++] = QPointF(px, py);
            for (int sIdx = 1; sIdx <= segs && n < 16; ++sIdx) {
                const double sf = static_cast<double>(sIdx) / segs;
                // Heading jitter: electrical direction changes, evolving in phase.
                // Per-arc evolution: its own rate (load-scaled) and phase seat,
                // so no two arcs crawl at the same pace.
                const double ph = flowPhase * evolRate * spdMul + seed * 1.7;
                heading += T.arcJitter * curveMul
                           * wob(seed + sIdx * 0.7, pass + sf, ph);
                // Containment: dampen the outward drift as we get far out, so the
                // path curls rather than shooting straight (the radial clamp
                // below is the hard limit; this is the soft bend).
                const double pull = T.inwardPull * sf * sf;
                heading *= (1.0 - pull * 0.5);
                const double step = segLen * (0.7 + 0.5 * (1.0 - sf));
                px += step * std::cos(heading);
                py -= step * std::sin(heading);
                // Radial clamp: never leave the cavity (machine forces it back).
                const double dx = px - c.x();
                const double dy = py - c.y();
                double rr = std::sqrt(dx * dx + dy * dy);
                const double rMax = reach * (1.0 - 0.15 * pull);
                if (rr > rMax) {
                    // Bend sharply along the boundary then turn inward.
                    const double wallAng = std::atan2(-dy, dx);
                    px = c.x() + rMax * std::cos(wallAng);
                    py = c.y() - rMax * std::sin(wallAng);
                    heading = wallAng + M_PI * 0.6;  // turn back inward
                    rr = rMax;
                }
                rNow = rr;
                pts[n++] = QPointF(px, py);
            }
            (void)rNow;
            // Contact brightening: how close the far end got to the wall.
            const double contact =
                std::clamp((std::sqrt(std::pow(pts[n - 1].x() - c.x(), 2)
                                      + std::pow(pts[n - 1].y() - c.y(), 2))
                            / cavityR - 0.7)
                               / 0.3,
                           0.0, 1.0);
            // Depth-pass colour + presence. Arcs are noticeably thicker now
            // (energy tearing through the volume), scaled by load and each arc's
            // own thickness personality. The white centreline stays narrow.
            const double thickK =
                thickMul * (1.0 + T.thicknessLoad * loadCurve);
            double baseAlpha;
            QColor body;
            double bodyW;
            if (pass == 0) {  // rear
                body = reactorPlasmaColor(thermal, true).darker(220);
                baseAlpha = T.rearAlpha;
                bodyW = std::max(1.6, cavityR * 0.030 * thickK);
            } else if (pass == 1) {  // middle
                body = reactorPlasmaColor(thermal, true);
                baseAlpha = T.midAlpha;
                bodyW = std::max(2.0, cavityR * 0.050 * thickK);
            } else {  // front
                body = reactorPlasmaColor(thermal, true).lighter(120);
                baseAlpha = T.frontAlpha;
                bodyW = std::max(2.4, cavityR * 0.060 * thickK);
            }
            // Thermal stress: only near the far/contact end, only when hot.
            const double stress =
                thermal > T.stressTempThreshold
                    ? (thermal - T.stressTempThreshold)
                          / (1.0 - T.stressTempThreshold) * contact
                    : 0.0;
            if (stress > 0.02) {
                QColor amber(196, 120, 44);
                body = QColor(
                    static_cast<int>(std::clamp(
                        body.red() + (amber.red() - body.red()) * stress, 0.0,
                        255.0)),
                    static_cast<int>(std::clamp(
                        body.green() + (amber.green() - body.green()) * stress,
                        0.0, 255.0)),
                    static_cast<int>(std::clamp(
                        body.blue() + (amber.blue() - body.blue()) * stress, 0.0,
                        255.0)));
            }
            QPainterPath path;
            path.moveTo(pts[0]);
            for (int k = 1; k < n; ++k) {
                path.lineTo(pts[k]);
            }
            const double a = baseAlpha * act;
            // (1) Halo -- extremely restrained, front pass only, removable.
            if (pass == 2 && T.haloAlpha > 0.0) {
                painter.setCompositionMode(QPainter::CompositionMode_Plus);
                QColor halo = body.lighter(140);
                halo.setAlphaF(static_cast<float>(
                    std::clamp(T.haloAlpha * act * (0.6 + 0.6 * contact), 0.0,
                               0.2)));
                QPen hp(halo);
                hp.setWidthF(std::min(bodyW * 1.8, cavityR * 0.11));
                hp.setCapStyle(Qt::RoundCap);
                hp.setJoinStyle(Qt::RoundJoin);
                painter.setPen(hp);
                painter.setBrush(Qt::NoBrush);
                painter.drawPath(path);
            }
            // (2) Saturated body -- the main visible electricity.
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            QColor bc = body;
            bc.setAlphaF(static_cast<float>(
                std::clamp(a * (0.7 + 0.3 * contact), 0.0, 0.95)));
            QPen bp(bc);
            bp.setWidthF(bodyW);
            bp.setCapStyle(Qt::RoundCap);
            bp.setJoinStyle(Qt::RoundJoin);
            painter.setPen(bp);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
            // (3) Thin white-blue core -- front (and contact) only.
            if (pass == 2) {
                painter.setCompositionMode(QPainter::CompositionMode_Plus);
                QColor coreC(226, 240, 255);
                coreC.setAlphaF(static_cast<float>(
                    std::clamp(T.coreLineAlpha * act * (0.5 + 0.5 * contact),
                               0.0, 0.95)));
                QPen cpn(coreC);
                cpn.setWidthF(std::clamp(bodyW * 0.28, 1.0, cavityR * 0.014));
                cpn.setCapStyle(Qt::RoundCap);
                painter.setPen(cpn);
                painter.drawPath(path);
                painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            }
            // Faint cold-blue reflected light on the nearest inner metal when an
            // arc's far end nears the wall (lighting only -- no geometry change).
            if (contact > 0.5 && pass == 2) {
                painter.setCompositionMode(QPainter::CompositionMode_Plus);
                QColor refl = reactorPlasmaColor(thermal, true);
                refl.setAlphaF(static_cast<float>(
                    std::clamp(T.metalReflect * act * (contact - 0.5) * 2.0, 0.0,
                               0.3)));
                QRadialGradient rg(pts[n - 1], cavityR * 0.4);
                rg.setColorAt(0.0, refl);
                QColor re = refl;
                re.setAlphaF(0.0f);
                rg.setColorAt(1.0, re);
                painter.setBrush(rg);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(pts[n - 1], cavityR * 0.4, cavityR * 0.4);
                painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            }
        };

        // Draw the FIXED pool across depth passes, rear -> front. Each arc has
        // its own deterministic PERSONALITY (speed, curvature, thickness, phase
        // seat, and a lifetime period), so the six never read as clones. A
        // continuous lifetime ENVELOPE makes each arc wax and wane on its own
        // slow, deliberately-desynchronised cycle: at idle some sit near-dormant
        // for long stretches while one or two dominate; as load rises the
        // envelope floor lifts (lifetimeOverlap) so more arcs are active at once
        // -- dense, not fast-forwarded. Utilization also independently drives
        // evolution speed, thickness, reach, branching and reconnection.
        const int passOf[6] = {0, 2, 1, 2, 0, 1};
        // Load-driven evolution rate: gentle, non-linear (^0.7) so high load is
        // more active without feeling like fast-forward.
        const double evolRate =
            T.evolSpeedBase + T.evolSpeedLoad * loadCurve;
        // Deliberately incommensurate lifetime multipliers -> peaks rarely align.
        const double lifeMul[6] = {1.00, 1.37, 0.73, 1.61, 0.89, 1.19};
        for (int i = 0; i < T.primaryArcs; ++i) {
            const double fi = static_cast<double>(i);
            // Per-arc personality, deterministic from the seed.
            const double pSpeed =
                1.0 + T.speedVariance * wob(fi, 1.0, 0.0);
            const double pCurve =
                1.0 + T.curvatureVariance * wob(fi, 2.0, 0.0);
            const double pThick =
                1.0 + T.thicknessVariance * wob(fi, 3.0, 0.0);
            const double pSeat = wob(fi, 4.0, 0.0);  // phase-seat offset [-1,1]
            // Lifetime envelope: continuous cosine on this arc's own slow period.
            // Floor lifts with load so arcs overlap (dense) at high CPU.
            const double lifePeriod =
                T.lifetimeBase
                * (1.0 + T.lifetimeVariance * (lifeMul[i % 6] - 1.0));
            const double envRaw =
                0.5 * (std::sin(flowPhase * lifePeriod + pSeat * 6.2831) + 1.0);
            const double floor = T.lifetimeOverlap * loadCurve;
            const double envelope = std::clamp(floor + (1.0 - floor) * envRaw,
                                               0.0, 1.0);
            // Angular seat: individual, drifting; less radial symmetry.
            const double ang0 = (kTwoPi * i) / T.primaryArcs
                                + 0.6 * pSeat
                                + 0.5 * std::sin(flowPhase * 0.2 * pSpeed
                                                 + fi * 1.3);
            // Activation = load gating * this arc's lifetime envelope.
            const double gate = std::clamp(fi / T.primaryArcs, 0.0, 1.0);
            const double loadAct =
                std::clamp((activation - gate * 0.5) / 0.5, 0.0, 1.0)
                * (0.5 + 0.5 * activation);
            const double act = loadAct * envelope;
            drawArc(i, ang0, passOf[i], act, false, evolRate, pSpeed, pCurve,
                    pThick);
            // Secondary branches: more common under load, gated by the parent's
            // envelope so they belong to the same living structure.
            for (int sB = 0; sB < T.secondaryMax; ++sB) {
                const double sAct =
                    std::clamp((activation - 0.45 - 0.15 * sB) / 0.4, 0.0, 1.0);
                // Reconnection frequency rises with load.
                const double reconThresh =
                    T.reconnectChance * (0.5 + 0.9 * loadCurve);
                const double recon =
                    0.5 * (std::sin(flowPhase * 0.4 * pSpeed + fi * 2.0
                                    + sB * 3.1)
                           + 1.0);
                const bool reconnect = recon < reconThresh;
                const double sAng = ang0 + (sB == 0 ? 0.6 : -0.7)
                                    + (reconnect ? M_PI : 0.0)
                                    + 0.3 * std::sin(flowPhase * 0.3 * pSpeed
                                                     + fi + sB);
                drawArc(100 + i * 3 + sB, sAng, 2, sAct * act, true, evolRate,
                        pSpeed, pCurve * 1.2, pThick * 0.6);
            }
        }

        // NUCLEUS: tiny, intensely bright blue-white, dead-centre, where the
        // branches originate/reconnect. A small living PULSE energises it from
        // the centre: slow faint heartbeat at idle, stronger/faster (never
        // flashing, never large) under load. Capped under the text-clearance
        // guard so the percentage stays readable. A dense seed -- never an orb.
        {
            const double pulseAmp =
                T.nucleusPulseAmpIdle
                + (T.nucleusPulseAmpLoad - T.nucleusPulseAmpIdle) * loadCurve;
            const double pulseSpeed =
                T.nucleusPulseSpeedIdle
                + (T.nucleusPulseSpeedLoad - T.nucleusPulseSpeedIdle) * loadCurve;
            const double beat = std::sin(flowPhase * pulseSpeed);
            const double nucR = std::min(coreClear * 0.9,
                                         cavityR * T.nucleusRadius)
                                * (1.0 + pulseAmp * beat);
            painter.setCompositionMode(QPainter::CompositionMode_Plus);
            QRadialGradient ng(c, std::max(2.0, nucR));
            QColor nWhite(232, 245, 255);
            nWhite.setAlphaF(static_cast<float>(
                std::clamp(0.8 + 0.18 * activation + 0.1 * pulseAmp * beat, 0.0,
                           0.98)));
            QColor nTint = coreLight.lighter(150);
            nTint.setAlphaF(static_cast<float>(0.5 + 0.3 * activation));
            QColor nEdge = coreLight;
            nEdge.setAlphaF(0.0f);
            ng.setColorAt(0.0, nWhite);
            ng.setColorAt(0.45, nWhite);
            ng.setColorAt(0.8, nTint);
            ng.setColorAt(1.0, nEdge);
            painter.setPen(Qt::NoPen);
            painter.setBrush(ng);
            painter.drawEllipse(c, std::max(2.0, nucR), std::max(2.0, nucR));
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }
    painter.restore();

    // Armored-glass inner bevel + ONE specular catch (over the glass): the star
    // catching the curved glass upper-left. Thickness cue.
    {
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QColor bevel = litBy(0.55, 0.5);
        bevel.setAlphaF(0.5f);
        QPen bp(bevel);
        bp.setWidthF(std::max(1.4, R * 0.012));
        painter.setPen(bp);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(c, R * T.glassInner, R * T.glassInner);
        QColor spec(226, 240, 255);
        spec.setAlphaF(static_cast<float>(std::clamp(0.12 + 0.08 * intensity,
                                                     0.0, 0.22)));
        QPen sp(spec);
        sp.setWidthF(std::max(1.2, R * 0.02));
        sp.setCapStyle(Qt::RoundCap);
        painter.setPen(sp);
        const double gr = R * T.glassInner * 0.9;
        painter.drawArc(QRectF(c.x() - gr, c.y() - gr, 2 * gr, 2 * gr),
                        deg2(112), deg2(46));
    }

    // No giant radial bloom. Glow is per-arc only (the faint halo pass inside
    // the electrical core), so the cavity stays dark and the electricity reads
    // crisp. Removing the halo pass leaves the core still excellent.

    painter.setCompositionMode(prevMode);
}

void renderEnergyLayer(QPainter& painter, const CpuInstrumentLayout& layout,
                       const InstrumentRenderModel& model) {
    // ENERGY is the primary personality surface. Personality reaches the
    // renderer ONLY as neutral scalars on the model; there is no subsystem
    // logic here. At neutral params (ambient 0, pulse 0, warmth 0) the effective
    // glow is exactly model.glowStrength and the accents are unchanged, so the
    // output is byte-identical to the pre-personality renderer.
    // Personality energy gives the conduits a RESTRAINED brightness lift --
    // brightness supports the motion (reactor pulse, flow, breathing) rather
    // than carrying the personality by itself. At neutral (energy 0),
    // effectiveGlow == glowStrength, so neutral instruments are byte-identical.
    const double energy = std::clamp(
        model.personality.ambientIntensity + model.personality.pulse, 0.0, 1.0);
    const double effectiveGlow = model.glowStrength * (1.0 + 0.6 * energy);
    const QColor utilAccent =
        applyWarmth(model.accents.utilization, model.personality.warmth);
    const QColor tempAccent =
        applyWarmth(model.accents.temperature, model.personality.warmth);

    // Reactor core pulse first (behind the conduits): the lead motion.
    paintReactorCore(painter, layout, model.personality.pulse,
                     model.personality.ambientIntensity,
                     model.personality.warmth, model.personality.flowPhase);

    const double flowStrength = std::clamp(model.personality.ambientIntensity,
                                          0.0, 1.0);
    paintSegmentedRing(painter, layout, layout.utilizationRing, utilAccent,
                       effectiveGlow, model.personality.flowPhase, flowStrength);
    // The inner (secondary) conduit is drawn only when the instrument declares
    // it has one. A single-ring instrument (e.g. Cooling V1) sets
    // hasSecondaryRing false and no inner ring is painted. The renderer acts on
    // the flag, never on subsystem identity.
    if (model.hasSecondaryRing) {
        paintSegmentedRing(painter, layout, layout.temperatureRing, tempAccent,
                           effectiveGlow, model.personality.flowPhase,
                           flowStrength);
    }
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
        model.mode, side, model.primary.progress, model.secondary.progress);

    renderStructureLayer(painter, layout, model);    // Layer 1
    renderEnergyLayer(painter, layout, model);       // Layer 2
    renderInformationLayer(painter, layout, model);  // Layer 3

    painter.restore();
}

}  // namespace InstrumentRenderer

}  // namespace darkspark::deck::instruments
