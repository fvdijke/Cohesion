#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// ===========================================================================
//  Step-sequencer pattern presets for the ARP (steps, on/off, velocity).
//  Optional rate / gate / swing (-1 = leave as is).
// ===========================================================================
struct ArpPattern
{
    juce::String name;
    int steps;
    const char* on;      // 16 chars: 'x' = on, '.' = off
    float vel[16];
    int rate = -1; float gate = -1.0f, swing = -1.0f;
};

inline const std::vector<ArpPattern>& arpPatterns()
{
    static const std::vector<ArpPattern> p = {
        { "Straight 16",      16, "xxxxxxxxxxxxxxxx", { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 }, 3, 0.70f, 0.0f },
        { "Accent 4",         16, "xxxxxxxxxxxxxxxx", { 1,.55f,.6f,.55f, 1,.55f,.6f,.55f, 1,.55f,.6f,.55f, 1,.55f,.6f,.55f }, 3, 0.65f, 0.0f },
        { "Trance Gate 1/16", 16, "xxxxxx.xxxx.xxxx", { .8f,.35f,.6f,.95f, .4f,.68f,.5f,.88f, .3f,.62f,.8f,.45f, .58f,.46f,.85f,.27f }, 3, 0.55f, 0.0f },
        { "Offbeat 8",        16, "..x...x...x...x.", { 1,1,.9f,1, 1,1,.9f,1, 1,1,.9f,1, 1,1,1,1 }, 3, 0.6f, 0.0f },
        { "Gallop",           16, "x.xxx.xxx.xxx.xx", { 1,0,.6f,.75f, 1,0,.6f,.75f, 1,0,.6f,.75f, 1,0,.6f,.75f }, 3, 0.5f, 0.0f },
        { "Syncopated",       16, "x..x..x.x..x..x.", { 1,0,0,.8f, 0,0,.9f,0, 1,0,0,.8f, 0,0,.9f,0 }, 3, 0.6f, 0.0f },
        { "Euclid 5/16",      16, "x..x..x..x..x...", { 1,0,0,.8f, 0,0,.9f,0, 0,.8f,0,0, .9f,0,0,0 }, 3, 0.7f, 0.0f },
        { "Euclid 7/16",      16, "x.x.x..x.x.x..x.", { 1,0,.7f,0, .9f,0,0,.75f, 0,.85f,0,.7f, 0,0,.9f,0 }, 3, 0.6f, 0.0f },
        { "Pulse 8",           8, "xxxxxxxx........", { 1,.7f,.85f,.7f, 1,.7f,.85f,.7f, 1,1,1,1, 1,1,1,1 }, 1, 0.5f, 0.0f },
        { "Rising",           16, "xxxxxxxxxxxxxxxx", { .25f,.3f,.35f,.4f, .45f,.5f,.55f,.6f, .65f,.7f,.75f,.8f, .85f,.9f,.95f,1 }, 3, 0.6f, 0.0f },
        { "Falling",          16, "xxxxxxxxxxxxxxxx", { 1,.95f,.9f,.85f, .8f,.75f,.7f,.65f, .6f,.55f,.5f,.45f, .4f,.35f,.3f,.25f }, 3, 0.6f, 0.0f },
        { "Shuffle",          16, "x.xxx.xxx.xxx.xx", { 1,0,.55f,.8f, 1,0,.55f,.8f, 1,0,.55f,.8f, 1,0,.55f,.8f }, 3, 0.55f, 0.45f },
        { "Triplet 12",       12, "xxxxxxxxxxxx....", { 1,.6f,.7f, 1,.6f,.7f, 1,.6f,.7f, 1,.6f,.7f, 1,1,1,1 }, 4, 0.6f, 0.0f },
        { "Sparse",           16, "x.....x...x.....", { 1,0,0,0, 0,0,.85f,0, 0,0,.9f,0, 0,0,0,0 }, 3, 0.9f, 0.0f },
        { "Broken",           16, "xx.x.xx.x.x.xx.x", { 1,.5f,0,.8f, 0,.7f,.9f,0, .6f,0,1,0, .7f,.5f,0,.85f }, 3, 0.5f, 0.15f },
        { "Long Notes 1/8",   16, "xxxxxxxxxxxxxxxx", { 1,.8f,.9f,.8f, 1,.8f,.9f,.8f, 1,.8f,.9f,.8f, 1,.8f,.9f,.8f }, 1, 0.95f, 0.0f },
    };
    return p;
}
