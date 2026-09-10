#include "Envelope.h"
#include <algorithm>
#include <cmath>

namespace
{
    inline double clamp01 (double v) noexcept
    {
        return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
    }

    /** Segment ilerlemesine (0..1) egri bukumu uygular.
        tension  0 -> duz cizgi
        tension >0 -> hizli baslar, yavas biter (ease-out)
        tension <0 -> yavas baslar, hizli biter (ease-in)        */
    inline double applyTension (double t, double tension) noexcept
    {
        t = clamp01 (t);

        if (std::abs (tension) < 1.0e-9)
            return t;

        // tension = +-1 -> us 1/16 .. 16
        const double k = std::pow (2.0, -tension * 4.0);
        return std::pow (t, k);
    }
}

Envelope::Envelope (double defaultValue)
    : fallback (defaultValue)
{
    clearTo (defaultValue);
}

void Envelope::clearTo (double value)
{
    fallback = value;
    points.clear();
    points.emplace_back (0.0, value, 0.0, false);
}

int Envelope::findSegment (double phase) const noexcept
{
    // Son noktasi phase'e esit ya da kucuk olan nokta: upper_bound - 1.
    // Ayni x'te iki nokta varsa (ani sicrama) sonuncusu secilir.
    const auto it = std::upper_bound (points.begin(), points.end(), phase,
                                      [] (double v, const EnvPoint& p) { return v < p.x; });

    return (int) (it - points.begin()) - 1;
}

bool Envelope::segmentContains (int index, double phase) const noexcept
{
    const int n = (int) points.size();

    if (index == -1)
        return phase < points.front().x;

    if (index < 0 || index >= n || points[(size_t) index].x > phase)
        return false;

    return index == n - 1 || points[(size_t) index + 1].x > phase;
}

double Envelope::evaluateSegment (int index, double phase) const noexcept
{
    const EnvPoint* p0 = nullptr;
    const EnvPoint* p1 = nullptr;
    double x0 = 0.0, x1 = 0.0;

    if (index < 0)
    {
        // pattern'in basindayiz ama ilk nokta ileride:
        // son noktadan ilk noktaya sarilan segmentin icindeyiz
        p0 = &points.back();
        x0 = p0->x - 1.0;
        p1 = &points.front();
        x1 = p1->x;
    }
    else
    {
        p0 = &points[(size_t) index];
        x0 = p0->x;

        if ((size_t) index + 1 < points.size())
        {
            p1 = &points[(size_t) index + 1];
            x1 = p1->x;
        }
        else
        {
            p1 = &points.front();
            x1 = p1->x + 1.0;
        }
    }

    if (p0->stepped)
        return p0->y;

    const double span = x1 - x0;

    if (span <= 1.0e-12)
        return p1->y;

    const double t = applyTension ((phase - x0) / span, p0->tension);
    return p0->y + (p1->y - p0->y) * t;
}

double Envelope::valueAt (double phase) const noexcept
{
    if (points.empty())
        return fallback;

    if (points.size() == 1)
        return points[0].y;

    phase -= std::floor (phase);
    return evaluateSegment (findSegment (phase), phase);
}

double Envelope::valueAt (double phase, int& segmentHint) const noexcept
{
    if (points.empty())
        return fallback;

    if (points.size() == 1)
        return points[0].y;

    phase -= std::floor (phase);

    const int n = (int) points.size();

    if (! segmentContains (segmentHint, phase))
    {
        // En sik durum: faz bir sonraki segmente gecti
        const int next = (segmentHint + 1 >= n) ? -1 : segmentHint + 1;

        segmentHint = segmentContains (next, phase) ? next : findSegment (phase);
    }

    return evaluateSegment (segmentHint, phase);
}

void Envelope::flipVertical()
{
    for (auto& p : points)
        p.y = 1.0 - p.y;
}

