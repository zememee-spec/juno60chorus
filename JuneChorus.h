// JuneChorus.h
// Framework-free DSP core for a BBD-style stereo chorus modelled on the
// TC Electronic June-60 V2 pedal (itself a recreation of the Juno-60 chorus).
//
// Signal flow (per sample):
//   mono in -> input low-pass ("BBD anti-alias") -> ring buffer
//   two modulated taps (triangle LFO, left/right in opposite phase)
//   -> + hiss -> output low-pass ("BBD reconstruction") -> mix with dry
//
// Everything that defines the *sound* lives in the TUNING section below.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace june60
{

// ============================================================================
//  TUNING  -  edit these to match the pedal once you have measured it
// ============================================================================

struct ModeTuning
{
    float rateHz;    // triangle LFO rate
    float centreMs;  // mean delay time
    float depthMs;   // peak deviation either side of the centre
};

struct SpeedSet
{
    ModeTuning chorus1;   // button I
    ModeTuning chorus2;   // button II
    ModeTuning chorus12;  // buttons I + II together
};

// Original Juno-60 speeds. This is what the V2's slow DIP-switch setting
// reproduces. NOTE: values are from memory of commonly quoted Juno-60
// measurements, not measured from the pedal. Treat as starting points.
constexpr SpeedSet kJunoSpeeds {
    { 0.50f, 3.3f, 1.7f },
    { 0.83f, 3.3f, 1.7f },
    { 9.00f, 3.3f, 0.5f }
};

// Speeds of the original V1 pedal (the V2's fast DIP-switch setting).
// PLACEHOLDER: the real numbers were not available, measure and replace.
constexpr SpeedSet kPedalV1Speeds {
    { 0.80f, 3.3f, 1.7f },
    { 1.30f, 3.3f, 1.7f },
    { 9.00f, 3.3f, 0.5f }
};

constexpr double kFilterHz = 7500.0;   // BBD band-limiting, applied before and after the delay
constexpr double kFilterQ  = 0.7071;
constexpr float  kDryGain  = 0.7071f;  // dry level while chorus is active
constexpr float  kWetGain  = 0.7071f;  // wet level while chorus is active
constexpr float  kHiss     = 0.00005f; // BBD noise floor, roughly -86 dBFS; 0 disables

// ============================================================================
//  Building blocks
// ============================================================================

constexpr double kPi = 3.14159265358979323846;

struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void setLowpass (double fs, double fc, double q)
    {
        fc = std::min (fc, fs * 0.45);
        const double w0 = 2.0 * kPi * fc / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 - cw) * 0.5) / a0);
        b1 = (float) ((1.0 - cw) / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * cw) / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void reset() { z1 = z2 = 0.0f; }

    float process (float x) noexcept   // transposed direct form II
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct Smoothed
{
    float current = 0.0f, target = 0.0f, coeff = 1.0f;

    void setTimeConstant (double seconds, double fs)
    {
        coeff = 1.0f - (float) std::exp (-1.0 / (seconds * fs));
    }
    void snap() { current = target; }
    float next() noexcept { current += coeff * (target - current); return current; }
};

// Ring buffer with 4-point Hermite interpolation for fractional reads.
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 16;
        while (size < maxDelaySamples + 8)
            size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = (uint32_t) (size - 1);
        writePos = 0;
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        writePos = 0;
    }

    void push (float x) noexcept
    {
        buf[writePos & mask] = x;
        ++writePos;
    }

    // delay is in samples (>= 2). 0 would be the most recently pushed sample.
    float read (float delay) const noexcept
    {
        const int di = (int) delay;
        const float t = 1.0f - (delay - (float) di);
        const uint32_t j = writePos - 1u - (uint32_t) di;

        const float y0 = buf[(j - 2u) & mask];
        const float y1 = buf[(j - 1u) & mask];
        const float y2 = buf[j & mask];
        const float y3 = buf[(j + 1u) & mask];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

private:
    std::vector<float> buf;
    uint32_t mask = 15;
    uint32_t writePos = 0;
};

