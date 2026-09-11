#pragma once

#include <cmath>

namespace FilterMap
{
    inline double cutoffHz (double laneValue, bool highPass) noexcept
    {
        const double v = laneValue < 0.0 ? 0.0 : (laneValue > 1.0 ? 1.0 : laneValue);
        const double closed = 1.0 - v;

        return highPass ? 20.0    * std::pow (12000.0 / 20.0, closed)
                        : 20000.0 * std::pow (40.0 / 20000.0, closed);
    }

    inline double roundedForLabel (double hz) noexcept
    {
        if (hz >= 1000.0)
            return std::round (hz / 100.0) / 10.0;

        return std::round (hz);
    }
}
