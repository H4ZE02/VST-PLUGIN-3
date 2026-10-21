#include "PluginEditor.h"
using namespace ui;

// ============================================================ look and feel
RRLookAndFeel::RRLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::ComboBox::backgroundColourId, col::panel2);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::dim);
    setColour (juce::PopupMenu::backgroundColourId, col::panel2);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::text);
    setColour (juce::Slider::trackColourId, col::accent);
    setColour (juce::Slider::backgroundColourId, col::line);
    setColour (juce::Slider::thumbColourId, col::text);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, col::line);
    setColour (juce::TextEditor::backgroundColourId, col::panel2);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::accent);
    setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.35f));
}

void RRLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float arcR = r - 3.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (col::line);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (col::accent.withAlpha (0.18f));    // restrained glow
        g.strokePath (val, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (s.isEnabled() ? col::accent : col::dim);
        g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float kr = arcR - 9.0f;
    juce::ColourGradient grad (juce::Colour (0xff2b2f38), c.x, c.y - kr, juce::Colour (0xff15171c), c.x, c.y + kr, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colour (0xff3a3f4a));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

    const float sa = std::sin (angle), ca = -std::cos (angle);
    g.setColour (col::text);
    g.drawLine (c.x + sa * kr * 0.35f, c.y + ca * kr * 0.35f, c.x + sa * kr * 0.85f, c.y + ca * kr * 0.85f, 2.5f);
}

void RRLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool primary = (bool) b.getProperties().getWithDefault ("primary", false);
    auto fill = primary ? col::accent.withAlpha (0.16f) : col::panel2;
    if (over) fill = fill.brighter (0.12f);
    if (down) fill = fill.brighter (0.25f);
    if (! b.isEnabled()) fill = col::panel;
    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (! b.isEnabled() ? col::line.withAlpha (0.5f) : primary ? col::accent.withAlpha (0.8f) : col::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void RRLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool primary = (bool) b.getProperties().getWithDefault ("primary", false);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (! b.isEnabled() ? col::dim.withAlpha (0.45f) : primary ? col::accent : col::text);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void RRLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? col::accent.withAlpha (0.14f) : col::panel2);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? col::accent.withAlpha (0.8f) : over ? col::dim : col::line);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    const float d = 8.0f;
    const float cx = r.getX() + 14.0f, cy = r.getCentreY();
    if (on) { g.setColour (col::accent.withAlpha (0.3f)); g.fillEllipse (cx - d, cy - d, d * 2, d * 2); }
    g.setColour (on ? col::accent : col::dim);
    g.fillEllipse (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    g.setFont (font (11.5f, true));
    g.setColour (on ? col::text : col::dim);
    g.drawText (b.getButtonText(), r.withTrimmedLeft (26.0f), juce::Justification::centredLeft);
}

// ============================================================ knob
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& title, bool bipolar)
{
    name.setText (title.toUpperCase(), juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (font (12.0f, true));
    name.setColour (juce::Label::textColourId, col::dim);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.getProperties().set ("bipolar", bipolar);
    slider.addMouseListener (this, false);
    addAndMakeVisible (slider);

    value.setJustificationType (juce::Justification::centred);
    value.setFont (font (14.0f, true));
    value.setColour (juce::Label::textColourId, col::text);
    value.setEditable (false, true, true);       // double-click the number to type a value
    value.onTextChange = [this]
    {
        slider.setValue (slider.getValueFromText (value.getText()), juce::sendNotificationSync);
        refreshText();
    };
    addAndMakeVisible (value);

    attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
    if (auto* p = apvts.getParameter (id))
        defaultValue = p->convertFrom0to1 (p->getDefaultValue());
    slider.onValueChange = [this] { refreshText(); };
    refreshText();
}

void Knob::refreshText()
{
    value.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromTop (18));
    value.setBounds (r.removeFromBottom (24));
    slider.setBounds (r.reduced (6, 2));
}

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown()) { slider.setValue (defaultValue, juce::sendNotificationSync); return; }
    value.showEditor();
}

