// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for NetworkTelemetryService, its selection policy, the
// discovery summary, and RX/TX rate via NetworkInterfaceProvider (reusing
// ThroughputRateCalculator). Synthetic providers -- no real sysfs. Links Qt6::Core.

#include <QObject>

#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

#include "models/MetricSample.hpp"
#include "services/NetworkInterfaceProvider.hpp"
#include "services/NetworkSensorProvider.hpp"
#include "services/NetworkTelemetryService.hpp"

using namespace darkspark::services;
using namespace darkspark::services::detail;
using darkspark::models::MetricId;
using darkspark::models::MetricSample;
using darkspark::models::MetricState;
using darkspark::models::MetricUnit;
using darkspark::models::MonotonicTimestamp;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

// A link-state sensor is the per-interface anchor the selection policy ranks.
NormalizedNetworkSensor linkSensor(const QString& iface, bool defaultRoute,
                                   bool loopback, bool physical, bool up) {
    NormalizedNetworkSensor s;
    s.kind = NetworkSensorKind::LinkState;
    s.unit = NetworkSensorUnit::Count;
    s.target.selectionKey = iface;
    s.target.isDefaultRoute = defaultRoute;
    s.target.isLoopback = loopback;
    s.target.isPhysical = physical;
    s.target.isUp = up;
    s.metadata.stableId = QStringLiteral("net:") + iface;
    const double v = up ? 1.0 : 0.0;
    s.read = [v]() { return std::optional<double>(v); };
    return s;
}

AttributedNetworkSensor attr(const char* prov, NormalizedNetworkSensor s) {
    return AttributedNetworkSensor{QString::fromUtf8(prov), std::move(s)};
}

class FakeProvider : public NetworkSensorProvider {
public:
    FakeProvider(QString name, std::vector<NormalizedNetworkSensor> sensors)
        : name_(std::move(name)), sensors_(std::move(sensors)) {}
    QString providerName() const override { return name_; }
    std::vector<NormalizedNetworkSensor> discover() const override {
        return sensors_;
    }
private:
    QString name_;
    std::vector<NormalizedNetworkSensor> sensors_;
};

class Collector : public QObject {
public:
    explicit Collector(NetworkTelemetryService* s) {
        connect(s, &darkspark::interfaces::ITelemetryProvider::readingChanged,
                this, [this](const MetricSample& m) { samples.push_back(m); });
    }
    std::vector<MetricSample> samples;
    [[nodiscard]] std::optional<MetricSample> latest(MetricId id) const {
        std::optional<MetricSample> f;
        for (const auto& s : samples)
            if (s.id() == id) f = s;
        return f;
    }
};

// ---- Selection policy ----
void test_default_route_selected() {
    std::vector<AttributedNetworkSensor> all{
        attr("interface", linkSensor("enp5s0", true, false, true, true)),
        attr("interface", linkSensor("wlan0", false, false, true, true)),
        attr("interface", linkSensor("lo", false, true, false, true)),
    };
    const auto sel = selectInterface(all, QString());
    CHECK(sel.selectedInterface == QStringLiteral("enp5s0"));
    CHECK(sel.defaultRouteInterface == QStringLiteral("enp5s0"));
    CHECK(!sel.usedDeterministicFallback);
}

void test_override_wins() {
    std::vector<AttributedNetworkSensor> all{
        attr("interface", linkSensor("enp5s0", true, false, true, true)),
        attr("interface", linkSensor("wlan0", false, false, true, true)),
    };
    const auto sel = selectInterface(all, QStringLiteral("wlan0"));
    CHECK(sel.selectedInterface == QStringLiteral("wlan0"));
}

