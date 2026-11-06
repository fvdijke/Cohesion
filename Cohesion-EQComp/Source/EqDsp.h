#pragma once

#include <cmath>

// ===========================================================================
//  COHESION - EQ/COMP : high-quality EQ building blocks.
//
//  TPT / ZDF State-Variable Filter (Zavalishin / Andrew Simper "Cytomic"
//  form). Chosen over classic biquads because it stays stable and accurate
//  under fast parameter modulation (dragging bands / automation) and keeps
//  precision at low frequencies. All maths in double for headroom.
// ===========================================================================

namespace ceqc
{
    static constexpr double kPi = 3.14159265358979323846;

    class SVF
    {
    public:
        enum Type { Bell = 0, LowShelf, HighShelf, Highpass, Lowpass, Allpass };

        void setSampleRate (double fs) { sampleRate = fs; }
        void reset()                   { ic1 = ic2 = 0.0; }

        void set (Type type, double freqHz, double q, double gainDb)
        {
            freqHz = (freqHz < 10.0 ? 10.0 : (freqHz > sampleRate * 0.49 ? sampleRate * 0.49 : freqHz));
            const double A = std::pow (10.0, gainDb / 40.0);
            double gg = std::tan (kPi * freqHz / sampleRate);

            switch (type)
            {
                case Bell:      k = 1.0 / (q * A);        m0 = 1.0;   m1 = k * (A * A - 1.0); m2 = 0.0;             break;
                case LowShelf:  gg /= std::sqrt (A); k = 1.0 / q; m0 = 1.0;   m1 = k * (A - 1.0);     m2 = (A * A - 1.0);    break;
                case HighShelf: gg *= std::sqrt (A); k = 1.0 / q; m0 = A * A; m1 = k * (1.0 - A) * A; m2 = (1.0 - A * A);    break;
                case Highpass:  k = 1.0 / q;              m0 = 1.0;   m1 = -k;                m2 = -1.0;            break;
                case Lowpass:   k = 1.0 / q;              m0 = 0.0;   m1 = 0.0;               m2 = 1.0;             break;
            }

            g  = gg;
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        inline float process (float in)
        {
            const double v0 = (double) in;
            const double v3 = v0 - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return (float) (m0 * v0 + m1 * v1 + m2 * v2);
        }

    private:
        double sampleRate = 44100.0;
        double g = 0.0, k = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
        double m0 = 1.0, m1 = 0.0, m2 = 0.0;
        double ic1 = 0.0, ic2 = 0.0;   // integrator state
    };
}
