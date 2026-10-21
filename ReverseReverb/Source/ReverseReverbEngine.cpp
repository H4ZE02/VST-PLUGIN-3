#include "ReverseReverbEngine.h"
#include <cmath>

namespace rr
{
namespace
{
    // Mutually prime delay lengths (samples @ 44.1 kHz)
    constexpr std::array<int, FDN::N> baseDelays { 1087, 1283, 1481, 1699, 1907, 2129, 2347, 2593 };
    // Orthogonal sign patterns used for stereo injection / extraction
    constexpr std::array<float, FDN::N> patL { 1, 1, -1, -1, 1, -1, 1, -1 };
    constexpr std::array<float, FDN::N> patR { 1, -1, 1, -1, -1, 1, 1, -1 };
    constexpr std::array<int, 4> apLens { 142, 107, 379, 277 };

    struct ModeParams { float wetGain; float curve; float drive; };
    ModeParams modeParams (int mode)
    {
        switch (juce::jlimit (0, 2, mode))
        {
            case 0:  return { 0.60f, 0.55f, 0.0f };  // subtle: gentle, late swell
            case 2:  return { 1.80f, 0.00f, 2.0f };  // extreme: loud, saturated swell
            default: return { 1.00f, 0.00f, 0.0f };  // balanced
        }
    }
}

void FDN::prepare (double fs, float size01, float rt60)
{
    const double scale = (fs / 44100.0) * (0.5 + 1.5 * (double) size01);
    rt60 = juce::jmax (0.05f, rt60);
    for (int i = 0; i < N; ++i)
    {
        const int len = juce::jmax (32, (int) std::lround ((double) baseDelays[(size_t) i] * scale));
        lines[(size_t) i].assign ((size_t) len, 0.0f);
        pos[(size_t) i] = 0;
        lp[(size_t) i] = 0.0f;
        gain[(size_t) i] = (float) std::pow (10.0, -3.0 * (double) len / (fs * (double) rt60));
    }
    damp = 0.12f + 0.33f * size01; // bigger rooms are darker
    const double apScale = fs / 44100.0;
    for (size_t i = 0; i < 4; ++i)
    {
        apL[i].init ((int) std::lround ((double) apLens[i] * apScale));
        apR[i].init ((int) std::lround ((double) (apLens[i] + 23) * apScale));
    }
}

void FDN::process (float inL, float inR, float& outL, float& outR) noexcept
{
    for (auto& a : apL) inL = a.process (inL);
    for (auto& a : apR) inR = a.process (inR);

    std::array<float, N> d {};
    float sum = 0.0f, oL = 0.0f, oR = 0.0f;
    for (size_t i = 0; i < (size_t) N; ++i)
    {
        d[i] = lines[i][(size_t) pos[i]];
        oL += patL[i] * d[i];
        oR += patR[i] * d[i];
        lp[i] += (1.0f - damp) * (d[i] - lp[i]);   // one-pole damping in the loop
        d[i] = lp[i] * gain[i];
        sum += d[i];
    }
    const float k = (2.0f / (float) N) * sum;
    for (size_t i = 0; i < (size_t) N; ++i)
    {
        const float in = 0.35f * (inL * patL[i] + inR * patR[i]);
        lines[i][(size_t) pos[i]] = d[i] - k + in;
        if (++pos[i] >= (int) lines[i].size()) pos[i] = 0;
    }
    constexpr float outScale = 0.35355339f; // 1/sqrt(8)
    outL = oL * outScale;
    outR = oR * outScale;
}

juce::AudioBuffer<float> ReverseReverb::renderWet (const float* inL, const float* inR, int numIn,
                                                   double fs, const RenderSettings& s, int& tailSamples,
                                                   std::atomic<float>* progress, float p0, float p1)
{
    const int T = juce::jmax (64, (int) std::lround ((double) s.lengthSec * fs));
    tailSamples = T;
    const int total = numIn + T;
    const auto mp = modeParams (s.mode);

    juce::AudioBuffer<float> wet (2, total);
    wet.clear();
    float* wl = wet.getWritePointer (0);
    float* wr = wet.getWritePointer (1);

    // 1) reverb of the REVERSED input, followed by silence so the tail rings out
    FDN fdn;
    fdn.prepare (fs, s.size, s.lengthSec * 0.9f);
    const int fade = juce::jmin (numIn / 4, (int) (0.002 * fs));
    for (int n = 0; n < total; ++n)
    {
        float l = 0.0f, r = 0.0f;
        if (n < numIn)
        {
            const int src = numIn - 1 - n;
            float g = 1.0f;
            if (n < fade) g = (float) n / (float) fade;                      // soft edges so
            else if (n >= numIn - fade) g = (float) (numIn - 1 - n) / (float) fade; // bursts never click
            l = inL[src] * g;
            r = inR[src] * g;
        }
        fdn.process (l, r, wl[n], wr[n]);
        if (progress != nullptr && (n & 8191) == 0)
            progress->store (p0 + (p1 - p0) * 0.8f * (float) n / (float) total);
    }

    // 2) reverse the result: the tail now builds up and lands on the sound
    std::reverse (wl, wl + total);
    std::reverse (wr, wr + total);

    // 3) shape: swell curve, saturation, depth, calibration, edge fade
    const float gainWet = s.depth * 1.5f * mp.wetGain * ReverseReverb::wetCalibration;
    const int edge = juce::jmin (T, (int) (0.008 * fs));
    for (int n = 0; n < total; ++n)
    {
        float env = 1.0f;
        if (n < T && mp.curve > 0.0f)
            env = std::pow ((float) n / (float) T, mp.curve);
        if (n < edge) env *= (float) n / (float) edge;
        float l = wl[n] * env * gainWet;
        float r = wr[n] * env * gainWet;
        if (mp.drive > 0.0f)
        {
            l = std::tanh (l * mp.drive) / mp.drive;
            r = std::tanh (r * mp.drive) / mp.drive;
        }
        wl[n] = l; wr[n] = r;
    }

    // 4) tone: tilt EQ (+ a lowpass when dark) -- applied after the reversal
    {
        auto hs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (fs, 3000.0f, 0.7f,
                      juce::Decibels::decibelsToGain (s.tone * 10.0f));
        auto ls = juce::dsp::IIR::Coefficients<float>::makeLowShelf (fs, 250.0f, 0.7f,
                      juce::Decibels::decibelsToGain (-s.tone * 6.0f));
        const float cutoff = s.tone < 0.0f ? 18000.0f * std::pow (0.12f, -s.tone) : 20000.0f;
        const float nyq = (float) (0.45 * fs);
        auto lpf = juce::dsp::IIR::Coefficients<float>::makeLowPass (fs, juce::jmin (cutoff, nyq));
        for (int ch = 0; ch < 2; ++ch)
        {
            juce::dsp::IIR::Filter<float> f1, f2, f3;
            f1.coefficients = hs; f2.coefficients = ls; f3.coefficients = lpf;
            float* p = wet.getWritePointer (ch);
            for (int n = 0; n < total; ++n)
                p[n] = f3.processSample (f2.processSample (f1.processSample (p[n])));
        }
    }

    // 5) width via mid/side
    for (int n = 0; n < total; ++n)
    {
        const float m = 0.5f * (wl[n] + wr[n]);
        const float sd = 0.5f * (wl[n] - wr[n]) * s.width;
        wl[n] = m + sd;
        wr[n] = m - sd;
    }
    if (progress != nullptr) progress->store (p0 + (p1 - p0) * 0.9f);
    return wet;
}

juce::AudioBuffer<float> ReverseReverb::renderFinal (const juce::AudioBuffer<float>& in, double fs,
                                                     const RenderSettings& s, float mix, float outDb,
                                                     std::atomic<float>* progress)
{
    const int numIn = in.getNumSamples();
    const float* iL = in.getReadPointer (0);
    const float* iR = in.getReadPointer (in.getNumChannels() > 1 ? 1 : 0);

    int T = 0;
    auto wet = renderWet (iL, iR, numIn, fs, s, T, progress, 0.0f, 0.95f);
    const int pre = (int) std::lround ((double) s.preDelayMs * 0.001 * fs);
    const int total = wet.getNumSamples() + pre;

    juce::AudioBuffer<float> out (2, total);
    out.clear();
    const float wetG = std::sin (mix * juce::MathConstants<float>::halfPi);
    const float dryG = std::cos (mix * juce::MathConstants<float>::halfPi);
    const float outG = juce::Decibels::decibelsToGain (outDb);

    for (int ch = 0; ch < 2; ++ch)
    {
        float* o = out.getWritePointer (ch);
        const float* w = wet.getReadPointer (ch);
        const float* d = ch == 0 ? iL : iR;
        for (int n = 0; n < wet.getNumSamples(); ++n) o[n] += w[n] * wetG;
        for (int n = 0; n < numIn; ++n)               o[T + pre + n] += d[n] * dryG;
        for (int n = 0; n < total; ++n)               o[n] *= outG;
    }

    // Peak protection: an export should never be clipped
    const float peak = out.getMagnitude (0, total);
    if (peak > 0.98f) out.applyGain (0.98f / peak);

    // Trim the near-silent first few ms of the -60 dB tail start
    if (progress != nullptr) progress->store (1.0f);
    return out;
}
} // namespace rr
