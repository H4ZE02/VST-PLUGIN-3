// Renders the editor to a PNG after simulating a capture, so the UI can be checked without a DAW.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <random>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    ReverseReverbProcessor proc;
    const double fs = 48000.0;
    proc.setRateAndBufferSizeDetails (fs, 512);
    proc.prepareToPlay (fs, 512);

    const bool doCapture = argc < 3 || juce::String (argv[2]) != "empty";
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    if (doCapture)
    {
        proc.requestCapture();
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        int64_t pos = 0;
        for (int b = 0; b < (int) (5.0 * fs / 512); ++b)
        {
            for (int i = 0; i < 512; ++i, ++pos)
            {
                const double t = (double) (pos % (int) (0.5 * fs)) / fs;
                const float env = (float) std::exp (-t * 14.0);
                const float v = 0.7f * env * u (rng);
                buf.setSample (0, i, v); buf.setSample (1, i, v * 0.9f);
            }
            proc.processBlock (buf, midi);
            if (b == 100) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (3000);   // async update + render thread
    }

    RRLookAndFeel laf;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (980, 700);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (argc > 1 ? argv[1] : "ui.png");
    out.deleteFile();
    juce::PNGImageFormat png;
    if (auto os = out.createOutputStream()) png.writeImageToStream (img, *os);
    std::cout << "state: cap " << proc.getCapState() << " render " << proc.getRenderState()
              << " file " << proc.getExportFile().getFullPathName() << "\n";
    ed.reset();
    return 0;
}
