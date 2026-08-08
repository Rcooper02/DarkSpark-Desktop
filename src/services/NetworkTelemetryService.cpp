// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/NetworkTelemetryService.hpp"

#include <QDateTime>
#include <QLoggingCategory>

#include "services/NetworkInterfaceProvider.hpp"

namespace darkspark::services {

using models::MetricId;
using models::MetricSample;
using models::MetricUnit;
using models::MonotonicTimestamp;

namespace {
Q_LOGGING_CATEGORY(lcNetwork, "darkspark.network")

constexpr int kPollIntervalMs = 1000;

MonotonicTimestamp nowProd() {
    return static_cast<MonotonicTimestamp>(QDateTime::currentMSecsSinceEpoch());
}

// Distinct interface candidates (from link-state sensors, which every interface
// has exactly one of), so the policy ranks each interface once.
struct IfaceCandidate {
    QString key;
    bool isDefaultRoute = false;
    bool isLoopback = false;
    bool isPhysical = false;
    bool isUp = false;
};

std::vector<IfaceCandidate> interfaceCandidates(
    const std::vector<detail::AttributedNetworkSensor>& sensors) {
    std::vector<IfaceCandidate> out;
    for (const auto& a : sensors) {
        if (a.sensor.kind != NetworkSensorKind::LinkState) {
            continue;  // one link-state sensor per interface: a clean anchor
        }
        IfaceCandidate c;
        c.key = a.sensor.target.selectionKey;
        c.isDefaultRoute = a.sensor.target.isDefaultRoute;
        c.isLoopback = a.sensor.target.isLoopback;
        c.isPhysical = a.sensor.target.isPhysical;
        c.isUp = a.sensor.target.isUp;
        out.push_back(c);
    }
    return out;
}

}  // namespace

namespace detail {

NetworkSelection selectInterface(
    const std::vector<AttributedNetworkSensor>& sensors,
    const QString& overrideInterface) {
    NetworkSelection sel;
    const std::vector<IfaceCandidate> ifs = interfaceCandidates(sensors);

    // Record the default-route interface for the summary regardless of what is
    // ultimately selected.
    for (const auto& c : ifs) {
        if (c.isDefaultRoute) {
            sel.defaultRouteInterface = c.key;
            break;
        }
    }

    if (ifs.empty()) {
        return sel;
    }

    // 1. Explicit override, if present among candidates. Loopback is only ever
    //    chosen this way.
    if (!overrideInterface.isEmpty()) {
        for (const auto& c : ifs) {
            if (c.key == overrideInterface) {
                sel.selectedInterface = c.key;
                return sel;
            }
        }
    }

    // 2. The interface carrying the default route (never loopback here).
    for (const auto& c : ifs) {
        if (c.isDefaultRoute && !c.isLoopback) {
            sel.selectedInterface = c.key;
            return sel;
        }
    }

    // 3. An active (up) non-loopback physical interface, lexicographic tie-break.
    const IfaceCandidate* best = nullptr;
    for (const auto& c : ifs) {
        if (c.isLoopback || !c.isPhysical || !c.isUp) {
            continue;
        }
        if (best == nullptr || c.key < best->key) {
            best = &c;
        }
    }
    if (best != nullptr) {
        sel.selectedInterface = best->key;
        return sel;
    }

    // 4. Deterministic fallback: lexicographically-first non-loopback interface.
    //    Never enumeration order; never loopback.
    const IfaceCandidate* fallback = nullptr;
    for (const auto& c : ifs) {
        if (c.isLoopback) {
            continue;
        }
        if (fallback == nullptr || c.key < fallback->key) {
            fallback = &c;
        }
    }
    if (fallback != nullptr) {
        sel.selectedInterface = fallback->key;
        sel.usedDeterministicFallback = true;
    }
    return sel;
}

NetworkDiscoverySummary summarizeDiscovery(
    const std::vector<AttributedNetworkSensor>& sensors,
    const NetworkSelection& selection) {
    NetworkDiscoverySummary out;
    out.selectedInterface = selection.selectedInterface;
    out.defaultRouteInterface = selection.defaultRouteInterface;
    out.usedDeterministicFallback = selection.usedDeterministicFallback;
    for (const auto& a : sensors) {
        if (a.sensor.target.selectionKey != selection.selectedInterface) {
            continue;
        }
        out.selectedIsPhysical = a.sensor.target.isPhysical;
        out.selectedIsLoopback = a.sensor.target.isLoopback;
        if (a.sensor.kind == NetworkSensorKind::LinkState) {
            out.linkUp = a.sensor.target.isUp;
        }
    }
    return out;
}

NetworkTelemetryService* makeWithProviders(
    std::vector<NetworkSensorProviderPtr> providers,
    std::function<MonotonicTimestamp()> now, QObject* parent) {
    return new NetworkTelemetryService(std::move(providers), std::move(now),
                                       parent);
}

void pollOnceForTest(NetworkTelemetryService& service) { service.poll(); }

}  // namespace detail

NetworkTelemetryService::NetworkTelemetryService(QObject* parent)
    : NetworkTelemetryService(
          [] {
              std::vector<NetworkSensorProviderPtr> p;
              p.push_back(std::make_shared<NetworkInterfaceProvider>());
              return p;
          }(),
          nowProd, parent) {}

NetworkTelemetryService::NetworkTelemetryService(
    std::vector<NetworkSensorProviderPtr> providers,
    std::function<MonotonicTimestamp()> now, QObject* parent)
    : interfaces::ITelemetryProvider(parent),
      providers_(std::move(providers)),
      now_(std::move(now)) {
    timer_ = new QTimer(this);
    timer_->setInterval(kPollIntervalMs);
    connect(timer_, &QTimer::timeout, this, [this]() { poll(); });
}

void NetworkTelemetryService::start() {
    if (timer_->isActive()) {
        return;
    }
    poll();
    timer_->start();
}

void NetworkTelemetryService::stop() { timer_->stop(); }

void NetworkTelemetryService::logDiscovery(
    const std::vector<detail::AttributedNetworkSensor>& all,
    const detail::NetworkSelection& sel) const {
    qCInfo(lcNetwork).noquote() << "Network Discovery:";
    QStringList seen;
    for (const auto& a : all) {
        if (!seen.contains(a.providerName)) {
            seen.append(a.providerName);
        }
    }
    for (const QString& pname : seen) {
        qCInfo(lcNetwork).noquote() << "Provider:" << pname;
        for (const auto& a : all) {
            // One line per interface: anchor on the link-state sensor so each
            // interface is listed once, with its classification.
            if (a.providerName == pname
                && a.sensor.kind == NetworkSensorKind::LinkState) {
                qCInfo(lcNetwork).noquote()
                    << QStringLiteral("  \u2713 %1 (%2%3%4)")
                           .arg(a.sensor.target.selectionKey,
                                a.sensor.target.isLoopback
                                    ? QStringLiteral("loopback")
                                    : (a.sensor.target.isPhysical
                                           ? QStringLiteral("physical")
                                           : QStringLiteral("virtual")),
                                a.sensor.target.isUp ? QStringLiteral(", up")
                                                     : QStringLiteral(", down"),
                                a.sensor.target.isDefaultRoute
                                    ? QStringLiteral(", default-route")
                                    : QString());
            }
        }
    }

    const detail::NetworkDiscoverySummary sum =
        detail::summarizeDiscovery(all, sel);
    qCInfo(lcNetwork).noquote()
        << "Selected interface:"
        << (sum.selectedInterface.isEmpty() ? QStringLiteral("(none)")
                                            : sum.selectedInterface);
    qCInfo(lcNetwork).noquote()
        << "Default-route interface:"
        << (sum.defaultRouteInterface.isEmpty()
                ? QStringLiteral("(undetermined)")
                : sum.defaultRouteInterface);
    qCInfo(lcNetwork).noquote()
        << "Link state:"
        << (sum.linkUp ? QStringLiteral("up") : QStringLiteral("down"));
    qCInfo(lcNetwork).noquote() << "Reason:";
    qCInfo(lcNetwork).noquote()
        << "  override>default-route>active-physical>first-non-loopback.";
    if (sum.usedDeterministicFallback) {
        qCInfo(lcNetwork).noquote()
            << "Default-route interface could not be determined.";
        qCInfo(lcNetwork).noquote()
            << "Falling back to deterministic active interface.";
    }
}

void NetworkTelemetryService::emitMetric(
    MetricId id, MetricUnit unit,
    const std::optional<detail::AttributedNetworkSensor>& sensor,
    std::optional<MetricSample>& lastSample, std::optional<double>& lastValue) {
    const MonotonicTimestamp t = now_ ? now_() : 0;
    if (!sensor.has_value()) {
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

void NetworkTelemetryService::poll() {
    std::vector<detail::AttributedNetworkSensor> all;
    for (const auto& p : providers_) {
        if (!p) {
            continue;
        }
        const QString pname = p->providerName();
        for (auto& s : p->discover()) {
            all.push_back(detail::AttributedNetworkSensor{pname, std::move(s)});
        }
    }

    const detail::NetworkSelection sel =
        detail::selectInterface(all, overrideInterface_);

    if (!loggedDiscovery_) {
        logDiscovery(all, sel);
        loggedDiscovery_ = true;
    }

    auto find = [&](NetworkSensorKind kind)
        -> std::optional<detail::AttributedNetworkSensor> {
        for (const auto& a : all) {
            if (a.sensor.kind == kind
                && a.sensor.target.selectionKey == sel.selectedInterface) {
                return a;
            }
        }
        return std::nullopt;
    };

    emitMetric(MetricId::NetworkReceiveRate, MetricUnit::BytesPerSecond,
               find(NetworkSensorKind::ReceiveRate), receiveRate_, lastReceive_);
    emitMetric(MetricId::NetworkTransmitRate, MetricUnit::BytesPerSecond,
               find(NetworkSensorKind::TransmitRate), transmitRate_,
               lastTransmit_);
    emitMetric(MetricId::NetworkReceivedBytes, MetricUnit::Bytes,
               find(NetworkSensorKind::ReceivedBytes), receivedBytes_,
               lastReceived_);
    emitMetric(MetricId::NetworkTransmittedBytes, MetricUnit::Bytes,
               find(NetworkSensorKind::TransmittedBytes), transmittedBytes_,
               lastTransmitted_);
    // Link state is a numeric 0/1 flag; carried on Bytes as a plain value, the
    // adapter interprets it by MetricId (no MetricUnit::LinkState).
    emitMetric(MetricId::NetworkLinkState, MetricUnit::Bytes,
               find(NetworkSensorKind::LinkState), linkState_, lastLink_);
}

QList<models::MetricSample> NetworkTelemetryService::currentSamples() const {
    QList<models::MetricSample> out;
    auto add = [&](const std::optional<MetricSample>& s, MetricId id) {
        out.append(s.value_or(MetricSample::unavailable(id, 0)));
    };
    add(receiveRate_, MetricId::NetworkReceiveRate);
    add(transmitRate_, MetricId::NetworkTransmitRate);
    add(receivedBytes_, MetricId::NetworkReceivedBytes);
    add(transmittedBytes_, MetricId::NetworkTransmittedBytes);
    add(linkState_, MetricId::NetworkLinkState);
    return out;
}

}  // namespace darkspark::services