// ============================================================ segmented control
Segmented::Segmented (juce::RangedAudioParameter& p, const juce::String& t, juce::StringArray i)
    : title (t.toUpperCase()), items (std::move (i)),
      attach (p, [this] (float v) { selected = juce::roundToInt (v); repaint(); })
{
    attach.sendInitialUpdate();
}

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setFont (font (12.0f, true));
    g.setColour (col::dim);
    g.drawText (title, r.removeFromTop (18.0f), juce::Justification::centred);
    r = r.reduced (14.0f, 6.0f);
    const float h = r.getHeight() / (float) items.size();
    for (int i = 0; i < items.size(); ++i)
    {
        auto seg = r.withHeight (h).translated (0.0f, h * (float) i).reduced (0.0f, 2.0f);
        const bool on = i == selected;
        g.setColour (on ? col::accent.withAlpha (0.16f) : col::panel2);
        g.fillRoundedRectangle (seg, 6.0f);
        g.setColour (on ? col::accent : col::line);
        g.drawRoundedRectangle (seg, 6.0f, 1.0f);
        g.setColour (on ? col::accent : col::dim);
        g.setFont (font (12.0f, true));
        g.drawText (items[i].toUpperCase(), seg, juce::Justification::centred);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (18.0f);
    r = r.reduced (14.0f, 6.0f);
    const int idx = juce::jlimit (0, items.size() - 1, (int) ((e.position.y - r.getY()) / (r.getHeight() / (float) items.size())));
    attach.setValueAsCompleteGesture ((float) idx);
}

// ============================================================ meter
void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto ledArea = r.removeFromTop (16.0f);
    auto label = r.removeFromBottom (16.0f);
    r = r.reduced (r.getWidth() * 0.22f, 2.0f);

    const bool clipped = cl.load();
    g.setColour (clipped ? col::clip : col::line);
    g.fillEllipse (ledArea.withSizeKeepingCentre (9.0f, 9.0f));
    if (clipped) { g.setColour (col::clip.withAlpha (0.3f)); g.fillEllipse (ledArea.withSizeKeepingCentre (16.0f, 16.0f)); }

    g.setColour (col::panel2);
    g.fillRoundedRectangle (r, 4.0f);
    const float db = juce::Decibels::gainToDecibels (lv.load(), -60.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto bar = r.withTrimmedTop (r.getHeight() * (1.0f - frac));
    juce::ColourGradient grad (col::clip, r.getX(), r.getY(), col::accent, r.getX(), r.getBottom(), false);
    grad.addColour (0.12, col::warn);
    grad.addColour (0.35, col::accent);
    g.saveState();
    g.reduceClipRegion (bar.toNearestInt());
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 4.0f);
    g.restoreState();

    g.setColour (col::bg.withAlpha (0.6f));
    for (float tick : { -6.0f, -12.0f, -24.0f, -48.0f })
        g.fillRect (r.getX(), r.getY() + r.getHeight() * (1.0f - (tick + 60.0f) / 60.0f), r.getWidth(), 1.0f);

    g.setFont (font (11.0f, true));
    g.setColour (col::dim);
    g.drawText (name, label, juce::Justification::centred);
}

// ============================================================ waveform
void Waveform::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel2);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    const int bins = (int) (peaks.size() / 2);
    float biggest = 0.0f;
    for (float v : peaks) biggest = juce::jmax (biggest, std::abs (v));
    auto inner = r.reduced (6.0f, 6.0f);
    if (bins == 0 || biggest < 1.0e-5f)
    {
        g.setColour (col::dim.withAlpha (0.7f));
        g.setFont (font (12.0f));
        g.drawText (placeholder, r, juce::Justification::centred);
    }
    else
    {
        const float mid = inner.getCentreY(), half = inner.getHeight() * 0.5f;
        const float norm = 1.0f / juce::jmax (0.25f, biggest);   // quiet captures still show up
        g.setColour (colour.withAlpha (0.15f));
        g.fillRect (inner.getX(), mid - 0.5f, inner.getWidth(), 1.0f);
        g.setColour (colour);
        for (int i = 0; i < bins; ++i)
        {
            const float x = inner.getX() + inner.getWidth() * ((float) i + 0.5f) / (float) bins;
            const float top = mid - peaks[(size_t) i * 2 + 1] * norm * half;
            const float bot = mid - peaks[(size_t) i * 2] * norm * half;
            g.fillRect (x - 0.5f, top, 1.2f, juce::jmax (1.0f, bot - top));
        }
    }
    auto head = [&] (float p, juce::Colour c)
    {
        if (p < 0.0f) return;
        const float x = inner.getX() + inner.getWidth() * juce::jlimit (0.0f, 1.0f, p);
        g.setColour (c);
        g.fillRect (x - 0.75f, r.getY() + 2.0f, 1.5f, r.getHeight() - 4.0f);
    };
    head (playhead, col::text);
    head (writeHead, col::clip);
}

