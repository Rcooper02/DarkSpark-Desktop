// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/GpuInstrument.hpp"

#include <algorithm>
#include <cmath>

#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QShowEvent>
#include <QHideEvent>
#include <QLoggingCategory>

#include "deck/instruments/InstrumentRenderModel.hpp"
#include "deck/instruments/CpuInstrumentLayout.hpp"
#include "deck/instruments/InstrumentRenderer.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::instruments {

using themes::LegacyTheme;

namespace {

// Diagnostic-only category for the GPU instrument. OFF by default; enable with
// QT_LOGGING_RULES="darkspark.instrument.gpu=true". Latched to fire ONCE for the
// first model receipt and ONCE for the first paint -- never per frame.
Q_LOGGING_CATEGORY(lcGpuInstrument, "darkspark.instrument.gpu")

/// GPU's milestone-1 accent: intentionally almost identical to CPU's cyan, with
/// only a very subtle warm shift so the two instruments read as the same family
/// while being distinguishable on close inspection. Full Forge identity is
/// deferred to the dedicated visual milestone; this is the single line that
/// will grow into it, and it lives here (not the renderer) so each subsystem
/// owns its own accent.
[[nodiscard]] QColor gpuUtilizationAccent() {
    QColor cyan = LegacyTheme::accentCyan();
    // Nudge very slightly toward warm/white: a barely-perceptible identity cue.
    return cyan.lighter(108);
}

}  // namespace

GpuInstrument::GpuInstrument(InstrumentSizeMode mode, QWidget* parent)
    : QWidget(parent), mode_(mode) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    applySizePolicyForMode();
}

GpuInstrument::~GpuInstrument() = default;

void GpuInstrument::applySizePolicyForMode() {
    // Mirror the validated CPU sizing: Small declares an intended maximum
    // footprint (cap at its hint) without a fixed lock; larger modes keep the
    // expanding default.
    if (mode_ == InstrumentSizeMode::Small) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setMaximumSize(sizeHint());
    } else {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }
}

void GpuInstrument::setModel(const GpuInstrumentModel& model) {
    if (!loggedFirstModel_) {
        loggedFirstModel_ = true;
        qCInfo(lcGpuInstrument)
            << "GpuInstrument first model: visible=" << isVisible()
            << "size=" << size()
            << "utilAvailable="
            << (model.utilizationAvailability != ValueAvailability::Absent)
            << "util=" << model.utilizationPercent
            << "tempAvailable="
            << (model.temperatureAvailability != ValueAvailability::Absent)
            << "temp=" << model.temperatureCelsius;
    }
    target_ = model;
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

    // VRAM is a slow-moving text caption, not an animated gauge: pass it
    // straight through (no interpolation) so the third line reflects the latest
    // reading immediately.
    displayed_.vramUsedBytes = target_.vramUsedBytes;
    displayed_.vramTotalBytes = target_.vramTotalBytes;
    displayed_.vramAvailability = target_.vramAvailability;

    if (interpolationSettled()) {
        displayed_.utilizationPercent = target_.utilizationPercent;
        displayed_.temperatureCelsius = target_.temperatureCelsius;
        update();
    } else if (clock_ != nullptr) {
        clock_->requestAnimation();
    }
}

bool GpuInstrument::interpolationSettled() const {
    const double du =
        std::fabs(displayed_.utilizationPercent - target_.utilizationPercent);
    const double dt =
        std::fabs(displayed_.temperatureCelsius - target_.temperatureCelsius);
    return du < 0.1 && dt < 0.1;
}

void GpuInstrument::advance(double /*deltaSeconds*/,
                              double /*clockSeconds*/) {
    constexpr double kEase = 0.22;
    displayed_.utilizationPercent +=
        (target_.utilizationPercent - displayed_.utilizationPercent) * kEase;
    displayed_.temperatureCelsius +=
        (target_.temperatureCelsius - displayed_.temperatureCelsius) * kEase;
    if (interpolationSettled()) {
        displayed_.utilizationPercent = target_.utilizationPercent;
        displayed_.temperatureCelsius = target_.temperatureCelsius;
    }
    update();
}

void GpuInstrument::setSizeMode(InstrumentSizeMode mode) {
    mode_ = mode;
    applySizePolicyForMode();
    updateGeometry();
    update();
}

