// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/CpuInstrument.hpp"

#include <algorithm>
#include <cmath>

#include <QPaintEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QTimer>
#include <QString>

#include "deck/instruments/InstrumentRenderer.hpp"
#include "deck/instruments/InstrumentRenderModel.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::instruments {

using themes::LegacyTheme;



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

void CpuInstrument::setTitle(const QString& title) {
    if (title_ == title) {
        return;
    }
    title_ = title;
    update();
}

void CpuInstrument::setAwaitingTelemetry(bool awaiting) {
    if (awaitingTelemetry_ == awaiting) {
        return;
    }
    awaitingTelemetry_ = awaiting;
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

    // CPU keeps its own model, interpolation, and accents; the DarkSpark visual
    // language itself lives in the shared InstrumentRenderer. Build the neutral
    // render description from the current (interpolated) presentation state and
    // hand it to the renderer. CPU's identity here is just its title and cyan
    // accent; everything about HOW an instrument looks is shared code.
    InstrumentRenderModel rm;
    rm.utilizationPercent = displayed_.utilizationPercent;
    rm.utilizationAvailability = displayed_.utilizationAvailability;
    rm.temperatureCelsius = displayed_.temperatureCelsius;
    rm.temperatureAvailability = displayed_.temperatureAvailability;
    rm.title = title_;
    rm.awaitingTelemetry = awaitingTelemetry_;
    rm.mode = mode_;
    rm.accents = InstrumentAccents{LegacyTheme::accentCyan(),
                                   LegacyTheme::accentPurple()};
    rm.glowStrength = 1.0;  // Idle; interaction growth is a future data change.

    InstrumentRenderer::paint(painter, rect(), rm);
}

}  // namespace darkspark::deck::instruments
