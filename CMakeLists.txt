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
    src/desktop/DesktopWindow.cpp
    src/desktop/DesktopWindow.hpp
    src/deck/DeckWindow.cpp
    src/deck/DeckWindow.hpp
    src/deck/pages/DeckPage.cpp
    src/deck/pages/DeckPage.hpp
    src/deck/cards/DashboardCard.cpp
    src/deck/cards/DashboardCard.hpp
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
