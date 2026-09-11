#include "EnvelopeEditor.h"
#include "FilterMap.h"
#include "Theme.h"
#include <cmath>

namespace
{
    /** 0.75 -> "3/4",  1.5 -> "1 1/2",  2 -> "2" */
    juce::String barsToText (double bars)
    {
        const int quarters = juce::roundToInt (bars * 4.0);
        const int whole    = quarters / 4;
        const int rest     = quarters % 4;

        static const char* fractions[] = { "", "1/4", "1/2", "3/4" };

        if (rest == 0)  return juce::String (whole);
        if (whole == 0) return fractions[rest];

        return juce::String (whole) + " " + fractions[rest];
    }

    /** 12000 -> "12k",  2400 -> "2.4k",  380 -> "380" */
    juce::String hzToText (double hz)
    {
        if (hz >= 1000.0)
        {
            const double k = hz / 1000.0;
            return (k >= 10.0 ? juce::String (juce::roundToInt (k))
                              : juce::String (k, 1)) + "k";
        }

        return juce::String (juce::roundToInt (hz));
    }

    /** Arayuz cizgileri ve yazilari: paletin yazi rengi, verilen saydamlikta. */
    juce::Colour ink (float alpha)
    {
        return Themes::current().text.withAlpha (alpha);
    }

    // Iki zarf editoru arasinda paylasilan pano - time'dan kopyalayip
    // volume'a yapistirmak da mumkun, ilginc sonuclar veriyor.
    std::vector<EnvPoint> envelopeClipboard;

    constexpr float kPointRadius   = 5.0f;
    constexpr float kHitRadius     = 10.0f;
    constexpr float kPlotInset     = 2.0f;
}

EnvelopeEditor::EnvelopeEditor (Envelope& envelopeToEdit, Style laneStyle)
    : env (envelopeToEdit), style (laneStyle), topIsOne (laneStyle != Style::time)
{
    setOpaque (true);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);

    // Klavye odagi ana pencerede kalsin ki Ctrl+Z her zaman calissin
    setMouseClickGrabsKeyboardFocus (false);
}

void EnvelopeEditor::resized()
{
    auto area = getLocalBounds().toFloat().reduced (kPlotInset);

    scaleColumn = area.removeFromLeft (34.0f);
    rulerRow    = showRuler ? area.removeFromTop (14.0f) : juce::Rectangle<float>();
    plot        = area;
}

//==============================================================================
float EnvelopeEditor::xToPixel (double x) const noexcept
{
    return plot.getX() + (float) x * plot.getWidth();
}

float EnvelopeEditor::yToPixel (double y) const noexcept
{
    const double v = topIsOne ? (1.0 - y) : y;
    return plot.getY() + (float) v * plot.getHeight();
}

double EnvelopeEditor::pixelToX (float px) const noexcept
{
    if (plot.getWidth() <= 0.0f)
        return 0.0;

    return juce::jlimit (0.0, 1.0, (double) ((px - plot.getX()) / plot.getWidth()));
}

double EnvelopeEditor::pixelToY (float py) const noexcept
{
    if (plot.getHeight() <= 0.0f)
        return 0.0;

    const double v = juce::jlimit (0.0, 1.0, (double) ((py - plot.getY()) / plot.getHeight()));
    return topIsOne ? (1.0 - v) : v;
}

double EnvelopeEditor::snapX (double x) const noexcept
{
    if (! snapEnabled || divisions <= 0)
        return x;

    return juce::jlimit (0.0, 1.0, std::round (x * divisions) / (double) divisions);
}

double EnvelopeEditor::snapY (double y) const noexcept
{
    if (! snapEnabled)
        return y;

    // Volume: 1/16'lik kademeler.  Filtre: 24 kademe (~yarim oktav).
    // Time: 1/32 pattern = 1/16'lik nota kadar geri.
    const double steps = style == Style::volume ? 16.0
                       : style == Style::filter ? 24.0
                                                : 32.0;
    return juce::jlimit (0.0, 1.0, std::round (y * steps) / steps);
}

//==============================================================================
int EnvelopeEditor::findPointAt (juce::Point<float> pos) const
{
    int best = -1;
    float bestDist = kHitRadius;

    for (int i = 0; i < env.getNumPoints(); ++i)
    {
        const auto& p = env.getPoint (i);
        const juce::Point<float> screen { xToPixel (p.x), yToPixel (p.y) };
        const float dist = screen.getDistanceFrom (pos);

        if (dist <= bestDist)
        {
            bestDist = dist;
            best = i;
        }
    }

    return best;
}

