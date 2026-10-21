#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset { const char* name; float tone, width, size, depth, output, mix, pre, length; int mode; float trigger; };
    const Preset factoryPresets[] = {
        { "Init - Balanced",    0, 100, 50,  70,  0, 50, 30, 1.2f, 1, 50 },
        { "Vocal Swell",       15, 120, 60,  75,  0, 45, 40, 1.5f, 1, 45 },
        { "Snare Rise",        10, 110, 40,  85, -1, 55, 20, 0.8f, 2, 60 },
        { "Kick Bloom",       -45,  80, 55,  80,  0, 40, 10, 1.0f, 1, 55 },
        { "Melody Wash",       20, 160, 80,  70, -1, 50, 60, 2.2f, 0, 40 },
        { "Transition Riser",  30, 180, 90,  95, -2, 70,  0, 2.5f, 2, 50 },
        { "Subtle Air",        35, 140, 50,  45,  0, 35, 50, 1.0f, 0, 45 },
        { "Dark Cavern",      -70, 150, 95,  85, -2, 60, 80, 2.5f, 1, 50 },
        { "Extreme Reverse",    0, 200, 70, 100, -3, 75,  0, 1.8f, 2, 55 },
    };

    std::vector<float> computePeaks (const float* l, const float* r, int n, int bins, int totalForScale)
    {
        std::vector<float> p ((size_t) bins * 2, 0.0f);
        if (n <= 0 || totalForScale <= 0) return p;
        for (int b = 0; b < bins; ++b)
        {
            const int a = (int) ((int64_t) b * totalForScale / bins);
            const int e = juce::jmin (n, (int) ((int64_t) (b + 1) * totalForScale / bins));
            float mn = 0.0f, mx = 0.0f;
            for (int i = a; i < e; ++i)
            {
                mn = juce::jmin (mn, l[i], r[i]);
                mx = juce::jmax (mx, l[i], r[i]);
            }
            p[(size_t) b * 2] = mn; p[(size_t) b * 2 + 1] = mx;
        }
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ReverseReverbProcessor::createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    auto pid = [] (const char* id) { return ParameterID { id, 1 }; };
    auto fromText = [] (const String& t) { return t.retainCharacters ("-+.0123456789").getFloatValue(); };
    auto num = [] (int decimals, const String& suffix) {
        return [decimals, suffix] (float v, int) { return String (v, decimals) + suffix; };
    };

    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<F> (pid (ids::tone), "Tone", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            return String (v > 0.0f ? "+" : "") + String (roundToInt (v)); }).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<F> (pid (ids::width), "Width", NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (num (0, "%")).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<F> (pid (ids::size), "Size", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (num (0, "%")).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<F> (pid (ids::depth), "Depth", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 70.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (num (0, "%")).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<F> (pid (ids::output), "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (num (1, " dB")).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<F> (pid (ids::mix), "Dry/Wet", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (num (0, "%")).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<F> (pid (ids::predelay), "Pre-Delay", NormalisableRange<float> (0.0f, 200.0f, 1.0f), 30.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (num (0, " ms")).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<F> (pid (ids::length), "Length", NormalisableRange<float> (0.2f, (float) rr::LiveSwell::maxLengthSec, 0.01f, 0.7f), 1.2f,
        AudioParameterFloatAttributes().withStringFromValueFunction (num (2, " s")).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<AudioParameterChoice> (pid (ids::mode), "Mix Mode", StringArray { "Subtle", "Balanced", "Extreme" }, 1));
    p.push_back (std::make_unique<F> (pid (ids::trigger), "Trigger", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (num (0, "%")).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<AudioParameterBool> (pid (ids::live), "Live Swells", true));
    p.push_back (std::make_unique<F> (pid (ids::capLen), "Capture Length", NormalisableRange<float> (0.5f, 10.0f, 0.1f), 4.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (num (1, " s")).withValueFromStringFunction (fromText)));
    p.push_back (std::make_unique<AudioParameterBool> (pid (ids::bypass), "Bypass", false));
    p.push_back (std::make_unique<AudioParameterChoice> (pid (ids::capUnit), "Capture Unit",
                 StringArray { "Seconds", "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, 0));
    return { p.begin(), p.end() };
}

ReverseReverbProcessor::ReverseReverbProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pTone = apvts.getRawParameterValue (ids::tone);     pWidth = apvts.getRawParameterValue (ids::width);
    pSize = apvts.getRawParameterValue (ids::size);     pDepth = apvts.getRawParameterValue (ids::depth);
    pOutput = apvts.getRawParameterValue (ids::output); pMix = apvts.getRawParameterValue (ids::mix);
    pPre = apvts.getRawParameterValue (ids::predelay);  pLength = apvts.getRawParameterValue (ids::length);
    pMode = apvts.getRawParameterValue (ids::mode);     pTrigger = apvts.getRawParameterValue (ids::trigger);
    pLive = apvts.getRawParameterValue (ids::live);     pCapLen = apvts.getRawParameterValue (ids::capLen);
    pBypass = apvts.getRawParameterValue (ids::bypass);
    pCapUnit = apvts.getRawParameterValue (ids::capUnit);

    // tidy up rendered files from earlier sessions (older than a day)
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ReverseReverb");
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.wav"))
        if (f.getCreationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::days (1))
            f.deleteFile();
}

ReverseReverbProcessor::~ReverseReverbProcessor()
{
    cancelPendingUpdate();
    renderGeneration++;                     // tell running renders to give up
    pool.removeAllJobs (true, 10000);
}

bool ReverseReverbProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

rr::RenderSettings ReverseReverbProcessor::currentSettings() const
{
    rr::RenderSettings s;
    s.tone = *pTone / 100.0f;      s.width = *pWidth / 100.0f;
    s.size = *pSize / 100.0f;      s.depth = *pDepth / 100.0f;
    s.preDelayMs = *pPre;          s.lengthSec = *pLength;
    s.mode = juce::roundToInt (pMode->load());
    return s;
}

std::array<float, 10> ReverseReverbProcessor::paramStamp() const
{
    return { *pTone, *pWidth, *pSize, *pDepth, *pOutput, *pMix, *pPre, *pLength, *pMode, 0.0f };
}

void ReverseReverbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool rateChanged = std::abs (sampleRate - sr) > 0.5;
    sr = sampleRate;
    maxChunk = juce::jmax (64, samplesPerBlock);
    scratch.setSize (4, maxChunk);
    live.prepare (sr);
    lastLive = *pLive > 0.5f;
    setLatencySamples (lastLive ? live.getLatencySamples() : 0);

    mixSmooth.reset (sr, 0.03);  mixSmooth.setCurrentAndTargetValue (*pMix / 100.0f);
    gainSmooth.reset (sr, 0.03); gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((float) *pOutput));

    const int maxCap = (int) (34.0 * sr) + 1;   // 8 bars down to 60 bpm
    if (captureBuf.getNumSamples() != maxCap || rateChanged)
    {
        // A capture made at another sample rate can't be reused, so drop it.
        capState = CapIdle; capCount = 0; ++captureVersion;
        captureBuf.setSize (2, maxCap);
        captureBuf.clear();
        previewPlaying = false;
        const juce::SpinLock::ScopedLockType sl (previewLock);
        previewAudio.setSize (0, 0);
        resultPeaks.clear();
        renderState = RenderNone;
    }
    captureRate = sr;
}

void ReverseReverbProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    processInternal (b, *pBypass > 0.5f);
}

void ReverseReverbProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    processInternal (b, true);
}

void ReverseReverbProcessor::processInternal (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // ---- host tempo (used for bar-synced capture) ----
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) hostBpm.store ((float) *bpm);
            if (auto ts = pos->getTimeSignature())
                hostBeatsPerBar.store ((float) ts->numerator * 4.0f / (float) juce::jmax (1, ts->denominator));
        }

    // ---- input metering ----
    {
        const float pk = juce::jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n));
        const float decay = std::exp (-(float) n / (0.3f * (float) sr));
        inLevel.store (juce::jmax (pk, inLevel.load() * decay));
        if (pk >= 0.999f) inClip.store (true);
    }

    // ---- capture (always taps the raw input) ----
    {
        int st = capState.load();
        int start = 0;
        if (st == CapWaiting)
        {
            start = -1;
            for (int i = 0; i < n; ++i)
                if (std::abs (L[i]) > 0.003f || std::abs (R[i]) > 0.003f) { start = i; break; }
            if (start >= 0)
            {
                int expected = CapWaiting;
                if (! capState.compare_exchange_strong (expected, CapRecording)) start = -1;
                else st = CapRecording;
            }
        }
        if (st == CapRecording && start >= 0)
        {
            const int have = capCount.load();
            const int target = capTarget.load();
            const int todo = juce::jmin (n - start, target - have);
            if (todo > 0)
            {
                captureBuf.copyFrom (0, have, L + start, todo);
                captureBuf.copyFrom (1, have, R + start, todo);
                capCount.store (have + todo);
            }
            if (have + todo >= target)
            {
                int expected = CapRecording;
                if (capState.compare_exchange_strong (expected, CapDone))
                {
                    ++captureVersion;
                    renderAfterCapture = true;
                    triggerAsyncUpdate();
                }
            }
        }
    }

    // ---- live reverse-reverb ----
    const bool liveOn = *pLive > 0.5f;
    if (liveOn != lastLive)
    {
        lastLive = liveOn;
        if (liveOn) live.reset();
        triggerAsyncUpdate();   // latency changes on the message thread
    }

    if (liveOn)
    {
        const auto settings = currentSettings();
        mixSmooth.setTargetValue (*pMix / 100.0f);
        gainSmooth.setTargetValue (juce::Decibels::decibelsToGain ((float) *pOutput));
        float* dl = scratch.getWritePointer (0); float* dr = scratch.getWritePointer (1);
        float* wl = scratch.getWritePointer (2); float* wr = scratch.getWritePointer (3);
        for (int off = 0; off < n; off += maxChunk)
        {
            const int c = juce::jmin (maxChunk, n - off);
            live.process ({ L + off, R + off, dl, dr, wl, wr, c }, settings, *pTrigger / 100.0f, ! bypassed);
            for (int i = 0; i < c; ++i)
            {
                const float m = mixSmooth.getNextValue();
                const float g = gainSmooth.getNextValue();
                const float wg = bypassed ? 0.0f : std::sin (m * juce::MathConstants<float>::halfPi);
                const float dg = bypassed ? 1.0f : std::cos (m * juce::MathConstants<float>::halfPi);
                const float og = bypassed ? 1.0f : g;
                L[off + i] = (dl[i] * dg + wl[i] * wg) * og;
                R[off + i] = (dr[i] * dg + wr[i] * wg) * og;
            }
        }
    }

    // ---- preview playback replaces the output so you can hear the render on its own ----
    if (previewPlaying.load())
    {
        juce::FloatVectorOperations::clear (L, n);
        juce::FloatVectorOperations::clear (R, n);
        const juce::SpinLock::ScopedTryLockType sl (previewLock);
        if (sl.isLocked() && previewAudio.getNumSamples() > 0)
        {
            const int pos = previewPos.load();
            const int len = previewAudio.getNumSamples();
            const int c = juce::jmin (n, len - pos);
            if (c > 0)
            {
                juce::FloatVectorOperations::copy (L, previewAudio.getReadPointer (0) + pos, c);
                juce::FloatVectorOperations::copy (R, previewAudio.getReadPointer (1) + pos, c);
                previewPos.store (pos + c);
            }
            if (pos + c >= len) previewPlaying.store (false);
        }
    }

    // ---- output metering ----
    {
        const float pk = juce::jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n));
        const float decay = std::exp (-(float) n / (0.3f * (float) sr));
        outLevel.store (juce::jmax (pk, outLevel.load() * decay));
        if (pk >= 0.999f) outClip.store (true);
    }
}

