#pragma once

#include <vector>

// ===========================================================================
//  Factory presets for Cohesion. Each preset is a full set of parameter
//  values. Choice parameters (ratio / attack / release) are stored as their
//  index into the choice list defined in PluginProcessor.cpp:
//     ratio   : 0 = 2:1, 1 = 4:1, 2 = 10:1
//     attack  : 0 = 0.1ms, 1 = 0.3ms, 2 = 1ms, 3 = 3ms, 4 = 10ms, 5 = 30ms
//     release : 0 = 0.1s, 1 = 0.3s, 2 = 0.6s, 3 = 1.2s, 4 = Auto
// ===========================================================================

namespace cohesion
{
    struct Preset
    {
        const char* name;
        bool  magic;
        float amount;
        float threshold;
        int   ratio;
        int   attack;
        int   release;
        float makeup;
        bool  scOn;
        float scFreq;
        float mix;
        float low;
        float air;
        float ceiling;
        int   knee;      // 0 = Soft, 1 = Hard
        float hpf;       // Hz (20 = effectively open)
        float lpf;       // Hz (20000 = effectively open)
    };

    inline const std::vector<Preset>& presets()
    {
        //                     name              magic amount  thr  rat atk rel  mk   scOn  scF   mix   low  air  ceil  knee  hpf      lpf
        static const std::vector<Preset> p = {
            { "Init (Flat)",     true,   0.0f,  0.0f, 1, 4, 4,  0.0f, false, 20.0f, 100.0f, 0.0f, 0.0f, -0.3f, 0, 20.0f, 20000.0f },
            { "Gentle Glue",     true,  30.0f, -6.0f, 0, 4, 4,  2.0f, false, 20.0f, 100.0f, 1.0f, 1.0f, -1.0f, 0, 20.0f, 20000.0f },
            { "Master Glue",     true,  50.0f, -8.0f, 1, 5, 4,  3.0f, false, 20.0f, 100.0f, 1.5f, 2.0f, -1.0f, 0, 20.0f, 20000.0f },
            { "Thick & Fat",     true,  70.0f,-12.0f, 1, 4, 1,  5.0f, false, 20.0f, 100.0f, 3.0f, 1.5f, -1.0f, 0, 20.0f, 20000.0f },
            { "Punchy Bus",      true,  40.0f, -8.0f, 1, 5, 4,  3.0f, true, 120.0f, 100.0f, 1.0f, 2.0f, -1.0f, 1, 30.0f, 20000.0f },
            { "Parallel Smash",  true,  80.0f,-20.0f, 2, 2, 0,  0.0f, false, 20.0f,  40.0f, 2.0f, 2.0f, -1.0f, 1, 20.0f, 20000.0f },
            { "Loud & Proud",    true,  65.0f,-10.0f, 1, 4, 4,  6.0f, false, 20.0f, 100.0f, 1.0f, 2.5f, -0.5f, 0, 20.0f, 20000.0f },
            { "Air & Weight",    true,  25.0f, -4.0f, 0, 5, 4,  1.0f, false, 20.0f, 100.0f, 3.0f, 3.0f, -1.0f, 0, 20.0f, 20000.0f },
        };
        return p;
    }
} // namespace cohesion