void Envelope::paintStep (double start, double end, double y)
{
    constexpr double eps = 1.0e-9;

    start = clamp01 (start);
    end   = juce::jlimit (start, 1.0, end);
    y     = clamp01 (y);

    // --- hucreden ONCE: gelen segment bozulmasin ---
    // Yeni nokta tek basina konsaydi, onceki segment boyanan degere dogru
    // bukulurdu (duz cizgi rampaya donerdi).  Hucre basina eski degeri tasiyan
    // bir nokta koyup boyanan noktayla ayni x'te bir sicrama yapiyoruz.
    int ownerIndex = -1;

    for (int i = 0; i < (int) points.size(); ++i)
        if (points[(size_t) i].x < start - eps)
            ownerIndex = i;

    const bool incomingStepped = (ownerIndex >= 0 ? points[(size_t) ownerIndex]
                                                  : points.back()).stepped;

    double preValue = 0.0;
    bool   pointAtStart = false;

    for (const auto& p : points)
    {
        if (std::abs (p.x - start) < eps)
        {
            preValue = p.y;          // ayni x'teki ciftin ilki = gelen segmentin sonu
            pointAtStart = true;
            break;
        }
    }

    if (! pointAtStart)
        preValue = valueAt (start);

    const bool needsPre = ! incomingStepped && std::abs (preValue - y) > eps;

    // --- hucreden SONRA: eski sekil devam etsin ---
    // Sonda nokta yoksa, oradaki mevcut degeri ve o segmentin karakterini
    // tasiyan bir nokta ekle.
    bool needsRestore = false;
    EnvPoint restore;

    if (end < 1.0 - eps)
    {
        bool hasPointAtEnd = false;

        for (const auto& p : points)
            if (std::abs (p.x - end) < eps)
                hasPointAtEnd = true;

        if (! hasPointAtEnd)
        {
            const int seg = findSegment (end);
            const auto& owner = seg >= 0 ? points[(size_t) seg] : points.back();

            restore = EnvPoint (end, valueAt (end), owner.tension, owner.stepped);
            needsRestore = true;
        }
    }

    // hucrenin icindeki (baslangic dahil, bitis haric) noktalari kaldir
    points.erase (std::remove_if (points.begin(), points.end(),
                                  [&] (const EnvPoint& p) { return p.x > start - eps && p.x < end - eps; }),
                  points.end());

    if (needsPre)
        points.emplace_back (start, preValue, 0.0, false);   // siralamada boyanandan once kalir

    points.emplace_back (start, y, 0.0, true);

    if (needsRestore)
        points.push_back (restore);

    sortPoints();
}

void Envelope::shift (double dx)
{
    dx -= std::floor (dx);

    if (dx < 1.0e-12 || dx > 1.0 - 1.0e-12 || points.empty())
        return;

    struct Item { EnvPoint point; double unwrapped; };
    std::vector<Item> items;
    items.reserve (points.size());

    for (const auto& p : points)
    {
        const double u = p.x + dx;
        EnvPoint q = p;

        // Ince bir izgaraya oturt: 1/24 gibi ikilik tabanda tam yazilamayan
        // adimlarda, ayni yere dusmesi gereken iki nokta 1e-17 farkla ayrilip
        // siralamayi bozmasin.
        q.x = std::round ((u - std::floor (u)) * 1.0e9) / 1.0e9;

        if (q.x >= 1.0)
            q.x = 0.0;

        items.push_back ({ q, u });
    }

    // Ayni x'e dusen iki noktadan, kaydirmadan once pattern'de daha GEC olan
    // (bir onceki turun sonu) once gelmeli - sicramanin yonu boyle korunur.
    std::stable_sort (items.begin(), items.end(), [] (const Item& a, const Item& b)
    {
        if (a.point.x != b.point.x)
            return a.point.x < b.point.x;

        return a.unwrapped > b.unwrapped;
    });

    for (size_t i = 0; i < points.size(); ++i)
        points[i] = items[i].point;
}