void test_loopback_excluded() {
    // Only loopback + a down interface exist; loopback must NOT be selected
    // (no override). The active-physical rule fails, so the deterministic
    // fallback picks the non-loopback interface even though it is down.
    std::vector<AttributedNetworkSensor> all{
        attr("interface", linkSensor("lo", false, true, false, true)),
        attr("interface", linkSensor("enp5s0", false, false, true, false)),
    };
    const auto sel = selectInterface(all, QString());
    CHECK(sel.selectedInterface == QStringLiteral("enp5s0"));
    CHECK(sel.usedDeterministicFallback);
}

void test_loopback_only_via_override() {
    std::vector<AttributedNetworkSensor> all{
        attr("interface", linkSensor("lo", false, true, false, true)),
    };
    // Without override: nothing non-loopback -> no selection.
    CHECK(selectInterface(all, QString()).selectedInterface.isEmpty());
    // With explicit override: loopback may be chosen.
    CHECK(selectInterface(all, QStringLiteral("lo")).selectedInterface
          == QStringLiteral("lo"));
}

void test_active_physical_when_no_default_route() {
    std::vector<AttributedNetworkSensor> all{
        attr("interface", linkSensor("docker0", false, false, false, true)),
        attr("interface", linkSensor("enp5s0", false, false, true, true)),
    };
    const auto sel = selectInterface(all, QString());
    CHECK(sel.selectedInterface == QStringLiteral("enp5s0"));  // physical, up
    CHECK(!sel.usedDeterministicFallback);
}

void test_deterministic_lexical_fallback() {
    // No default route, none up/physical: fallback = lexicographically-first
    // non-loopback, deterministically (not enumeration order).
    std::vector<AttributedNetworkSensor> a{
        attr("interface", linkSensor("zzz0", false, false, false, false)),
        attr("interface", linkSensor("aaa0", false, false, false, false)),
    };
    std::vector<AttributedNetworkSensor> r{
        attr("interface", linkSensor("aaa0", false, false, false, false)),
        attr("interface", linkSensor("zzz0", false, false, false, false)),
    };
    CHECK(selectInterface(a, QString()).selectedInterface == QStringLiteral("aaa0"));
    CHECK(selectInterface(r, QString()).selectedInterface == QStringLiteral("aaa0"));
    CHECK(selectInterface(a, QString()).usedDeterministicFallback);
}

// ---- Discovery summary ----
void test_discovery_summary_identity() {
    std::vector<AttributedNetworkSensor> all{
        attr("interface", linkSensor("enp5s0", true, false, true, true)),
    };
    const auto sel = selectInterface(all, QString());
    const auto sum = summarizeDiscovery(all, sel);
    CHECK(sum.selectedInterface == QStringLiteral("enp5s0"));
    CHECK(sum.defaultRouteInterface == QStringLiteral("enp5s0"));
    CHECK(sum.selectedIsPhysical);
    CHECK(!sum.selectedIsLoopback);
    CHECK(sum.linkUp);
    CHECK(!sum.usedDeterministicFallback);
}

void test_discovery_summary_fallback_flag() {
    std::vector<AttributedNetworkSensor> all{
        attr("interface", linkSensor("lo", false, true, false, true)),
        attr("interface", linkSensor("enp5s0", false, false, true, false)),
    };
    const auto sel = selectInterface(all, QString());
    const auto sum = summarizeDiscovery(all, sel);
    CHECK(sum.selectedInterface == QStringLiteral("enp5s0"));
    CHECK(sum.defaultRouteInterface.isEmpty());
    CHECK(sum.usedDeterministicFallback);
}

// ---- Service emission + rate through the real provider ----
std::vector<NetworkSensorProviderPtr> providerWith(
    NetworkInterfaceProvider::InterfaceSource src,
    NetworkInterfaceProvider::Clock clock) {
    std::vector<NetworkSensorProviderPtr> v;
    v.push_back(std::make_shared<NetworkInterfaceProvider>(std::move(src),
                                                           std::move(clock)));
    return v;
}