int EnvelopeEditor::findSegmentAt (double phase) const
{
    const int n = env.getNumPoints();

    if (n == 0)
        return -1;

    int result = n - 1;   // varsayilan: sarilan son segment

    for (int i = 0; i < n; ++i)
        if (env.getPoint (i).x <= phase)
            result = i;

    return result;
}

//==============================================================================
EnvelopeEditor::SegmentSpan EnvelopeEditor::getSegmentSpan (int index) const
{
    const int n = env.getNumPoints();
    const auto& p0 = env.getPoint (index);

    if (index + 1 < n)
    {
        const auto& p1 = env.getPoint (index + 1);
        return { p0.x, p0.y, p1.x, p1.y, p0.stepped };
    }

    const auto& p1 = env.getPoint (0);
    return { p0.x, p0.y, p1.x + 1.0, p1.y, p0.stepped };
}

bool EnvelopeEditor::handleVisible (int index) const
{
    if (! juce::isPositiveAndBelow (index, env.getNumPoints()) || env.getNumPoints() < 2)
        return false;

    const auto s = getSegmentSpan (index);

    // basamakta, duz segmentte ya da dar segmentte bukulecek bir sey yok
    return ! s.stepped
        && std::abs (s.y1 - s.y0) > 1.0e-6
        && (s.x1 - s.x0) * plot.getWidth() > 26.0f;
}

juce::Point<float> EnvelopeEditor::handlePosition (int index) const
{
    const auto s = getSegmentSpan (index);

    double mid = 0.5 * (s.x0 + s.x1);
    mid -= std::floor (mid);

    return { xToPixel (mid), yToPixel (env.valueAt (mid)) };
}

int EnvelopeEditor::findHandleAt (juce::Point<float> pos) const
{
    for (int i = 0; i < env.getNumPoints(); ++i)
        if (handleVisible (i) && handlePosition (i).getDistanceFrom (pos) <= 8.0f)
            return i;

    return -1;
}

double EnvelopeEditor::tensionForMidpoint (int index, double value) const
{
    // Egri t^k, k = 2^(-4*tension).  Segmentin ortasi (t = 0.5) degeri
    // y0 + (y1 - y0) * 0.5^k.  Farenin gosterdigi degere ulasmak icin k'yi
    // cozuyoruz - boylece tutamak fareyi birebir takip ediyor.
    const auto s = getSegmentSpan (index);

    double f = (value - s.y0) / (s.y1 - s.y0);
    f = juce::jlimit (2.0e-5, 0.957, f);

    const double k = std::log (f) / std::log (0.5);
    return juce::jlimit (-1.0, 1.0, -std::log2 (k) / 4.0);
}

int EnvelopeEditor::cellAt (double x) const noexcept
{
    const int div = juce::jmax (1, divisions);
    return juce::jlimit (0, div - 1, (int) std::floor (x * div));
}

void EnvelopeEditor::paintCell (int cell, double y)
{
    const double div = (double) juce::jmax (1, divisions);
    env.paintStep (cell / div, (cell + 1) / div, y);
}

void EnvelopeEditor::paintAlong (const juce::MouseEvent& e)
{
    const int cell = cellAt (pixelToX (e.position.x));
    const double y = snapY (pixelToY (e.position.y));

    // hizli surukleme hucre atlamasin: aradaki butun hucreleri de boya
    const int from = (lastPaintCell < 0) ? cell : lastPaintCell;
    const int lo = juce::jmin (from, cell), hi = juce::jmax (from, cell);

    for (int c = lo; c <= hi; ++c)
        paintCell (c, y);

    lastPaintCell = cell;

    if (onChange)
        onChange();

    repaint();
}

//==============================================================================
void EnvelopeEditor::setPlayheadPhase (double phase)
{
    phase -= std::floor (phase);

    if (std::abs (phase - playhead) > 1.0e-4)
    {
        playhead = phase;
        repaint();
    }
}

void EnvelopeEditor::setPatternBars (int bars)
{
    const int clamped = juce::jlimit (1, 4, bars);

    if (clamped != patternBars)
    {
        patternBars = clamped;
        repaint();
    }
}

void EnvelopeEditor::setFilterHighPass (bool highPass)
{
    if (highPass != filterHighPass)
    {
        filterHighPass = highPass;
        repaint();
    }
}

void EnvelopeEditor::setGridDivisions (int divisionsPerPattern)
{
    divisions = juce::jmax (1, divisionsPerPattern);
    repaint();
}

