#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Canonical channel <-> frequency table, in the fixed MSP wire order used by
// the ExpressLRS/Betaflight band+channel protocol: A,B,E,F,R,L, 8 channels each.
#define CHANNEL_DEF_COUNT 48

typedef struct {
    const char *name;   // e.g. "R1"
    uint16_t freq_mhz;
    int8_t hw_band;      // DM6302 PLL table: 0=race (incl. E/F), 1=low, -1=not tunable on HDZero HW
    int8_t hw_index;      // 0-based position within that table (this is what DM6302_SetChannel expects), -1 if hw_band < 0
} channel_def_t;

extern const channel_def_t g_channel_defs[CHANNEL_DEF_COUNT];

const channel_def_t *channel_by_name(const char *name);
const channel_def_t *channel_by_freq(uint16_t freq_mhz);
const channel_def_t *channel_by_msp_index(uint8_t msp_index);
int channel_msp_index(const channel_def_t *def); // 0..CHANNEL_DEF_COUNT-1

// User-selectable predefined lists of channels (e.g. "Race Band", "Racing L+R").
// Defaults are compiled in; channel_sets_load() may override them from an
// SD-card file at boot (see channel_table.c).
#define MAX_CHANNEL_SET_SIZE 12
#define MAX_CHANNEL_SETS     16 // the Source page picks this via a dropdown, so no small hard UI cap is needed
#define MAX_CHANNEL_SET_NAME_LEN 3 // e.g. "R1", "F4" -- longest channel name plus NUL

typedef struct {
    char label[24];
    uint8_t count;
    char names[MAX_CHANNEL_SET_SIZE][MAX_CHANNEL_SET_NAME_LEN + 1];
} channel_set_t;

extern channel_set_t g_channel_sets[MAX_CHANNEL_SETS];
extern uint8_t g_channel_set_count; // number of entries actually populated in g_channel_sets, <= MAX_CHANNEL_SETS

// Populates g_channel_sets/g_channel_set_count, either by parsing
// CHANNEL_SETS_FILE from the SD card or, if that's missing/invalid, from the
// compiled-in defaults. Call once at boot before anything reads g_channel_sets.
void channel_sets_load(void);

uint8_t channel_set_size(uint8_t set_index);
const char *channel_set_channel_name(uint8_t set_index, uint8_t ch /* 1-based */);

// Finds `name` within the given set; on success writes its 1-based position to *ch_out.
bool channel_set_find(uint8_t set_index, const char *name, uint8_t *ch_out);

// Resolves channel `ch` (1-based, within set_index) to the DM6302 tuning params.
// Returns false if the channel isn't tunable on HDZero HW (shouldn't happen for
// channels that are actually part of a channel set, but guards against bad data).
bool channel_set_tune(uint8_t set_index, uint8_t ch, uint8_t *hw_band, uint8_t *hw_index);

#ifdef __cplusplus
}
#endif
