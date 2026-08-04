// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for the shared GPU device selection rule. All scenarios
// are driven through injected candidate/hwmon lists -- no test touches real
// sysfs. Uses QString, so it links Qt6::Core.

#include <QString>

#include <cstdio>
#include <optional>
#include <vector>

#include "services/GpuDeviceSelector.hpp"

using namespace darkspark::services;
using namespace darkspark::services::detail;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

// Model the user's actual machine:
//   card0 = integrated (Granite Ridge), shallow PCIe path, hwmon3
//   card1 = discrete   (Navi 48 9070),  deep PCIe path,    hwmon2
GpuCandidate integrated() {
    GpuCandidate c;
    c.drmCardPath = "/sys/class/drm/card0";
    c.resolvedDevicePath = "/sys/devices/pci0000:00/0000:00:08.1/0000:0e:00.0";
    c.busyPercentPath = "/sys/class/drm/card0/device/gpu_busy_percent";
    c.pcieDepth = 2;
    return c;
}
GpuCandidate discrete() {
    GpuCandidate c;
    c.drmCardPath = "/sys/class/drm/card1";
    c.resolvedDevicePath =
        "/sys/devices/pci0000:00/0000:00:01.1/0000:01:00.0/0000:02:00.0/0000:03:00.0";
    c.busyPercentPath = "/sys/class/drm/card1/device/gpu_busy_percent";
    c.pcieDepth = 4;
    return c;
}
GpuHwmonCandidate hwmonIntegrated() {
    return {"/sys/class/hwmon/hwmon3",
            "/sys/devices/pci0000:00/0000:00:08.1/0000:0e:00.0"};
}
GpuHwmonCandidate hwmonDiscrete() {
    return {"/sys/class/hwmon/hwmon2",
            "/sys/devices/pci0000:00/0000:00:01.1/0000:01:00.0/0000:02:00.0/0000:03:00.0"};
}

// 1 + 2. Two AMD GPUs, integrated enumerated first, discrete second: the
// discrete GPU must be selected.
void test_two_gpus_selects_discrete() {
    const auto sel = selectFrom({integrated(), discrete()},
                                {hwmonIntegrated(), hwmonDiscrete()}, QString());
    CHECK(sel.has_value());
    CHECK(sel->drmCardPath == "/sys/class/drm/card1");        // discrete
    CHECK(sel->resolvedDevicePath == discrete().resolvedDevicePath);
}

// 3. Utilization and thermal paths correspond to the same resolved device.
void test_util_and_thermal_same_device() {
    const auto sel = selectFrom({integrated(), discrete()},
                                {hwmonIntegrated(), hwmonDiscrete()}, QString());
    CHECK(sel.has_value());
    CHECK(sel->hasHwmon);
    // The busy path's card and the hwmon must resolve to the SAME device path.
    CHECK(sel->busyPercentPath.startsWith(sel->drmCardPath));
    CHECK(sel->hwmonPath == "/sys/class/hwmon/hwmon2");        // discrete's hwmon
    // And crucially both join on the discrete resolved device path.
    CHECK(sel->resolvedDevicePath == hwmonDiscrete().resolvedDevicePath);
}

// 4. Fallback: only one AMD GPU exists -> it is selected regardless of depth.
void test_single_gpu_fallback() {
    const auto sel = selectFrom({integrated()}, {hwmonIntegrated()}, QString());
    CHECK(sel.has_value());
    CHECK(sel->drmCardPath == "/sys/class/drm/card0");
    CHECK(sel->hasHwmon);
}

// 5. Missing hwmon temperature still allows utilization.
void test_missing_hwmon_allows_utilization() {
    // Discrete GPU present, but no hwmon joins to it (empty hwmon list).
    const auto sel = selectFrom({discrete()}, {}, QString());
    CHECK(sel.has_value());
    CHECK(sel->busyPercentPath == discrete().busyPercentPath);  // util works
    CHECK(!sel->hasHwmon);                                      // temp absent
    CHECK(sel->hwmonPath.isEmpty());
}

// Also: hwmon exists but for a DIFFERENT device -> not joined.
void test_hwmon_for_other_device_not_joined() {
    // Selected discrete, but only the integrated hwmon is present.
    const auto sel = selectFrom({discrete()}, {hwmonIntegrated()}, QString());
    CHECK(sel.has_value());
    CHECK(!sel->hasHwmon);  // integrated hwmon must NOT bind to discrete GPU
}

// 6. Deterministic override for future explicit GPU selection: force the
// integrated GPU even though discrete would win by depth.
void test_explicit_override() {
    const auto sel = selectFrom({integrated(), discrete()},
                                {hwmonIntegrated(), hwmonDiscrete()},
                                integrated().resolvedDevicePath);
    CHECK(sel.has_value());
    CHECK(sel->drmCardPath == "/sys/class/drm/card0");   // overridden to integrated
    CHECK(sel->hwmonPath == "/sys/class/hwmon/hwmon3");  // its matching hwmon
}

// Override that matches nothing falls back to automatic selection.
void test_override_no_match_falls_back() {
    const auto sel = selectFrom({integrated(), discrete()},
                                {hwmonIntegrated(), hwmonDiscrete()},
                                "/sys/devices/nonexistent");
    CHECK(sel.has_value());
    CHECK(sel->drmCardPath == "/sys/class/drm/card1");  // back to discrete
}

// No candidates at all -> nullopt.
void test_no_candidates() {
    const auto sel = selectFrom({}, {}, QString());
    CHECK(!sel.has_value());
}

// Order independence: discrete enumerated FIRST still selected.
void test_order_independence() {
    const auto sel = selectFrom({discrete(), integrated()},
                                {hwmonDiscrete(), hwmonIntegrated()}, QString());
    CHECK(sel.has_value());
    CHECK(sel->drmCardPath == "/sys/class/drm/card1");
}

// The full injected-sources entry point wires through selectFrom.
void test_sources_entry_point() {
    GpuDeviceSources s;
    s.enumerateCandidates = []() {
        return std::vector<GpuCandidate>{integrated(), discrete()};
    };
    s.enumerateHwmon = []() {
        return std::vector<GpuHwmonCandidate>{hwmonIntegrated(), hwmonDiscrete()};
    };
    s.explicitDevicePath = []() { return QString(); };
    const auto sel = selectGpuDevice(s);
    CHECK(sel.has_value());
    CHECK(sel->drmCardPath == "/sys/class/drm/card1");
}

}  // namespace

int main() {
    test_two_gpus_selects_discrete();
    test_util_and_thermal_same_device();
    test_single_gpu_fallback();
    test_missing_hwmon_allows_utilization();
    test_hwmon_for_other_device_not_joined();
    test_explicit_override();
    test_override_no_match_falls_back();
    test_no_candidates();
    test_order_independence();
    test_sources_entry_point();
    if (g_failures == 0) {
        std::puts("All GpuDeviceSelector tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d GPU selector check(s) failed.\n", g_failures);
    return 1;
}
