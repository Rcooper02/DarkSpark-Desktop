// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/GpuInstrument.hpp"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QTimer>

#include "deck/instruments/InstrumentRenderModel.hpp"
#include "deck/instruments/InstrumentRenderer.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::instruments {

using themes::LegacyTheme;

namespace {

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
    : QWidget(parent), mode_(mode), transitionTimer_(new QTimer(this)) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    applySizePolicyForMode();
    transitionTimer_->setInterval(16);
    connect(transitionTimer_, &QTimer::timeout, this,
            &GpuInstrument::advanceInterpolation);
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

    if (interpolationSettled()) {
        displayed_.utilizationPercent = target_.utilizationPercent;
        displayed_.temperatureCelsius = target_.temperatureCelsius;
        update();
    } else if (!transitionTimer_->isActive()) {
        transitionTimer_->start();
    }
}

bool GpuInstrument::interpolationSettled() const {
    const double du =
        std::fabs(displayed_.utilizationPercent - target_.utilizationPercent);
    const double dt =
        std::fabs(displayed_.temperatureCelsius - target_.temperatureCelsius);
    return du < 0.1 && dt < 0.1;
}

void GpuInstrument::advanceInterpolation() {
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
    QPainter painter(this);

    // GPU keeps its own model and interpolation; the DarkSpark visual language
    // lives in the shared InstrumentRenderer. Build the neutral render model
    // from the current (interpolated) state. GPU's milestone-1 identity is just
    // its title and a very subtle accent variation.
    InstrumentRenderModel rm;
    rm.utilizationPercent = displayed_.utilizationPercent;
    rm.utilizationAvailability = displayed_.utilizationAvailability;
    rm.temperatureCelsius = displayed_.temperatureCelsius;
    rm.temperatureAvailability = displayed_.temperatureAvailability;
    rm.title = QStringLiteral("GPU");
    rm.awaitingTelemetry = false;
    rm.mode = mode_;
    rm.accents = InstrumentAccents{gpuUtilizationAccent(),
                                   LegacyTheme::accentPurple()};
    rm.glowStrength = 1.0;

    InstrumentRenderer::paint(painter, rect(), rm);
}

}  // namespace darkspark::deck::instruments
