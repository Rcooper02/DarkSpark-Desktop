# SPDX-License-Identifier: GPL-3.0-or-later
#
# DarkSpark Desktop — Deck-0 build.
#
# Validation platform: Fedora 44 (authoritative). No other platform is claimed
# as validated. No dependencies are fetched; Qt6 Widgets must be provided by
# the system.
#
# CMake floor rationale: 3.21 is the minimum that provides stable Qt6 support
# via find_package(Qt6) together with the features used here. The target
# machine reports CMake 4.3.0, which satisfies this comfortably; we deliberately
# do not raise the floor to 4.x because nothing in this build requires it and a
# lower honest floor keeps the project buildable on more current systems.
cmake_minimum_required(VERSION 3.21)

project(DarkSparkDesktop
    VERSION 0.1.0
    DESCRIPTION "DarkSpark Desktop — Desktop Command Center (Deck-0 shell)"
    LANGUAGES CXX
)

# --- Language standard -------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Qt automation -----------------------------------------------------------
set(CMAKE_AUTOMOC ON)
# AUTOUIC/AUTORCC are off: no .ui or .qrc files in Deck-0.
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# --- Options -----------------------------------------------------------------
option(DARKSPARK_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors (on for project CI/dev)" ON)

# --- Dependencies (system only; no FetchContent) -----------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

# Prefer the target build directory layout to stay out-of-tree.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# --- Project-controlled warnings ---------------------------------------------
# Applied to our targets only, never forced on dependencies.
add_library(darkspark_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(darkspark_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(DARKSPARK_WARNINGS_AS_ERRORS)
        target_compile_options(darkspark_warnings INTERFACE -Werror)
    endif()
endif()

# --- Executable --------------------------------------------------------------
qt_standard_project_setup()

qt_add_executable(darkspark-desktop
    src/main.cpp
    src/application/Application.cpp
    src/application/Application.hpp
    src/models/MetricSample.cpp
    src/models/MetricSample.hpp
    src/models/SensorDefinition.hpp
    src/interfaces/ITelemetryProvider.hpp
    src/services/CpuTelemetryService.cpp
    src/services/CpuTelemetryService.hpp
    src/services/MemoryTelemetryService.cpp
    src/services/MemoryTelemetryService.hpp
    src/services/HwmonDiscovery.cpp
    src/services/HwmonDiscovery.hpp
    src/services/CpuThermalService.cpp
    src/services/CpuThermalService.hpp
    src/services/GpuTelemetryService.cpp
    src/services/GpuTelemetryService.hpp
    src/services/GpuThermalService.cpp
    src/services/GpuThermalService.hpp
    src/services/GpuVramService.cpp
    src/services/GpuVramService.hpp
    src/services/GpuDeviceSelector.cpp
    src/services/GpuDeviceSelector.hpp
    src/desktop/DesktopWindow.cpp
    src/desktop/DesktopWindow.hpp
    src/deck/DeckWindow.cpp
    src/deck/DeckWindow.hpp
    src/deck/pages/DeckPage.cpp
    src/deck/pages/DeckPage.hpp
    src/deck/pages/CommandDeckPage.cpp
    src/deck/pages/SelectionOverlay.cpp
    src/deck/pages/SelectionOverlay.hpp
    src/deck/pages/CommandDeckPage.hpp
    src/deck/layout/DeckLayout.cpp
    src/deck/layout/DeckLayout.hpp
    src/deck/layout/InstrumentFactory.cpp
    src/deck/layout/InstrumentFactory.hpp
    src/deck/layout/DeckLayoutSerializer.cpp
    src/deck/layout/DeckLayoutSerializer.hpp
    src/deck/layout/DeckLayoutCollection.cpp
    src/deck/layout/DeckLayoutCollection.hpp
    src/deck/layout/DeckLayoutEdits.cpp
    src/deck/layout/DeckLayoutEdits.hpp
    src/deck/layout/LayoutPersistenceService.cpp
    src/deck/layout/LayoutPersistenceService.hpp
    src/deck/cards/DashboardCard.cpp
    src/deck/cards/DashboardCard.hpp
    src/deck/cards/StatusIndicator.cpp
    src/deck/cards/StatusIndicator.hpp
    src/deck/instruments/InstrumentSizeMode.hpp
    src/deck/instruments/InstrumentState.hpp
    src/deck/instruments/CpuInstrumentModel.hpp
    src/deck/instruments/CpuInstrumentModelAdapter.cpp
    src/deck/instruments/CpuInstrumentModelAdapter.hpp
    src/deck/instruments/GpuInstrument.cpp
    src/deck/instruments/GpuInstrument.hpp
    src/deck/instruments/GpuInstrumentModel.hpp
    src/deck/instruments/GpuInstrumentModelAdapter.cpp
    src/deck/instruments/GpuInstrumentModelAdapter.hpp
    src/deck/instruments/MemoryInstrument.cpp
    src/deck/instruments/MemoryInstrument.hpp
    src/deck/instruments/MemoryInstrumentModel.hpp
    src/deck/instruments/MemoryInstrumentModelAdapter.cpp
    src/deck/instruments/MemoryInstrumentModelAdapter.hpp
    src/deck/instruments/CoolingInstrument.cpp
    src/deck/instruments/CoolingInstrument.hpp
    src/deck/instruments/CoolingInstrumentModel.hpp
    src/deck/instruments/CoolingInstrumentModelAdapter.cpp
    src/deck/instruments/CoolingInstrumentModelAdapter.hpp
    src/deck/instruments/RingNormalizationPolicy.hpp
    src/services/CoolingSensorProvider.hpp
    src/services/HwmonCoolingProvider.cpp
    src/services/HwmonCoolingProvider.hpp
    src/services/CoolingTelemetryService.cpp
    src/services/CoolingTelemetryService.hpp
    src/services/SensorMetadata.hpp
    src/services/StorageSensorProvider.hpp
    src/services/ThroughputRateCalculator.hpp
    src/services/FilesystemProvider.cpp
    src/services/FilesystemProvider.hpp
    src/services/NVMeProvider.cpp
    src/services/NVMeProvider.hpp
    src/services/DiskStatsProvider.cpp
    src/services/DiskStatsProvider.hpp
    src/services/StorageTelemetryService.cpp
    src/services/StorageTelemetryService.hpp
    src/deck/instruments/StorageInstrument.cpp
    src/deck/instruments/StorageInstrument.hpp
    src/deck/instruments/StorageInstrumentModel.hpp
    src/deck/instruments/StorageInstrumentModelAdapter.cpp
    src/deck/instruments/StorageInstrumentModelAdapter.hpp
    src/services/NetworkSensorProvider.hpp
    src/services/NetworkInterfaceProvider.cpp
    src/services/NetworkInterfaceProvider.hpp
    src/services/NetworkTelemetryService.cpp
    src/services/NetworkTelemetryService.hpp
    src/deck/instruments/NetworkInstrument.cpp
    src/deck/instruments/NetworkInstrument.hpp
    src/deck/instruments/NetworkInstrumentModel.hpp
    src/deck/instruments/NetworkInstrumentModelAdapter.cpp
    src/deck/instruments/NetworkInstrumentModelAdapter.hpp
    src/deck/instruments/InstrumentRenderer.cpp
    src/deck/instruments/InstrumentRenderer.hpp
    src/deck/instruments/InstrumentRenderModel.hpp
    src/deck/instruments/AnimationClock.cpp
    src/deck/instruments/AnimationClock.hpp
    src/deck/instruments/InstrumentPersonality.hpp
    src/deck/instruments/PersonalityMapping.cpp
    src/deck/instruments/PersonalityMapping.hpp
    src/deck/instruments/CpuInstrumentLayout.cpp
    src/deck/instruments/CpuInstrumentLayout.hpp
    src/deck/instruments/CpuInstrument.cpp
    src/deck/instruments/CpuInstrument.hpp
    src/deck/instruments/InstrumentPreviewPage.cpp
    src/deck/instruments/InstrumentPreviewPage.hpp
    src/deck/navigation/PageManager.cpp
    src/deck/navigation/PageManager.hpp
    src/deck/navigation/PageIndicator.cpp
    src/deck/navigation/PageIndicator.hpp
    src/deck/animations/PageTransition.cpp
    src/deck/animations/PageTransition.hpp
    src/themes/LegacyTheme.cpp
    src/themes/LegacyTheme.hpp
)

target_include_directories(darkspark-desktop PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(darkspark-desktop PRIVATE
    Qt6::Widgets
    darkspark_warnings)

install(TARGETS darkspark-desktop
    RUNTIME DESTINATION bin)

# --- Tests -------------------------------------------------------------------
# Dependency-free model tests. The model layer has no Qt/UI/service dependency,
# so the test target compiles MetricSample.cpp directly and does NOT link Qt.
# It links only darkspark_warnings so tests build under the same strict policy.
include(CTest)

if(BUILD_TESTING)
    add_executable(darkspark-models-test
        tests/models/MetricSampleTest.cpp
        src/models/MetricSample.cpp
    )

    target_include_directories(darkspark-models-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    target_link_libraries(darkspark-models-test PRIVATE
        darkspark_warnings
    )

    add_test(
        NAME darkspark-models-test
        COMMAND darkspark-models-test
    )

    # Pure geometry test for the CPU instrument layout. Qt-free: the layout is
    # deliberately separated from painting so it can be reasoned about and
    # tested without a display.
    add_executable(darkspark-instruments-test
        tests/deck/instruments/CpuInstrumentLayoutTest.cpp
        src/deck/instruments/CpuInstrumentLayout.cpp
    )
    target_include_directories(darkspark-instruments-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-instruments-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-instruments-test
        COMMAND darkspark-instruments-test
    )

    # Adapter test: Qt-free integration logic (MetricSample -> presentation
    # model). Separate executable so it has its own main(); compiles MetricSample
    # and the adapter directly, links no Qt.
    add_executable(darkspark-instrument-adapter-test
        tests/deck/instruments/CpuInstrumentModelAdapterTest.cpp
        src/deck/instruments/CpuInstrumentModelAdapter.cpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-instrument-adapter-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-instrument-adapter-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-instrument-adapter-test
        COMMAND darkspark-instrument-adapter-test
    )

    # Service tests. CpuTelemetryService derives from QObject, so this target
    # links Qt6::Core (never Qt6::Widgets). The service header is listed
    # explicitly so AUTOMOC processes its Q_OBJECT for this target.
    add_executable(darkspark-services-test
        tests/services/CpuTelemetryServiceTest.cpp
        src/services/CpuTelemetryService.cpp
        src/services/CpuTelemetryService.hpp
        src/services/MemoryTelemetryService.cpp
        src/services/MemoryTelemetryService.hpp
        src/services/HwmonDiscovery.cpp
        src/services/HwmonDiscovery.hpp
        src/services/CpuThermalService.cpp
        src/services/CpuThermalService.hpp
	src/interfaces/ITelemetryProvider.hpp
        src/models/MetricSample.cpp
    )

    target_include_directories(darkspark-services-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    target_link_libraries(darkspark-services-test PRIVATE
        Qt6::Core
        darkspark_warnings
    )

    add_test(
        NAME darkspark-services-test
        COMMAND darkspark-services-test
    )

    # DeckLayout test: pure-data layout model -- default validity, invariants,
    # graceful unknown-id handling. Qt-free (own main()).
    add_executable(darkspark-deck-layout-test
        tests/deck/layout/DeckLayoutTest.cpp
        src/deck/layout/DeckLayout.cpp
    )
    target_include_directories(darkspark-deck-layout-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-deck-layout-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-deck-layout-test
        COMMAND darkspark-deck-layout-test
    )

    # DeckLayout serializer test: DeckLayout<->JSON + enum/string mappings,
    # round-trip, malformed/unknown/invalid. Links Qt6::Core (QJson); own main().
    add_executable(darkspark-deck-layout-serializer-test
        tests/deck/layout/DeckLayoutSerializerTest.cpp
        src/deck/layout/DeckLayout.cpp
        src/deck/layout/DeckLayoutCollection.cpp
        src/deck/layout/DeckLayoutSerializer.cpp
    )
    target_include_directories(darkspark-deck-layout-serializer-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-deck-layout-serializer-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-deck-layout-serializer-test
        COMMAND darkspark-deck-layout-serializer-test
    )

    # Layout persistence service test: file-facing load/save/recover flows,
    # driven against a temp path (never the real config dir). Links Qt6::Core.
    add_executable(darkspark-layout-persistence-test
        tests/deck/layout/LayoutPersistenceServiceTest.cpp
        src/deck/layout/DeckLayout.cpp
        src/deck/layout/DeckLayoutCollection.cpp
        src/deck/layout/DeckLayoutSerializer.cpp
        src/deck/layout/LayoutPersistenceService.cpp
    )
    target_include_directories(darkspark-layout-persistence-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-layout-persistence-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-layout-persistence-test
        COMMAND darkspark-layout-persistence-test
    )

    # DeckLayoutCollection test: multi-page collection validity, unique ids,
    # activePageId, empty-page validity. Links Qt6::Core (QString); own main().
    add_executable(darkspark-deck-layout-collection-test
        tests/deck/layout/DeckLayoutCollectionTest.cpp
        src/deck/layout/DeckLayout.cpp
        src/deck/layout/DeckLayoutCollection.cpp
    )
    target_include_directories(darkspark-deck-layout-collection-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-deck-layout-collection-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-deck-layout-collection-test
        COMMAND darkspark-deck-layout-collection-test
    )

    # DeckLayoutEdits test: pure-data edit ops (find/canPlace/move/enable),
    # move preserves span+sizeMode, overlap/negative rejected. Qt-free; own main.
    add_executable(darkspark-deck-layout-edits-test
        tests/deck/layout/DeckLayoutEditsTest.cpp
        src/deck/layout/DeckLayout.cpp
        src/deck/layout/DeckLayoutEdits.cpp
    )
    target_include_directories(darkspark-deck-layout-edits-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-deck-layout-edits-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-deck-layout-edits-test
        COMMAND darkspark-deck-layout-edits-test
    )

    # CommandDeckPage edit test: Edit Mode on the real page -- pointer identity
    # preserved across move/disable/enable/cancel, Cancel restores exactly, Save
    # emits layoutCommitted with a valid layout, invalid edit not committed.
    # Links Qt6::Widgets (QWidget + event loop); own main().
    add_executable(darkspark-command-deck-page-edit-test
        tests/deck/pages/CommandDeckPageEditTest.cpp
        src/deck/pages/CommandDeckPage.cpp
        src/deck/pages/SelectionOverlay.cpp
        src/deck/layout/DeckLayout.cpp
        src/deck/layout/DeckLayoutEdits.cpp
        src/deck/layout/InstrumentFactory.cpp
        src/deck/instruments/CpuInstrument.cpp
        src/deck/instruments/CpuInstrumentLayout.cpp
        src/deck/instruments/GpuInstrument.cpp
        src/deck/instruments/MemoryInstrument.cpp
        src/deck/instruments/CoolingInstrument.cpp
        src/deck/instruments/StorageInstrument.cpp
        src/deck/instruments/NetworkInstrument.cpp
        src/deck/instruments/InstrumentRenderer.cpp
        src/deck/instruments/PersonalityMapping.cpp
        src/deck/instruments/AnimationClock.cpp
        src/themes/LegacyTheme.cpp
    )
    target_include_directories(darkspark-command-deck-page-edit-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-command-deck-page-edit-test PRIVATE
        Qt6::Widgets
        darkspark_warnings)
    add_test(
        NAME darkspark-command-deck-page-edit-test
        COMMAND darkspark-command-deck-page-edit-test
    )

    # PageManager test: page-type-agnostic hosting (addPage(QWidget*)),
    # navigation among three pages, activePageChanged. Links Qt6::Widgets
    # (QWidget + event loop); own main().
    add_executable(darkspark-page-manager-test
        tests/deck/navigation/PageManagerTest.cpp
        src/deck/navigation/PageManager.cpp
        src/deck/navigation/PageManager.hpp
        src/deck/navigation/PageIndicator.cpp
        src/deck/navigation/PageIndicator.hpp
        src/deck/animations/PageTransition.cpp
        src/deck/animations/PageTransition.hpp
        src/themes/LegacyTheme.cpp
        src/themes/LegacyTheme.hpp
    )
    target_include_directories(darkspark-page-manager-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-page-manager-test PRIVATE
        Qt6::Widgets
        darkspark_warnings)
    add_test(
        NAME darkspark-page-manager-test
        COMMAND darkspark-page-manager-test
    )

    # GPU adapter test: Qt-free, its own executable (own main()).
    add_executable(darkspark-gpu-adapter-test
        tests/deck/instruments/GpuInstrumentModelAdapterTest.cpp
        src/deck/instruments/GpuInstrumentModelAdapter.cpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-gpu-adapter-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-gpu-adapter-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-gpu-adapter-test
        COMMAND darkspark-gpu-adapter-test
    )

    # Memory adapter test: pure join/derive/clamp/state logic via injected
    # MetricSamples. Qt-free (own main()); mirrors the GPU adapter test.
    add_executable(darkspark-memory-adapter-test
        tests/deck/instruments/MemoryInstrumentModelAdapterTest.cpp
        src/deck/instruments/MemoryInstrumentModelAdapter.cpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-memory-adapter-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-memory-adapter-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-memory-adapter-test
        COMMAND darkspark-memory-adapter-test
    )

    # Cooling adapter test: role-based join/ignore/no-fabrication. Qt-free.
    add_executable(darkspark-cooling-adapter-test
        tests/deck/instruments/CoolingInstrumentModelAdapterTest.cpp
        src/deck/instruments/CoolingInstrumentModelAdapter.cpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-cooling-adapter-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-cooling-adapter-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-cooling-adapter-test
        COMMAND darkspark-cooling-adapter-test
    )

    # Cooling normalization policy test: adaptive activity fill. Qt-free.
    add_executable(darkspark-cooling-normalization-test
        tests/deck/instruments/RingNormalizationPolicyTest.cpp
    )
    target_include_directories(darkspark-cooling-normalization-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-cooling-normalization-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-cooling-normalization-test
        COMMAND darkspark-cooling-normalization-test
    )

    # Personality mapping test: pure continuous mappings -- clamping, smoothing,
    # monotonicity, warmth continuity, breathing bounds, decay on unavailable
    # telemetry, phase wrapping, determinism, and neutral-reproduces-neutral.
    # Qt-free.
    add_executable(darkspark-personality-mapping-test
        tests/deck/instruments/PersonalityMappingTest.cpp
        src/deck/instruments/PersonalityMapping.cpp
    )
    target_include_directories(darkspark-personality-mapping-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-personality-mapping-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-personality-mapping-test
        COMMAND darkspark-personality-mapping-test
    )

    # Animation clock test: subscription, one-driver ticking, hidden/unsubscribed
    # clients do not advance, pause halts, and the clock stops when no subscriber
    # wants animation. Links Qt6::Core for QTimer/QObject; own main() with a
    # QCoreApplication event loop pumped deterministically.
    add_executable(darkspark-animation-clock-test
        tests/deck/instruments/AnimationClockTest.cpp
        src/deck/instruments/AnimationClock.cpp
    )
    target_include_directories(darkspark-animation-clock-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-animation-clock-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-animation-clock-test
        COMMAND darkspark-animation-clock-test
    )

    # Cooling service test: provider aggregation + role selection + Fresh/Stale/
    # Unavailable via synthetic providers. Links Qt6::Core; the interface header
    # is listed (tab-indented) for AUTOMOC of the base meta-object.
    add_executable(darkspark-cooling-service-test
        tests/services/CoolingTelemetryServiceTest.cpp
        src/services/CoolingTelemetryService.cpp
        src/services/CoolingTelemetryService.hpp
        src/services/HwmonCoolingProvider.cpp
        src/services/HwmonCoolingProvider.hpp
        src/services/GpuDeviceSelector.cpp
        src/services/GpuDeviceSelector.hpp
	src/interfaces/ITelemetryProvider.hpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-cooling-service-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-cooling-service-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-cooling-service-test
        COMMAND darkspark-cooling-service-test
    )

    # Storage adapter test: joins all six metrics, retains temp/throughput,
    # ignores unrelated. Qt-free.
    add_executable(darkspark-storage-adapter-test
        tests/deck/instruments/StorageInstrumentModelAdapterTest.cpp
        src/deck/instruments/StorageInstrumentModelAdapter.cpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-storage-adapter-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-storage-adapter-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-storage-adapter-test
        COMMAND darkspark-storage-adapter-test
    )

    # Storage rate-calculator test: first=Unavailable, correct rate, counter
    # reset, elapsed-time validation. Qt-free.
    add_executable(darkspark-storage-ratecalc-test
        tests/services/ThroughputRateCalculatorTest.cpp
    )
    target_include_directories(darkspark-storage-ratecalc-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-storage-ratecalc-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-storage-ratecalc-test
        COMMAND darkspark-storage-ratecalc-test
    )

    # Storage service test: provider aggregation, filesystem + disk selection
    # policies, Fresh/Stale/Unavailable, rate via DiskStatsProvider. Qt6::Core;
    # the interface header is listed (tab-indented) for AUTOMOC.
    add_executable(darkspark-storage-service-test
        tests/services/StorageTelemetryServiceTest.cpp
        src/services/StorageTelemetryService.cpp
        src/services/StorageTelemetryService.hpp
        src/services/FilesystemProvider.cpp
        src/services/NVMeProvider.cpp
        src/services/DiskStatsProvider.cpp
	src/interfaces/ITelemetryProvider.hpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-storage-service-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-storage-service-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-storage-service-test
        COMMAND darkspark-storage-service-test
    )

    # Network adapter test: joins all five metrics, retains cumulative bytes +
    # link state + identity, ignores unrelated. Qt-free.
    add_executable(darkspark-network-adapter-test
        tests/deck/instruments/NetworkInstrumentModelAdapterTest.cpp
        src/deck/instruments/NetworkInstrumentModelAdapter.cpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-network-adapter-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-network-adapter-test PRIVATE
        darkspark_warnings)
    add_test(
        NAME darkspark-network-adapter-test
        COMMAND darkspark-network-adapter-test
    )

    # Network service test: interface selection policy, discovery summary +
    # explicit fallback, and RX/TX rate via NetworkInterfaceProvider (reusing
    # ThroughputRateCalculator). Qt6::Core; the interface header is listed
    # (tab-indented) for AUTOMOC.
    add_executable(darkspark-network-service-test
        tests/services/NetworkTelemetryServiceTest.cpp
        src/services/NetworkTelemetryService.cpp
        src/services/NetworkTelemetryService.hpp
        src/services/NetworkInterfaceProvider.cpp
	src/interfaces/ITelemetryProvider.hpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-network-service-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-network-service-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-network-service-test
        COMMAND darkspark-network-service-test
    )

    # GPU service test: the GPU providers derive from QObject, so this target
    # links Qt6::Core. Driven through the injected-source seams. Own main().
    add_executable(darkspark-gpu-service-test
        tests/services/GpuTelemetryServiceTest.cpp
        src/services/GpuTelemetryService.cpp
        src/services/GpuTelemetryService.hpp
        src/services/GpuThermalService.cpp
        src/services/GpuThermalService.hpp
        src/services/GpuDeviceSelector.cpp
        src/services/GpuDeviceSelector.hpp
	src/interfaces/ITelemetryProvider.hpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-gpu-service-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-gpu-service-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-gpu-service-test
        COMMAND darkspark-gpu-service-test
    )

    # GPU VRAM service test: injected-source seams (used/total readings), valid
    # parse, malformed/missing/used>total -> Unavailable, and the "gpu-vram" key
    # on emitted samples. QObject-derived, so links Qt6::Core. Own main().
    add_executable(darkspark-gpu-vram-service-test
        tests/services/GpuVramServiceTest.cpp
        src/services/GpuVramService.cpp
        src/services/GpuVramService.hpp
        src/services/GpuDeviceSelector.cpp
        src/services/GpuDeviceSelector.hpp
	src/interfaces/ITelemetryProvider.hpp
        src/models/MetricSample.cpp
    )
    target_include_directories(darkspark-gpu-vram-service-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-gpu-vram-service-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-gpu-vram-service-test
        COMMAND darkspark-gpu-vram-service-test
    )

    # GPU device-selection test: pure selection rule via injected candidate
    # lists. Links Qt6::Core (QString). Own main().
    add_executable(darkspark-gpu-selector-test
        tests/services/GpuDeviceSelectorTest.cpp
        src/services/GpuDeviceSelector.cpp
        src/services/GpuDeviceSelector.hpp
    )
    target_include_directories(darkspark-gpu-selector-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(darkspark-gpu-selector-test PRIVATE
        Qt6::Core
        darkspark_warnings)
    add_test(
        NAME darkspark-gpu-selector-test
        COMMAND darkspark-gpu-selector-test
    )
endif()
