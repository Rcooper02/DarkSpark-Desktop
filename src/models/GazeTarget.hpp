// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_MODELS_GAZETARGET_HPP
#define DARKSPARK_MODELS_GAZETARGET_HPP

#include <algorithm>

namespace darkspark::models {

/// Normalized location of a tracked subject within a camera frame.
/// Both axes use -1.0..1.0, with (0, 0) at frame center.
struct GazeTarget {
    double horizontal = 0.0;
    double vertical = 0.0;

    [[nodiscard]] constexpr GazeTarget clamped() const {
        return {std::clamp(horizontal, -1.0, 1.0),
                std::clamp(vertical, -1.0, 1.0)};
    }

    bool operator==(const GazeTarget&) const = default;
};

}  // namespace darkspark::models

#endif  // DARKSPARK_MODELS_GAZETARGET_HPP
