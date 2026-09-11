#pragma once

#include <juce_core/juce_core.h>
#include <vector>

struct EnvPoint
{
    double x       = 0.0;
    double y       = 0.0;
    double tension = 0.0;
    bool   stepped = false;

    EnvPoint() = default;
    EnvPoint (double xx, double yy, double t = 0.0, bool step = false)
        : x (xx), y (yy), tension (t), stepped (step) {}
};

class Envelope
{
public:
    explicit Envelope (double defaultValue = 0.0);

    double valueAt (double phase) const noexcept;

    double valueAt (double phase, int& segmentHint) const noexcept;

    bool isFlatAt (double value) const noexcept;

    void  clearTo (double value);

    void  resetToDefault()                           { clearTo (fallback); }

    void  flipVertical();

    void  paintStep (double start, double end, double y);

    void  shift (double dx);
    int   addPoint (double x, double y, double tension = 0.0, bool stepped = false);
    void  removePoint (int index);
    void  movePoint (int index, double newX, double newY);
    void  setTension (int index, double tension);
    void  setStepped (int index, bool stepped);

    int   getNumPoints() const noexcept              { return (int) points.size(); }
    const EnvPoint& getPoint (int i) const noexcept  { return points[(size_t) i]; }
    const std::vector<EnvPoint>& getPoints() const noexcept { return points; }
    void  reserve (int numPoints)                    { points.reserve ((size_t) numPoints); }

    void  setPoints (const std::vector<EnvPoint>& newPoints);
    void  setPoints (std::vector<EnvPoint>&& newPoints);

    juce::String toString() const;
    bool fromString (const juce::String& text);

private:
    int findSegment (double wrappedPhase) const noexcept;
    bool segmentContains (int index, double wrappedPhase) const noexcept;
    double evaluateSegment (int index, double wrappedPhase) const noexcept;

    void sortPoints();
    void finalisePoints();

    std::vector<EnvPoint> points;
    double fallback = 0.0;
};
