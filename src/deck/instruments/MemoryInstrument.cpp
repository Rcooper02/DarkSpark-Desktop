// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/MemoryInstrument.hpp"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QString>
#include <QTimer>

#include "deck/instruments/CpuInstrumentLayout.hpp"
#include "deck/instruments/InstrumentRenderModel.hpp"
#include "deck/instruments/InstrumentRenderer.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::instruments {

using themes::LegacyTheme;

namespace {

// Binary gigabyte: Linux memory tooling reports GiB, so "32.0 GB" here matches
// what a user sees from free(1)/top rather than disk-style decimal GB.
constexpr double kBytesPerGiB = 1024.0 * 1024.0 * 1024.0;

}  // namespace

QString MemoryInstrument::formatGigabytes(double bytes) {
    const double gib = bytes / kBytesPerGiB;
    // One decimal place: enough precision to distinguish common sizes without a
    // noisy fractional tail.
    return QString::number(gib, 'f', 1);
}

QString MemoryInstrument::formatSecondaryLine(const MemoryInstrumentModel& m) {
    // Only present a "used / total GB" line when both figures are available.
    // Otherwise return empty so the shared renderer shows its neutral "--"
    // placeholder instead of a fabricated number.
    if (m.usedAvailability == ValueAvailability::Absent
        || m.totalAvailability == ValueAvailability::Absent) {
        return QString();
    }
    return formatGigabytes(m.usedBytes) + QStringLiteral(" / ")
           + formatGigabytes(m.totalBytes) + QStringLiteral(" GB");
}

MemoryInstrument::MemoryInstrument(InstrumentSizeMode mode, QWidget* parent)
    : QWidget(parent), mode_(mode), transitionTimer_(new QTimer(this)) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    applySizePolicyForMode();
    transitionTimer_->setInterval(16);
    connect(transitionTimer_, &QTimer::timeout, this,
            &MemoryInstrument::advanceInterpolation);
}

MemoryInstrument::~MemoryInstrument() = default;

void MemoryInstrument::applySizePolicyForMode() {
    // Mirror the validated CPU/GPU sizing: Small declares an intended maximum
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

void MemoryInstrument::setModel(const MemoryInstrumentModel& model) {
    target_ = model;
    // On a fresh transition from Absent, snap the displayed value so the first
    // reading does not sweep up from zero.
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
    displayed_.utilizationAvailability = target_.utilizationAvailability;
    displayed_.usedAvailability = target_.usedAvailability;
    displayed_.totalAvailability = target_.totalAvailability;
    displayed_.availableAvailability = target_.availableAvailability;

    if (interpolationSettled()) {
        displayed_.utilizationPercent = target_.utilizationPercent;
        displayed_.usedBytes = target_.usedBytes;
        displayed_.totalBytes = target_.totalBytes;
        displayed_.availableBytes = target_.availableBytes;
        update();
    } else if (!transitionTimer_->isActive()) {
        transitionTimer_->start();
    }
}

bool MemoryInstrument::interpolationSettled() const {
    const double du =
        std::fabs(displayed_.utilizationPercent - target_.utilizationPercent);
    // Byte figures are large; scale the settle threshold to ~1 MiB so tiny
    // deltas do not keep the timer running.
    constexpr double kByteEpsilon = 1024.0 * 1024.0;
    const double dUsed = std::fabs(displayed_.usedBytes - target_.usedBytes);
    const double dTotal = std::fabs(displayed_.totalBytes - target_.totalBytes);
    return du < 0.1 && dUsed < kByteEpsilon && dTotal < kByteEpsilon;
}

void MemoryInstrument::advanceInterpolation() {
    constexpr double kEase = 0.22;
    displayed_.utilizationPercent +=
        (target_.utilizationPercent - displayed_.utilizationPercent) * kEase;
    displayed_.usedBytes += (target_.usedBytes - displayed_.usedBytes) * kEase;
    displayed_.totalBytes += (target_.totalBytes - displayed_.totalBytes) * kEase;
    displayed_.availableBytes +=
        (target_.availableBytes - displayed_.availableBytes) * kEase;
    if (interpolationSettled()) {
        displayed_.utilizationPercent = target_.utilizationPercent;
        displayed_.usedBytes = target_.usedBytes;
        displayed_.totalBytes = target_.totalBytes;
        displayed_.availableBytes = target_.availableBytes;
        transitionTimer_->stop();
    }
    update();
}

void MemoryInstrument::setSizeMode(InstrumentSizeMode mode) {
    mode_ = mode;
    applySizePolicyForMode();
    updateGeometry();
    update();
}

QSize MemoryInstrument::sizeHint() const {
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

QSize MemoryInstrument::minimumSizeHint() const { return QSize(160, 160); }

void MemoryInstrument::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);

    // Memory keeps its own model and interpolation; the DarkSpark visual
    // language lives in the shared InstrumentRenderer. Build the neutral render
    // model from the current (interpolated) state. Memory's identity in this
    // functional milestone is just its title and its "used / total GB"
    // secondary line -- no Library personality yet.
    InstrumentRenderModel rm;
    rm.utilizationPercent = displayed_.utilizationPercent;
    rm.utilizationAvailability = displayed_.utilizationAvailability;

    // Memory owns its secondary presentation: "used / total GB", and the
    // inner-ring fraction as the used/total ratio.
    rm.secondaryText = formatSecondaryLine(displayed_);
    // The secondary reading is available when the byte line could be formed.
    rm.secondaryAvailability = rm.secondaryText.isEmpty()
                                   ? ValueAvailability::Absent
                                   : displayed_.usedAvailability;
    double ratio = 0.0;
    if (displayed_.totalBytes > 0.0) {
        ratio = std::clamp(displayed_.usedBytes / displayed_.totalBytes, 0.0, 1.0);
    }
    rm.secondaryValue = ratio;
    rm.hasSecondaryRing = true;

    rm.title = QStringLiteral("Memory");
    rm.awaitingTelemetry = false;
    rm.mode = mode_;
    // Reuse the shared accents: cyan primary conduit, purple secondary conduit,
    // exactly as CPU. No memory-specific palette in this functional milestone.
    rm.accents = InstrumentAccents{LegacyTheme::accentCyan(),
                                   LegacyTheme::accentPurple()};
    rm.glowStrength = 1.0;

    InstrumentRenderer::paint(painter, rect(), rm);
}

}  // namespace darkspark::deck::instruments
