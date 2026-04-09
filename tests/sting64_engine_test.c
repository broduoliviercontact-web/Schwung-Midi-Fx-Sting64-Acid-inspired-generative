/*
 * tests/sting64_engine_test.c — Sting64 engine unit tests
 *
 * Tests the portable engine in isolation: no Schwung headers, no host API.
 * Covers: init defaults, step count, scale quantization, chaos scatter,
 * pick_note bounds, root transposition, scale interval correctness.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "../src/dsp/sting64_engine.h"

/* ------------------------------------------------------------------ */
/* Minimal test harness                                                 */
/* ------------------------------------------------------------------ */

static int passed = 0;
static int failed = 0;

#define CHECK(cond) do { \
    if (cond) { \
        passed++; \
    } else { \
        fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
        failed++; \
    } \
} while (0)

#define CHECK_EQ(a, b) do { \
    if ((a) == (b)) { \
        passed++; \
    } else { \
        fprintf(stderr, "FAIL [%s:%d]: %s == %s  (%d != %d)\n", \
                __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); \
        failed++; \
    } \
} while (0)

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Return 1 if note is in the given scale (any octave) */
static int note_in_scale(int note, const int *scale, int len)
{
    int chroma = ((note % 12) + 12) % 12;
    for (int i = 0; i < len; i++) {
        if (scale[i] == chroma) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

static void test_init_defaults(void)
{
    StingEngine e;
    sting64_engine_init(&e);

    CHECK_EQ(e.steps_count, 16);
    CHECK_EQ(e.scale_index, STING64_SCALE_MINOR_PENT);
    CHECK_EQ(e.root,        0);
    CHECK_EQ(e.swing,       0);
    CHECK_EQ(e.seed,        1);
    CHECK_EQ(e.step_pos,    0);

    /* density default ~0.90: 230/255 ≈ 0.902 */
    CHECK(e.density > 225 && e.density < 235);

    /* chaos default 0.0 */
    CHECK(e.chaos == 0);

    /* velocity default ~0.75: 96/127 ≈ 0.756 */
    CHECK(e.velocity > 90 && e.velocity < 100);
}

static void test_get_steps(void)
{
    StingEngine e;
    sting64_engine_init(&e);

    e.steps_count = 4;  CHECK_EQ(sting64_engine_get_steps(&e),  4);
    e.steps_count = 11; CHECK_EQ(sting64_engine_get_steps(&e), 11);
    e.steps_count = 29; CHECK_EQ(sting64_engine_get_steps(&e), 29);
    e.steps_count = 64; CHECK_EQ(sting64_engine_get_steps(&e), 64);

    /* clamp out of range */
    e.steps_count = 0;  CHECK_EQ(sting64_engine_get_steps(&e), 1);
    e.steps_count = 99; CHECK_EQ(sting64_engine_get_steps(&e), 64);
}

static void test_pick_note_midi_range(void)
{
    StingEngine e;
    sting64_engine_init(&e);

    /* Run 256 steps with full chaos — all notes must be valid MIDI */
    e.chaos = 255;
    for (int i = 0; i < 256; i++) {
        uint8_t note = sting64_engine_pick_note(&e);
        CHECK(note <= 127);
    }
}

static void test_density_zero_always_rests(void)
{
    StingEngine e;
    sting64_engine_init(&e);

    e.density = 0;
    for (int i = 0; i < 32; i++) {
        e.step_pos = i % sting64_engine_get_steps(&e);
        CHECK_EQ(sting64_engine_should_play_step(&e), 0);
    }
}

static void test_density_full_always_plays(void)
{
    StingEngine e;
    sting64_engine_init(&e);

    e.density = 255;
    for (int i = 0; i < 32; i++) {
        e.step_pos = i % sting64_engine_get_steps(&e);
        CHECK_EQ(sting64_engine_should_play_step(&e), 1);
    }
}

static void test_density_protects_downbeats(void)
{
    StingEngine e;
    int downbeats = 0;
    int weaks = 0;

    sting64_engine_init(&e);
    e.steps_count = 16;
    e.density = 96; /* intentionally sparse */
    e.rand_state = 0x12345678;

    for (int i = 0; i < 256; i++) {
        e.step_pos = i % 16;
        if (sting64_engine_should_play_step(&e)) {
            if ((e.step_pos % 4) == 0)
                downbeats++;
            else if ((e.step_pos % 4) != 2)
                weaks++;
        }
    }

    CHECK(downbeats > 0);
    CHECK(downbeats >= weaks / 2);
}

static void test_density_keeps_first_step_when_nonzero(void)
{
    StingEngine e;

    sting64_engine_init(&e);
    e.steps_count = 16;
    e.density = 8; /* extremely sparse, but not zero */
    e.seed = 1234;
    sting64_engine_invalidate_sequence(&e);

    for (int i = 0; i < 32; i++) {
        e.step_pos = 0;
        CHECK_EQ(sting64_engine_should_play_step(&e), 1);
    }
}

static void test_pick_note_zero_chaos(void)
{
    StingEngine e;
    sting64_engine_init(&e);

    /* At chaos=0 the offset is 0 — result is always root quantized to scale */
    e.chaos = 0;
    e.root  = 0;
    e.scale_index = STING64_SCALE_IONIAN;

    uint8_t first = sting64_engine_pick_note(&e);
    for (int i = 0; i < 32; i++) {
        CHECK_EQ(sting64_engine_pick_note(&e), first);
    }
}

static void test_steps_define_loop_length(void)
{
    StingEngine e;
    uint8_t loop8_a[8];
    uint8_t loop8_b[8];
    uint8_t loop16_a[16];
    uint8_t loop16_b[16];

    sting64_engine_init(&e);
    e.density = 255;
    e.chaos = 180;
    e.scale_index = STING64_SCALE_DORIAN;
    e.root = 5;

    e.steps_count = 8;
    sting64_engine_invalidate_sequence(&e);
    for (int i = 0; i < 8; i++) {
        e.step_pos = i;
        CHECK_EQ(sting64_engine_should_play_step(&e), 1);
        loop8_a[i] = sting64_engine_pick_note(&e);
    }
    for (int i = 0; i < 8; i++) {
        e.step_pos = i;
        loop8_b[i] = sting64_engine_pick_note(&e);
        CHECK_EQ(loop8_a[i], loop8_b[i]);
    }

    e.steps_count = 16;
    sting64_engine_invalidate_sequence(&e);
    for (int i = 0; i < 16; i++) {
        e.step_pos = i;
        CHECK_EQ(sting64_engine_should_play_step(&e), 1);
        loop16_a[i] = sting64_engine_pick_note(&e);
    }
    for (int i = 0; i < 16; i++) {
        e.step_pos = i;
        loop16_b[i] = sting64_engine_pick_note(&e);
        CHECK_EQ(loop16_a[i], loop16_b[i]);
    }

    /* The 16-step loop should not just be a duplicated 8-step cycle. */
    CHECK(memcmp(loop16_a, loop16_a + 8, 8) != 0);
}

static void test_seed_changes_loop_content(void)
{
    StingEngine e;
    uint8_t loop_a[16];
    uint8_t loop_b[16];
    int differs = 0;

    sting64_engine_init(&e);
    e.steps_count = 16;
    e.density = 255;
    e.chaos = 180;
    e.scale_index = STING64_SCALE_DORIAN;
    e.root = 5;

    e.seed = 1;
    sting64_engine_invalidate_sequence(&e);
    for (int i = 0; i < 16; i++) {
        e.step_pos = i;
        loop_a[i] = sting64_engine_pick_note(&e);
    }

    e.seed = 2;
    sting64_engine_invalidate_sequence(&e);
    for (int i = 0; i < 16; i++) {
        e.step_pos = i;
        loop_b[i] = sting64_engine_pick_note(&e);
        if (loop_a[i] != loop_b[i])
            differs = 1;
    }

    CHECK(differs);
}

static void test_strong_steps_are_more_harmonically_stable(void)
{
    int strong_total = 0;
    int weak_total = 0;

    for (uint32_t seed = 1; seed <= 128; seed++) {
        StingEngine e;
        int strong_dist;
        int weak_dist;

        sting64_engine_init(&e);
        e.steps_count = 16;
        e.density = 255;
        e.chaos = 255;
        e.root = 0;
        e.scale_index = STING64_SCALE_CHROMATIC;
        e.seed = seed;
        sting64_engine_invalidate_sequence(&e);

        e.step_pos = 0;
        strong_dist = sting64_engine_pick_note(&e) - 60;
        if (strong_dist < 0) strong_dist = -strong_dist;

        e.step_pos = 1;
        weak_dist = sting64_engine_pick_note(&e) - 60;
        if (weak_dist < 0) weak_dist = -weak_dist;

        strong_total += strong_dist;
        weak_total += weak_dist;
    }

    CHECK(strong_total < weak_total);
}

static void test_pick_note_always_in_scale(void)
{
    /* All notes produced must lie in the selected scale */
    static const int ionian[7]    = { 0, 2, 4, 5, 7, 9, 11 };
    static const int aeolian[7]   = { 0, 2, 3, 5, 7, 8, 10 };
    static const int pent_maj[5]  = { 0, 2, 4, 7, 9 };
    static const int whole_tone[6] = { 0, 2, 4, 6, 8, 10 };

    StingEngine e;
    sting64_engine_init(&e);
    e.chaos = 200;

    /* ionian */
    e.scale_index = STING64_SCALE_IONIAN;
    for (int i = 0; i < 128; i++) {
        uint8_t n = sting64_engine_pick_note(&e);
        CHECK(note_in_scale(n, ionian, 7));
    }

    /* aeolian */
    e.scale_index = STING64_SCALE_AEOLIAN;
    for (int i = 0; i < 128; i++) {
        uint8_t n = sting64_engine_pick_note(&e);
        CHECK(note_in_scale(n, aeolian, 7));
    }

    /* major pentatonic */
    e.scale_index = STING64_SCALE_MAJOR_PENT;
    for (int i = 0; i < 128; i++) {
        uint8_t n = sting64_engine_pick_note(&e);
        CHECK(note_in_scale(n, pent_maj, 5));
    }

    /* whole tone */
    e.scale_index = STING64_SCALE_WHOLE_TONE;
    for (int i = 0; i < 128; i++) {
        uint8_t n = sting64_engine_pick_note(&e);
        CHECK(note_in_scale(n, whole_tone, 6));
    }
}

static void test_scale_intervals_differ(void)
{
    /* ionian and aeolian must produce different note sets — proves
     * scale tables are actually distinct. Run many steps, collect
     * unique chromas, and verify at least one differs. */
    StingEngine e;
    sting64_engine_init(&e);
    e.chaos = 200;
    e.rand_state = 0x12345678;

    int ionian_chromas[12]  = {0};
    int aeolian_chromas[12] = {0};

    e.scale_index = STING64_SCALE_IONIAN;
    for (int i = 0; i < 512; i++) {
        uint8_t n = sting64_engine_pick_note(&e);
        ionian_chromas[n % 12] = 1;
    }

    /* Reset RNG to same seed */
    e.rand_state  = 0x12345678;
    e.scale_index = STING64_SCALE_AEOLIAN;
    for (int i = 0; i < 512; i++) {
        uint8_t n = sting64_engine_pick_note(&e);
        aeolian_chromas[n % 12] = 1;
    }

    /* Aeolian has b3 (3) and b6 (8); ionian has M3 (4) and M6 (9).
     * At high chaos, both sets should be populated. Check that they differ. */
    int differs = 0;
    for (int i = 0; i < 12; i++) {
        if (ionian_chromas[i] != aeolian_chromas[i]) {
            differs = 1;
            break;
        }
    }
    CHECK(differs);
}

static void test_root_transposition(void)
{
    StingEngine e;
    sting64_engine_init(&e);
    e.chaos = 0;  /* deterministic: same note every step */

    e.root = 0;
    uint8_t note_zero = sting64_engine_pick_note(&e);

    e.rand_state = 0xDEADBEEF;  /* reset RNG to same state */
    e.root = 12;
    uint8_t note_up = sting64_engine_pick_note(&e);

    /* Transposing up 12 semitones must raise the note by exactly one octave */
    CHECK_EQ((int)note_up - (int)note_zero, 12);
}

static void test_root_clamp_bounds(void)
{
    StingEngine e;
    sting64_engine_init(&e);
    e.chaos = 0;

    /* min root: −24 semitones below C4 = C2 (MIDI 36) */
    e.root = -24;
    uint8_t lo = sting64_engine_pick_note(&e);
    CHECK(lo >= 0 && lo <= 127);

    /* max root: +24 semitones above C4 = C6 (MIDI 84) */
    e.root = 24;
    uint8_t hi = sting64_engine_pick_note(&e);
    CHECK(hi >= 0 && hi <= 127);
    CHECK(hi > lo);
}

static void test_rand_state_advances(void)
{
    StingEngine e;
    sting64_engine_init(&e);
    e.chaos = 128;

    uint32_t state_before = e.rand_state;
    sting64_engine_pick_note(&e);
    CHECK(e.rand_state != state_before);
}

static void test_scale_change_rebuilds_loop(void)
{
    StingEngine e;
    uint8_t ionian_loop[16];
    uint8_t aeolian_loop[16];
    int differs = 0;

    sting64_engine_init(&e);
    e.steps_count  = 16;
    e.density      = 255;
    e.chaos       = 200;
    e.root        = 0;

    e.scale_index = STING64_SCALE_IONIAN;
    sting64_engine_invalidate_sequence(&e);
    for (int i = 0; i < 16; i++) {
        e.step_pos = i;
        ionian_loop[i] = sting64_engine_pick_note(&e);
    }

    e.scale_index = STING64_SCALE_AEOLIAN;
    sting64_engine_invalidate_sequence(&e);
    for (int i = 0; i < 16; i++) {
        e.step_pos = i;
        aeolian_loop[i] = sting64_engine_pick_note(&e);
    }

    for (int i = 0; i < 16; i++) {
        if (ionian_loop[i] != aeolian_loop[i]) {
            differs = 1;
            break;
        }
    }
    CHECK(differs);
}

static void test_chromatic_allows_dense_pitch_space(void)
{
    StingEngine e;
    int chromas[12] = {0};
    int seen = 0;

    sting64_engine_init(&e);
    e.steps_count = 64;
    e.density = 255;
    e.chaos = 255;
    e.scale_index = STING64_SCALE_CHROMATIC;
    sting64_engine_invalidate_sequence(&e);

    for (int i = 0; i < sting64_engine_get_steps(&e); i++) {
        e.step_pos = i;
        uint8_t n = sting64_engine_pick_note(&e);
        chromas[n % 12] = 1;
    }

    for (int i = 0; i < 12; i++)
        seen += chromas[i];

    CHECK(seen >= 8);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    test_init_defaults();
    test_get_steps();
    test_pick_note_midi_range();
    test_density_zero_always_rests();
    test_density_full_always_plays();
    test_density_protects_downbeats();
    test_density_keeps_first_step_when_nonzero();
    test_pick_note_zero_chaos();
    test_steps_define_loop_length();
    test_seed_changes_loop_content();
    test_strong_steps_are_more_harmonically_stable();
    test_pick_note_always_in_scale();
    test_scale_intervals_differ();
    test_root_transposition();
    test_root_clamp_bounds();
    test_rand_state_advances();
    test_scale_change_rebuilds_loop();
    test_chromatic_allows_dense_pitch_space();

    printf("Engine tests: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
