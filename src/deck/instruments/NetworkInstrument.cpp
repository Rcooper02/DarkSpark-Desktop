// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/NetworkInstrument.hpp"

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
constexpr double kKiB = 1024.0;
constexpr double kMiB = 1024.0 * 1024.0;
constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;

// A presentation-only reference ceiling for the ring fill: throughput has no
// fixed maximum, so the ring is a coarse activity indication anchored at a
// nominal 1 GiB/s. This is visualization state, never a capacity claim; the true
// rate is always the number shown.
constexpr double kRingReferenceBytesPerSec = kGiB;
}  // namespace

QString NetworkInstrument::formatRate(double bytesPerSec, QString& suffixOut) {
    const double v = std::max(0.0, bytesPerSec);
    if (v >= kGiB) {
        suffixOut = QStringLiteral(" GB/s");
        return QString::number(v / kGiB, 'f', 1);
    }
    if (v >= kMiB) {
        suffixOut = QStringLiteral(" MB/s");
        return QString::number(v / kMiB, 'f', 1);
    }
    if (v >= kKiB) {
        suffixOut = QStringLiteral(" KB/s");
        return QString::number(v / kKiB, 'f', 1);
    }
    suffixOut = QStringLiteral(" B/s");
    return QString::number(v, 'f', 0);
}

NetworkInstrument::NetworkInstrument(InstrumentSizeMode mode, QWidget* parent)
    : QWidget(parent), mode_(mode), transitionTimer_(new QTimer(this)) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    applySizePolicyForMode();
    transitionTimer_->setInterval(16);
    connect(transitionTimer_, &QTimer::timeout, this,
            &NetworkInstrument::advanceInterpolation);
}

NetworkInstrument::~NetworkInstrument() = default;

void NetworkInstrument::applySizePolicyForMode() {
    if (mode_ == InstrumentSizeMode::Small) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setMaximumSize(sizeHint());
    } else {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }
}

void NetworkInstrument::setModel(const NetworkInstrumentModel& model) {
    target_ = model;
    // Prime the two displayed rates on first availability so they do not ease up
    // from a fabricated 0.
    if (displayed_.receiveAvailability == ValueAvailability::Absent
        && target_.receiveAvailability != ValueAvailability::Absent) {
        displayed_.receiveBytesPerSec = target_.receiveBytesPerSec;
    }
    if (displayed_.transmitAvailability == ValueAvailability::Absent
        && target_.transmitAvailability != ValueAvailability::Absent) {
        displayed_.transmitBytesPerSec = target_.transmitBytesPerSec;
    }
    // Availability and the retained (non-face) values copy straight through; the
    // two displayed rates interpolate.
    displayed_.receiveAvailability = target_.receiveAvailability;
    displayed_.transmitAvailability = target_.transmitAvailability;
    displayed_.receivedBytes = target_.receivedBytes;
    displayed_.receivedAvailability = target_.receivedAvailability;
    displayed_.transmittedBytes = target_.transmittedBytes;
    displayed_.transmittedAvailability = target_.transmittedAvailability;
    displayed_.linkState = target_.linkState;
    displayed_.linkAvailability = target_.linkAvailability;
    displayed_.interfaceName = target_.interfaceName;
    displayed_.isDefaultRoute = target_.isDefaultRoute;
    displayed_.isPhysical = target_.isPhysical;
    displayed_.isLoopback = target_.isLoopback;

    if (interpolationSettled()) {
        displayed_.receiveBytesPerSec = target_.receiveBytesPerSec;
        displayed_.transmitBytesPerSec = target_.transmitBytesPerSec;
        update();
    } else if (!transitionTimer_->isActive()) {
        transitionTimer_->start();
    }
}

bool NetworkInstrument::interpolationSettled() const {
    constexpr double kRateEpsilon = 1024.0;  // 1 KB/s
    const double dRx =
        std::fabs(displayed_.receiveBytesPerSec - target_.receiveBytesPerSec);
    const double dTx =
        std::fabs(displayed_.transmitBytesPerSec - target_.transmitBytesPerSec);
    return dRx < kRateEpsilon && dTx < kRateEpsilon;
}

void NetworkInstrument::advanceInterpolation() {
    constexpr double kEase = 0.22;
    // Display easing only, following the established CPU/GPU/Storage pattern. The
    // raw telemetry (target_) is untouched and truthful; no averaging/filtering/
    // history.
    displayed_.receiveBytesPerSec +=
        (target_.receiveBytesPerSec - displayed_.receiveBytesPerSec) * kEase;
    displayed_.transmitBytesPerSec +=
        (target_.transmitBytesPerSec - displayed_.transmitBytesPerSec) * kEase;
    if (interpolationSettled()) {
        displayed_.receiveBytesPerSec = target_.receiveBytesPerSec;
        displayed_.transmitBytesPerSec = target_.transmitBytesPerSec;
        transitionTimer_->stop();
    }
    update();
}

void NetworkInstrument::setSizeMode(InstrumentSizeMode mode) {
    mode_ = mode;
    applySizePolicyForMode();
    updateGeometry();
    update();
}

QSize NetworkInstrument::sizeHint() const {
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

QSize NetworkInstrument::minimumSizeHint() const { return QSize(160, 160); }

void NetworkInstrument::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);

    InstrumentRenderModel rm;
    // Primary: download throughput, adaptive unit formatting; outer ring is a
    // presentation-only activity fill against a nominal reference (never a
    // capacity claim). The true rate is always the number shown.
    QString rxSuffix;
    rm.primary.text = formatRate(displayed_.receiveBytesPerSec, rxSuffix);
    rm.primary.suffix = rxSuffix;
    rm.primary.availability = displayed_.receiveAvailability;
    rm.primary.progress = positionalFraction(displayed_.receiveBytesPerSec, 0.0,
                                             kRingReferenceBytesPerSec);

    // Secondary: upload throughput, same adaptive formatting.
    QString txSuffix;
    rm.secondary.text = formatRate(displayed_.transmitBytesPerSec, txSuffix);
    rm.secondary.suffix = txSuffix;
    rm.secondary.availability = displayed_.transmitAvailability;
    rm.secondary.progress = 0.0;
    rm.hasSecondaryRing = false;  // single-ring instrument this milestone

    rm.title = QStringLiteral("Network");
    rm.awaitingTelemetry = false;
    rm.mode = mode_;
    rm.accents = InstrumentAccents{LegacyTheme::accentCyan(),
                                   LegacyTheme::accentPurple()};
    rm.glowStrength = 1.0;

    InstrumentRenderer::paint(painter, rect(), rm);
}

}  // namespace darkspark::deck::instruments
