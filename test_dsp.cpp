// Sanity tests for the framework-free DSP core.
// Build: g++ -std=c++17 -O2 test_dsp.cpp -o test_dsp && ./test_dsp

#include <cmath>
#include <cstdio>
#include <vector>

#include "JuneChorus.h"

static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok)
        ++failures;
}

int main()
{
    const double fs = 44100.0;

    // 1. Hermite delay line must reproduce a linear ramp exactly at fractional delays.
    {
        june60::DelayLine dl;
        dl.prepare (1000);
        const int N = 500;
        for (int n = 0; n < N; ++n)
            dl.push ((float) n);

        const float newest = (float) (N - 1);
        double worst = 0.0;
        for (float d = 2.0f; d < 300.0f; d += 0.37f)
            worst = std::max (worst, (double) std::fabs (dl.read (d) - (newest - d)));
        check (worst < 1e-3, "delay line: fractional reads exact on a ramp");

        check (std::fabs (dl.read (10.0f) - (newest - 10.0f)) < 1e-4f, "delay line: integer delay exact");
    }

    // 2. Chorus I, stereo: finite, bounded, L and R differ, LFO range and rate are right.
    {
        june60::JuneChorus ch;
        ch.prepare (fs);
        ch.setMode (true, false, true, false);
        ch.snap();

        const int total = (int) (12.0 * fs);
        double sumDiff = 0.0, peak = 0.0;
        bool finite = true;

        float dMin = 1e9f, dMax = -1e9f;
        int crossings = 0;
        float prev = 0.0f;
        const int measureStart = (int) (2.0 * fs);

        for (int n = 0; n < total; ++n)
        {
            const float in = 0.5f * std::sin (2.0f * (float) june60::kPi * 1000.0f * (float) n / (float) fs);
            float l = 0.0f, r = 0.0f;
            ch.process (in, l, r);

            if (! std::isfinite (l) || ! std::isfinite (r))
                finite = false;
            peak = std::max (peak, (double) std::max (std::fabs (l), std::fabs (r)));
            sumDiff += std::fabs (l - r);

            if (n >= measureStart)
            {
                const float d = ch.lastDelayMsL();
                dMin = std::min (dMin, d);
                dMax = std::max (dMax, d);

                const float centred = d - 3.3f;
                if (prev < 0.0f && centred >= 0.0f)
                    ++crossings;
                prev = centred;
            }
        }

        check (finite, "chorus I stereo: output finite");
        check (peak < 1.5, "chorus I stereo: output bounded");
        check (sumDiff > 1.0, "chorus I stereo: L and R differ");
        check (dMin > 1.5f && dMin < 1.7f && dMax > 4.9f && dMax < 5.1f, "chorus I: delay swings ~1.6 to ~5.0 ms");

        const double measuredRate = crossings / 10.0;   // 10 s measured window
        std::printf ("      measured LFO rate: %.2f Hz (expected 0.50)\n", measuredRate);
        check (std::fabs (measuredRate - 0.5) < 0.11, "chorus I: LFO rate ~0.5 Hz");
    }

    // 3. Mono output: L == R once the mono smoother has settled.
    {
        june60::JuneChorus ch;
        ch.prepare (fs);
        ch.setMode (true, true, false, false);
        ch.snap();

        double worst = 0.0;
        for (int n = 0; n < (int) (2.0 * fs); ++n)
        {
            const float in = 0.5f * std::sin (2.0f * (float) june60::kPi * 440.0f * (float) n / (float) fs);
            float l = 0.0f, r = 0.0f;
            ch.process (in, l, r);
            if (n > (int) fs)
                worst = std::max (worst, (double) std::fabs (l - r));
        }
        check (worst < 1e-6, "chorus I+II mono: L equals R");
    }

    // 4. Neither button: signal fades to unity dry.
    {
        june60::JuneChorus ch;
        ch.prepare (fs);
        ch.setMode (false, false, true, false);
        ch.snap();

        double worst = 0.0;
        for (int n = 0; n < (int) (1.0 * fs); ++n)
        {
            const float in = 0.5f * std::sin (2.0f * (float) june60::kPi * 220.0f * (float) n / (float) fs);
            float l = 0.0f, r = 0.0f;
            ch.process (in, l, r);
            worst = std::max (worst, (double) std::max (std::fabs (l - in), std::fabs (r - in)));
        }
        check (worst < 1e-4, "no buttons: output equals input");
    }

    // 5. Speed set switch changes the LFO rate (V1 set is faster for chorus I).
    {
        auto measureRate = [fs] (bool v1)
        {
            june60::JuneChorus ch;
            ch.prepare (fs);
            ch.setMode (true, false, true, v1);
            ch.snap();
            int crossings = 0;
            float prev = 0.0f;
            for (int n = 0; n < (int) (12.0 * fs); ++n)
            {
                float l = 0.0f, r = 0.0f;
                ch.process (0.0f, l, r);
                if (n >= (int) (2.0 * fs))
                {
                    const float c = ch.lastDelayMsL() - 3.3f;
                    if (prev < 0.0f && c >= 0.0f)
                        ++crossings;
                    prev = c;
                }
            }
            return crossings / 10.0;
        };
        const double juno = measureRate (false), v1 = measureRate (true);
        std::printf ("      Juno speed set: %.2f Hz, V1 speed set: %.2f Hz\n", juno, v1);
        check (v1 > juno, "speed set: V1 faster than Juno for chorus I");
    }

    std::printf ("\n%s\n", failures == 0 ? "All tests passed." : "Some tests FAILED.");
    return failures == 0 ? 0 : 1;
}
