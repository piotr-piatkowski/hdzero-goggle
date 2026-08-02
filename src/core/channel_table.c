#include "channel_table.h"

#include <string.h>

// hw_index is 0-based, matching what DM6302_SetChannel() actually expects
// (existing callers always pass `g_setting.scan.channel - 1`); it is the
// position within the DM6302's fixed 12-slot race PLL table (R1..R8,E1,F1,F2,F4)
// or 8-slot low PLL table (L1..L8), NOT the same as a channel's position
// within a user-selected channel set.
// clang-format off
const channel_def_t g_channel_defs[CHANNEL_DEF_COUNT] = {
    // name,  freq, hw_band, hw_index
    {"A1", 5865, -1, -1}, {"A2", 5845, -1, -1}, {"A3", 5825, -1, -1}, {"A4", 5805, -1, -1},
    {"A5", 5785, -1, -1}, {"A6", 5765, -1, -1}, {"A7", 5745, -1, -1}, {"A8", 5725, -1, -1},

    {"B1", 5733, -1, -1}, {"B2", 5752, -1, -1}, {"B3", 5771, -1, -1}, {"B4", 5790, -1, -1},
    {"B5", 5809, -1, -1}, {"B6", 5828, -1, -1}, {"B7", 5847, -1, -1}, {"B8", 5866, -1, -1},

    {"E1", 5705,  0,  8}, {"E2", 5685, -1, -1}, {"E3", 5665, -1, -1}, {"E4", 5645, -1, -1},
    {"E5", 5885, -1, -1}, {"E6", 5905, -1, -1}, {"E7", 5925, -1, -1}, {"E8", 5945, -1, -1},

    {"F1", 5740,  0,  9}, {"F2", 5760,  0, 10}, {"F3", 5780, -1, -1}, {"F4", 5800,  0, 11},
    {"F5", 5820, -1, -1}, {"F6", 5840, -1, -1}, {"F7", 5860, -1, -1}, {"F8", 5880,  0,  6}, // F8 shares R7's freq/PLL slot

    {"R1", 5658,  0,  0}, {"R2", 5695,  0,  1}, {"R3", 5732,  0,  2}, {"R4", 5769,  0,  3},
    {"R5", 5806,  0,  4}, {"R6", 5843,  0,  5}, {"R7", 5880,  0,  6}, {"R8", 5917,  0,  7},

    {"L1", 5362,  1,  0}, {"L2", 5399,  1,  1}, {"L3", 5436,  1,  2}, {"L4", 5473,  1,  3},
    {"L5", 5510,  1,  4}, {"L6", 5547,  1,  5}, {"L7", 5584,  1,  6}, {"L8", 5621,  1,  7},
};

const channel_set_t g_channel_sets[] = {
    {"Race Band", 12, {"R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "E1", "F1", "F2", "F4"}},
    {"Low Band",   8, {"L1", "L2", "L3", "L4", "L5", "L6", "L7", "L8"}},
    {"Racing L+R",10, {"R1", "R2", "R3", "R5", "R7", "R8", "F2", "F4", "L7", "L8"}},
};
// clang-format on

const uint8_t g_channel_set_count = sizeof(g_channel_sets) / sizeof(g_channel_sets[0]);

_Static_assert(sizeof(g_channel_sets) / sizeof(g_channel_sets[0]) <= MAX_CHANNEL_SETS,
               "too many channel sets for the Source page's btn_group widget");

const channel_def_t *channel_by_name(const char *name) {
    for (int i = 0; i < CHANNEL_DEF_COUNT; i++) {
        if (strcmp(g_channel_defs[i].name, name) == 0) {
            return &g_channel_defs[i];
        }
    }
    return NULL;
}

const channel_def_t *channel_by_freq(uint16_t freq_mhz) {
    for (int i = 0; i < CHANNEL_DEF_COUNT; i++) {
        if (g_channel_defs[i].freq_mhz == freq_mhz) {
            return &g_channel_defs[i];
        }
    }
    return NULL;
}

const channel_def_t *channel_by_msp_index(uint8_t msp_index) {
    if (msp_index >= CHANNEL_DEF_COUNT) {
        return NULL;
    }
    return &g_channel_defs[msp_index];
}

int channel_msp_index(const channel_def_t *def) {
    return (int)(def - g_channel_defs);
}

uint8_t channel_set_size(uint8_t set_index) {
    if (set_index >= g_channel_set_count) {
        return 0;
    }
    return g_channel_sets[set_index].count;
}

const char *channel_set_channel_name(uint8_t set_index, uint8_t ch) {
    if (set_index >= g_channel_set_count || ch == 0 || ch > g_channel_sets[set_index].count) {
        return "";
    }
    return g_channel_sets[set_index].names[ch - 1];
}

bool channel_set_find(uint8_t set_index, const char *name, uint8_t *ch_out) {
    if (set_index >= g_channel_set_count) {
        return false;
    }
    const channel_set_t *set = &g_channel_sets[set_index];
    for (uint8_t i = 0; i < set->count; i++) {
        if (strcmp(set->names[i], name) == 0) {
            *ch_out = i + 1;
            return true;
        }
    }
    return false;
}

bool channel_set_tune(uint8_t set_index, uint8_t ch, uint8_t *hw_band, uint8_t *hw_index) {
    const char *name = channel_set_channel_name(set_index, ch);
    const channel_def_t *def = channel_by_name(name);
    if (def == NULL || def->hw_band < 0) {
        return false;
    }
    *hw_band = (uint8_t)def->hw_band;
    *hw_index = (uint8_t)def->hw_index;
    return true;
}