bool Envelope::isFlatAt (double value) const noexcept
{
    if (points.empty())
        return std::abs (fallback - value) < 1.0e-9;

    for (const auto& p : points)
        if (std::abs (p.y - value) > 1.0e-9)
            return false;

    return true;
}

void Envelope::sortPoints()
{
    std::stable_sort (points.begin(), points.end(),
                      [] (const EnvPoint& a, const EnvPoint& b) { return a.x < b.x; });
}

int Envelope::addPoint (double x, double y, double tension, bool stepped)
{
    x = clamp01 (x);
    y = clamp01 (y);

    points.emplace_back (x, y, tension, stepped);
    sortPoints();

    for (size_t i = 0; i < points.size(); ++i)
        if (points[i].x == x && points[i].y == y)
            return (int) i;

    return (int) points.size() - 1;
}

void Envelope::removePoint (int index)
{
    // en az bir nokta her zaman kalsin
    if (points.size() <= 1)
        return;

    if (juce::isPositiveAndBelow (index, (int) points.size()))
        points.erase (points.begin() + index);
}

void Envelope::movePoint (int index, double newX, double newY)
{
    if (! juce::isPositiveAndBelow (index, (int) points.size()))
        return;

    // siralamayi bozmamak icin komsularla sinirla - boylece GUI'de
    // surukleme sirasinda noktanin index'i degismez
    const double lower = (index > 0) ? points[(size_t) index - 1].x : 0.0;
    const double upper = (index + 1 < (int) points.size()) ? points[(size_t) index + 1].x : 1.0;

    points[(size_t) index].x = juce::jlimit (lower, upper, clamp01 (newX));
    points[(size_t) index].y = clamp01 (newY);
}

void Envelope::setTension (int index, double tension)
{
    if (juce::isPositiveAndBelow (index, (int) points.size()))
        points[(size_t) index].tension = juce::jlimit (-1.0, 1.0, tension);
}

void Envelope::setStepped (int index, bool stepped)
{
    if (juce::isPositiveAndBelow (index, (int) points.size()))
        points[(size_t) index].stepped = stepped;
}

void Envelope::finalisePoints()
{
    if (points.empty())
    {
        points.emplace_back (0.0, fallback, 0.0, false);
        return;
    }

    for (auto& p : points)
    {
        p.x       = clamp01 (p.x);
        p.y       = clamp01 (p.y);
        p.tension = juce::jlimit (-1.0, 1.0, p.tension);
    }

    sortPoints();
}

void Envelope::setPoints (const std::vector<EnvPoint>& newPoints)
{
    // assign, mevcut kapasite yeterliyse bellek ayirmaz -
    // bu yol ses thread'inden de guvenle cagrilabilsin diye onemli
    points.assign (newPoints.begin(), newPoints.end());
    finalisePoints();
}

void Envelope::setPoints (std::vector<EnvPoint>&& newPoints)
{
    points = std::move (newPoints);
    finalisePoints();
}

juce::String Envelope::toString() const
{
    juce::StringArray parts;

    for (const auto& p : points)
        parts.add (juce::String (p.x, 6) + "," + juce::String (p.y, 6) + ","
                 + juce::String (p.tension, 4) + "," + (p.stepped ? "1" : "0"));

    return parts.joinIntoString (";");
}

bool Envelope::fromString (const juce::String& text)
{
    juce::StringArray parts;
    parts.addTokens (text, ";", "");
    parts.removeEmptyStrings();

    if (parts.isEmpty())
        return false;

    std::vector<EnvPoint> parsed;
    parsed.reserve ((size_t) parts.size());

    for (const auto& part : parts)
    {
        juce::StringArray f;
        f.addTokens (part, ",", "");

        if (f.size() < 4)
            return false;

        parsed.emplace_back (f[0].getDoubleValue(),
                             f[1].getDoubleValue(),
                             f[2].getDoubleValue(),
                             f[3].getIntValue() != 0);
    }

    setPoints (std::move (parsed));
    return true;
}