// ============================================================================
//  The chorus
// ============================================================================

class JuneChorus
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        maxDelaySamples = (float) (0.011 * fs);
        delay.prepare ((int) std::ceil (0.012 * fs));

        inLp.setLowpass (fs, kFilterHz, kFilterQ);
        outLpL.setLowpass (fs, kFilterHz, kFilterQ);
        outLpR.setLowpass (fs, kFilterHz, kFilterQ);

        for (auto* s : { &rate, &centre, &depth })
            s->setTimeConstant (0.05, fs);
        for (auto* s : { &wet, &mono })
            s->setTimeConstant (0.02, fs);

        reset();
    }

    void reset()
    {
        delay.reset();
        inLp.reset();
        outLpL.reset();
        outLpR.reset();
        phase = 0.0f;
        snap();
    }

    // Jump the smoothed parameters straight to their targets (use after prepare).
    void snap()
    {
        for (auto* s : { &rate, &centre, &depth, &wet, &mono })
            s->snap();
    }

    // c1 / c2 are the two mode buttons. With neither pressed the effect
    // fades to dry. useV1Speeds selects the V1 pedal speed set.
    void setMode (bool c1, bool c2, bool stereo, bool useV1Speeds) noexcept
    {
        const SpeedSet& set = useV1Speeds ? kPedalV1Speeds : kJunoSpeeds;
        const ModeTuning* m = nullptr;

        if (c1 && c2)  m = &set.chorus12;
        else if (c1)   m = &set.chorus1;
        else if (c2)   m = &set.chorus2;

        if (m != nullptr)
        {
            rate.target   = m->rateHz;
            centre.target = m->centreMs;
            depth.target  = m->depthMs;
            wet.target    = 1.0f;
        }
        else
        {
            wet.target = 0.0f;
        }

        mono.target = stereo ? 0.0f : 1.0f;
    }

    void process (float in, float& outL, float& outR) noexcept
    {
        const float r = rate.next();
        const float c = centre.next();
        const float d = depth.next();
        const float w = wet.next();
        const float m = mono.next();

        phase += r / (float) fs;
        if (phase >= 1.0f)
            phase -= 1.0f;

        const float tri = 1.0f - 4.0f * std::fabs (phase - 0.5f);   // -1 .. +1
        const float msToSamples = (float) fs * 0.001f;

        lastDelayL = clampDelay ((c + d * tri) * msToSamples);
        const float delayR = clampDelay ((c - d * tri) * msToSamples);

        delay.push (inLp.process (in));

        const float wetL = outLpL.process (delay.read (lastDelayL) + kHiss * nextNoise());
        const float wetR = outLpR.process (delay.read (delayR)     + kHiss * nextNoise());

        const float dry = in * (1.0f - w * (1.0f - kDryGain));
        const float g = kWetGain * w;

        const float stereoL = dry + g * wetL;
        const float stereoR = dry + g * wetR;
        const float monoOut = dry + g * 0.5f * (wetL + wetR);

        outL = stereoL + m * (monoOut - stereoL);
        outR = stereoR + m * (monoOut - stereoR);
    }

    // Left-channel delay of the last processed sample, in milliseconds (for testing/metering).
    float lastDelayMsL() const noexcept { return lastDelayL / ((float) fs * 0.001f); }

private:
    float clampDelay (float samples) const noexcept
    {
        return std::clamp (samples, 2.0f, maxDelaySamples);
    }

    float nextNoise() noexcept   // xorshift32, uniform in [-1, 1)
    {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return (float) (int32_t) seed * (1.0f / 2147483648.0f);
    }

    double fs = 44100.0;
    float maxDelaySamples = 400.0f;
    float phase = 0.0f;
    float lastDelayL = 0.0f;
    uint32_t seed = 0x1234abcdu;

    DelayLine delay;
    Biquad inLp, outLpL, outLpR;
    Smoothed rate, centre, depth, wet, mono;
};

} // namespace june60