// ============================================================ drag zone
void DragZone::paint (juce::Graphics& g)
{
    const bool ready = isReady && isReady();
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const auto c = ready ? (stale ? col::warn : col::accent) : col::dim.withAlpha (0.5f);

    g.setColour (ready ? c.withAlpha (hover ? 0.2f : 0.1f) : col::panel2);
    g.fillRoundedRectangle (r, 10.0f);

    juce::Path outline, dashed;
    outline.addRoundedRectangle (r, 10.0f);
    const float dashes[] = { 7.0f, 5.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (c);
    g.fillPath (dashed);

    // little waveform glyph
    const auto mid = r.getCentre().translated (0.0f, -16.0f);
    const float heights[] = { 6, 12, 20, 30, 20, 12, 6 };
    g.setColour (c);
    for (int i = 0; i < 7; ++i)
        g.fillRoundedRectangle (mid.x - 24.0f + (float) i * 8.0f, mid.y - heights[i] * 0.5f, 4.0f, heights[i], 2.0f);

    g.setFont (font (16.0f, true));
    g.setColour (ready ? col::text : col::dim);
    g.drawText ("DRAG TO DAW", r.withTrimmedTop (r.getHeight() * 0.52f).withHeight (24.0f), juce::Justification::centred);
    g.setFont (font (11.0f));
    g.setColour (col::dim);
    g.drawText (! ready ? "Capture a sound to enable"
                        : stale ? "Settings changed - REGENERATE first"
                                : "Drag the rendered WAV onto a track",
                r.withTrimmedTop (r.getHeight() * 0.52f + 26.0f).withHeight (18.0f), juce::Justification::centred);
}

void DragZone::mouseDrag (const juce::MouseEvent& e)
{
    if (started || e.getDistanceFromDragStart() < 6) return;
    if (! (isReady && isReady() && getFile)) return;
    const auto f = getFile();
    if (! f.existsAsFile()) return;
    started = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

// ============================================================ main panel
MainPanel::MainPanel (ReverseReverbProcessor& p)
    : proc (p),
      kPre (p.apvts, ids::predelay, "Pre-Delay"), kLength (p.apvts, ids::length, "Length"),
      kSize (p.apvts, ids::size, "Size"), kTone (p.apvts, ids::tone, "Tone", true),
      kWidth (p.apvts, ids::width, "Width"), kDepth (p.apvts, ids::depth, "Depth"),
      kMix (p.apvts, ids::mix, "Dry/Wet"), kOut (p.apvts, ids::output, "Output"),
      kTrigger (p.apvts, ids::trigger, "Trigger"),
      mode (*p.apvts.getParameter (ids::mode), "Mix Mode", { "Subtle", "Balanced", "Extreme" }),
      inMeter (p.inLevel, p.inClip, "IN"), outMeter (p.outLevel, p.outClip, "OUT")
{
    for (auto* c : std::initializer_list<juce::Component*> { &kPre, &kLength, &kSize, &kTone, &kWidth, &kDepth,
                                                              &kMix, &kOut, &kTrigger, &mode, &inMeter, &outMeter })
        addAndMakeVisible (c);

    addAndMakeVisible (liveBtn);   addAndMakeVisible (bypassBtn);
    liveAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ids::live, liveBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ids::bypass, bypassBtn);

    // presets
    for (int i = 0; i < p.getNumPrograms(); ++i) presetBox.addItem (p.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (juce::jlimit (0, p.getNumPrograms() - 1, p.getCurrentProgram()), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    prevBtn.onClick = [this] { auto i = (presetBox.getSelectedItemIndex() + proc.getNumPrograms() - 1) % proc.getNumPrograms();
                               presetBox.setSelectedItemIndex (i, juce::sendNotificationSync); };
    nextBtn.onClick = [this] { auto i = (presetBox.getSelectedItemIndex() + 1) % proc.getNumPrograms();
                               presetBox.setSelectedItemIndex (i, juce::sendNotificationSync); };
    saveBtn.onClick = [this] { savePreset(); };
    loadBtn.onClick = [this] { loadPreset(); };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn, &saveBtn, &loadBtn })
        addAndMakeVisible (c);

    // capture length
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (ids::capUnit)))
        unitBox.addItemList (choice->choices, 1);
    unitAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ids::capUnit, unitBox);
    capSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    capSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    capAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids::capLen, capSlider);
    addAndMakeVisible (unitBox); addAndMakeVisible (capSlider);

    // preview
    capWave.setPlaceholder ("Press CAPTURE, then play your loop or sample in the DAW");
    resultWave.setColour (col::accent2);
    resultWave.setPlaceholder ("The reverse-reverb render appears here");
    addAndMakeVisible (capWave); addAndMakeVisible (resultWave);

    dragZone.isReady = [this] { return proc.getRenderState() == ReverseReverbProcessor::RenderReady && proc.getExportFile().existsAsFile(); };
    dragZone.getFile = [this] { return proc.getExportFile(); };
    addAndMakeVisible (dragZone);

    for (auto* b : { &captureBtn, &playBtn, &stopBtn, &regenBtn, &clearBtn, &exportBtn })
        addAndMakeVisible (b);
    captureBtn.getProperties().set ("primary", true);
    captureBtn.onClick = [this] { proc.requestCapture(); };
    playBtn.onClick    = [this] { proc.play(); };
    stopBtn.onClick    = [this] { proc.stop(); };
    regenBtn.onClick   = [this] { proc.regenerate(); };
    clearBtn.onClick   = [this] { proc.requestClear(); };
    exportBtn.onClick  = [this] { exportWav(); };

    status.setFont (font (12.0f, true));
    status.setColour (juce::Label::textColourId, col::dim);
    latencyLabel.setFont (font (11.0f));
    latencyLabel.setColour (juce::Label::textColourId, col::dim);
    latencyLabel.setJustificationType (juce::Justification::centred);
    dragNote.setFont (font (11.0f));
    dragNote.setColour (juce::Label::textColourId, col::dim);
    dragNote.setText ("Drag-out depends on your DAW. If a drop is refused, press EXPORT WAV and drag the saved file in.",
                      juce::dontSendNotification);
    for (auto* l : { &status, &latencyLabel, &dragNote }) addAndMakeVisible (l);

    setSize (W, H);
    startTimerHz (30);
}

