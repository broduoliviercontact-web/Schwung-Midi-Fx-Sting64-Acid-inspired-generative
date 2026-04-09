/*
 * dsp/sting64_engine.c — Sting64 portable engine implementation
 *
 * No Schwung or platform dependencies. Testable on any machine.
 */

#include "sting64_engine.h"

/* ------------------------------------------------------------------ */
/* Scale tables                                                         */
/* Scale degrees (semitones above root octave, 0–11).                 */
/* Order LOCKED — must match STING64_SCALE_* constants.               */
/* ------------------------------------------------------------------ */

static const int SCALE_TABLES[STING64_SCALE_COUNT][12] = {
    /* ionian             */ { 0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0 },
    /* aeolian            */ { 0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0 },
    /* dorian             */ { 0, 2, 3, 5, 7, 9, 10, 0, 0, 0, 0, 0 },
    /* mixolydian         */ { 0, 2, 4, 5, 7, 9, 10, 0, 0, 0, 0, 0 },
    /* phrygian           */ { 0, 1, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0 },
    /* lydian             */ { 0, 2, 4, 6, 7, 9, 11, 0, 0, 0, 0, 0 },
    /* locrian            */ { 0, 1, 3, 5, 6, 8, 10, 0, 0, 0, 0, 0 },
    /* major_pent         */ { 0, 2, 4, 7, 9, 0, 0, 0, 0, 0, 0, 0 },
    /* minor_pent         */ { 0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0 },
    /* major_blues        */ { 0, 2, 3, 4, 7, 9, 0, 0, 0, 0, 0, 0 },
    /* minor_blues        */ { 0, 3, 5, 6, 7, 10, 0, 0, 0, 0, 0, 0 },
    /* harmonic_minor     */ { 0, 2, 3, 5, 7, 8, 11, 0, 0, 0, 0, 0 },
    /* melodic_minor      */ { 0, 2, 3, 5, 7, 9, 11, 0, 0, 0, 0, 0 },
    /* phrygian_dominant  */ { 0, 1, 4, 5, 7, 8, 10, 0, 0, 0, 0, 0 },
    /* double_harmonic    */ { 0, 1, 4, 5, 7, 8, 11, 0, 0, 0, 0, 0 },
    /* whole_tone         */ { 0, 2, 4, 6, 8, 10, 0, 0, 0, 0, 0, 0 },
    /* diminished_wh      */ { 0, 2, 3, 5, 6, 8, 9, 11, 0, 0, 0, 0 },
    /* diminished_hw      */ { 0, 1, 3, 4, 6, 7, 9, 10, 0, 0, 0, 0 },
    /* chromatic          */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
};

static const int SCALE_LENS[STING64_SCALE_COUNT] = {
    7, 7, 7, 7, 7, 7, 7, 5, 5, 6, 6, 7, 7, 7, 7, 6, 8, 8, 12
};

/* Position-aware step hierarchy.
 * The first bar hit is the strongest anchor, then other quarter-note downbeats,
 * then upbeats, then weak subdivisions. This hierarchy drives both density and
 * pitch stability so high chaos still produces phrases with a clear center. */
#define STING64_STEP_STRENGTH_BAR       3
#define STING64_STEP_STRENGTH_DOWNBEAT  2
#define STING64_STEP_STRENGTH_UPBEAT    1
#define STING64_STEP_STRENGTH_WEAK      0

#define STING64_DROP_PRIO_BAR        0    /* never drop step 0 when density > 0 */
#define STING64_DROP_PRIO_DOWNBEAT   38   /* strongly protected quarter notes */
#define STING64_DROP_PRIO_UPBEAT     153  /* moderately protected */
#define STING64_DROP_PRIO_WEAK       255  /* weak subdivisions drop first */

/* ------------------------------------------------------------------ */
/* Private: LCG random number generator                                */
/* Knuth multiplicative LCG — no global state, deterministic.         */
/* ------------------------------------------------------------------ */