QSize GpuInstrument::sizeHint() const {
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

QSize GpuInstrument::minimumSizeHint() const { return QSize(160, 160); }

void GpuInstrument::paintEvent(QPaintEvent* /*event*/) {
    if (!loggedFirstPaint_) {
        loggedFirstPaint_ = true;
        qCInfo(lcGpuInstrument)
            << "GpuInstrument first paint: visible=" << isVisible()
            << "size=" << size()
            << "utilAvailable="
            << (displayed_.utilizationAvailability != ValueAvailability::Absent)
            << "tempAvailable="
            << (displayed_.temperatureAvailability != ValueAvailability::Absent);
    }
    QPainter painter(this);

    // GPU keeps its own model and interpolation; the DarkSpark visual language
    // lives in the shared InstrumentRenderer. Build the neutral render model
    // from the current (interpolated) state. GPU's milestone-1 identity is just
    // its title and a very subtle accent variation.
    InstrumentRenderModel rm;
    // GPU formats its own primary identically to CPU: integer percent, "%"
    // suffix, outer ring from the 0-100 range.
    rm.primary.availability = displayed_.utilizationAvailability;
    rm.primary.text = QString::number(displayed_.utilizationPercent, 'f', 0);
    rm.primary.suffix = QStringLiteral("%");
    rm.primary.progress =
        positionalFraction(displayed_.utilizationPercent, 0.0, 100.0);

    // GPU owns its secondary presentation, identically to CPU: temperature value
    // + "\u00B0C" suffix; "--\u00B0C" (renderable) when unavailable.
    if (displayed_.temperatureAvailability != ValueAvailability::Absent) {
        rm.secondary.availability = displayed_.temperatureAvailability;
        rm.secondary.text =
            QString::number(displayed_.temperatureCelsius, 'f', 0);
    } else {
        rm.secondary.availability = ValueAvailability::Live;
        rm.secondary.text = QStringLiteral("--");
    }
    rm.secondary.suffix = QStringLiteral("\u00B0C");
    rm.secondary.progress = positionalFraction(displayed_.temperatureCelsius,
                                               layout_constants::kTempSpanLowC,
                                               layout_constants::kTempSpanHighC);
    rm.hasSecondaryRing = true;
    rm.title = QStringLiteral("GPU");
    rm.awaitingTelemetry = false;
    rm.mode = mode_;
    rm.accents = InstrumentAccents{gpuUtilizationAccent(),
                                   LegacyTheme::accentPurple()};
    rm.glowStrength = 1.0;

    InstrumentRenderer::paint(painter, rect(), rm);

    // Third caption line: "VRAM x.x / y.y GB", drawn directly here (NOT via the
    // shared InstrumentRenderer, which is frozen and has only primary/secondary
    // slots). Temperature stays the secondary line; this sits below it. When the
    // reading is unavailable it degrades to "VRAM -- / -- GB" without touching
    // utilization or temperature.
    {
        const double side =
            std::min(rect().width(), rect().height());
        constexpr double kBytesPerGiB = 1024.0 * 1024.0 * 1024.0;
        QString vramText;
        if (displayed_.vramAvailability != ValueAvailability::Absent
            && displayed_.vramTotalBytes > 0.0) {
            const double usedGb = displayed_.vramUsedBytes / kBytesPerGiB;
            const double totalGb = displayed_.vramTotalBytes / kBytesPerGiB;
            vramText = QStringLiteral("VRAM %1 / %2 GB")
                           .arg(usedGb, 0, 'f', 1)
                           .arg(totalGb, 0, 'f', 1);
        } else {
            vramText = QStringLiteral("VRAM -- / -- GB");
        }

        QFont f(font());
        const int vramPx = (mode_ == InstrumentSizeMode::Small) ? 11 : 13;
        f.setPixelSize(vramPx);
        f.setWeight(QFont::Medium);
        painter.setFont(f);
        painter.setPen(LegacyTheme::textSecondary());
        // Sit just below the secondary (temperature) line. secondaryY is at
        // centerY + side*0.16 (Small) / *0.135 (Large); place this a line under.
        const double centerY = rect().y() + rect().height() / 2.0;
        const double thirdY =
            centerY
            + side * ((mode_ == InstrumentSizeMode::Small) ? 0.245 : 0.205);
        const QRectF box(rect().x(), thirdY - vramPx, rect().width(),
                         vramPx * 1.8);
        painter.drawText(box, Qt::AlignHCenter | Qt::AlignVCenter, vramText);
    }
}


void GpuInstrument::setAnimationClock(AnimationClock* clock) {
    clock_ = clock;
    // If already visible when wired, subscribe now.
    if (clock_ != nullptr && isVisible() && !subscribed_) {
        clock_->subscribe(this);
        subscribed_ = true;
    }
}

bool GpuInstrument::wantsContinuousAnimation() const {
    // This instrument animates only to complete an in-flight interpolation; once
    // settled it needs no ticks (no idle personality motion in V1).
    return !interpolationSettled();
}

void GpuInstrument::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (clock_ != nullptr && !subscribed_) {
        clock_->subscribe(this);
        subscribed_ = true;
        if (!interpolationSettled()) {
            clock_->requestAnimation();
        }
    }
}

void GpuInstrument::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (clock_ != nullptr && subscribed_) {
        clock_->unsubscribe(this);
        subscribed_ = false;
    }
}

}  // namespace darkspark::deck::instruments
