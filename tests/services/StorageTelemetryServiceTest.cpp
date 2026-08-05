// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for StorageTelemetryService and its selection policies.
// Uses synthetic providers -- no real sysfs. Links Qt6::Core.

#include <QObject>

#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

#include "models/MetricSample.hpp"
#include "services/StorageSensorProvider.hpp"
#include "services/StorageTelemetryService.hpp"

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

NormalizedStorageSensor fsSensor(StorageSensorKind kind, const QString& mount,
                                 const QString& device, bool root,
                                 bool writableLocal, double total,
                                 std::optional<double> value) {
    NormalizedStorageSensor s;
    s.kind = kind;
    s.unit = (kind == StorageSensorKind::FilesystemUtilization)
                 ? StorageSensorUnit::Percent
                 : StorageSensorUnit::Bytes;
    s.target.selectionKey = mount;
    s.target.backingDeviceHint = device;
    s.target.isRootFilesystem = root;
    s.target.isWritableLocal = writableLocal;
    s.target.totalBytesForRanking = total;
    s.metadata.stableId = QStringLiteral("fs:") + mount;
    s.read = [value]() { return value; };
    return s;
}

NormalizedStorageSensor tempSensor(const QString& driveKey,
                                   std::optional<double> value) {
    NormalizedStorageSensor s;
    s.kind = StorageSensorKind::Temperature;
    s.unit = StorageSensorUnit::Celsius;
    s.target.selectionKey = driveKey;
    s.target.backingDeviceHint = driveKey;
    s.metadata.stableId = QStringLiteral("nvme:") + driveKey;
    s.read = [value]() { return value; };
    return s;
}

AttributedStorageSensor attr(const char* prov, NormalizedStorageSensor s) {
    return AttributedStorageSensor{QString::fromUtf8(prov), std::move(s)};
}

class FakeProvider : public StorageSensorProvider {
public:
    FakeProvider(QString name, std::vector<NormalizedStorageSensor> sensors)
        : name_(std::move(name)), sensors_(std::move(sensors)) {}
    QString providerName() const override { return name_; }
    std::vector<NormalizedStorageSensor> discover() const override {
        return sensors_;
    }
private:
    QString name_;
    std::vector<NormalizedStorageSensor> sensors_;
};

