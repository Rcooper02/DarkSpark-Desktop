// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/StorageInstrument.hpp"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QString>
#include <QTimer>

#include "deck/instruments/CpuInstrumentLayout.hpp"  // positionalFraction
#include "deck/instruments/InstrumentRenderModel.hpp"
#include "deck/instruments/InstrumentRenderer.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::instruments {

using themes::LegacyTheme;

namespace {
constexpr double kBytesPerGiB = 1024.0 * 1024.0 * 1024.0;
}  // namespace

QString StorageInstrument::formatGigabytes(double bytes) {
    return QString::number(bytes / kBytesPerGiB, 'f', 0);
}

QString StorageInstrument::formatSecondaryLine(const StorageInstrumentModel& m) {
    if (m.usedAvailability == ValueAvailability::Absent
        || m.totalAvailability == ValueAvailability::Absent) {
        return QString();
    }
    return formatGigabytes(m.usedBytes) + QStringLiteral(" / ")
           + formatGigabytes(m.totalBytes);
}

StorageInstrument::StorageInstrument(InstrumentSizeMode mode, QWidget* parent)
    : QWidget(parent), mode_(mode), transitionTimer_(new QTimer(this)) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    applySizePolicyForMode();
    transitionTimer_->setInterval(16);
    connect(transitionTimer_, &QTimer::timeout, this,
            &StorageInstrument::advanceInterpolation);
}

StorageInstrument::~StorageInstrument() = default;

void StorageInstrument::applySizePolicyForMode() {
    if (mode_ == InstrumentSizeMode::Small) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setMaximumSize(sizeHint());
    } else {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }
}

void StorageInstrument::setModel(const StorageInstrumentModel& model) {
    target_ = model;
    if (displayed_.utilizationAvailability == ValueAvailability::Absent
        && target_.utilizationAvailability != ValueAvailability::Absent) {
        displayed_.utilizationPercent = target_.utilizationPercent;
    }
    if (displayed_.usedAvailability == ValueAvailability::Absent
        && target_.usedAvailability != ValueAvailability::Absent) {
        displayed_.usedBytes = target_.usedBytes;
    }
    if (displayed_.totalAvailability == ValueAvailability::Absent
        && target_.totalAvailability != ValueAvailability::Absent) {
        displayed_.totalBytes = target_.totalBytes;
    }
    // Throughput is retained (not on the V1 face) but is interpolated too, so a
    // later detail view reads an already-smoothed displayed value. Prime on
    // first availability exactly like the face values, to avoid easing up from 0.
    if (displayed_.readAvailability == ValueAvailability::Absent
        && target_.readAvailability != ValueAvailability::Absent) {
        displayed_.readBytesPerSec = target_.readBytesPerSec;
    }
    if (displayed_.writeAvailability == ValueAvailability::Absent
        && target_.writeAvailability != ValueAvailability::Absent) {
        displayed_.writeBytesPerSec = target_.writeBytesPerSec;
    }
    // Availability and the retained (non-face) values copy straight through;
    // the displayed numerics interpolate.
    displayed_.utilizationAvailability = target_.utilizationAvailability;
    displayed_.usedAvailability = target_.usedAvailability;
    displayed_.totalAvailability = target_.totalAvailability;
    displayed_.temperatureCelsius = target_.temperatureCelsius;
    displayed_.temperatureAvailability = target_.temperatureAvailability;
    displayed_.readAvailability = target_.readAvailability;
    displayed_.writeAvailability = target_.writeAvailability;

    if (interpolationSettled()) {
        displayed_.utilizationPercent = target_.utilizationPercent;
        displayed_.usedBytes = target_.usedBytes;
        displayed_.totalBytes = target_.totalBytes;
        update();
    } else if (!transitionTimer_->isActive()) {
        transitionTimer_->start();
    }
}