static uint32_t lcg_next(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static uint32_t mix_u32(uint32_t seed, uint32_t value)
{
    seed ^= value + 0x9E3779B9u + (seed << 6) + (seed >> 2);
    return seed;
}

/* ------------------------------------------------------------------ */
/* Private: nearest-neighbor scale quantization                        */
/*                                                                     */
/* Finds the scale note in the repeating scale pattern closest to raw. */
/* Searches current octave and one octave in each direction to handle  */
/* wrap-around at octave boundaries.                                   */
/* ------------------------------------------------------------------ */

static int quantize_to_scale(int raw, uint8_t scale_idx)
{
    if (scale_idx >= STING64_SCALE_COUNT)
        scale_idx = STING64_SCALE_IONIAN;

    const int *scale = SCALE_TABLES[scale_idx];
    int        len   = SCALE_LENS[scale_idx];

    /* Decompose raw into (octave_base, chroma) where chroma is 0–11.
     * Use floored division so chroma is always non-negative. */
    int chroma = ((raw % 12) + 12) % 12;
    int octave_base = raw - chroma;   /* always a multiple of 12 */

    int best      = raw;
    int best_dist = 999;

    /* Search current octave and one neighbor in each direction */
    for (int oct = -1; oct <= 1; oct++) {
        int base = octave_base + oct * 12;
        for (int i = 0; i < len; i++) {
            int candidate = base + scale[i];
            int d = candidate - raw;
            if (d < 0) d = -d;
            if (d < best_dist) {
                best_dist = d;
                best      = candidate;
            }
        }
    }

    if (best < 0)   best = 0;
    if (best > 127) best = 127;
    return best;
}

static uint8_t step_strength_at(int steps, int step_pos)
{
    int steps_per_beat = steps / 4;
    int pos_in_beat;

    if (steps_per_beat < 1)
        steps_per_beat = 1;

    if (step_pos < 0)
        step_pos = 0;

    if (step_pos == 0)
        return STING64_STEP_STRENGTH_BAR;

    pos_in_beat = step_pos % steps_per_beat;

    if (pos_in_beat == 0)
        return STING64_STEP_STRENGTH_DOWNBEAT;

    if (steps_per_beat > 1 && pos_in_beat == steps_per_beat / 2)
        return STING64_STEP_STRENGTH_UPBEAT;

    return STING64_STEP_STRENGTH_WEAK;
}

static uint8_t step_drop_priority_at(int steps, int step_pos)
{
    switch (step_strength_at(steps, step_pos)) {
    case STING64_STEP_STRENGTH_BAR:
        return STING64_DROP_PRIO_BAR;
    case STING64_STEP_STRENGTH_DOWNBEAT:
        return STING64_DROP_PRIO_DOWNBEAT;
    case STING64_STEP_STRENGTH_UPBEAT:
        return STING64_DROP_PRIO_UPBEAT;
    default:
        return STING64_DROP_PRIO_WEAK;
    }
}

static int chaos_spread_for_step(uint8_t chaos, uint8_t strength)
{
    int base_spread = (int)((chaos / 255.0f) * 24.0f + 0.5f);

    switch (strength) {
    case STING64_STEP_STRENGTH_BAR:
        return (base_spread + 3) / 4;
    case STING64_STEP_STRENGTH_DOWNBEAT:
        return (base_spread + 1) / 2;
    case STING64_STEP_STRENGTH_UPBEAT:
        return (base_spread * 3 + 3) / 4;
    default:
        return base_spread;
    }
}

static int anchor_offset_for_step(uint32_t *rng, uint8_t strength, int base_spread)
{
    if (base_spread < 2)
        return 0;

    switch (strength) {
    case STING64_STEP_STRENGTH_BAR:
        return (lcg_next(rng) & 1u) ? 0 : 12;
    case STING64_STEP_STRENGTH_DOWNBEAT:
        switch (lcg_next(rng) % 3u) {
        case 0:
            return 0;
        case 1:
            return 7;
        default:
            return 12;
        }
    default:
        return 0;
    }
}

static uint32_t sequence_seed(const StingEngine *e)
{
    uint32_t seed = 0xA5A5F00Du;

    seed = mix_u32(seed, e->steps_count);
    seed = mix_u32(seed, e->density);
    seed = mix_u32(seed, e->chaos);
    seed = mix_u32(seed, e->scale_index);
    seed = mix_u32(seed, (uint8_t)(e->root + 24));
    seed = mix_u32(seed, e->seed);

    return seed;
}

static void rebuild_sequence(StingEngine *e)
{
    int saved_step_pos = e->step_pos;
    int steps = sting64_engine_get_steps(e);
    uint32_t rng = sequence_seed(e);

    for (int i = 0; i < STING64_MAX_STEPS; i++) {
        e->step_gate[i] = 0;
        e->step_note[i] = 60;
    }

    for (int i = 0; i < steps; i++) {
        uint8_t strength;
        uint8_t priority;
        uint32_t drop_threshold;
        uint32_t random_byte;
        int spread;
        int base_spread;
        int anchor;
        int offset = 0;
        int raw;
        int note;

        strength = step_strength_at(steps, i);
        priority = step_drop_priority_at(steps, i);

        if (e->density == 0) {
            e->step_gate[i] = 0;
            continue;
        }

        if (e->density == 255) {
            e->step_gate[i] = 1;
        } else {
            drop_threshold = ((uint32_t)(255 - e->density) * priority + 127u) / 255u;
            random_byte = lcg_next(&rng) >> 24;
            e->step_gate[i] = random_byte >= drop_threshold;
        }

        if (!e->step_gate[i])
            continue;

        base_spread = (int)((e->chaos / 255.0f) * 24.0f + 0.5f);
        spread = chaos_spread_for_step(e->chaos, strength);
        anchor = anchor_offset_for_step(&rng, strength, base_spread);
        if (spread > 0) {
            uint32_t r = lcg_next(&rng);
            offset = (int)(r % (uint32_t)(2 * spread + 1)) - spread;
        }

        raw = 60 + (int)e->root + anchor + offset;
        note = quantize_to_scale(raw, e->scale_index);
        e->step_note[i] = (uint8_t)note;
    }

    e->rand_state = rng;
    e->step_pos = saved_step_pos;
    e->sequence_dirty = 0;
    e->sequence_hash = sequence_seed(e);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void sting64_engine_init(StingEngine *e)
{
    e->steps_count = 16;
    e->density     = 230;   /* ~0.90 normalized — matches module.json default */
    e->chaos       = 0;     /* 0.0 normalized — matches module.json default */
    e->swing       = 0;
    e->scale_index = STING64_SCALE_MINOR_PENT; /* matches module.json default */
    e->root        = 0;
    e->velocity    = 96;    /* ~0.75 normalized (96/127) */
    e->seed        = 1;
    e->step_pos    = 0;
    e->rand_state  = 0xDEADBEEF;
    e->sequence_dirty = 1;
    e->sequence_hash = 0;

    for (int i = 0; i < STING64_MAX_STEPS; i++) {
        e->step_gate[i] = 0;
        e->step_note[i] = 60;
    }
}

int sting64_engine_get_steps(const StingEngine *e)
{
    if (e->steps_count < 1)
        return 1;
    if (e->steps_count > STING64_MAX_STEPS)
        return STING64_MAX_STEPS;
    return e->steps_count;
}

int sting64_engine_should_play_step(StingEngine *e)
{
    int step_pos = e->step_pos;

    if (e->sequence_dirty || e->sequence_hash != sequence_seed(e))
        rebuild_sequence(e);

    if (step_pos < 0)
        step_pos = 0;

    step_pos %= sting64_engine_get_steps(e);
    return e->step_gate[step_pos] ? 1 : 0;
}

uint8_t sting64_engine_pick_note(StingEngine *e)
{
    int step_pos = e->step_pos;

    if (e->sequence_dirty || e->sequence_hash != sequence_seed(e))
        rebuild_sequence(e);

    if (step_pos < 0)
        step_pos = 0;

    step_pos %= sting64_engine_get_steps(e);
    return e->step_note[step_pos];
}

void sting64_engine_invalidate_sequence(StingEngine *e)
{
    e->sequence_dirty = 1;
}