class Collector : public QObject {
public:
    explicit Collector(StorageTelemetryService* s) {
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

// ---- Filesystem selection policy ----
void test_fs_prefers_root() {
    std::vector<AttributedStorageSensor> all{
        attr("fs", fsSensor(StorageSensorKind::FilesystemUtilization, "/home",
                            "/dev/nvme0n1p3", false, true, 900.0, 10.0)),
        attr("fs", fsSensor(StorageSensorKind::FilesystemUtilization, "/",
                            "/dev/nvme0n1p2", true, true, 500.0, 50.0)),
    };
    CHECK(selectFilesystem(all, QString()) == QStringLiteral("/"));
}

void test_fs_largest_writable_when_no_root() {
    std::vector<AttributedStorageSensor> all{
        attr("fs", fsSensor(StorageSensorKind::FilesystemUtilization, "/data",
                            "/dev/sdb1", false, true, 2000.0, 10.0)),
        attr("fs", fsSensor(StorageSensorKind::FilesystemUtilization, "/home",
                            "/dev/sda1", false, true, 900.0, 20.0)),
    };
    CHECK(selectFilesystem(all, QString()) == QStringLiteral("/data"));  // largest
}

void test_fs_override_wins() {
    std::vector<AttributedStorageSensor> all{
        attr("fs", fsSensor(StorageSensorKind::FilesystemUtilization, "/",
                            "/dev/nvme0n1p2", true, true, 500.0, 50.0)),
        attr("fs", fsSensor(StorageSensorKind::FilesystemUtilization, "/mnt/x",
                            "/dev/sdb1", false, true, 100.0, 5.0)),
    };
    CHECK(selectFilesystem(all, QStringLiteral("/mnt/x")) == QStringLiteral("/mnt/x"));
}

void test_fs_order_independence() {
    // Same set, reversed order, must select the same filesystem.
    auto a = fsSensor(StorageSensorKind::FilesystemUtilization, "/data", "/dev/sdb1", false, true, 2000.0, 10.0);
    auto b = fsSensor(StorageSensorKind::FilesystemUtilization, "/home", "/dev/sda1", false, true, 900.0, 20.0);
    std::vector<AttributedStorageSensor> f{attr("fs", a), attr("fs", b)};
    std::vector<AttributedStorageSensor> r{attr("fs", b), attr("fs", a)};
    CHECK(selectFilesystem(f, QString()) == selectFilesystem(r, QString()));
}

// ---- Disk selection policy ----
void test_disk_prefers_backing_device() {
    std::vector<AttributedStorageSensor> all{
        attr("nvme", tempSensor("nvme0n1", 40.0)),
        attr("nvme", tempSensor("nvme1n1", 45.0)),
    };
    // Filesystem "/" backed by /dev/nvme1n1p2 -> should pick nvme1n1.
    const auto sel = selectDriveFor(all, QStringLiteral("/"),
                                    QStringLiteral("/dev/nvme1n1p2"));
    CHECK(sel.selectedDriveKey == QStringLiteral("nvme1n1"));
    CHECK(sel.driveMatchedFilesystem);
}

void test_disk_nvme_fallback_when_no_match() {
    std::vector<AttributedStorageSensor> all{
        attr("nvme", tempSensor("nvme1n1", 45.0)),
        attr("nvme", tempSensor("nvme0n1", 40.0)),
    };
    // Backing device unknown -> deterministic primary NVMe = lexicographically
    // first "nvme0n1".
    const auto sel = selectDriveFor(all, QStringLiteral("/"), QString());
    CHECK(sel.selectedDriveKey == QStringLiteral("nvme0n1"));
    CHECK(!sel.driveMatchedFilesystem);
}

void test_disk_order_independence() {
    auto a = tempSensor("nvme1n1", 45.0);
    auto b = tempSensor("nvme0n1", 40.0);
    std::vector<AttributedStorageSensor> f{attr("nvme", a), attr("nvme", b)};
    std::vector<AttributedStorageSensor> r{attr("nvme", b), attr("nvme", a)};
    CHECK(selectDriveFor(f, QStringLiteral("/"), QString()).selectedDriveKey
          == selectDriveFor(r, QStringLiteral("/"), QString()).selectedDriveKey);
}

// ---- Service emission + aggregation ----
void test_service_emits_six_and_selects() {
    QObject owner;
    std::vector<StorageSensorProviderPtr> provs;
    provs.push_back(std::make_shared<FakeProvider>(
        QStringLiteral("filesystem"),
        std::vector<NormalizedStorageSensor>{
            fsSensor(StorageSensorKind::FilesystemUtilization, "/", "/dev/nvme0n1p2", true, true, 500.0, 73.0),
            fsSensor(StorageSensorKind::FilesystemUsedBytes, "/", "/dev/nvme0n1p2", true, true, 500.0, 4.5e11),
            fsSensor(StorageSensorKind::FilesystemTotalBytes, "/", "/dev/nvme0n1p2", true, true, 500.0, 6.2e11)}));
    provs.push_back(std::make_shared<FakeProvider>(
        QStringLiteral("nvme"),
        std::vector<NormalizedStorageSensor>{tempSensor("nvme0n1", 41.0)}));
    auto* svc = makeWithProviders(std::move(provs),
                                  []() { return MonotonicTimestamp(100); }, &owner);
    Collector col(svc);
    pollOnceForTest(*svc);

    const auto u = col.latest(MetricId::StorageUtilization);
    CHECK(u.has_value());
    if (u && u->value()) CHECK(*u->value() == 73.0);
    const auto t = col.latest(MetricId::StorageTemperature);
    CHECK(t.has_value());
    if (t && t->value()) CHECK(*t->value() == 41.0);  // nvme0n1 backs "/"
    // Read/write present as Unavailable (no diskstats provider here) -- honest.
    const auto rr = col.latest(MetricId::StorageReadRate);
    CHECK(rr.has_value());
    if (rr) CHECK(rr->state() == MetricState::Unavailable);
}

void test_service_no_filesystem_unavailable() {
    QObject owner;
    std::vector<StorageSensorProviderPtr> provs;
    provs.push_back(std::make_shared<FakeProvider>(
        QStringLiteral("filesystem"), std::vector<NormalizedStorageSensor>{}));
    auto* svc = makeWithProviders(std::move(provs),
                                  []() { return MonotonicTimestamp(100); }, &owner);
    Collector col(svc);
    pollOnceForTest(*svc);
    const auto u = col.latest(MetricId::StorageUtilization);
    CHECK(u.has_value());
    if (u) CHECK(u->state() == MetricState::Unavailable);
}

// ---- Discovery summary is part of the tested contract ----

// A filesystem utilization sensor carrying full identity (device, fstype, hwmon
// via the drive) so summarizeDiscovery can report it.
NormalizedStorageSensor fsSensorFull(StorageSensorKind kind,
                                     const QString& mount, const QString& device,
                                     const QString& fsType, double total,
                                     std::optional<double> value) {
    NormalizedStorageSensor s = fsSensor(kind, mount, device, true, true, total,
                                         value);
    s.target.filesystemType = fsType;
    return s;
}

NormalizedStorageSensor rateSensor(StorageSensorKind kind,
                                   const QString& driveKey,
                                   std::optional<double> value) {
    NormalizedStorageSensor s;
    s.kind = kind;
    s.unit = StorageSensorUnit::BytesPerSecond;
    s.target.selectionKey = driveKey;
    s.target.backingDeviceHint = driveKey;
    s.metadata.stableId = QStringLiteral("disk:") + driveKey;
    s.read = [value]() { return value; };
    return s;
}

NormalizedStorageSensor tempSensorHwmon(const QString& driveKey,
                                        const QString& hwmon,
                                        std::optional<double> value) {
    NormalizedStorageSensor s = tempSensor(driveKey, value);
    s.target.hwmonName = hwmon;
    return s;
}

void test_discovery_summary_reports_identity() {
    std::vector<AttributedStorageSensor> all{
        attr("filesystem", fsSensorFull(StorageSensorKind::FilesystemUtilization,
                                        "/", "nvme0n1", "ext4", 2.0e12, 50.0)),
        attr("filesystem", fsSensorFull(StorageSensorKind::FilesystemTotalBytes,
                                        "/", "nvme0n1", "ext4", 2.0e12, 2.0e12)),
        attr("nvme", tempSensorHwmon("nvme0n1", "hwmon2", 40.0)),
        attr("diskstats", rateSensor(StorageSensorKind::ReadRate, "nvme0n1",
                                     1.0e8)),
        attr("diskstats", rateSensor(StorageSensorKind::WriteRate, "nvme0n1",
                                     5.0e7)),
    };
    const auto fsKey = selectFilesystem(all, QString());
    const auto sel = selectDriveFor(all, fsKey, QStringLiteral("nvme0n1"));
    const auto sum = summarizeDiscovery(all, sel);

    // Every required identity field must be present and correct.
    CHECK(sum.selectedFilesystem == QStringLiteral("/"));
    CHECK(sum.backingDevice == QStringLiteral("nvme0n1"));
    CHECK(sum.filesystemType == QStringLiteral("ext4"));
    CHECK(sum.capacityBytes == 2.0e12);
    CHECK(sum.selectedNvmeHwmon == QStringLiteral("hwmon2"));
    CHECK(sum.diskStatsDevice == QStringLiteral("nvme0n1"));
    // The drive backed the filesystem, so this is NOT a fallback.
    CHECK(!sum.usedDeterministicFallback);
}

void test_discovery_summary_reports_fallback() {
    // Backing device cannot be tied to any drive (filesystem device does not
    // contain any drive key), so the disk policy must fall back deterministically
    // AND the summary must record that it did.
    std::vector<AttributedStorageSensor> all{
        attr("filesystem", fsSensorFull(StorageSensorKind::FilesystemUtilization,
                                        "/", "mapper-cryptroot", "ext4", 1.0e12,
                                        50.0)),
        attr("filesystem", fsSensorFull(StorageSensorKind::FilesystemTotalBytes,
                                        "/", "mapper-cryptroot", "ext4", 1.0e12,
                                        1.0e12)),
        attr("nvme", tempSensorHwmon("nvme1n1", "hwmon3", 42.0)),
        attr("nvme", tempSensorHwmon("nvme0n1", "hwmon2", 40.0)),
    };
    const auto fsKey = selectFilesystem(all, QString());
    // Backing device "mapper-cryptroot" contains neither drive key.
    const auto sel = selectDriveFor(all, fsKey, QStringLiteral("mapper-cryptroot"));
    const auto sum = summarizeDiscovery(all, sel);

    // A drive was still chosen (deterministic primary NVMe = nvme0n1), but the
    // summary explicitly records that it was a fallback, not a match.
    CHECK(!sel.driveMatchedFilesystem);
    CHECK(sel.selectedDriveKey == QStringLiteral("nvme0n1"));
    CHECK(sum.usedDeterministicFallback);
    CHECK(sum.selectedNvmeHwmon == QStringLiteral("hwmon2"));
}

}  // namespace

int main() {
    test_fs_prefers_root();
    test_fs_largest_writable_when_no_root();
    test_fs_override_wins();
    test_fs_order_independence();
    test_disk_prefers_backing_device();
    test_disk_nvme_fallback_when_no_match();
    test_disk_order_independence();
    test_service_emits_six_and_selects();
    test_service_no_filesystem_unavailable();
    test_discovery_summary_reports_identity();
    test_discovery_summary_reports_fallback();
    if (g_failures == 0) {
        std::puts("All StorageTelemetryService tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d storage service check(s) failed.\n", g_failures);
    return 1;
}