void ReverseReverbProcessor::handleAsyncUpdate()
{
    setLatencySamples (lastLive ? live.getLatencySamples() : 0);
    if (renderAfterCapture.exchange (false))
        regenerate();
}

// ------------------------------------------------------------------ capture / preview

void ReverseReverbProcessor::requestCapture()
{
    const int s = capState.load();
    if (s == CapWaiting || s == CapRecording)      // second press cancels
    {
        capState.store (CapIdle); capCount.store (0); ++captureVersion;
        return;
    }
    stop();
    renderGeneration++;
    renderState.store (RenderNone);
    {
        const juce::SpinLock::ScopedLockType sl (previewLock);
        previewAudio.setSize (0, 0);
        resultPeaks.clear();
        exportFile = juce::File();
    }
    const int target = juce::jlimit (1, captureBuf.getNumSamples() - 1, (int) (getCaptureSeconds() * sr));
    capTarget.store (target);
    capCount.store (0);
    ++captureVersion;
    capState.store (CapWaiting);
}

double ReverseReverbProcessor::getCaptureSeconds() const
{
    const int unit = juce::roundToInt (pCapUnit->load());
    if (unit <= 0) return (double) *pCapLen;
    static constexpr int bars[] = { 1, 2, 4, 8 };
    const double bpm = hostBpm.load() > 20.0f ? (double) hostBpm.load() : 120.0;   // 120 if the host gives no tempo
    return (double) bars[juce::jlimit (0, 3, unit - 1)] * (double) hostBeatsPerBar.load() * 60.0 / bpm;
}