void MainPanel::resized()
{
    prevBtn.setBounds (470, 18, 28, 26);  presetBox.setBounds (502, 18, 200, 26); nextBtn.setBounds (706, 18, 28, 26);
    saveBtn.setBounds (742, 18, 56, 26);  loadBtn.setBounds (802, 18, 56, 26);
    bypassBtn.setBounds (870, 18, 90, 26);

    const int x0 = 30, y0 = 81, cw = 140, ch = 150;
    Knob* row1[] = { &kPre, &kLength, &kSize, &kTone, &kWidth };
    Knob* row2[] = { &kDepth, &kMix, &kOut, &kTrigger };
    for (int i = 0; i < 5; ++i) row1[i]->setBounds (x0 + i * cw, y0, cw, ch);
    for (int i = 0; i < 4; ++i) row2[i]->setBounds (x0 + i * cw, y0 + ch, cw, ch);
    mode.setBounds (x0 + 4 * cw, y0 + ch, cw, ch);

    inMeter.setBounds (778, 84, 60, 240);  outMeter.setBounds (858, 84, 60, 240);
    liveBtn.setBounds (776, 340, 168, 28);
    latencyLabel.setBounds (766, 369, 188, 16);

    unitBox.setBounds (330, 410, 110, 24);  capSlider.setBounds (446, 410, 190, 24);
    capWave.setBounds (36, 450, 600, 60);   resultWave.setBounds (36, 532, 600, 62);
    status.setBounds (36, 614, 600, 20);
    captureBtn.setBounds (36, 642, 104, 28);  playBtn.setBounds (146, 642, 70, 28);  stopBtn.setBounds (222, 642, 70, 28);
    regenBtn.setBounds (298, 642, 120, 28);   clearBtn.setBounds (424, 642, 70, 28);

    dragZone.setBounds (660, 432, 284, 150);
    exportBtn.setBounds (660, 592, 284, 30);
    dragNote.setBounds (660, 628, 284, 44);
}

