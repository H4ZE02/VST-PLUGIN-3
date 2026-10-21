#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "LiveSwell.h"

namespace ids
{
    inline constexpr auto tone = "tone";       inline constexpr auto width = "width";
    inline constexpr auto size = "size";       inline constexpr auto depth = "depth";
    inline constexpr auto output = "output";   inline constexpr auto mix = "mix";
    inline constexpr auto predelay = "predelay"; inline constexpr auto length = "length";
    inline constexpr auto mode = "mode";       inline constexpr auto trigger = "trigger";
    inline constexpr auto live = "live";       inline constexpr auto capLen = "caplen";
    inline constexpr auto bypass = "bypass";
    inline constexpr auto capUnit = "capunit";
}

class ReverseReverbProcessor : public juce::AudioProcessor,
                               private juce::AsyncUpdater
{
public:
    ReverseReverbProcessor();
    ~ReverseReverbProcessor() override;

    // ---- AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Reverse Reverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return rr::LiveSwell::maxLengthSec; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (ids::bypass); }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- parameters / presets ----
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    rr::RenderSettings currentSettings() const;
    bool saveUserPreset (const juce::File&);
    bool loadUserPreset (const juce::File&);

    // ---- capture / preview (UI side) ----
    enum CapState { CapIdle = 0, CapWaiting, CapRecording, CapDone };
    enum RenderState { RenderNone = 0, RenderBusy, RenderReady };

    void requestCapture();              // toggles: start waiting for audio, or cancel
    void requestClear();
    void regenerate();                  // re-render the captured sound with current settings
    void play();
    void stop();
    bool isPlaying() const { return previewPlaying.load(); }
    float getPlayPosition01() const;
    int  getCapState() const { return capState.load(); }
    int  getRenderState() const { return renderState.load(); }
    float getRenderProgress() const { return renderProgress.load(); }
    float getCaptureProgress01() const;
    int  getCaptureVersion() const { return captureVersion.load(); }
    bool hasCapture() const { return capState.load() == CapDone; }
    bool isPreviewStale() const;
    juce::File getExportFile() const;
    std::vector<float> getCapturePeaks (int bins) const;       // (min,max) pairs
    std::vector<float> getResultPeaks() const;                 // (min,max) pairs
    bool exportCopyTo (const juce::File& dest) const;

    double getCaptureSeconds() const;   // seconds or bars, resolved with the host tempo
    std::atomic<float> hostBpm { 0.0f }, hostBeatsPerBar { 4.0f };

    // ---- meters ----
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };
    std::atomic<bool> inClip { false }, outClip { false };

    double getCurrentSampleRate() const { return sr; }

private:
    void processInternal (juce::AudioBuffer<float>&, bool bypassed);
    void handleAsyncUpdate() override;
    std::array<float, 10> paramStamp() const;

    double sr = 44100.0;
    rr::LiveSwell live;
    juce::AudioBuffer<float> scratch;     // dry/wet scratch
    int maxChunk = 512;
    bool lastLive = true;
    juce::SmoothedValue<float> mixSmooth, gainSmooth;

    std::atomic<float> *pTone, *pWidth, *pSize, *pDepth, *pOutput, *pMix, *pPre, *pLength,
                       *pMode, *pTrigger, *pLive, *pCapLen, *pBypass, *pCapUnit;

    // capture
    juce::AudioBuffer<float> captureBuf;
    std::atomic<int> capState { CapIdle }, capCount { 0 }, capTarget { 0 }, captureVersion { 0 };
    std::atomic<bool> capRequest { false }, clearRequest { false }, renderAfterCapture { false };
    double captureRate = 0.0;

    // preview
    mutable juce::SpinLock previewLock;
    juce::AudioBuffer<float> previewAudio;
    std::vector<float> resultPeaks;
    juce::File exportFile;
    std::array<float, 10> renderedStamp {};
    std::atomic<int> renderState { RenderNone }, renderGeneration { 0 };
    std::atomic<float> renderProgress { 0.0f };
    std::atomic<bool> previewPlaying { false };
    std::atomic<int> previewPos { 0 }, previewLen { 0 };
    juce::ThreadPool pool { 1 };

    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseReverbProcessor)
};
