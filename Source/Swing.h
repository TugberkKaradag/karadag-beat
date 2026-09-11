#pragma once

#include <vector>
#include "Envelope.h"

namespace Swing
{
    constexpr double kStraight = 0.5;
    constexpr double kMax      = 0.75;

    double patternToReal (double u, int cells, double amount) noexcept;

    double realToPattern (double t, int cells, double amount) noexcept;

    bool isActive (int cells, double amount) noexcept;

    void apply (const std::vector<EnvPoint>& src, std::vector<EnvPoint>& dst,
                int cells, double amount, bool isTimeLane);
}