void MainPanel::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto panel = [&] (juce::Rectangle<float> r)
    {
        g.setColour (col::panel); g.fillRoundedRectangle (r, 12.0f);
        g.setColour (col::line);  g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
    };
    panel ({ 20, 72, 720, 318 });
    panel ({ 756, 72, 204, 318 });
    panel ({ 20, 402, 940, 278 });

    g.setColour (col::accent);
    g.fillEllipse (22, 24, 10, 10);
    g.setColour (col::accent.withAlpha (0.25f));
    g.fillEllipse (18, 20, 18, 18);
    g.setColour (col::text);
    g.setFont (font (22.0f, true));
    g.drawText ("REVERSE REVERB", 44, 14, 260, 30, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (font (11.0f));
    g.drawText ("swells that build into the hit", 44, 38, 260, 16, juce::Justification::centredLeft);

    g.setFont (font (12.0f, true));
    g.setColour (col::dim);
    g.drawText ("PREVIEW", 36, 410, 120, 24, juce::Justification::centredLeft);
    g.drawText ("CAPTURE LENGTH", 190, 410, 136, 24, juce::Justification::centredRight);
    g.setFont (font (10.5f, true));
    g.drawText ("CAPTURED SOUND", 36, 436, 200, 12, juce::Justification::centredLeft);
    g.drawText ("REVERSE REVERB RESULT", 36, 518, 240, 12, juce::Justification::centredLeft);

    // progress bar
    auto bar = juce::Rectangle<float> (36, 602, 600, 5);
    g.setColour (col::panel2);
    g.fillRoundedRectangle (bar, 2.5f);
    if (progress > 0.0f)
    {
        g.setColour (progressColour);
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (5.0f, bar.getWidth() * juce::jlimit (0.0f, 1.0f, progress))), 2.5f);
    }
}

