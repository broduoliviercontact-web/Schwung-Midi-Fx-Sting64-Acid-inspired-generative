/*
 * dsp/sting64_engine.h — Sting64 portable engine
 *
 * No Schwung or platform dependencies. Testable on any machine.
 *
 * Responsibilities:
 * - step position tracking
 * - musical note thinning based on step strength
 * - chaos-scattered, scale-quantized pitch selection
 * - parameter storage (all uint8_t / int8_t encoded)
 *
 * Note lifecycle and MIDI output are handled by the host wrapper.
 */

#ifndef STING64_ENGINE_H
#define STING64_ENGINE_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Scale indices — order LOCKED, must match module.json chain_params   */
/* ------------------------------------------------------------------ */

#define STING64_SCALE_IONIAN      0
#define STING64_SCALE_AEOLIAN     1
#define STING64_SCALE_DORIAN      2
#define STING64_SCALE_MIXOLYDIAN  3
#define STING64_SCALE_PHRYGIAN    4
#define STING64_SCALE_LYDIAN      5
#define STING64_SCALE_LOCRIAN     6
#define STING64_SCALE_MAJOR_PENT  7
#define STING64_SCALE_MINOR_PENT  8
#define STING64_SCALE_MAJOR_BLUES 9
#define STING64_SCALE_MINOR_BLUES 10
#define STING64_SCALE_HARMONIC_MINOR 11
#define STING64_SCALE_MELODIC_MINOR 12
#define STING64_SCALE_PHRYGIAN_DOMINANT 13
#define STING64_SCALE_DOUBLE_HARMONIC 14
#define STING64_SCALE_WHOLE_TONE  15
#define STING64_SCALE_DIMINISHED_WH 16
#define STING64_SCALE_DIMINISHED_HW 17
#define STING64_SCALE_CHROMATIC   18
#define STING64_SCALE_COUNT       19

#define STING64_MAX_STEPS 64

/* ------------------------------------------------------------------ */
/* Engine state                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    /* parameters */
    uint8_t  steps_count;   /* loop length 1..64 — default 16                */
    uint8_t  density;       /* note density 0–255 — default ~192 (0.75)      */
    uint8_t  chaos;         /* pitch spread 0–255 — default ~64 (0.25)       */
    uint8_t  swing;         /* swing amount 0–255 — default 0                */
    uint8_t  scale_index;   /* STING64_SCALE_* — default STING64_SCALE_IONIAN */
    int8_t   root;          /* semitone transposition −24..+24 — default 0   */
    uint8_t  velocity;      /* MIDI velocity 0–127 — default ~96 (0.75)      */
    uint32_t seed;          /* melodic seed — default 1                       */

    /* sequencer state */
    int      step_pos;      /* current step 0..steps-1                        */

    /* cached sequence for the active loop length */
    uint8_t  step_gate[STING64_MAX_STEPS];
    uint8_t  step_note[STING64_MAX_STEPS];
    uint8_t  sequence_dirty;
    uint32_t sequence_hash;

    /* private RNG state (LCG, not for external use) */
    uint32_t rand_state;
} StingEngine;

/* ------------------------------------------------------------------ */
/* API                                                                  */
/* ------------------------------------------------------------------ */

/*
 * sting64_engine_init — initialize engine with V1 defaults.
 * Must be called before any other function.
 */
void sting64_engine_init(StingEngine *e);

/*
 * sting64_engine_get_steps — return the active step count (8/16/32/64).
 */
int sting64_engine_get_steps(const StingEngine *e);

/*
 * sting64_engine_should_play_step — decide if the current step speaks.
 *
 * Uses a position-aware thinning rule inspired by strong/weak beat
 * hierarchies: downbeats are protected first, weaker subdivisions drop first.
 * Advances the internal RNG state unless density is pinned at 0 or 255.
 * Returns 1 when the step should emit a note, 0 for a musical rest.
 */
int sting64_engine_should_play_step(StingEngine *e);

/*
 * sting64_engine_invalidate_sequence — mark the cached melody loop dirty.
 *
 * Call this after changing any parameter that alters melodic identity or
 * rest placement. The loop will rebuild lazily on the next query.
 */
void sting64_engine_invalidate_sequence(StingEngine *e);

/*
 * sting64_engine_pick_note — select a MIDI note for the current step.
 *
 * Applies chaos scatter, then quantizes to the selected scale.
 * Advances the internal RNG state.
 * Returns a valid MIDI note in 0–127.
 */
uint8_t sting64_engine_pick_note(StingEngine *e);

#endif /* STING64_ENGINE_H */