//==============================================================================
void EnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    snapEnabled = ! e.mods.isShiftDown();

    const auto pos = e.position;
    int index = findPointAt (pos);

    // --- cizim modu ---
    if (! e.mods.isRightButtonDown() && (drawMode || e.mods.isAltDown()))
    {
        if (onEditBegin)
            onEditBegin();

        painting = true;
        lastPaintCell = -1;
        paintAlong (e);
        return;
    }

    const int handle = (index < 0) ? findHandleAt (pos) : -1;

    if (e.mods.isRightButtonDown() && handle >= 0)
    {
        if (onEditBegin)
            onEditBegin();

        env.setTension (handle, 0.0);

        if (onChange)
            onChange();

        repaint();
        return;
    }

    if (handle >= 0)
    {
        if (onEditBegin)
            onEditBegin();

        dragHandle = handle;
        repaint();
        return;
    }

    if (e.mods.isRightButtonDown())
    {
        if (index >= 0)
        {
            if (onEditBegin)
                onEditBegin();

            env.removePoint (index);
            hoverPoint = -1;

            if (onChange)
                onChange();

            repaint();
        }
        else
        {
            showContextMenu();
        }

        return;
    }

    if (onEditBegin)
        onEditBegin();

    if (index < 0)
    {
        // bosluga tiklandi: yeni nokta ekle ve hemen suruklemeye basla.
        // yeni nokta, tiklanan yerdeki segmentin seklini devralsin
        const double x = snapX (pixelToX (pos.x));
        const double y = snapY (pixelToY (pos.y));

        const int seg = findSegmentAt (x);
        const double tension = (seg >= 0) ? env.getPoint (seg).tension : 0.0;
        const bool stepped   = (seg >= 0) ? env.getPoint (seg).stepped : false;

        index = env.addPoint (x, y, tension, stepped);
    }

    dragPoint  = index;
    hoverPoint = index;

    if (onChange)
        onChange();

    repaint();
}

void EnvelopeEditor::showContextMenu()
{
    juce::PopupMenu menu;

    // Menu kendi bakisini component'ten devralmiyor, elle vermek gerekiyor
    menu.setLookAndFeel (&getLookAndFeel());

    menu.addSectionHeader (style == Style::volume ? "Volume envelope"
                         : style == Style::filter ? "Filter envelope"
                                                  : "Time envelope");
    menu.addItem (1, "Reset");
    menu.addItem (2, "Flip vertically");
    menu.addSeparator();
    menu.addItem (3, "Copy");
    menu.addItem (4, "Paste", ! envelopeClipboard.empty());

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
        [this] (int result)
        {
            if (result == 0)
                return;

            // Kopyalama zarfi degistirmiyor, geri alma adimi da gerekmiyor
            if (result == 3)
            {
                envelopeClipboard = env.getPoints();
                return;
            }

            if (onEditBegin)
                onEditBegin();

            if      (result == 1) env.resetToDefault();
            else if (result == 2) env.flipVertical();
            else if (result == 4 && ! envelopeClipboard.empty())
                                  env.setPoints (envelopeClipboard);

            hoverPoint = -1;
            dragPoint  = -1;

            if (onChange)
                onChange();

            repaint();
        });
}

void EnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (painting)
    {
        snapEnabled = ! e.mods.isShiftDown();
        paintAlong (e);
        return;
    }

    if (dragHandle >= 0)
    {
        env.setTension (dragHandle, tensionForMidpoint (dragHandle, pixelToY (e.position.y)));

        if (onChange)
            onChange();

        repaint();
        return;
    }

    if (dragPoint < 0)
        return;

    snapEnabled = ! e.mods.isShiftDown();

    const double x = snapX (pixelToX (e.position.x));
    const double y = snapY (pixelToY (e.position.y));

    env.movePoint (dragPoint, x, y);

    if (onChange)
        onChange();

    repaint();
}

void EnvelopeEditor::mouseUp (const juce::MouseEvent&)
{
    painting      = false;
    lastPaintCell = -1;
    dragHandle    = -1;
    dragPoint = -1;
    snapEnabled = true;
    repaint();
}

void EnvelopeEditor::mouseMove (const juce::MouseEvent& e)
{
    const int index  = findPointAt (e.position);
    const int handle = (index < 0) ? findHandleAt (e.position) : -1;

    if (index != hoverPoint || handle != hoverHandle)
    {
        hoverPoint  = index;
        hoverHandle = handle;
        repaint();
    }
}

