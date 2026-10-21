#pragma once
#include "PluginProcessor.h"

namespace ui
{
    namespace col
    {
        const juce::Colour bg      { 0xff0d0e11 };
        const juce::Colour panel   { 0xff15171c };
        const juce::Colour panel2  { 0xff1b1e24 };
        const juce::Colour line    { 0xff2a2e36 };
        const juce::Colour text    { 0xffe8eaee };
        const juce::Colour dim     { 0xff8b919c };
        const juce::Colour accent  { 0xff3ee0c3 };
        const juce::Colour accent2 { 0xff9b8cff };
        const juce::Colour warn    { 0xffffb454 };
        const juce::Colour clip    { 0xffff4d5e };
    }
    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
}

class RRLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RRLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle,
                           float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return ui::font (12.0f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (13.0f); }
    juce::Font getPopupMenuFont() override { return ui::font (13.0f); }
};

// Large rotary knob + name + numeric readout. Drag to change; double-click the
// knob or the number to type a value; Alt+double-click resets to default.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, bool bipolar = false);
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    void refreshText();
    juce::Slider slider;
    juce::Label name, value;
    double defaultValue = 0.0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
};

class Segmented : public juce::Component
{
public:
    Segmented (juce::RangedAudioParameter&, const juce::String& title, juce::StringArray items);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::String title; juce::StringArray items; int selected = 1;
    juce::ParameterAttachment attach;
};

class Meter : public juce::Component
{
public:
    Meter (std::atomic<float>& level, std::atomic<bool>& clip, juce::String label)
        : lv (level), cl (clip), name (std::move (label)) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { cl.store (false); }
private:
    std::atomic<float>& lv; std::atomic<bool>& cl; juce::String name;
};

class Waveform : public juce::Component
{
public:
    void setPeaks (std::vector<float> p) { peaks = std::move (p); repaint(); }
    void setColour (juce::Colour c) { colour = c; }
    void setPlayhead (float p) { if (p != playhead) { playhead = p; repaint(); } }
    void setWriteHead (float p) { if (p != writeHead) { writeHead = p; repaint(); } }
    void setPlaceholder (juce::String s) { placeholder = std::move (s); repaint(); }
    void paint (juce::Graphics&) override;
private:
    std::vector<float> peaks; juce::Colour colour { ui::col::accent };
    float playhead = -1.0f, writeHead = -1.0f; juce::String placeholder;
};

class DragZone : public juce::Component
{
public:
    std::function<juce::File()> getFile;
    std::function<bool()> isReady;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { started = false; }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void setStale (bool s) { if (stale != s) { stale = s; repaint(); } }
private:
    bool started = false, hover = false, stale = false;
};

class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 980, H = 700;
    explicit MainPanel (ReverseReverbProcessor&);
    ~MainPanel() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void exportWav();
    void savePreset();
    void loadPreset();

    ReverseReverbProcessor& proc;
    Knob kPre, kLength, kSize, kTone, kWidth, kDepth, kMix, kOut, kTrigger;
    Segmented mode;
    Meter inMeter, outMeter;
    juce::ToggleButton liveBtn { "LIVE SWELLS" }, bypassBtn { "BYPASS" };
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, loadBtn { "LOAD" };
    juce::ComboBox presetBox, unitBox;
    juce::Slider capSlider;
    Waveform capWave, resultWave;
    DragZone dragZone;
    juce::TextButton captureBtn { "CAPTURE" }, playBtn { "PLAY" }, stopBtn { "STOP" },
                     regenBtn { "REGENERATE" }, clearBtn { "CLEAR" }, exportBtn { "EXPORT WAV" };
    juce::Label status, latencyLabel, dragNote;
    float progress = 0.0f; juce::Colour progressColour { ui::col::accent };
    int lastCapVersion = -1, lastRenderState = -1;
    juce::String lastExportPath;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> liveAtt, bypassAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> capAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> unitAtt;
};

class ReverseReverbEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReverseReverbEditor (ReverseReverbProcessor&);
    ~ReverseReverbEditor() override;
    void resized() override;
private:
    RRLookAndFeel laf;
    MainPanel panel;
};