bool StorageInstrument::interpolationSettled() const {
    const double du =
        std::fabs(displayed_.utilizationPercent - target_.utilizationPercent);
    constexpr double kByteEpsilon = 1024.0 * 1024.0;
    const double dUsed = std::fabs(displayed_.usedBytes - target_.usedBytes);
    const double dTotal = std::fabs(displayed_.totalBytes - target_.totalBytes);
    // Throughput eases like the face values (raw target retained in the model);
    // a 1 MB/s epsilon settles it without a visible jump.
    constexpr double kRateEpsilon = 1024.0 * 1024.0;
    const double dRead =
        std::fabs(displayed_.readBytesPerSec - target_.readBytesPerSec);
    const double dWrite =
        std::fabs(displayed_.writeBytesPerSec - target_.writeBytesPerSec);
    return du < 0.1 && dUsed < kByteEpsilon && dTotal < kByteEpsilon
           && dRead < kRateEpsilon && dWrite < kRateEpsilon;
}

void StorageInstrument::advanceInterpolation() {
    constexpr double kEase = 0.22;
    displayed_.utilizationPercent +=
        (target_.utilizationPercent - displayed_.utilizationPercent) * kEase;
    displayed_.usedBytes += (target_.usedBytes - displayed_.usedBytes) * kEase;
    displayed_.totalBytes += (target_.totalBytes - displayed_.totalBytes) * kEase;
    // Throughput eases toward the raw target with the SAME approach CPU/GPU use
    // for utilization: presentation-only smoothing, no averaging/filtering/
    // history. The stored telemetry (target_) is untouched and truthful.
    displayed_.readBytesPerSec +=
        (target_.readBytesPerSec - displayed_.readBytesPerSec) * kEase;
    displayed_.writeBytesPerSec +=
        (target_.writeBytesPerSec - displayed_.writeBytesPerSec) * kEase;
    if (interpolationSettled()) {
        displayed_.utilizationPercent = target_.utilizationPercent;
        displayed_.usedBytes = target_.usedBytes;
        displayed_.totalBytes = target_.totalBytes;
        displayed_.readBytesPerSec = target_.readBytesPerSec;
        displayed_.writeBytesPerSec = target_.writeBytesPerSec;
        transitionTimer_->stop();
    }
    update();
}

void StorageInstrument::setSizeMode(InstrumentSizeMode mode) {
    mode_ = mode;
    applySizePolicyForMode();
    updateGeometry();
    update();
}

QSize StorageInstrument::sizeHint() const {
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

QSize StorageInstrument::minimumSizeHint() const { return QSize(160, 160); }

void StorageInstrument::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);

    InstrumentRenderModel rm;
    // Primary: filesystem utilization, integer percent + "%", outer ring from
    // the 0-100 range -- exactly the CPU/Memory percentage presentation.
    rm.primary.availability = displayed_.utilizationAvailability;
    rm.primary.text = QString::number(displayed_.utilizationPercent, 'f', 0);
    rm.primary.suffix = QStringLiteral("%");
    rm.primary.progress =
        positionalFraction(displayed_.utilizationPercent, 0.0, 100.0);

    // Secondary: "used / total" value + " GB" suffix (the Memory pattern).
    const QString secText = formatSecondaryLine(displayed_);
    rm.secondary.text = secText;
    rm.secondary.suffix = secText.isEmpty() ? QString() : QStringLiteral(" GB");
    rm.secondary.availability = secText.isEmpty()
                                    ? ValueAvailability::Absent
                                    : displayed_.usedAvailability;
    double ratio = 0.0;
    if (displayed_.totalBytes > 0.0) {
        ratio = std::clamp(displayed_.usedBytes / displayed_.totalBytes, 0.0, 1.0);
    }
    rm.secondary.progress = ratio;
    rm.hasSecondaryRing = false;  // single-ring instrument this milestone

    rm.title = QStringLiteral("Storage");
    rm.awaitingTelemetry = false;
    rm.mode = mode_;
    rm.accents = InstrumentAccents{LegacyTheme::accentCyan(),
                                   LegacyTheme::accentPurple()};
    rm.glowStrength = 1.0;

    InstrumentRenderer::paint(painter, rect(), rm);
}

}  // namespace darkspark::deck::instruments
