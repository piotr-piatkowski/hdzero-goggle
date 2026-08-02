#include "channel_table.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <log/log.h>

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

static const channel_set_t default_channel_sets[] = {
    {"Race Band", 12, {"R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "E1", "F1", "F2", "F4"}},
    {"Low Band",   8, {"L1", "L2", "L3", "L4", "L5", "L6", "L7", "L8"}},
    {"Racing L+R",10, {"R1", "R2", "R3", "R5", "R7", "R8", "F2", "F4", "L7", "L8"}},
};
// clang-format on

_Static_assert(sizeof(default_channel_sets) / sizeof(default_channel_sets[0]) <= MAX_CHANNEL_SETS,
               "too many default channel sets for the Source page's btn_group widget");

channel_set_t g_channel_sets[MAX_CHANNEL_SETS];
uint8_t g_channel_set_count;

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

#ifdef EMULATOR_BUILD
#define CHANNEL_SETS_FILE "channels.txt"
#else
#define CHANNEL_SETS_FILE "/mnt/extsd/channels.txt"
#endif

static void trim(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

// Parses one "<set name> : <ch1>, <ch2>, ..." line into *set.
// Returns false (line ignored) if it has no ':', no name, or no valid channels.
static bool parse_channel_set_line(char *line, int line_no, channel_set_t *set) {
    char *colon = strchr(line, ':');
    if (colon == NULL) {
        LOGE("%s line %d: missing ':' -- line ignored", CHANNEL_SETS_FILE, line_no);
        return false;
    }
    *colon = '\0';
    char *name_part = line;
    char *list_part = colon + 1;
    trim(name_part);
    trim(list_part);

    if (name_part[0] == '\0') {
        LOGE("%s line %d: empty channel set name -- line ignored", CHANNEL_SETS_FILE, line_no);
        return false;
    }

    snprintf(set->label, sizeof(set->label), "%s", name_part);
    set->count = 0;

    char *save = NULL;
    for (char *tok = strtok_r(list_part, ",", &save); tok != NULL; tok = strtok_r(NULL, ",", &save)) {
        trim(tok);
        if (tok[0] == '\0') {
            continue;
        }
        if (channel_by_name(tok) == NULL) {
            LOGE("%s line %d: unknown channel '%s' -- ignored", CHANNEL_SETS_FILE, line_no, tok);
            continue;
        }
        if (set->count >= MAX_CHANNEL_SET_SIZE) {
            LOGE("%s line %d: set '%s' has more than %d channels -- extra ignored", CHANNEL_SETS_FILE, line_no, set->label, MAX_CHANNEL_SET_SIZE);
            break;
        }
        snprintf(set->names[set->count], sizeof(set->names[0]), "%s", tok);
        set->count++;
    }

    if (set->count == 0) {
        LOGE("%s line %d: set '%s' has no valid channels -- line ignored", CHANNEL_SETS_FILE, line_no, set->label);
        return false;
    }
    return true;
}

// Reads at most MAX_CHANNEL_SETS lines (extra lines beyond that are ignored).
static uint8_t parse_channel_sets_file(FILE *file, channel_set_t *sets) {
    uint8_t set_count = 0;
    char line[256];

    for (int line_no = 1; line_no <= MAX_CHANNEL_SETS && fgets(line, sizeof(line), file) != NULL; line_no++) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') {
            continue; // blank line -- still counts against the MAX_CHANNEL_SETS line cap
        }
        if (parse_channel_set_line(line, line_no, &sets[set_count])) {
            set_count++;
        }
    }
    return set_count;
}

void channel_sets_load(void) {
    uint8_t set_count = 0;
    FILE *file = fopen(CHANNEL_SETS_FILE, "r");
    if (file != NULL) {
        set_count = parse_channel_sets_file(file, g_channel_sets);
        fclose(file);
        if (set_count == 0) {
            LOGE("%s: no valid channel sets found, falling back to defaults", CHANNEL_SETS_FILE);
        }
    }

    if (set_count == 0) {
        memcpy(g_channel_sets, default_channel_sets, sizeof(default_channel_sets));
        set_count = sizeof(default_channel_sets) / sizeof(default_channel_sets[0]);
        LOGI("Using %d default HDZero channel set(s)", set_count);
    } else {
        LOGI("Loaded %d HDZero channel set(s) from %s", set_count, CHANNEL_SETS_FILE);
    }

    g_channel_set_count = set_count;
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
