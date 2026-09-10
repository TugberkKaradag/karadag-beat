#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Envelope.h"

/**
    Duzenlenebilir zarf editoru - Gross Beat'in cizim alaninin karsiligi.

    Fare:
      sol tik (bosluk)    -> yeni nokta ekle ve suruklemeye basla
      sol tik (nokta)     -> noktayi surukle   (Shift = snap kapali)
      sag tik (nokta)     -> noktayi sil
      cift tik (nokta)    -> basamak / egri modunu degistir
      tutamak (segment)   -> surukleyince egri bukulur, cift / sag tik sifirlar
      tekerlek (segment)  -> segmentin egrisini (tension) bukur
      Alt + surukle       -> cizim modu: gecilen her izgara hucresine basamak boyar
*/
class EnvelopeEditor  : public juce::Component
{
public:
    EnvelopeEditor (Envelope& envelopeToEdit, bool isVolumeStyle);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseMove        (const juce::MouseEvent&) override;
    void mouseExit        (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Zarf her degistiginde cagrilir (ses thread'ine yayin icin). */
    std::function<void()> onChange;

    /** Bir duzenleme hareketi baslamadan HEMEN ONCE cagrilir - geri alma
        yiginina o anki durumun kaydedilmesi icin. Surukleme boyunca degil,
        hareket basina bir kez tetiklenir. */
    std::function<void()> onEditBegin;

    void setPlayheadPhase (double phase);
    void setGridDivisions (int divisionsPerPattern);
    int  getGridDivisions() const noexcept { return divisions; }
    void setAccentColour (juce::Colour c)  { accent = c; repaint(); }

    /** Ustte bar/vurus numaralarini gosteren cetvel seridi. */
    void setShowRuler (bool shouldShow)    { showRuler = shouldShow; resized(); repaint(); }

    /** Pattern kac bar surer - cetvel ve dikey olcek buna gore etiketlenir. */
    void setPatternBars (int bars);

    /** Arkaya cizilecek dalga formu: pattern boyunca esit dilimlerde tepe degerleri. */
    void setWaveform (const std::vector<float>& peaks)  { waveform = peaks; }

    /** Cizim modu: sol tikla surukleme nokta eklemek yerine basamak boyar.
        Kapaliyken de Alt basili tutarak gecici olarak kullanilabilir. */
    void setDrawMode (bool shouldDraw)     { drawMode = shouldDraw; repaint(); }

    /** Disaridan (preset yuklemesi gibi) zarf degistiginde cagrilir. */
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

    // --- tension tutamaklari ---
    struct SegmentSpan { double x0, y0, x1, y1; bool stepped; };
    SegmentSpan getSegmentSpan (int index) const;
    bool handleVisible (int index) const;
    juce::Point<float> handlePosition (int index) const;
    int  findHandleAt (juce::Point<float> pos) const;
    double tensionForMidpoint (int index, double value) const;

    // --- cizim modu ---
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
    const bool volumeStyle;

    juce::Rectangle<float> plot, scaleColumn, rulerRow;
    juce::Colour accent { 0xff35d0c8 };
    bool showRuler = false;
    int  patternBars = 2;

    int    divisions   = 32;      // 2 bar boyunca kac adim (32 = 1/16'lik)
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
