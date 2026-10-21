// Console sanity tests for the DSP. Build with -DRR_BUILD_TESTS=ON.
#include "LiveSwell.h"
#include <iostream>
#include <cmath>
#include <random>

using namespace rr;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::cout << "FAIL: " << msg << "\n"; } else std::cout << "ok:   " << msg << "\n"; } while (0)

static float rmsRange (const juce::AudioBuffer<float>& b, int a, int e)
{
    double s = 0; a = juce::jmax (0, a); e = juce::jmin (e, b.getNumSamples());
    for (int n = a; n < e; ++n) s += b.getSample (0, n) * b.getSample (0, n) + b.getSample (1, n) * b.getSample (1, n);
    return (float) std::sqrt (s / (2.0 * juce::jmax (1, e - a)));
}

int main()
{
    const double fs = 48000.0;
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);

    // a 0.2 s decaying noise "snare"
    const int N = (int) (0.4 * fs);
    juce::AudioBuffer<float> snare (2, N);
    for (int n = 0; n < N; ++n)
    {
        const float env = std::exp (-(float) n / (0.05f * (float) fs));
        snare.setSample (0, n, 0.8f * env * u (rng));
        snare.setSample (1, n, 0.8f * env * u (rng));
    }

    RenderSettings s; s.lengthSec = 1.5f; s.preDelayMs = 40; s.depth = 0.7f;
    for (int mode = 0; mode < 3; ++mode)
    {
        s.mode = mode;
        auto out = ReverseReverb::renderFinal (snare, fs, s, 0.5f, 0.0f);
        const int T = (int) (s.lengthSec * fs), pre = (int) (0.04 * fs);
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < out.getNumSamples(); ++n)
            finite = finite && std::isfinite (out.getSample (ch, n));
        const float peak = out.getMagnitude (0, out.getNumSamples());
        const float early = rmsRange (out, T / 2, T / 2 + 2000);                // mid-swell
        const float late  = rmsRange (out, T - 4800, T);                        // just before the hit
        const float hit   = rmsRange (out, T + pre, T + pre + 2400);
        std::cout << "mode " << mode << ": peak " << peak << " early " << early << " late " << late
                  << " hit " << hit << " len " << out.getNumSamples() << "\n";
        CHECK (finite, "mode " << mode << " finite");
        CHECK (peak <= 0.981f && peak > 0.05f, "mode " << mode << " peak in range");
        CHECK (late > early * 1.5f, "mode " << mode << " swell builds toward the hit");
    }

    // wet-only level vs dry (calibration)
    {
        s.mode = 1; s.depth = 1.0f; int T = 0;
        auto wet = ReverseReverb::renderWet (snare.getReadPointer (0), snare.getReadPointer (1), N, fs, s, T);
        const float dryR = rmsRange (snare, 0, N);
        const float wetR = rmsRange (wet, T - (int) (0.3 * fs), T + N);
        std::cout << "calibration: dry rms " << dryR << " wet rms near hit " << wetR << "\n";
    }

    // Live path: click train through LiveSwell
    {
        LiveSwell live; live.prepare (fs);
        const int latency = live.getLatencySamples();
        const int blocks = (int) ((latency + 6 * fs) / 512);
        std::vector<float> inL (512), inR (512), dL (512), dR (512), wL (512), wR (512);
        std::vector<float> allDry, allWet;
        s.mode = 1; s.depth = 0.8f; s.lengthSec = 1.0f; s.preDelayMs = 0;
        int64_t pos = 0; int clickAt = (int) (1.0 * fs);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i, ++pos)
            {
                const int64_t rel = pos - clickAt;
                float v = 0.0f;
                if (rel >= 0 && rel < (int) (0.15 * fs)) v = 0.9f * u (rng) * std::exp (-(float) rel / (0.03f * (float) fs));
                inL[(size_t) i] = inR[(size_t) i] = v;
            }
            live.process ({ inL.data(), inR.data(), dL.data(), dR.data(), wL.data(), wR.data(), 512 }, s, 0.5f, true);
            allDry.insert (allDry.end(), dL.begin(), dL.end());
            allWet.insert (allWet.end(), wL.begin(), wL.end());
            if (b == 10) juce::Thread::sleep (0); // let worker run
            juce::Thread::sleep (3);               // simulate real-time pacing of the render thread
        }
        int dryHit = -1; for (size_t n = 0; n < allDry.size(); ++n) if (std::abs (allDry[n]) > 0.05f) { dryHit = (int) n; break; }
        int wetPeak = -1; float wp = 0; for (size_t n = 0; n < allWet.size(); ++n) if (std::abs (allWet[n]) > wp) { wp = std::abs (allWet[n]); wetPeak = (int) n; }
        double eBefore = 0, eAfter = 0;
        for (int n = dryHit - (int) (0.5 * fs); n < dryHit; ++n) eBefore += allWet[(size_t) n] * allWet[(size_t) n];
        for (int n = dryHit - (int) (1.5 * fs); n < dryHit - (int) (1.0 * fs); ++n) eAfter += allWet[(size_t) n] * allWet[(size_t) n];
        std::cout << "latency " << latency << " (" << latency / fs << " s); dry hit at " << dryHit
                  << " expected " << clickAt + latency << "; wet peak at " << wetPeak << " (" << (wetPeak - dryHit) / fs * 1000 << " ms vs hit), wet peak " << wp << "\n";
        CHECK (std::abs (dryHit - (clickAt + latency)) < 600, "dry delayed by exactly the reported latency");
        CHECK (wp > 0.02f, "swell was produced");
        CHECK (std::abs (wetPeak - dryHit) < (int) (0.35 * fs), "swell peaks around the hit");
        CHECK (eBefore > eAfter * 5.0, "swell energy builds up into the hit");
    }
    std::cout << (failures == 0 ? "ALL PASSED\n" : "SOME FAILED\n");
    return failures;
}
