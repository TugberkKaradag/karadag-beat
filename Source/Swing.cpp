#include "Swing.h"
#include <cmath>

namespace
{
    double clampAmount (double amount) noexcept
    {
        return amount < Swing::kStraight ? Swing::kStraight
                                         : (amount > Swing::kMax ? Swing::kMax : amount);
    }

    double patternToRealWrapped (double u, int cells, double amount) noexcept
    {
        const double whole = std::floor (u);
        return whole + Swing::patternToReal (u - whole, cells, amount);
    }
}

bool Swing::isActive (int cells, double amount) noexcept
{
    return cells >= 2 && clampAmount (amount) > kStraight + 1.0e-6;
}

double Swing::patternToReal (double u, int cells, double amount) noexcept
{
    if (! isActive (cells, amount) || u <= 0.0 || u >= 1.0)
        return u;

    const double a    = clampAmount (amount);
    const double pair = 2.0 / (double) cells;
    const double k    = std::floor (u / pair);

    if ((k + 1.0) * pair > 1.0 + 1.0e-12)
        return u;

    const double f = (u - k * pair) / pair;
    const double r = f < 0.5 ? (f / 0.5) * a
                             : a + ((f - 0.5) / 0.5) * (1.0 - a);

    return (k + r) * pair;
}

double Swing::realToPattern (double t, int cells, double amount) noexcept
{
    if (! isActive (cells, amount) || t <= 0.0 || t >= 1.0)
        return t;

    const double a    = clampAmount (amount);
    const double pair = 2.0 / (double) cells;
    const double k    = std::floor (t / pair);

    if ((k + 1.0) * pair > 1.0 + 1.0e-12)
        return t;

    const double r = (t - k * pair) / pair;
    const double f = r < a ? 0.5 * (r / a)
                           : 0.5 + 0.5 * ((r - a) / (1.0 - a));

    return (k + f) * pair;
}

void Swing::apply (const std::vector<EnvPoint>& src, std::vector<EnvPoint>& dst,
                   int cells, double amount, bool isTimeLane)
{
    dst.assign (src.begin(), src.end());

    if (! isActive (cells, amount))
        return;

    for (auto& p : dst)
    {
        const double x = p.x;
        const double xReal = patternToReal (x, cells, amount);

        if (isTimeLane)
        {
            const double sourceReal = patternToRealWrapped (x - p.y, cells, amount);
            const double yReal = xReal - sourceReal;
            p.y = yReal < 0.0 ? 0.0 : (yReal > 1.0 ? 1.0 : yReal);
        }

        p.x = xReal;
    }
}