void ReverseReverbProcessor::requestClear()
{
    stop();
    renderGeneration++;
    capState.store (CapIdle); capCount.store (0); ++captureVersion;
    renderState.store (RenderNone);
    const juce::SpinLock::ScopedLockType sl (previewLock);
    previewAudio.setSize (0, 0);
    resultPeaks.clear();
    exportFile = juce::File();
}

void ReverseReverbProcessor::regenerate()
{
    if (capState.load() != CapDone) return;
    const int n = capCount.load();
    if (n <= 0) return;
    stop();

    juce::AudioBuffer<float> copy (2, n);
    copy.copyFrom (0, 0, captureBuf, 0, 0, n);
    copy.copyFrom (1, 0, captureBuf, 1, 0, n);

    const auto settings = currentSettings();
    const float mix = *pMix / 100.0f, outDb = *pOutput;
    const auto stamp = paramStamp();
    const double rate = captureRate;
    const int gen = ++renderGeneration;
    renderProgress.store (0.0f);
    renderState.store (RenderBusy);

    pool.addJob ([this, copy, settings, mix, outDb, stamp, rate, gen]
    {
        if (gen != renderGeneration.load()) return;
        auto out = rr::ReverseReverb::renderFinal (copy, rate, settings, mix, outDb, &renderProgress);
        if (gen != renderGeneration.load()) return;

        auto peaks = computePeaks (out.getReadPointer (0), out.getReadPointer (1), out.getNumSamples(), 600, out.getNumSamples());

        // Write a real WAV to a temp folder: this is what DRAG TO DAW hands to the host.
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ReverseReverb");
        dir.createDirectory();
        auto file = dir.getNonexistentChildFile ("ReverseReverb_" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S"), ".wav");
        {
            juce::WavAudioFormat wav;
            if (auto stream = file.createOutputStream())
            {
                if (std::unique_ptr<juce::AudioFormatWriter> w { wav.createWriterFor (stream.get(), rate, 2, 24, {}, 0) })
                {
                    stream.release();   // the writer owns it now
                    w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
                }
            }
        }
        if (gen != renderGeneration.load()) return;
        {
            const juce::SpinLock::ScopedLockType sl (previewLock);
            previewAudio = std::move (out);
            resultPeaks = std::move (peaks);
            exportFile = file;
            renderedStamp = stamp;
            previewLen.store (previewAudio.getNumSamples());
        }
        renderProgress.store (1.0f);
        renderState.store (RenderReady);
    });
}

void ReverseReverbProcessor::play()
{
    if (renderState.load() != RenderReady || previewLen.load() <= 0) return;
    previewPos.store (0);
    previewPlaying.store (true);
}

void ReverseReverbProcessor::stop() { previewPlaying.store (false); }

float ReverseReverbProcessor::getPlayPosition01() const
{
    const int len = previewLen.load();
    return len > 0 ? (float) previewPos.load() / (float) len : 0.0f;
}

float ReverseReverbProcessor::getCaptureProgress01() const
{
    const int t = capTarget.load();
    return t > 0 ? (float) capCount.load() / (float) t : 0.0f;
}

bool ReverseReverbProcessor::isPreviewStale() const
{
    if (renderState.load() != RenderReady) return false;
    const juce::SpinLock::ScopedLockType sl (previewLock);
    return renderedStamp != paramStamp();
}

juce::File ReverseReverbProcessor::getExportFile() const
{
    const juce::SpinLock::ScopedLockType sl (previewLock);
    return exportFile;
}

std::vector<float> ReverseReverbProcessor::getCapturePeaks (int bins) const
{
    const int st = capState.load();
    if (st == CapIdle || st == CapWaiting || captureBuf.getNumSamples() == 0)
        return std::vector<float> ((size_t) bins * 2, 0.0f);
    // Read while the audio thread may still be writing: harmless for a drawing.
    const int n = capCount.load();
    return computePeaks (captureBuf.getReadPointer (0), captureBuf.getReadPointer (1), n, bins,
                         st == CapDone ? n : capTarget.load());
}

std::vector<float> ReverseReverbProcessor::getResultPeaks() const
{
    const juce::SpinLock::ScopedLockType sl (previewLock);
    return resultPeaks;
}

bool ReverseReverbProcessor::exportCopyTo (const juce::File& dest) const
{
    const auto src = getExportFile();
    return src.existsAsFile() && src.copyFileTo (dest);
}

// ------------------------------------------------------------------ presets / state

int ReverseReverbProcessor::getNumPrograms() { return (int) std::size (factoryPresets); }
const juce::String ReverseReverbProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? factoryPresets[i].name : juce::String();
}

void ReverseReverbProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    const auto& p = factoryPresets[i];
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set (ids::tone, p.tone);     set (ids::width, p.width);   set (ids::size, p.size);
    set (ids::depth, p.depth);   set (ids::output, p.output); set (ids::mix, p.mix);
    set (ids::predelay, p.pre);  set (ids::length, p.length); set (ids::mode, (float) p.mode);
    set (ids::trigger, p.trigger);
}

void ReverseReverbProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ReverseReverbProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

bool ReverseReverbProcessor::saveUserPreset (const juce::File& f)
{
    if (auto xml = apvts.copyState().createXml()) return xml->writeTo (f);
    return false;
}

bool ReverseReverbProcessor::loadUserPreset (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
        if (xml->hasTagName (apvts.state.getType()))
        {
            // keep the user's bypass state; presets only carry sound settings
            const bool byp = *pBypass > 0.5f;
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            if (auto* b = apvts.getParameter (ids::bypass)) b->setValueNotifyingHost (byp ? 1.0f : 0.0f);
            return true;
        }
    return false;
}

juce::AudioProcessorEditor* ReverseReverbProcessor::createEditor() { return new ReverseReverbEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReverseReverbProcessor(); }