void test_first_rate_unavailable_then_correct() {
    // Real NetworkInterfaceProvider reused with an injected snapshot + clock, so
    // the ThroughputRateCalculator path is exercised unchanged.
    std::uint64_t rx = 1000;
    std::uint64_t tx = 500;
    std::int64_t t = 0;
    auto src = [&rx, &tx]() {
        NetworkInterfaceSnapshot s;
        s.name = QStringLiteral("enp5s0");
        s.rxBytes = rx;
        s.txBytes = tx;
        s.isUp = true;
        s.isPhysical = true;
        s.isDefaultRoute = true;
        return std::vector<NetworkInterfaceSnapshot>{s};
    };
    auto clock = [&t]() { return t; };

    QObject owner;
    auto* svc = makeWithProviders(providerWith(src, clock),
                                  []() { return MonotonicTimestamp(0); }, &owner);
    Collector col(svc);

    // First poll: no prior counters -> rate Unavailable.
    t = 0;
    pollOnceForTest(*svc);
    {
        const auto rr = col.latest(MetricId::NetworkReceiveRate);
        CHECK(rr.has_value());
        if (rr) CHECK(rr->state() == MetricState::Unavailable);
    }
    // Second poll: +2000 rx, +1000 tx over 1000 ms -> 2000 / 1000 B/s.
    rx = 3000;
    tx = 1500;
    t = 1000;
    col.samples.clear();
    pollOnceForTest(*svc);
    {
        const auto rr = col.latest(MetricId::NetworkReceiveRate);
        const auto tr = col.latest(MetricId::NetworkTransmitRate);
        CHECK(rr && rr->state() == MetricState::Fresh);
        if (rr && rr->value()) CHECK(*rr->value() == 2000.0);
        CHECK(tr && tr->state() == MetricState::Fresh);
        if (tr && tr->value()) CHECK(*tr->value() == 1000.0);
    }
    // Third poll: counter reset (decrease). The ThroughputRateCalculator
    // returns nullopt (its reset contract is asserted directly in
    // ThroughputRateCalculatorTest). At the SERVICE layer a missing fresh
    // reading with a prior value is honestly reported as Stale/LastKnown holding
    // the last-known rate -- the same Fresh/Stale/Unavailable contract every
    // subsystem uses. It must NOT be Fresh (no new rate was computed) and must
    // never fabricate a new or negative value.
    rx = 10;
    tx = 5;
    t = 2000;
    col.samples.clear();
    pollOnceForTest(*svc);
    {
        const auto rr = col.latest(MetricId::NetworkReceiveRate);
        CHECK(rr);
        if (rr) {
            CHECK(rr->state() != MetricState::Fresh);
            CHECK(rr->state() == MetricState::Stale);
            if (rr->value()) CHECK(*rr->value() >= 0.0);
        }
    }
}

void test_malformed_input_no_crash_unavailable() {
    // Snapshot source returns nothing (as if /proc/net/dev was malformed/empty).
    auto src = []() { return std::vector<NetworkInterfaceSnapshot>{}; };
    auto clock = []() { return std::int64_t(0); };
    QObject owner;
    auto* svc = makeWithProviders(providerWith(src, clock),
                                  []() { return MonotonicTimestamp(0); }, &owner);
    Collector col(svc);
    pollOnceForTest(*svc);
    const auto rr = col.latest(MetricId::NetworkReceiveRate);
    CHECK(rr.has_value());
    if (rr) CHECK(rr->state() == MetricState::Unavailable);
}

}  // namespace

int main() {
    test_default_route_selected();
    test_override_wins();
    test_loopback_excluded();
    test_loopback_only_via_override();
    test_active_physical_when_no_default_route();
    test_deterministic_lexical_fallback();
    test_discovery_summary_identity();
    test_discovery_summary_fallback_flag();
    test_first_rate_unavailable_then_correct();
    test_malformed_input_no_crash_unavailable();
    if (g_failures == 0) {
        std::puts("All NetworkTelemetryService tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d network service check(s) failed.\n", g_failures);
    return 1;
}