void EnvelopeEditor::mouseExit (const juce::MouseEvent&)
{
    if (hoverPoint != -1 || hoverHandle != -1)
    {
        hoverPoint = hoverHandle = -1;
        repaint();
    }
}

void EnvelopeEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (drawMode || e.mods.isAltDown())
        return;

    const int index = findPointAt (e.position);

    if (index < 0)
    {
        const int handle = findHandleAt (e.position);

        if (handle >= 0)
        {
            if (onEditBegin)
                onEditBegin();

            env.setTension (handle, 0.0);

            if (onChange)
                onChange();

            repaint();
        }

        return;
    }

    if (onEditBegin)
        onEditBegin();

    env.setStepped (index, ! env.getPoint (index).stepped);

    if (onChange)
        onChange();

    repaint();
}

void EnvelopeEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int seg = findSegmentAt (pixelToX (e.position.x));

    if (seg < 0 || env.getPoint (seg).stepped)
        return;

    if (onEditBegin)
        onEditBegin();

    const double delta = wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f) * 2.0;
    env.setTension (seg, env.getPoint (seg).tension + delta);

    if (onChange)
        onChange();

    repaint();
}

//==============================================================================
void EnvelopeEditor::drawGrid (juce::Graphics& g) const
{
    // yatay cizgiler: 4 esit dilim
    g.setColour (ink (0.06f));

    for (int i = 1; i < 4; ++i)
    {
        const float y = plot.getY() + plot.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
    }

    // dikey cizgiler: adim / vurus / bar
    const int steps = juce::jmax (1, divisions);

    for (int i = 0; i <= steps; ++i)
    {
        const float x = xToPixel ((double) i / (double) steps);
        const int perBar  = juce::jmax (1, steps / patternBars);
        const int perBeat = juce::jmax (1, steps / (patternBars * 4));

        const bool isBar  = (i % perBar == 0);
        const bool isBeat = (i % perBeat == 0);

        if (isBar)
            g.setColour (ink (0.28f));
        else if (isBeat)
            g.setColour (ink (0.13f));
        else
            g.setColour (ink (0.05f));

        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
}

void EnvelopeEditor::drawCurve (juce::Graphics& g) const
{
    const int width = juce::jmax (2, (int) plot.getWidth());

    juce::Path curve, fill;

    for (int i = 0; i <= width; ++i)
    {
        const double phase = (double) i / (double) width;
        const float x = plot.getX() + (float) i;
        const float y = yToPixel (env.valueAt (phase));

        if (i == 0)
        {
            curve.startNewSubPath (x, y);
            fill .startNewSubPath (x, yToPixel (0.0));
            fill .lineTo (x, y);
        }
        else
        {
            curve.lineTo (x, y);
            fill .lineTo (x, y);
        }
    }

    // dolguyu taban cizgisine kapat
    fill.lineTo (plot.getRight(), yToPixel (0.0));
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.30f), plot.getX(), plot.getY(),
                                             accent.withAlpha (0.04f), plot.getX(), plot.getBottom(),
                                             false));
    g.fillPath (fill);

    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

void EnvelopeEditor::drawWaveform (juce::Graphics& g) const
{
    const int bins = (int) waveform.size();

    if (bins < 2)
        return;

    // Sessiz malzeme de gorunsun diye dB olcegi: -48 dBFS ... 0 dBFS -> 0 ... 1
    auto shape = [] (float peak)
    {
        if (peak <= 1.0e-5f)
            return 0.0f;

        return juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (peak) + 48.0f) / 48.0f);
    };

    const float centre = plot.getCentreY();
    const float half   = plot.getHeight() * 0.46f;

    juce::Path wave;
    wave.startNewSubPath (plot.getX(), centre);

    for (int i = 0; i < bins; ++i)
        wave.lineTo (xToPixel ((i + 0.5) / bins), centre - half * shape (waveform[(size_t) i]));

    wave.lineTo (plot.getRight(), centre);

    for (int i = bins - 1; i >= 0; --i)
        wave.lineTo (xToPixel ((i + 0.5) / bins), centre + half * shape (waveform[(size_t) i]));

    wave.closeSubPath();

    g.setColour (ink (0.07f));
    g.fillPath (wave);
}

void EnvelopeEditor::drawHandles (juce::Graphics& g) const
{
    if (drawMode)
        return;

    for (int i = 0; i < env.getNumPoints(); ++i)
    {
        if (! handleVisible (i))
            continue;

        const auto c = handlePosition (i);
        const bool active = (i == hoverHandle || i == dragHandle);
        const float r = active ? 5.0f : 3.6f;
        const auto box = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);

        if (active)
        {
            g.setColour (accent);
            g.fillEllipse (box);
        }
        else
        {
            g.setColour (accent.withAlpha (0.55f));
            g.drawEllipse (box, 1.3f);
        }
    }
}

