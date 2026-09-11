#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Envelope.h"

class EnvelopeEditor  : public juce::Component
{
public:
    enum class Style { time, volume, filter };

    EnvelopeEditor (Envelope& envelopeToEdit, Style laneStyle);
    EnvelopeEditor (Envelope& envelopeToEdit, bool isVolumeStyle)
        : EnvelopeEditor (envelopeToEdit, isVolumeStyle ? Style::volume : Style::time) {}

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseMove        (const juce::MouseEvent&) override;
    void mouseExit        (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    std::function<void()> onChange;

    std::function<void()> onEditBegin;

    void setPlayheadPhase (double phase);
    void setGridDivisions (int divisionsPerPattern);
    int  getGridDivisions() const noexcept { return divisions; }
    void setAccentColour (juce::Colour c)  { accent = c; repaint(); }

    void setShowRuler (bool shouldShow)    { showRuler = shouldShow; resized(); repaint(); }

    void setPatternBars (int bars);

    void setFilterHighPass (bool highPass);

    void setWaveform (const std::vector<float>& peaks)  { waveform = peaks; }

    void setDrawMode (bool shouldDraw)     { drawMode = shouldDraw; repaint(); }

    void envelopeChangedExternally()
    {
        hoverPoint = dragPoint = hoverHandle = dragHandle = -1;
        repaint();
    }

private:
    float  xToPixel (double x) const noexcept;
    float  yToPixel (double y) const noexcept;
    double pixelToX (float px) const noexcept;
    double pixelToY (float py) const noexcept;

    double snapX (double x) const noexcept;
    double snapY (double y) const noexcept;

    void showContextMenu();

    int findPointAt (juce::Point<float> pos) const;
    int findSegmentAt (double phase) const;

    struct SegmentSpan { double x0, y0, x1, y1; bool stepped; };
    SegmentSpan getSegmentSpan (int index) const;
    bool handleVisible (int index) const;
    juce::Point<float> handlePosition (int index) const;
    int  findHandleAt (juce::Point<float> pos) const;
    double tensionForMidpoint (int index, double value) const;

    int  cellAt (double x) const noexcept;
    void paintCell (int cell, double y);
    void paintAlong (const juce::MouseEvent&);

    void drawGrid (juce::Graphics&) const;
    void drawCurve (juce::Graphics&) const;
    void drawPoints (juce::Graphics&) const;
    void drawHandles (juce::Graphics&) const;
    void drawWaveform (juce::Graphics&) const;
    void drawScale (juce::Graphics&) const;
    void drawRuler (juce::Graphics&) const;

    Envelope& env;
    const Style style;
    const bool topIsOne;
    bool filterHighPass = false;

    juce::Rectangle<float> plot, scaleColumn, rulerRow;
    juce::Colour accent { 0xff35d0c8 };
    bool showRuler = false;
    int  patternBars = 2;

    int    divisions   = 32;
    bool   snapEnabled = true;
    double playhead    = 0.0;

    int dragPoint   = -1;
    int hoverPoint  = -1;
    int dragHandle  = -1;
    int hoverHandle = -1;

    std::vector<float> waveform;

    bool drawMode      = false;
    bool painting      = false;
    int  lastPaintCell = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeEditor)
};