void MainPanel::timerCallback()
{
    const int cs = proc.getCapState(), rs = proc.getRenderState();
    const bool ready = rs == ReverseReverbProcessor::RenderReady;
    const bool stale = proc.isPreviewStale();

    // waveforms
    const int cv = proc.getCaptureVersion();
    if (cs == ReverseReverbProcessor::CapRecording || cv != lastCapVersion)
    {
        capWave.setPeaks (proc.getCapturePeaks (600));
        lastCapVersion = cv;
    }
    capWave.setWriteHead (cs == ReverseReverbProcessor::CapRecording ? proc.getCaptureProgress01() : -1.0f);

    const auto path = proc.getExportFile().getFullPathName();
    if (path != lastExportPath || rs != lastRenderState)
    {
        resultWave.setPeaks (ready ? proc.getResultPeaks() : std::vector<float>());
        lastExportPath = path; lastRenderState = rs;
    }
    resultWave.setPlayhead (proc.isPlaying() ? proc.getPlayPosition01() : -1.0f);

    // status + progress
    juce::String msg; juce::Colour c = col::dim;
    float prog = 0.0f; progressColour = col::accent;
    if (cs == ReverseReverbProcessor::CapWaiting)
    { msg = "LISTENING - press play in your DAW and the capture starts with the first sound"; c = col::warn; }
    else if (cs == ReverseReverbProcessor::CapRecording)
    {
        prog = proc.getCaptureProgress01(); progressColour = col::clip; c = col::clip;
        msg = "RECORDING INCOMING AUDIO  " + juce::String (prog * (float) proc.getCaptureSeconds(), 1) + " / "
              + juce::String (proc.getCaptureSeconds(), 1) + " s";
    }
    else if (rs == ReverseReverbProcessor::RenderBusy)
    { prog = proc.getRenderProgress(); progressColour = col::accent2; c = col::accent2; msg = "GENERATING REVERSE REVERB..."; }
    else if (ready && stale)
    { msg = "SETTINGS CHANGED - press REGENERATE to update the preview and export"; c = col::warn; }
    else if (ready)
    { prog = 1.0f; msg = "READY - PLAY it, or drag the result into your DAW"; c = col::accent; }
    else if (cs == ReverseReverbProcessor::CapDone)
    { msg = "Captured. Press REGENERATE to render."; }
    else
    { msg = "Press CAPTURE, then play your loop or sample in the DAW"; }
    status.setText (msg, juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, c);
    if (std::abs (prog - progress) > 0.001f) { progress = prog; repaint (20, 598, 620, 12); }

    // buttons
    captureBtn.setButtonText (cs == ReverseReverbProcessor::CapWaiting || cs == ReverseReverbProcessor::CapRecording ? "CANCEL" : "CAPTURE");
    playBtn.setEnabled (ready && ! proc.isPlaying());
    stopBtn.setEnabled (proc.isPlaying());
    regenBtn.setEnabled (cs == ReverseReverbProcessor::CapDone && rs != ReverseReverbProcessor::RenderBusy);
    const bool regenPrimary = stale && regenBtn.isEnabled();
    if ((bool) regenBtn.getProperties().getWithDefault ("primary", false) != regenPrimary)
    { regenBtn.getProperties().set ("primary", regenPrimary); regenBtn.repaint(); }
    clearBtn.setEnabled (cs != ReverseReverbProcessor::CapIdle || rs != ReverseReverbProcessor::RenderNone);
    exportBtn.setEnabled (ready);
    capSlider.setEnabled (unitBox.getSelectedItemIndex() <= 0);
    dragZone.setStale (stale);
    dragZone.repaint();

    // latency read-out
    const bool live = liveBtn.getToggleState();
    latencyLabel.setText (live ? "Reported latency " + juce::String ((double) proc.getLatencySamples() / juce::jmax (1.0, proc.getCurrentSampleRate()), 1)
                                 + " s (host compensates)"
                               : "Live swells off - no latency", juce::dontSendNotification);
    inMeter.repaint(); outMeter.repaint();
}

void MainPanel::exportWav()
{
    const auto src = proc.getExportFile();
    if (! src.existsAsFile()) return;
    chooser = std::make_unique<juce::FileChooser> ("Export WAV",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (src.getFileName()), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File()) proc.exportCopyTo (f.withFileExtension ("wav"));
        });
}

void MainPanel::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Save preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("My Preset.rrpreset"), "*.rrpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File()) proc.saveUserPreset (f.withFileExtension ("rrpreset"));
        });
}

void MainPanel::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Load preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.rrpreset");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) proc.loadUserPreset (f);
        });
}

// ============================================================ editor wrapper (scales the fixed-size panel)
ReverseReverbEditor::ReverseReverbEditor (ReverseReverbProcessor& p)
    : juce::AudioProcessorEditor (&p), panel (p)
{
    panel.setLookAndFeel (&laf);
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (MainPanel::W * 3 / 4, MainPanel::H * 3 / 4, MainPanel::W * 2, MainPanel::H * 2);
    getConstrainer()->setFixedAspectRatio ((double) MainPanel::W / (double) MainPanel::H);
    setSize (MainPanel::W, MainPanel::H);
}

ReverseReverbEditor::~ReverseReverbEditor() { panel.setLookAndFeel (nullptr); }

void ReverseReverbEditor::resized()
{
    panel.setBounds (0, 0, MainPanel::W, MainPanel::H);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainPanel::W));
}
