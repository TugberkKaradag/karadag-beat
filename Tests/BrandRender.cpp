/*
    Logodan uygulama simgesini ve kurulum sihirbazinin gorsellerini uretir.
    Hepsi Source/Brand.* ve Toxic Bloom paletinden cizilir, elle cizilmis dosya yok.

    Kullanim:  BrandRender.exe <repo klasoru>

      resources/icon.png            1024 px - Standalone uygulamanin simgesi (CMake ICON_BIG)
      installer/icon.ico            16-256 px - Setup.exe ve kaldirici simgesi
      installer/wizard*.png         karsilama / bitis sayfasindaki dikey gorsel (100/150/200 %)
      installer/wizard-small*.png   diger sayfalarin sag ustundeki kucuk gorsel
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/Brand.h"
#include "../Source/Theme.h"

#include <cstdio>

namespace
{
    const Theme& palette()  { return Themes::all().front(); }     // Toxic Bloom

    //==========================================================================
    juce::Image renderIcon (int size)
    {
        juce::Image image (juce::Image::ARGB, size, size, true);

        // Graphics kapanmadan goruntu okunursa cizim eksik cikabiliyor - kendi kapsaminda
        {
        juce::Graphics g (image);

        const auto& t = palette();
        const float n = (float) size;
        const auto bounds = juce::Rectangle<float> (n, n);

        juce::Path plate;
        plate.addRoundedRectangle (bounds, n * 0.22f);

        g.setGradientFill (juce::ColourGradient (t.panel.brighter (0.12f), 0.0f, 0.0f,
                                                 t.plot, 0.0f, n, false));
        g.fillPath (plate);

        if (size >= 32)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.strokePath (plate, juce::PathStrokeType (juce::jmax (1.0f, n / 128.0f)));
        }

        // Glif editordeki oranlarla: cizgi kutu genisliginin 1/12'si, nokta ~1/5'i.
        // Kucuk boyutlarda ince cizgi kaybolmasin diye alt sinir var.
        const float boxW = n * 0.60f;
        const auto box = juce::Rectangle<float> (boxW, boxW * 20.0f / 24.0f)
                           .withCentre ({ n * 0.47f, n * 0.54f });

        const float stroke = juce::jmax (1.4f, boxW * 0.085f);
        const float dot    = juce::jmax (3.0f, boxW * 0.20f);

        Brand::drawGlyph (g, box, stroke, t.timeAccent, t.volumeAccent, dot);
        }

        return image;
    }

    //==========================================================================
    /** Karsilama sayfasindaki dikey gorsel.  Tabanda uc lane'in motifi: basamak,
        pump ve filtre egrisi - eklentinin ne yaptigini bir bakista anlatir. */
    juce::Image renderWizard (float scale)
    {
        const int w = juce::roundToInt (164.0f * scale);
        const int h = juce::roundToInt (314.0f * scale);

        juce::Image image (juce::Image::RGB, w, h, true);

        {
        juce::Graphics g (image);

        const auto& t = palette();
        const float W = (float) w, H = (float) h;

        g.setGradientFill (juce::ColourGradient (t.panel, 0.0f, 0.0f, t.plot, 0.0f, H, false));
        g.fillAll();

        // logo
        const float boxW = W * 0.46f;
        const auto box = juce::Rectangle<float> (boxW, boxW * 20.0f / 24.0f).withCentre ({ W * 0.48f, H * 0.19f });
        Brand::drawGlyph (g, box, boxW * 0.075f, t.timeAccent, t.volumeAccent, boxW * 0.17f);

        // wordmark
        g.setColour (t.textDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f * scale)).withExtraKerningFactor (0.34f));
        g.drawText ("KARADAG", juce::Rectangle<float> (0.0f, H * 0.305f, W, 14.0f * scale),
                    juce::Justification::centred);

        g.setColour (t.text);
        g.setFont (juce::Font (juce::FontOptions (30.0f * scale, juce::Font::bold)).withExtraKerningFactor (0.04f));
        g.drawText ("BEAT", juce::Rectangle<float> (0.0f, H * 0.34f, W, 36.0f * scale),
                    juce::Justification::centred);

        // lane motifleri
        const float left = W * 0.12f, right = W * 0.88f, span = right - left;
        const float laneH = H * 0.085f;
        const float stroke = 1.6f * scale;

        auto laneTop = [&] (int i) { return H * 0.60f + (float) i * H * 0.115f; };

        // time: repeat basamaklari
        {
            juce::Path p;
            const float top = laneTop (0);
            const int steps = 8;

            for (int i = 0; i < steps; ++i)
            {
                const float x0 = left + span * (float) i / steps;
                const float x1 = left + span * (float) (i + 1) / steps;
                const float y  = (i % 2 == 0) ? top : top + laneH * 0.55f;

                if (i == 0) p.startNewSubPath (x0, y);
                else        p.lineTo (x0, y);

                p.lineTo (x1, y);
            }

            g.setColour (t.timeAccent.withAlpha (0.75f));
            g.strokePath (p, juce::PathStrokeType (stroke));
        }

        // volume: pump
        {
            juce::Path p;
            const float top = laneTop (1), bottom = top + laneH;
            const int beats = 4;

            for (int b = 0; b < beats; ++b)
            {
                const float x0 = left + span * (float) b / beats;
                const float x1 = left + span * (float) (b + 1) / beats;

                if (b == 0) p.startNewSubPath (x0, bottom);
                else        p.lineTo (x0, bottom);

                p.quadraticTo (x0 + (x1 - x0) * 0.08f, top, x0 + (x1 - x0) * 0.9f, top);
                p.lineTo (x1, top);
            }

            g.setColour (t.volumeAccent.withAlpha (0.75f));
            g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }

        // filter: kapanip acilan sweep
        {
            juce::Path p;
            const float top = laneTop (2), bottom = top + laneH;

            p.startNewSubPath (left, top);
            p.cubicTo (left + span * 0.28f, top, left + span * 0.36f, bottom, left + span * 0.5f, bottom);
            p.cubicTo (left + span * 0.64f, bottom, left + span * 0.72f, top, right, top);

            g.setColour (t.filterAccent.withAlpha (0.75f));
            g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }
        }

        return image;
    }

    //==========================================================================
    bool writePng (const juce::Image& image, const juce::File& file)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        juce::PNGImageFormat png;
        juce::FileOutputStream out (file);

        if (! out.openedOk() || ! png.writeImageToStream (image, out))
        {
            std::printf ("yazilamadi: %s\n", file.getFullPathName().toRawUTF8());
            return false;
        }

        std::printf ("%s  (%d x %d)\n", file.getFileName().toRawUTF8(), image.getWidth(), image.getHeight());
        return true;
    }

    /** Coklu boyutlu .ico.  256 px PNG olarak, kucukler 32 bit DIB olarak girer -
        eski araclar ve Windows'un kendisi kucuk boyutlarda DIB bekleyebiliyor. */
    bool writeIco (const juce::Array<int>& sizes, const juce::File& file)
    {
        juce::Array<juce::MemoryBlock> payloads;

        for (const int size : sizes)
        {
            const auto image = renderIcon (size);
            juce::MemoryBlock block;
            juce::MemoryOutputStream out (block, false);

            if (size >= 256)
            {
                juce::PNGImageFormat png;
                png.writeImageToStream (image, out);
            }
            else
            {
                // BITMAPINFOHEADER; yukseklik renk + maske icin iki kat
                out.writeInt (40);
                out.writeInt (size);
                out.writeInt (size * 2);
                out.writeShort (1);
                out.writeShort (32);
                out.writeInt (0);
                out.writeInt (0);
                out.writeInt (0);
                out.writeInt (0);
                out.writeInt (0);
                out.writeInt (0);

                const juce::Image::BitmapData bits (image, juce::Image::BitmapData::readOnly);

                for (int y = size - 1; y >= 0; --y)          // alttan uste
                    for (int x = 0; x < size; ++x)
                    {
                        const auto c = bits.getPixelColour (x, y);   // premultiplied degil
                        out.writeByte ((char) c.getBlue());
                        out.writeByte ((char) c.getGreen());
                        out.writeByte ((char) c.getRed());
                        out.writeByte ((char) c.getAlpha());
                    }

                // AND maskesi: alfa kanali varken hep 0, satirlar 4 bayta hizali
                const int maskRow = ((size + 31) / 32) * 4;
                out.writeRepeatedByte (0, (size_t) (maskRow * size));
            }

            out.flush();
            payloads.add (block);
        }

        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream out (file);

        if (! out.openedOk())
            return false;

        out.writeShort (0);                       // ayrilmis
        out.writeShort (1);                       // tip: simge
        out.writeShort ((short) sizes.size());

        int offset = 6 + 16 * sizes.size();

        for (int i = 0; i < sizes.size(); ++i)
        {
            const int size = sizes[i];
            out.writeByte ((char) (size >= 256 ? 0 : size));
            out.writeByte ((char) (size >= 256 ? 0 : size));
            out.writeByte (0);                    // palet yok
            out.writeByte (0);
            out.writeShort (1);                   // renk duzlemi
            out.writeShort (32);                  // bit derinligi
            out.writeInt ((int) payloads[i].getSize());
            out.writeInt (offset);
            offset += (int) payloads[i].getSize();
        }

        for (const auto& block : payloads)
            out.write (block.getData(), block.getSize());

        std::printf ("%s  (%d boyut)\n", file.getFileName().toRawUTF8(), sizes.size());
        return true;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const auto root = argc > 1 ? juce::File (juce::String (argv[1]))
                               : juce::File::getCurrentWorkingDirectory();

    if (! root.getChildFile ("CMakeLists.txt").existsAsFile())
    {
        std::printf ("repo klasoru bulunamadi: %s\n", root.getFullPathName().toRawUTF8());
        return 1;
    }

    bool ok = true;

    ok &= writePng (renderIcon (1024), root.getChildFile ("resources/icon.png"));
    ok &= writeIco ({ 16, 24, 32, 48, 64, 256 }, root.getChildFile ("installer/icon.ico"));

    const struct { float scale; const char* suffix; } dpis[] = { { 1.0f, "" }, { 1.5f, "-150" }, { 2.0f, "-200" } };

    for (const auto& d : dpis)
    {
        ok &= writePng (renderWizard (d.scale),
                        root.getChildFile ("installer/wizard" + juce::String (d.suffix) + ".png"));
        ok &= writePng (renderIcon (juce::roundToInt (55.0f * d.scale)),
                        root.getChildFile ("installer/wizard-small" + juce::String (d.suffix) + ".png"));
    }

    return ok ? 0 : 1;
}
