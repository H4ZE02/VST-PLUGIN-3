#pragma once
#include "ReverseReverbEngine.h"
#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

namespace rr
{
// Real-time reverse reverb. A swell has to be computed from audio that has not
// "happened" yet, so the dry signal is delayed by a fixed latency (reported to
// the host for delay compensation). Each detected onset triggers a background
// render of its swell, which is mixed in so it peaks exactly on the hit.
class LiveSwell : private juce::Thread
{
public:
    static constexpr double maxLengthSec  = 2.5;
    static constexpr double maxPreDelaySec = 0.2;
    static constexpr double burstSec      = 0.25;  // audio after the onset fed to the reverb
    static constexpr double marginSec     = 0.15;  // time allowed for the render thread

    LiveSwell() : juce::Thread ("ReverseReverb render") {}
    ~LiveSwell() override { stopThread (3000); }

    void prepare (double sampleRate);
    void reset();
    int  getLatencySamples() const noexcept { return latency; }

    struct Block
    {
        const float* inL; const float* inR;
        float* dryL; float* dryR;      // delayed dry out
        float* wetL; float* wetR;      // swell out
        int numSamples;
    };
    // numSamples must be <= the maxBlock given to prepare (processor chunks it)
    void process (const Block& b, const RenderSettings& s, float trigger01, bool detect);

private:
    void run() override;

    enum SlotState { Free = 0, Queued, Rendering, Ready };
    struct Slot
    {
        std::atomic<int> state { Free };
        std::vector<float> burstL, burstR;
        RenderSettings settings;
        int64_t burstStart = 0;
        int tail = 0;
        juce::AudioBuffer<float> result;
    };

    void collectReady();
    void launchPending();

    double fs = 44100.0;
    int latency = 0, burstLen = 0, ringMask = 0;
    std::vector<float> inRingL, inRingR, swellL, swellR;
    int64_t now = 0;

    static constexpr int numSlots = 8;
    Slot slots[numSlots];
    juce::WaitableEvent wake;

    struct Pending { int64_t start; RenderSettings settings; };
    Pending pending[numSlots];
    int numPending = 0;

    float fast = 0, slow = 0;
    float aF = 0, rF = 0, aS = 0, rS = 0;
    int refractory = 0, refractoryLen = 0;
    RenderSettings lastSettings;
};
} // namespace rr
