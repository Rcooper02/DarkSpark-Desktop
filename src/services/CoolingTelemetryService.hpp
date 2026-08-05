// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COOLINGTELEMETRYSERVICE_HPP
#define DARKSPARK_SERVICES_COOLINGTELEMETRYSERVICE_HPP

#include <QTimer>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"
#include "services/CoolingSensorProvider.hpp"

namespace darkspark::services {

// Forward-declared in the services namespace (NOT in detail) so the detail
// helpers below name services::CoolingTelemetryService.
class CoolingTelemetryService;

namespace detail {

/// A discovered sensor tagged with which provider produced it, so discovery
/// logging can attribute each sensor to its source ("Provider: hwmon ...").
struct AttributedSensor {
    QString providerName;
    NormalizedCoolingSensor sensor;
};

/// Pure role selection over the aggregated, attributed sensors. Chooses the
/// primary RPM source by priority (pump > CPU fan > GPU fan > case fan), with a
/// stableId tie-break, and a secondary (coolant temp preferred, else a distinct
/// second fan). Returns no primary only when NO rpm sensor exists at all.
struct CoolingSelection {
    std::optional<AttributedSensor> primary;
    std::optional<AttributedSensor> secondary;
};
[[nodiscard]] CoolingSelection selectFrom(
    const std::vector<AttributedSensor>& sensors);

/// Test seam: build a service over an explicit provider list and time source.
[[nodiscard]] CoolingTelemetryService* makeWithProviders(
    std::vector<CoolingSensorProviderPtr> providers,
    std::function<models::MonotonicTimestamp()> now, QObject* parent);
void pollOnceForTest(CoolingTelemetryService& service);

}  // namespace detail

/// Aggregates cooling telemetry across one or more CoolingSensorProviders and
/// emits role-based samples. It is a SELECTOR, not a metadata processor and not
/// an hwmon reader: it consumes normalized sensors, reads metadata only for
/// logging/selection/diagnostics, and never modifies it. It is presentation-free
/// (no colours, strings-for-users, rings, or formatting).
///
/// Emits:
///   MetricId::CoolingPrimary      (Rpm)      -- the selected primary activity
///   MetricId::CoolingSecondary    (Rpm)      -- a distinct second fan, if any
///   MetricId::CoolingCoolantTemp  (Celsius)  -- coolant temperature, if any
/// Never fabricates: a missing/malformed read yields Unavailable (or Stale if a
/// prior value existed). Role assignment is decided here by discovery, so the
/// telemetry contract stays independent of hardware topology.
class CoolingTelemetryService : public interfaces::ITelemetryProvider {
    Q_OBJECT

public:
    explicit CoolingTelemetryService(QObject* parent = nullptr);

    void start() override;
    void stop() override;
    [[nodiscard]] QList<models::MetricSample> currentSamples() const override;

private:
    friend CoolingTelemetryService* detail::makeWithProviders(
        std::vector<CoolingSensorProviderPtr> providers,
        std::function<models::MonotonicTimestamp()> now, QObject* parent);
    friend void detail::pollOnceForTest(CoolingTelemetryService& service);

    CoolingTelemetryService(std::vector<CoolingSensorProviderPtr> providers,
                            std::function<models::MonotonicTimestamp()> now,
                            QObject* parent);

    void poll();
    void logDiscovery(const std::vector<detail::AttributedSensor>& all,
                      const detail::CoolingSelection& sel) const;
    void emitRole(models::MetricId id, models::MetricUnit unit,
                  const std::optional<detail::AttributedSensor>& sensor,
                  std::optional<models::MetricSample>& lastSample,
                  std::optional<double>& lastValue);

    std::vector<CoolingSensorProviderPtr> providers_;
    std::function<models::MonotonicTimestamp()> now_;
    QTimer* timer_ = nullptr;
    bool loggedDiscovery_ = false;

    std::optional<models::MetricSample> primary_;
    std::optional<models::MetricSample> secondary_;
    std::optional<models::MetricSample> coolant_;
    std::optional<double> lastPrimary_;
    std::optional<double> lastSecondary_;
    std::optional<double> lastCoolant_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_COOLINGTELEMETRYSERVICE_HPP