void EnvelopeEditor::drawPoints (juce::Graphics& g) const
{
    for (int i = 0; i < env.getNumPoints(); ++i)
    {
        const auto& p = env.getPoint (i);
        const float x = xToPixel (p.x);
        const float y = yToPixel (p.y);

        const bool active = (i == hoverPoint || i == dragPoint);
        const float r = active ? kPointRadius + 2.0f : kPointRadius;

        g.setColour (juce::Colours::black.withAlpha (0.55f));

        if (p.stepped)
            g.fillRect (juce::Rectangle<float> (r * 2.2f, r * 2.2f).withCentre ({ x, y }));
        else
            g.fillEllipse (juce::Rectangle<float> (r * 2.2f, r * 2.2f).withCentre ({ x, y }));

        g.setColour (active ? Themes::current().text : accent.brighter (0.35f));

        if (p.stepped)
            g.fillRect (juce::Rectangle<float> (r * 1.6f, r * 1.6f).withCentre ({ x, y }));
        else
            g.fillEllipse (juce::Rectangle<float> (r * 1.6f, r * 1.6f).withCentre ({ x, y }));
    }
}

void EnvelopeEditor::drawScale (juce::Graphics& g) const
{
    g.setFont (juce::FontOptions (9.5f));

    // dar panellerde etiketler ust uste binmesin
    const int steps = (plot.getHeight() < 90.0f) ? 2 : 4;

    for (int i = 0; i <= steps; ++i)
    {
        const double value = (double) i / (double) steps;

        // Time: 0 = canli, 1 = pattern boyu geride (bar cinsinden).  Volume: yuzde.
        // Filtre: o yuksekligin kesim frekansi.
        const juce::String text = style == Style::volume ? juce::String (juce::roundToInt (value * 100.0))
                                : style == Style::filter ? hzToText (FilterMap::cutoffHz (value, filterHighPass))
                                                         : barsToText (value * patternBars);

        // uc noktalardaki etiketler cizim alaninin disina tasmasin
        const float y = juce::jlimit (plot.getY() + 7.0f, plot.getBottom() - 7.0f,
                                      yToPixel (value));
        const auto row = juce::Rectangle<float> (scaleColumn.getX(), y - 7.0f,
                                                 scaleColumn.getWidth() - 5.0f, 14.0f);

        g.setColour (ink (value == 0.0 ? 0.42f : 0.26f));
        g.drawText (text, row, juce::Justification::centredRight);
    }

}

void EnvelopeEditor::drawRuler (juce::Graphics& g) const
{
    if (rulerRow.isEmpty())
        return;

    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));

    // 4/4 varsayimi: her bar 4 ceyrek nota
    const int beats = patternBars * 4;

    for (int k = 0; k < beats; ++k)
    {
        const float x = xToPixel ((double) k / (double) beats);
        const bool barStart = (k % 4 == 0);

        g.setColour (ink (barStart ? 0.55f : 0.22f));
        g.drawText (barStart ? juce::String (k / 4 + 1)
                             : juce::String (k / 4 + 1) + "." + juce::String (k % 4 + 1),
                    juce::Rectangle<float> (x + 3.0f, rulerRow.getY(), 40.0f, rulerRow.getHeight()),
                    juce::Justification::centredLeft);
    }
}

void EnvelopeEditor::paint (juce::Graphics& g)
{
    const auto& theme = Themes::current();

    g.fillAll (theme.plot);

    drawScale (g);
    drawRuler (g);

    drawGrid (g);
    drawWaveform (g);
    drawCurve (g);

    // calan kafa
    const float px = xToPixel (playhead);
    g.setColour (theme.playhead.withAlpha (0.8f));
    g.drawVerticalLine ((int) px, plot.getY(), plot.getBottom());

    // kafanin zarf uzerindeki konumu
    g.setColour (theme.playhead);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f)
                     .withCentre ({ px, yToPixel (env.valueAt (playhead)) }));

    drawHandles (g);
    drawPoints (g);

    // cizim modu: ince bir cerceve ve imlecin altindaki hucre
    if (drawMode)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.drawRect (plot.reduced (1.0f), 1.5f);
    }

    g.setColour (theme.edge);
    g.drawRect (plot, 1.0f);
}
