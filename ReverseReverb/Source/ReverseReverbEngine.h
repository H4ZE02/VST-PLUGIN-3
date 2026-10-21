#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>

namespace rr
{
// Everything that shapes one reverse-reverb swell. Plain data so it can be
// copied from the audio thread to a render thread.
struct RenderSettings
{
    float tone      = 0.0f;   // -1 (dark) .. +1 (bright)
    float width     = 1.0f;   //  0 (mono) ..  2 (extra wide)
    float size      = 0.5f;   //  0..1 room size
    float depth     = 0.7f;   //  0..1 swell intensity
    float preDelayMs = 30.0f; //  gap between end of swell and the sound
    float lengthSec = 1.2f;   //  swell length
    int   mode      = 1;      //  0 subtle, 1 balanced, 2 extreme
};

// 8-line feedback delay network with Householder mixing, input diffusion and
// per-line damping. Decay time (RT60) is exact because each line's gain is
// derived from its own delay length.
class FDN
{
public:
    static constexpr int N = 8;
    void prepare (double sampleRate, float size01, float rt60Seconds);
    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    struct Allpass
    {
        std::vector<float> buf; int pos = 0; float g = 0.65f;
        void init (int len) { buf.assign ((size_t) juce::jmax (2, len), 0.0f); pos = 0; }
        float process (float x) noexcept
        {
            const float d = buf[(size_t) pos];
            const float w = x + g * d;
            buf[(size_t) pos] = w;
            if (++pos >= (int) buf.size()) pos = 0;
            return d - g * w;
        }
    };

    std::array<std::vector<float>, N> lines;
    std::array<int, N> pos {};
    std::array<float, N> gain {};
    std::array<float, N> lp {};
    std::array<Allpass, 4> apL, apR;
    float damp = 0.3f;
};

// Reverse reverb = reverse( reverb( reverse (x) ) ).  Equivalent to convolving
// the sound with a time-reversed room impulse response, so the reverb swells
// UP INTO each hit instead of decaying after it.
class ReverseReverb
{
public:
    // Wet-only swell. Returned buffer has numIn + tailSamples samples; sample
    // (tailSamples + n) lines up in time with input sample n.
    static juce::AudioBuffer<float> renderWet (const float* inL, const float* inR, int numIn,
                                               double sampleRate, const RenderSettings& s,
                                               int& tailSamples,
                                               std::atomic<float>* progress = nullptr,
                                               float progressStart = 0.0f, float progressEnd = 1.0f);

    // Final printed result for the PREVIEW / export: swell + (pre-delayed) dry,
    // blended with the DRY/WET control and OUTPUT gain, peak-protected.
    static juce::AudioBuffer<float> renderFinal (const juce::AudioBuffer<float>& input,
                                                 double sampleRate, const RenderSettings& s,
                                                 float mix01, float outputDb,
                                                 std::atomic<float>* progress = nullptr);

    static constexpr float wetCalibration = 1.0f; // tuned in EngineTest
};
} // namespace rr
