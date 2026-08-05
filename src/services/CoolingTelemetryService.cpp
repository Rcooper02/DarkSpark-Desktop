// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CoolingTelemetryService.hpp"

#include <QDateTime>
#include <QLoggingCategory>

#include <algorithm>

#include "services/HwmonCoolingProvider.hpp"

namespace darkspark::services {

using models::MetricId;
using models::MetricSample;
using models::MetricUnit;
using models::MonotonicTimestamp;

namespace {
Q_LOGGING_CATEGORY(lcCooling, "darkspark.cooling")

constexpr int kPollIntervalMs = 1000;

int rpmPriority(CoolingSensorRole role) {
    switch (role) {
    case CoolingSensorRole::PumpRpm:
        return 0;
    case CoolingSensorRole::CpuFanRpm:
        return 1;
    case CoolingSensorRole::GpuFanRpm:
        return 2;
    case CoolingSensorRole::CaseFanRpm:
        return 3;
    case CoolingSensorRole::CoolantTemp:
        return 99;
    }
    return 99;
}

const char* roleName(CoolingSensorRole role) {
    switch (role) {
    case CoolingSensorRole::PumpRpm:
        return "Pump";
    case CoolingSensorRole::CpuFanRpm:
        return "CPU Fan";
    case CoolingSensorRole::GpuFanRpm:
        return "GPU Fan";
    case CoolingSensorRole::CaseFanRpm:
        return "Case Fan";
    case CoolingSensorRole::CoolantTemp:
        return "Coolant Temp";
    }
    return "Unknown";
}

MonotonicTimestamp nowProd() {
    return static_cast<MonotonicTimestamp>(QDateTime::currentMSecsSinceEpoch());
}

}  // namespace

namespace detail {

CoolingSelection selectFrom(const std::vector<AttributedSensor>& sensors) {
    CoolingSelection sel;

    const AttributedSensor* bestRpm = nullptr;
    for (const AttributedSensor& a : sensors) {
        if (a.sensor.role == CoolingSensorRole::CoolantTemp) {
            continue;
        }
        if (bestRpm == nullptr
            || rpmPriority(a.sensor.role) < rpmPriority(bestRpm->sensor.role)
            || (rpmPriority(a.sensor.role) == rpmPriority(bestRpm->sensor.role)
                && a.sensor.metadata.stableId
                       < bestRpm->sensor.metadata.stableId)) {
            bestRpm = &a;
        }
    }
    if (bestRpm != nullptr) {
        sel.primary = *bestRpm;
    }

    const AttributedSensor* coolant = nullptr;
    for (const AttributedSensor& a : sensors) {
        if (a.sensor.role != CoolingSensorRole::CoolantTemp) {
            continue;
        }
        if (coolant == nullptr
            || a.sensor.metadata.stableId < coolant->sensor.metadata.stableId) {
            coolant = &a;
        }
    }
    if (coolant != nullptr) {
        sel.secondary = *coolant;
    } else {
        const AttributedSensor* secondFan = nullptr;
        for (const AttributedSensor& a : sensors) {
            if (a.sensor.role == CoolingSensorRole::CoolantTemp) {
                continue;
            }
            if (sel.primary
                && a.sensor.metadata.stableId
                       == sel.primary->sensor.metadata.stableId) {
                continue;
            }
            if (secondFan == nullptr
                || rpmPriority(a.sensor.role)
                       < rpmPriority(secondFan->sensor.role)
                || (rpmPriority(a.sensor.role)
                        == rpmPriority(secondFan->sensor.role)
                    && a.sensor.metadata.stableId
                           < secondFan->sensor.metadata.stableId)) {
                secondFan = &a;
            }
        }
        if (secondFan != nullptr) {
            sel.secondary = *secondFan;
        }
    }
    return sel;
}

CoolingTelemetryService* makeWithProviders(
    std::vector<CoolingSensorProviderPtr> providers,
    std::function<MonotonicTimestamp()> now, QObject* parent) {
    return new CoolingTelemetryService(std::move(providers), std::move(now),
                                       parent);
}

void pollOnceForTest(CoolingTelemetryService& service) { service.poll(); }

}  // namespace detail

CoolingTelemetryService::CoolingTelemetryService(QObject* parent)
    : CoolingTelemetryService(
          [] {
              std::vector<CoolingSensorProviderPtr> p;
              p.push_back(std::make_shared<HwmonCoolingProvider>());
              return p;
          }(),
          nowProd, parent) {}

CoolingTelemetryService::CoolingTelemetryService(
    std::vector<CoolingSensorProviderPtr> providers,
    std::function<MonotonicTimestamp()> now, QObject* parent)
    : interfaces::ITelemetryProvider(parent),
      providers_(std::move(providers)),
      now_(std::move(now)) {
    timer_ = new QTimer(this);
    timer_->setInterval(kPollIntervalMs);
    connect(timer_, &QTimer::timeout, this, [this]() { poll(); });
}

void CoolingTelemetryService::start() {
    if (timer_->isActive()) {
        return;
    }
    poll();
    timer_->start();
}

void CoolingTelemetryService::stop() { timer_->stop(); }

void CoolingTelemetryService::logDiscovery(
    const std::vector<detail::AttributedSensor>& all,
    const detail::CoolingSelection& sel) const {
    qCInfo(lcCooling).noquote() << "Cooling Discovery";
    // Group by provider so each provider identifies itself.
    QStringList seenProviders;
    for (const auto& a : all) {
        if (!seenProviders.contains(a.providerName)) {
            seenProviders.append(a.providerName);
        }
    }
    for (const QString& pname : seenProviders) {
        qCInfo(lcCooling).noquote() << "Provider:" << pname;
        for (const auto& a : all) {
            if (a.providerName != pname) {
                continue;
            }
            qCInfo(lcCooling).noquote()
                << QStringLiteral("  \u2713 %1 (%2)")
                       .arg(QString::fromUtf8(roleName(a.sensor.role)),
                            a.sensor.metadata.stableId);
        }
    }
    qCInfo(lcCooling).noquote() << "Selected:";
    qCInfo(lcCooling).noquote()
        << "  Primary:  "
        << (sel.primary ? QString::fromUtf8(roleName(sel.primary->sensor.role))
                        : QStringLiteral("none"));
    qCInfo(lcCooling).noquote()
        << "  Secondary:"
        << (sel.secondary
                ? QString::fromUtf8(roleName(sel.secondary->sensor.role))
                : QStringLiteral("none"));
    qCInfo(lcCooling).noquote() << "Reason:";
    qCInfo(lcCooling).noquote()
        << "  Highest-priority RPM source selected as primary; coolant "
           "temperature (or a distinct second fan) as secondary.";
}

void CoolingTelemetryService::emitRole(
    MetricId id, MetricUnit unit,
    const std::optional<detail::AttributedSensor>& sensor,
    std::optional<MetricSample>& lastSample, std::optional<double>& lastValue) {
    const MonotonicTimestamp t = now_ ? now_() : 0;
    if (!sensor.has_value()) {
        // Role not present at all: honest Unavailable, no fabricated value.
        const auto s = MetricSample::unavailable(id, t);
        lastSample = s;
        emit readingChanged(s);
        return;
    }
    const std::string key = sensor->sensor.metadata.stableId.toStdString();
    const std::optional<double> value =
        sensor->sensor.read ? sensor->sensor.read() : std::nullopt;
    if (value.has_value()) {
        lastValue = value;
        if (const auto s = MetricSample::tryFresh(id, *value, unit, t, key)) {
            lastSample = s;
            emit readingChanged(*s);
        }
        return;
    }
    if (lastValue.has_value()) {
        if (const auto s =
                MetricSample::tryStale(id, *lastValue, unit, t, key)) {
            lastSample = s;
            emit readingChanged(*s);
        }
    } else {
        const auto s = MetricSample::unavailable(id, t, key);
        lastSample = s;
        emit readingChanged(s);
    }
}

void CoolingTelemetryService::poll() {
    // Aggregate across all providers, attributing each sensor to its source.
    std::vector<detail::AttributedSensor> all;
    for (const auto& p : providers_) {
        if (!p) {
            continue;
        }
        const QString pname = p->providerName();
        for (auto& s : p->discover()) {
            all.push_back(detail::AttributedSensor{pname, std::move(s)});
        }
    }

    const detail::CoolingSelection sel = detail::selectFrom(all);

    if (!loggedDiscovery_) {
        logDiscovery(all, sel);
        loggedDiscovery_ = true;
    }

    emitRole(MetricId::CoolingPrimary, MetricUnit::Rpm, sel.primary, primary_,
             lastPrimary_);

    // Secondary: either a coolant temperature or a distinct second fan.
    if (sel.secondary
        && sel.secondary->sensor.role == CoolingSensorRole::CoolantTemp) {
        emitRole(MetricId::CoolingCoolantTemp, MetricUnit::Celsius, sel.secondary,
                 coolant_, lastCoolant_);
    } else {
        emitRole(MetricId::CoolingSecondary, MetricUnit::Rpm, sel.secondary,
                 secondary_, lastSecondary_);
    }
}

QList<models::MetricSample> CoolingTelemetryService::currentSamples() const {
    QList<models::MetricSample> out;
    out.append(primary_.value_or(
        MetricSample::unavailable(MetricId::CoolingPrimary, 0)));
    if (secondary_.has_value()) {
        out.append(*secondary_);
    }
    if (coolant_.has_value()) {
        out.append(*coolant_);
    }
    return out;
}

}  // namespace darkspark::services
