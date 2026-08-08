// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_NETWORKTELEMETRYSERVICE_HPP
#define DARKSPARK_SERVICES_NETWORKTELEMETRYSERVICE_HPP

#include <QTimer>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"
#include "services/NetworkSensorProvider.hpp"

namespace darkspark::services {

// Forward-declared in the services namespace (not in detail) so detail helpers
// name services::NetworkTelemetryService.
class NetworkTelemetryService;

namespace detail {

/// A discovered sensor tagged with its producing provider, for logging.
struct AttributedNetworkSensor {
    QString providerName;
    NormalizedNetworkSensor sensor;
};

/// The outcome of the deterministic active-interface selection policy.
///   selectedInterface     : the interface chosen for the live instrument
///   defaultRouteInterface : the interface carrying the default route, if any
///   usedDeterministicFallback : true if selected by fallback rather than by the
///                               default-route / active-physical rules
struct NetworkSelection {
    QString selectedInterface;
    QString defaultRouteInterface;
    bool usedDeterministicFallback = false;
};

/// The transparent discovery summary: the resolved identity behind a selection.
/// The SAME information the service logs at startup, exposed as data so the
/// discovery contract is testable and cannot silently disappear in a refactor.
/// No presentation, drives no UI: logDiscovery reads it to produce the log text,
/// and tests assert on it directly.
struct NetworkDiscoverySummary {
    QString selectedInterface;      ///< chosen active interface
    QString defaultRouteInterface;  ///< default-route interface, empty if none
    bool selectedIsPhysical = false;
    bool selectedIsLoopback = false;
    bool linkUp = false;            ///< link state of the selected interface
    bool usedDeterministicFallback = false;
};

/// Active-interface selection policy (deterministic, order-independent):
///   1. explicit override (future seam; honoured when set and present)
///   2. the interface carrying the default route
///   3. an active (up) non-loopback physical interface, lexicographic tie-break
///   4. deterministic fallback: lexicographically-first non-loopback interface
/// Loopback is never selected unless supplied as the explicit override. Returns
/// the selection (selectedInterface empty only if nothing qualifies at all).
[[nodiscard]] NetworkSelection selectInterface(
    const std::vector<AttributedNetworkSensor>& sensors,
    const QString& overrideInterface);

/// Build the discovery summary from the aggregated sensors + the selection.
/// Pure and deterministic; the single source of the identity the service both
/// logs and (via tests) guarantees. Reads only what the providers supplied.
[[nodiscard]] NetworkDiscoverySummary summarizeDiscovery(
    const std::vector<AttributedNetworkSensor>& sensors,
    const NetworkSelection& selection);

/// Test seam: build a service over explicit providers and a clock.
[[nodiscard]] NetworkTelemetryService* makeWithProviders(
    std::vector<NetworkSensorProviderPtr> providers,
    std::function<models::MonotonicTimestamp()> now, QObject* parent);
void pollOnceForTest(NetworkTelemetryService& service);

}  // namespace detail

/// Aggregates network telemetry across one or more NetworkSensorProviders,
/// applies the deterministic active-interface selection policy, and emits the
/// five approved role-based metrics for the selected interface. It is a SELECTOR
/// and EMITTER only: presentation-free (no colours, strings-for-users, rings,
/// formatting, or progress), and it never computes rates (the provider owns that
/// via ThroughputRateCalculator). Never fabricates: missing/malformed/first-rate
/// reads become Unavailable (or Stale if a prior value existed).
///
/// Emits: NetworkReceiveRate (BytesPerSecond), NetworkTransmitRate
/// (BytesPerSecond), NetworkReceivedBytes (Bytes), NetworkTransmittedBytes
/// (Bytes), NetworkLinkState (a numeric 0/1 flag). Per the approved decision no
/// MetricUnit::LinkState is introduced; link state is carried as a plain value
/// on an existing unit and the adapter/instrument interpret it by MetricId, not
/// by unit.
class NetworkTelemetryService : public interfaces::ITelemetryProvider {
    Q_OBJECT

public:
    explicit NetworkTelemetryService(QObject* parent = nullptr);

    void start() override;
    void stop() override;
    [[nodiscard]] QList<models::MetricSample> currentSamples() const override;

private:
    friend NetworkTelemetryService* detail::makeWithProviders(
        std::vector<NetworkSensorProviderPtr> providers,
        std::function<models::MonotonicTimestamp()> now, QObject* parent);
    friend void detail::pollOnceForTest(NetworkTelemetryService& service);

    NetworkTelemetryService(std::vector<NetworkSensorProviderPtr> providers,
                            std::function<models::MonotonicTimestamp()> now,
                            QObject* parent);

    void poll();
    void logDiscovery(const std::vector<detail::AttributedNetworkSensor>& all,
                      const detail::NetworkSelection& sel) const;
    void emitMetric(models::MetricId id, models::MetricUnit unit,
                    const std::optional<detail::AttributedNetworkSensor>& sensor,
                    std::optional<models::MetricSample>& lastSample,
                    std::optional<double>& lastValue);

    std::vector<NetworkSensorProviderPtr> providers_;
    std::function<models::MonotonicTimestamp()> now_;
    QTimer* timer_ = nullptr;
    bool loggedDiscovery_ = false;
    QString overrideInterface_;  // future explicit-override seam; empty in V1

    std::optional<models::MetricSample> receiveRate_;
    std::optional<models::MetricSample> transmitRate_;
    std::optional<models::MetricSample> receivedBytes_;
    std::optional<models::MetricSample> transmittedBytes_;
    std::optional<models::MetricSample> linkState_;
    std::optional<double> lastReceive_;
    std::optional<double> lastTransmit_;
    std::optional<double> lastReceived_;
    std::optional<double> lastTransmitted_;
    std::optional<double> lastLink_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_NETWORKTELEMETRYSERVICE_HPP
