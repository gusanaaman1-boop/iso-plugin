#pragma once

//  Parameter IDs are part of the saved-state contract: renaming one orphans
//  every session that used it. Add, never rename.
namespace iso::id
{
    inline constexpr const char* lowMid   = "xover_low_mid";
    inline constexpr const char* midHigh  = "xover_mid_high";

    inline constexpr const char* lowGain  = "low_gain";
    inline constexpr const char* midGain  = "mid_gain";
    inline constexpr const char* highGain = "high_gain";

    inline constexpr const char* lowKill  = "low_kill";
    inline constexpr const char* midKill  = "mid_kill";
    inline constexpr const char* highKill = "high_kill";

    inline constexpr const char* slope    = "slope";       // 0 = 12 dB/oct, 1 = 24 dB/oct
    inline constexpr const char* floorMode= "floor";       // 0 = ISO (kill), 1 = EQ (-26 dB)

    inline constexpr const char* filter   = "filter";      // -1..+1 bipolar sweep
    inline constexpr const char* resonance= "resonance";   // 0..1

    inline constexpr const char* trim     = "trim";
    inline constexpr const char* bypass   = "bypass";

    inline constexpr int stateVersion = 1;
}
