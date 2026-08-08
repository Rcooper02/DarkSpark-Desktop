// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/CoolingInstrument.hpp"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QString>
#include <QShowEvent>
#include <QHideEvent>

#include "deck/instruments/InstrumentRenderModel.hpp"
#include "deck/instruments/InstrumentRenderer.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::instruments {

using themes::LegacyTheme;

QString CoolingInstrument::formatSecondaryText(const CoolingInstrumentModel& m) {
    if (m.secondaryTempAvailability != ValueAvailability::Absent) {
        return QString::number(m.secondaryTempCelsius, 'f', 0);
    }
    if (m.secondaryRpmAvailability != ValueAvailability::Absent) {
        return QString::number(m.secondaryRpm, 'f', 0);
    }
    return QString();
}

QString CoolingInstrument::secondarySuffix(const CoolingInstrumentModel& m) {
    if (m.secondaryTempAvailability != ValueAvailability::Absent) {
        return QStringLiteral("\u00B0C");
    }
    if (m.secondaryRpmAvailability != ValueAvailability::Absent) {
        return QStringLiteral(" RPM");
    }
    return QString();
}

CoolingInstrument::CoolingInstrument(InstrumentSizeMode mode, QWidget* parent)
    : QWidget(parent),
      mode_(mode),
      ringPolicy_(std::make_unique<AdaptiveObservedMaxPolicy>()) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    applySizePolicyForMode();
}

CoolingInstrument::~CoolingInstrument() = default;

void CoolingInstrument::applySizePolicyForMode() {
    if (mode_ == InstrumentSizeMode::Small) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setMaximumSize(sizeHint());
    } else {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }
}

void CoolingInstrument::setModel(const CoolingInstrumentModel& model) {
    target_ = model;
    if (displayed_.primaryAvailability == ValueAvailability::Absent
        && target_.primaryAvailability != ValueAvailability::Absent) {
        displayed_.primaryRpm = target_.primaryRpm;
    }
    displayed_.primaryAvailability = target_.primaryAvailability;
    displayed_.secondaryTempAvailability = target_.secondaryTempAvailability;
    displayed_.secondaryRpmAvailability = target_.secondaryRpmAvailability;
    if (interpolationSettled()) {
        displayed_ = target_;
        update();
    } else if (clock_ != nullptr) {
        clock_->requestAnimation();
    }
}

bool CoolingInstrument::interpolationSettled() const {
    const double dP = std::fabs(displayed_.primaryRpm - target_.primaryRpm);
    const double dT =
        std::fabs(displayed_.secondaryTempCelsius - target_.secondaryTempCelsius);
    const double dS = std::fabs(displayed_.secondaryRpm - target_.secondaryRpm);
    return dP < 1.0 && dT < 0.1 && dS < 1.0;
}

void CoolingInstrument::advance(double /*deltaSeconds*/,
                              double /*clockSeconds*/) {
    constexpr double kEase = 0.22;
    displayed_.primaryRpm +=
        (target_.primaryRpm - displayed_.primaryRpm) * kEase;
    displayed_.secondaryTempCelsius +=
        (target_.secondaryTempCelsius - displayed_.secondaryTempCelsius) * kEase;
    displayed_.secondaryRpm +=
        (target_.secondaryRpm - displayed_.secondaryRpm) * kEase;
    if (interpolationSettled()) {
        displayed_.primaryRpm = target_.primaryRpm;
        displayed_.secondaryTempCelsius = target_.secondaryTempCelsius;
        displayed_.secondaryRpm = target_.secondaryRpm;
    }
    update();
}

void CoolingInstrument::setSizeMode(InstrumentSizeMode mode) {
    mode_ = mode;
    applySizePolicyForMode();
    updateGeometry();
    update();
}

QSize CoolingInstrument::sizeHint() const {
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

QSize CoolingInstrument::minimumSizeHint() const { return QSize(160, 160); }

void CoolingInstrument::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);

    InstrumentRenderModel rm;
    // Primary: raw RPM with a " RPM" suffix -- never a fabricated percentage.
    // The outer ring fill comes from the interchangeable normalization policy;
    // it is visualization state, not a capacity claim. The true RPM is always
    // the number shown.
    rm.primary.availability = displayed_.primaryAvailability;
    rm.primary.text = QString::number(displayed_.primaryRpm, 'f', 0);
    rm.primary.suffix = QStringLiteral(" RPM");
    rm.primary.progress = ringPolicy_->normalize(displayed_.primaryRpm);

    // Secondary: coolant temp or second fan RPM; the renderer composes
    // text + suffix and shows "--" when Absent.
    rm.secondary.text = formatSecondaryText(displayed_);
    rm.secondary.suffix = secondarySuffix(displayed_);
    const bool hasSecondary =
        displayed_.secondaryTempAvailability != ValueAvailability::Absent
        || displayed_.secondaryRpmAvailability != ValueAvailability::Absent;
    rm.secondary.availability =
        hasSecondary ? ValueAvailability::Live : ValueAvailability::Absent;
    rm.secondary.progress = 0.0;
    rm.hasSecondaryRing = false;  // single-ring instrument this milestone

    rm.title = QStringLiteral("Cooling");
    rm.awaitingTelemetry = false;
    rm.mode = mode_;
    rm.accents = InstrumentAccents{LegacyTheme::accentCyan(),
                                   LegacyTheme::accentPurple()};
    rm.glowStrength = 1.0;

    InstrumentRenderer::paint(painter, rect(), rm);
}


void CoolingInstrument::setAnimationClock(AnimationClock* clock) {
    clock_ = clock;
    if (clock_ != nullptr && isVisible() && !subscribed_) {
        clock_->subscribe(this);
        subscribed_ = true;
    }
}

bool CoolingInstrument::wantsContinuousAnimation() const {
    return !interpolationSettled();
}

void CoolingInstrument::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (clock_ != nullptr && !subscribed_) {
        clock_->subscribe(this);
        subscribed_ = true;
        if (!interpolationSettled()) {
            clock_->requestAnimation();
        }
    }
}

void CoolingInstrument::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (clock_ != nullptr && subscribed_) {
        clock_->unsubscribe(this);
        subscribed_ = false;
    }
}

}  // namespace darkspark::deck::instruments
