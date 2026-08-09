// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_GPUVRAMSERVICE_HPP
#define DARKSPARK_SERVICES_GPUVRAMSERVICE_HPP

#include <QString>
#include <QTimer>

#include <functional>
#include <optional>

#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::services {

// Forward-declared in the enclosing services namespace (NOT inside detail) so
// the detail helpers below refer to this class, services::GpuVramService,
// rather than declaring a distinct detail::GpuVramService.
class GpuVramService;

namespace detail {

/// A raw VRAM reading: used and total bytes, as read from amdgpu sysfs. Both
/// are required for a valid reading; either missing yields nullopt upstream.
struct GpuVramReading {
    std::uint64_t usedBytes = 0;
    std::uint64_t totalBytes = 0;
};

/// Injected collaborators for deterministic testing of GPU VRAM.
///
/// `readVram` returns the used/total byte pair for the selected GPU --
/// production: reads mem_info_vram_used / mem_info_vram_total from the SAME
/// physical device chosen by the shared GPU selector; tests: a scripted pair or
/// nullopt (missing files, malformed content, or used > total). It is called
/// once per poll so a device appearing/disappearing is handled naturally.
/// `now` returns a monotonic timestamp. Both have production defaults; tests
/// script them so no test touches real sysfs, the real clock, or a live GPU.
struct GpuVramSources {
    std::function<std::optional<GpuVramReading>()> readVram;
    std::function<models::MonotonicTimestamp()> now;
};

[[nodiscard]] GpuVramService* makeWithSources(GpuVramSources sources,
                                              QObject* parent);
void pollOnceForTest(GpuVramService& service);

}  // namespace detail

/// GPU VRAM telemetry for AMD (amdgpu), sourced from the GPU's sysfs device
/// directory (mem_info_vram_used / mem_info_vram_total).
///
/// Implements ITelemetryProvider, publishing two joined samples per poll:
/// MetricId::MemoryUsedBytes and MetricId::MemoryTotalBytes, BOTH carrying the
/// stable sensor key "gpu-vram". The key is essential: it distinguishes GPU
/// VRAM from SYSTEM RAM, which MemoryTelemetryService emits under the same
/// MetricIds but WITHOUT a key. The GPU adapter consumes only keyed samples;
/// the system Memory adapter consumes only keyless ones. They never cross.
///
/// The device is resolved ONCE at construction via the shared GPU selector, so
/// VRAM binds to the same physical GPU as utilization and temperature, and the
/// selection is not re-run every poll.
///
/// Missing files, malformed content, or used > total yields Unavailable (or
/// Stale if a prior valid value exists), never a throw. Utilization and
/// temperature are unaffected -- they are independent providers.
///
/// Ownership: a QObject owned by its Qt parent. Threading: GUI thread only.
class GpuVramService : public interfaces::ITelemetryProvider {
    Q_OBJECT

public:
    static constexpr const char* kVramKey = "gpu-vram";

    explicit GpuVramService(QObject* parent = nullptr);
    ~GpuVramService() override;

    void start() override;
    void stop() override;

    [[nodiscard]] QList<models::MetricSample> currentSamples() const override;

private:
    friend GpuVramService* detail::makeWithSources(detail::GpuVramSources,
                                                   QObject*);
    friend void detail::pollOnceForTest(GpuVramService&);

    GpuVramService(detail::GpuVramSources sources, QObject* parent);

    void poll();

    detail::GpuVramSources sources_;
    QTimer* timer_;
    std::optional<detail::GpuVramReading> lastValid_;
    models::MetricSample currentUsed_;
    models::MetricSample currentTotal_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_GPUVRAMSERVICE_HPP
