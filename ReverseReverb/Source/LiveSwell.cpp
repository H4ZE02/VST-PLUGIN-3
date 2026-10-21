#include "LiveSwell.h"
#include <cmath>

namespace rr
{
void LiveSwell::prepare (double sampleRate)
{
    stopThread (3000);
    fs = sampleRate;
    burstLen = (int) std::lround (burstSec * fs);
    latency  = (int) std::ceil ((maxLengthSec + maxPreDelaySec + burstSec + marginSec) * fs);

    int size = 1;
    while (size < latency + 65536) size <<= 1;
    ringMask = size - 1;
    inRingL.assign ((size_t) size, 0.0f); inRingR.assign ((size_t) size, 0.0f);
    swellL.assign ((size_t) size, 0.0f);  swellR.assign ((size_t) size, 0.0f);

    for (auto& s : slots)
    {
        s.state.store (Free);
        s.burstL.assign ((size_t) burstLen, 0.0f);
        s.burstR.assign ((size_t) burstLen, 0.0f);
    }
    numPending = 0;
    now = 0;
    fast = slow = 0.0f;
    refractory = 0;
    refractoryLen = (int) (0.15 * fs);
    auto coef = [this] (double sec) { return (float) (1.0 - std::exp (-1.0 / (sec * fs))); };
    aF = coef (0.0005); rF = coef (0.02);
    aS = coef (0.05);   rS = coef (0.4);
    startThread (juce::Thread::Priority::normal);
}

void LiveSwell::reset()
{
    std::fill (inRingL.begin(), inRingL.end(), 0.0f);
    std::fill (inRingR.begin(), inRingR.end(), 0.0f);
    std::fill (swellL.begin(), swellL.end(), 0.0f);
    std::fill (swellR.begin(), swellR.end(), 0.0f);
    numPending = 0;
    for (auto& s : slots)
    {
        int expected = Queued;
        s.state.compare_exchange_strong (expected, Free);
        expected = Ready;
        s.state.compare_exchange_strong (expected, Free);
    }
    fast = slow = 0.0f;
}

void LiveSwell::process (const Block& b, const RenderSettings& s, float trigger01, bool detect)
{
    lastSettings = s;
    collectReady();

    const float ratio = 4.0f - 2.6f * trigger01;   // more sensitive -> lower ratio
    constexpr float floorLevel = 0.0035f;           // about -49 dBFS
    const int preDetect = (int) (0.004 * fs);

    for (int i = 0; i < b.numSamples; ++i)
    {
        const int w = (int) (now & ringMask);
        inRingL[(size_t) w] = b.inL[i];
        inRingR[(size_t) w] = b.inR[i];

        const int rd = (int) ((now - latency) & ringMask);
        b.dryL[i] = now >= latency ? inRingL[(size_t) rd] : 0.0f;
        b.dryR[i] = now >= latency ? inRingR[(size_t) rd] : 0.0f;

        b.wetL[i] = swellL[(size_t) w]; swellL[(size_t) w] = 0.0f;
        b.wetR[i] = swellR[(size_t) w]; swellR[(size_t) w] = 0.0f;

        const float m = 0.5f * (std::abs (b.inL[i]) + std::abs (b.inR[i]));
        fast += (m > fast ? aF : rF) * (m - fast);
        slow += (m > slow ? aS : rS) * (m - slow);
        if (refractory > 0) --refractory;
        else if (detect && fast > floorLevel && fast > slow * ratio && numPending < numSlots)
        {
            pending[numPending++] = { juce::jmax<int64_t> (0, now - preDetect), s };
            refractory = refractoryLen;
        }
        ++now;
    }
    launchPending();
}

void LiveSwell::launchPending()
{
    int keep = 0;
    for (int p = 0; p < numPending; ++p)
    {
        if (pending[p].start + burstLen > now) { pending[keep++] = pending[p]; continue; }
        Slot* slot = nullptr;
        for (auto& s : slots) if (s.state.load (std::memory_order_acquire) == Free) { slot = &s; break; }
        if (slot == nullptr) continue;      // render thread busy: skip this swell
        for (int n = 0; n < burstLen; ++n)
        {
            const int idx = (int) ((pending[p].start + n) & ringMask);
            slot->burstL[(size_t) n] = inRingL[(size_t) idx];
            slot->burstR[(size_t) n] = inRingR[(size_t) idx];
        }
        slot->settings = pending[p].settings;
        slot->burstStart = pending[p].start;
        slot->state.store (Queued, std::memory_order_release);
        wake.signal();
    }
    numPending = keep;
}

void LiveSwell::collectReady()
{
    for (auto& s : slots)
    {
        if (s.state.load (std::memory_order_acquire) != Ready) continue;
        const int pre = (int) std::lround ((double) s.settings.preDelayMs * 0.001 * fs);
        const int64_t startAbs = s.burstStart + latency - pre - s.tail;
        const int len = s.result.getNumSamples();
        const int j0 = (int) juce::jmax<int64_t> (0, now - startAbs);   // late? skip the part already gone
        const float* rl = s.result.getReadPointer (0);
        const float* rr = s.result.getReadPointer (1);
        for (int j = j0; j < len; ++j)
        {
            const int idx = (int) ((startAbs + j) & ringMask);
            swellL[(size_t) idx] += rl[j];
            swellR[(size_t) idx] += rr[j];
        }
        s.state.store (Free, std::memory_order_release);
    }
}

void LiveSwell::run()
{
    while (! threadShouldExit())
    {
        bool did = false;
        for (auto& s : slots)
        {
            if (s.state.load (std::memory_order_acquire) != Queued) continue;
            s.state.store (Rendering, std::memory_order_relaxed);
            int tail = 0;
            s.result = ReverseReverb::renderWet (s.burstL.data(), s.burstR.data(), burstLen, fs,
                                                 s.settings, tail);
            s.tail = tail;
            s.state.store (Ready, std::memory_order_release);
            did = true;
        }
        if (! did) wake.wait (100);
    }
}
} // namespace rr
