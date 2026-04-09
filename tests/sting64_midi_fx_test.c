/*
 * tests/sting64_midi_fx_test.c — Sting64 host wrapper tests
 *
 * Tests the full plugin API (create_instance, set/get_param, process_midi,
 * save_state, load_state). Sting64 uses internal timing or MIDI clock bytes
 * carried through process_midi(); it does not depend on host clock callbacks.
 *
 * Covers:
 * - create_instance returns non-NULL
 * - get_param returns snprintf result (> 0) for every known key
 * - get_param returns -1 for unknown keys
 * - set_param / get_param round-trip for all supported parameters
 * - Move param formats: raw int, raw float-formatted, normalized float
 * - process_midi passes through unrecognized messages
 * - process_midi handles 0xFC (transport stop) cleanly
 * - save_state / load_state round-trip
 * - load_state with incomplete state uses defaults without crash
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../src/host/midi_fx_api_v1.h"
#include "../src/host/plugin_api_v1.h"

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

#define CHECK_EQ_STR(a, b) do { \
    if (strcmp((a), (b)) == 0) { \
        passed++; \
    } else { \
        fprintf(stderr, "FAIL [%s:%d]: strcmp(\"%s\", \"%s\") != 0\n", \
                __FILE__, __LINE__, (a), (b)); \
        failed++; \
    } \
} while (0)

#define CHECK_NEAR(a, b, tol) do { \
    double _a = (double)(a), _b = (double)(b), _t = (double)(tol); \
    if (fabs(_a - _b) <= _t) { \
        passed++; \
    } else { \
        fprintf(stderr, "FAIL [%s:%d]: |%f - %f| > %f\n", \
                __FILE__, __LINE__, _a, _b, _t); \
        failed++; \
    } \
} while (0)

/* ------------------------------------------------------------------ */
/* Plugin vtable — loaded once at startup                              */
/* ------------------------------------------------------------------ */

static midi_fx_api_v1_t *api = NULL;
/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

static void test_create_instance(void)
{
    void *inst = api->create_instance(NULL, NULL);
    CHECK(inst != NULL);
    api->destroy_instance(inst);
}

static void test_get_param_returns_snprintf(void)
{
    void *inst = api->create_instance(NULL, NULL);
    CHECK(inst != NULL);

    char buf[64];
    static const char *keys[] = {
        "density", "chaos", "swing", "gate", "seed", "rate", "sync", "steps", "scale", "root", "velocity", "bpm", NULL
    };
    for (int i = 0; keys[i]; i++) {
        int r = api->get_param(inst, keys[i], buf, sizeof(buf));
        /* Must be > 0 (snprintf result for a non-empty string) */
        CHECK(r > 0);
    }

    /* Unknown key must return -1 */
    CHECK(api->get_param(inst, "__unknown__", buf, sizeof(buf)) == -1);

    api->destroy_instance(inst);
}

static void test_set_get_density(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    api->set_param(inst, "density", "0.7500");
    api->get_param(inst, "density", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.75, 0.01);

    api->set_param(inst, "density", "0.0000");
    api->get_param(inst, "density", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.0, 0.01);

    api->set_param(inst, "density", "1.0000");
    api->get_param(inst, "density", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 1.0, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_chaos(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    api->set_param(inst, "chaos", "0.7500");
    api->get_param(inst, "chaos", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.75, 0.01);

    api->set_param(inst, "chaos", "0.0000");
    api->get_param(inst, "chaos", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.0, 0.01);

    api->set_param(inst, "chaos", "1.0000");
    api->get_param(inst, "chaos", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 1.0, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_swing(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    /* raw value in declared range 0.0–0.5 */
    api->set_param(inst, "swing", "0.3300");
    api->get_param(inst, "swing", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.33, 0.01);

    /* "0.5000" is at the raw max of the declared 0–0.5 range → max swing */
    api->set_param(inst, "swing", "0.5000");
    api->get_param(inst, "swing", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.50, 0.01);

    /* normalized 0–1 form Move sends when value exceeds declared range:
     * "1.0000" → remapped to 0.5 raw (max swing) */
    api->set_param(inst, "swing", "1.0000");
    api->get_param(inst, "swing", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.50, 0.01);

    api->set_param(inst, "swing", "0.0000");
    api->get_param(inst, "swing", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.0, 0.01);

    api->destroy_instance(inst);
}

static void test_set_get_gate(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    api->get_param(inst, "gate", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.5, 0.02);

    api->set_param(inst, "gate", "0.2500");
    api->get_param(inst, "gate", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.25, 0.02);

    api->set_param(inst, "gate", "1.0000");
    api->get_param(inst, "gate", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 1.0, 0.02);

    api->set_param(inst, "gate", "0.0000");
    api->get_param(inst, "gate", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.0, 0.02);

    api->destroy_instance(inst);
}

static void test_set_get_sync(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    api->get_param(inst, "sync", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "move");

    api->set_param(inst, "sync", "move");
    api->get_param(inst, "sync", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "move");

    api->set_param(inst, "sync", "1.0000");
    api->get_param(inst, "sync", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "move");

    api->set_param(inst, "sync", "0");
    api->get_param(inst, "sync", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "internal");

    api->destroy_instance(inst);
}

static void test_set_get_rate(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/16");

    api->set_param(inst, "rate", "1/4D");
    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/4D");

    api->set_param(inst, "rate", "1/4");
    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/4");

    api->set_param(inst, "rate", "1/8T");
    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/8T");

    api->set_param(inst, "rate", "8");
    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/8");

    api->set_param(inst, "rate", "1/16D");
    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/16D");

    api->set_param(inst, "rate", "1/32D");
    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/32D");

    api->set_param(inst, "rate", "1/64");
    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/64");

    api->set_param(inst, "rate", "1.0000");
    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/64T");

    api->set_param(inst, "rate", "0");
    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/4D");

    api->destroy_instance(inst);
}

static void test_set_get_seed(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    api->get_param(inst, "seed", buf, sizeof(buf));
    CHECK((unsigned)atoi(buf) == 1u);

    api->set_param(inst, "seed", "1234");
    api->get_param(inst, "seed", buf, sizeof(buf));
    CHECK((unsigned)atoi(buf) == 1234u);

    api->set_param(inst, "seed", "1.0000");
    api->get_param(inst, "seed", buf, sizeof(buf));
    CHECK((unsigned)atoi(buf) == 65535u);

    api->destroy_instance(inst);
}

static void test_set_get_steps(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    api->set_param(inst, "steps", "4");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 4);

    api->set_param(inst, "steps", "11");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 11);

    api->set_param(inst, "steps", "29");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 29);

    /* float-formatted raw */
    api->set_param(inst, "steps", "7.0000");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 7);

    /* normalized float: 0.0 → min int (1) */
    api->set_param(inst, "steps", "0.0000");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 1);

    /* normalized float: 1.0 → max int (64) */
    api->set_param(inst, "steps", "1.0000");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 64);

    /* clamp low/high */
    api->set_param(inst, "steps", "-9");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 1);

    api->set_param(inst, "steps", "999");
    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 64);

    api->destroy_instance(inst);
}

static void test_set_get_scale(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    /* canonical names round-trip */
    static const char *scales[] = {
        "ionian", "aeolian", "dorian", "mixolydian", "phrygian", "lydian",
        "locrian", "major_pent", "minor_pent", "major_blues", "minor_blues",
        "harmonic_minor", "melodic_minor", "phrygian_dominant",
        "double_harmonic", "whole_tone", "diminished_wh", "diminished_hw",
        "chromatic"
    };
    for (int i = 0; i < 19; i++) {
        api->set_param(inst, "scale", scales[i]);
        api->get_param(inst, "scale", buf, sizeof(buf));
        CHECK_EQ_STR(buf, scales[i]);
    }

    /* aliases */
    api->set_param(inst, "scale", "major");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "ionian");

    api->set_param(inst, "scale", "minor");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "aeolian");

    api->set_param(inst, "scale", "blues");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "minor_blues");

    /* raw index */
    api->set_param(inst, "scale", "2");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "dorian");

    /* normalized float should reach the last enum cleanly */
    api->set_param(inst, "scale", "1.0000");
    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "chromatic");

    api->destroy_instance(inst);
}

static void test_set_get_root(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    api->get_param(inst, "density", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.90, 0.02);

    /* positive */
    api->set_param(inst, "root", "12");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 12);

    /* negative */
    api->set_param(inst, "root", "-12");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == -12);

    /* zero */
    api->set_param(inst, "root", "0");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 0);

    /* float-formatted int */
    api->set_param(inst, "root", "7.0000");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 7);

    /* normalized float form should map back into the declared range */
    api->set_param(inst, "root", "0.0000");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == -24);

    api->set_param(inst, "root", "0.5000");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 0);

    api->set_param(inst, "root", "1.0000");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 24);

    /* clamp at +24 */
    api->set_param(inst, "root", "99");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 24);

    /* clamp at -24 */
    api->set_param(inst, "root", "-99");
    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == -24);

    api->destroy_instance(inst);
}

static void test_set_get_velocity(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];

    api->set_param(inst, "velocity", "0.5000");
    api->get_param(inst, "velocity", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.5, 0.01);

    api->set_param(inst, "velocity", "1.0000");
    api->get_param(inst, "velocity", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 1.0, 0.01);

    api->set_param(inst, "velocity", "0.0000");
    api->get_param(inst, "velocity", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.0, 0.01);

    api->destroy_instance(inst);
}

static void test_process_midi_passthrough(void)
{
    void *inst = api->create_instance(NULL, NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* CC message — must be forwarded unchanged */
    uint8_t cc[3] = { 0xB0, 0x07, 0x7F };
    int n = api->process_midi(inst, cc, 3, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 1);
    CHECK(out[0][0] == 0xB0);
    CHECK(out[0][1] == 0x07);
    CHECK(out[0][2] == 0x7F);

    /* Program change — forwarded */
    uint8_t pc[2] = { 0xC0, 0x01 };
    n = api->process_midi(inst, pc, 2, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 1);
    CHECK(out[0][0] == 0xC0);

    api->destroy_instance(inst);
}

static void test_process_midi_note_on_passthrough(void)
{
    void *inst = api->create_instance(NULL, NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* Note-on latches channel and is forwarded */
    uint8_t noteon[3] = { 0x93, 0x3C, 0x64 };  /* ch 4, C4, vel 100 */
    int n = api->process_midi(inst, noteon, 3, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 1);
    CHECK(out[0][0] == 0x93);

    api->destroy_instance(inst);
}

static void test_process_midi_transport_stop(void)
{
    void *inst = api->create_instance(NULL, NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* Transport stop with no active note: no output, no crash */
    uint8_t stop[1] = { 0xFC };
    int n = api->process_midi(inst, stop, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);

    /* Any output messages must be note-offs only */
    for (int i = 0; i < n; i++) {
        CHECK((out[i][0] & 0xF0) == 0x80);
    }

    api->destroy_instance(inst);
}

static void test_tick_no_crash(void)
{
    void *inst = api->create_instance(NULL, NULL);

    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int     lens[MIDI_FX_MAX_OUT_MSGS];

    /* Free-running mode: tick produces notes — just verify no crash and
     * all returned messages are valid 3-byte channel MIDI */
    int n = api->tick(inst, 512, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n >= 0 && n <= MIDI_FX_MAX_OUT_MSGS);
    for (int i = 0; i < n; i++) {
        uint8_t type = out[i][0] & 0xF0;
        CHECK(type == 0x80 || type == 0x90);  /* note-off or note-on */
        CHECK(out[i][1] <= 127);               /* note in MIDI range */
    }

    /* frames=0 must be safe and produce no output */
    n = api->tick(inst, 0, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 0);

    api->destroy_instance(inst);
}

static void test_move_sync_waits_for_midi_clock(void)
{
    char buf[64];
    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int lens[MIDI_FX_MAX_OUT_MSGS];

    void *inst = api->create_instance(NULL, NULL);
    api->set_param(inst, "sync", "move");
    api->set_param(inst, "density", "1.0000");

    CHECK(api->tick(inst, 6000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS) == 0);
    CHECK(api->get_param(inst, "sync_warn", buf, sizeof(buf)) > 0);
    CHECK_EQ_STR(buf, "Waiting for Move MIDI clock");

    api->destroy_instance(inst);
}

static void test_move_sync_processes_transport_and_clock(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char buf[64];
    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int lens[MIDI_FX_MAX_OUT_MSGS];
    uint8_t start[1] = { 0xFA };
    uint8_t stop[1] = { 0xFC };
    uint8_t clock[1] = { 0xF8 };
    int n;

    api->set_param(inst, "sync", "move");
    api->set_param(inst, "density", "1.0000");
    api->set_param(inst, "rate", "1/8T");
    api->set_param(inst, "steps", "4");

    n = api->process_midi(inst, start, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n > 0);
    CHECK((out[0][0] & 0xF0) == 0x90);

    api->get_param(inst, "sync_warn", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "");

    n = 0;
    for (int i = 0; i < 3; i++)
        n += api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 0);

    n = api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n > 0);
    CHECK((out[0][0] & 0xF0) == 0x80);

    n = 0;
    for (int i = 0; i < 3; i++)
        n += api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 0);

    n = api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n > 0);
    CHECK((out[n - 1][0] & 0xF0) == 0x90);

    n = api->process_midi(inst, stop, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n >= 0);
    api->get_param(inst, "sync_warn", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "transport stopped");

    CHECK(api->tick(inst, 6000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS) == 0);

    api->destroy_instance(inst);
}

static void test_rate_changes_internal_timing(void)
{
    void *inst = api->create_instance(NULL, NULL);
    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int lens[MIDI_FX_MAX_OUT_MSGS];

    api->set_param(inst, "sync", "internal");
    api->set_param(inst, "density", "1.0000");
    api->set_param(inst, "steps", "4");
    api->set_param(inst, "rate", "1/4D");
    CHECK(api->tick(inst, 6000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS) == 0);

    api->set_param(inst, "rate", "1/32T");
    CHECK(api->tick(inst, 6000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS) > 0);

    api->set_param(inst, "rate", "1/64T");
    CHECK(api->tick(inst, 2000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS) > 0);

    api->destroy_instance(inst);
}

static void test_fractional_move_rates_work(void)
{
    void *inst = api->create_instance(NULL, NULL);
    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int lens[MIDI_FX_MAX_OUT_MSGS];
    uint8_t start[1] = { 0xFA };
    uint8_t clock[1] = { 0xF8 };
    int note_ons = 0;

    api->set_param(inst, "sync", "move");
    api->set_param(inst, "density", "1.0000");
    api->set_param(inst, "gate", "1.0000");

    api->set_param(inst, "rate", "1/32D");
    CHECK(api->process_midi(inst, start, 1, out, lens, MIDI_FX_MAX_OUT_MSGS) > 0);
    for (int i = 0; i < 4; i++) {
        int n = api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
        for (int j = 0; j < n; j++)
            if ((out[j][0] & 0xF0) == 0x90)
                note_ons++;
    }
    CHECK(note_ons == 0);
    CHECK(api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS) > 0);

    api->set_param(inst, "rate", "1/64");
    CHECK(api->process_midi(inst, start, 1, out, lens, MIDI_FX_MAX_OUT_MSGS) > 0);
    note_ons = 0;
    for (int i = 0; i < 3; i++) {
        int n = api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
        for (int j = 0; j < n; j++)
            if ((out[j][0] & 0xF0) == 0x90)
                note_ons++;
    }
    CHECK(note_ons >= 2);

    api->destroy_instance(inst);
}

static void test_gate_changes_internal_note_length(void)
{
    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int lens[MIDI_FX_MAX_OUT_MSGS];
    void *inst;

    inst = api->create_instance(NULL, NULL);
    api->set_param(inst, "sync", "internal");
    api->set_param(inst, "density", "1.0000");
    api->set_param(inst, "steps", "4");
    api->set_param(inst, "rate", "1/8");
    api->set_param(inst, "gate", "1.0000");
    CHECK(api->tick(inst, 12000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS) > 0);
    CHECK(api->tick(inst, 2000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS) == 0);
    api->destroy_instance(inst);

    inst = api->create_instance(NULL, NULL);
    api->set_param(inst, "sync", "internal");
    api->set_param(inst, "density", "1.0000");
    api->set_param(inst, "steps", "4");
    api->set_param(inst, "rate", "1/8");
    api->set_param(inst, "gate", "0.1000");
    CHECK(api->tick(inst, 12000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS) > 0);
    CHECK(api->tick(inst, 2000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS) > 0);

    api->destroy_instance(inst);
}

static void test_gate_changes_move_clock_note_length(void)
{
    void *inst = api->create_instance(NULL, NULL);
    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int lens[MIDI_FX_MAX_OUT_MSGS];
    uint8_t start[1] = { 0xFA };
    uint8_t clock[1] = { 0xF8 };
    int n;

    api->set_param(inst, "sync", "move");
    api->set_param(inst, "density", "1.0000");
    api->set_param(inst, "rate", "1/8");

    api->set_param(inst, "gate", "0.1000");
    n = api->process_midi(inst, start, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n > 0);
    n = api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 0);
    n = api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n > 0);
    CHECK((out[0][0] & 0xF0) == 0x80);

    api->set_param(inst, "gate", "1.0000");
    n = api->process_midi(inst, start, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n > 0);
    n = 0;
    for (int i = 0; i < 6; i++)
        n += api->process_midi(inst, clock, 1, out, lens, MIDI_FX_MAX_OUT_MSGS);
    CHECK(n == 0);

    api->destroy_instance(inst);
}

static void test_seed_changes_generated_loop(void)
{
    void *inst = api->create_instance(NULL, NULL);
    uint8_t out[MIDI_FX_MAX_OUT_MSGS][3];
    int lens[MIDI_FX_MAX_OUT_MSGS];
    int first_note = -1;
    int second_note = -1;

    api->set_param(inst, "sync", "internal");
    api->set_param(inst, "density", "1.0000");
    api->set_param(inst, "chaos", "1.0000");
    api->set_param(inst, "steps", "8");
    api->set_param(inst, "rate", "1/32T");
    api->set_param(inst, "seed", "100");

    for (int i = 0; i < 8 && first_note < 0; i++) {
        int n = api->tick(inst, 6000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS);
        for (int j = 0; j < n; j++) {
            if ((out[j][0] & 0xF0) == 0x90) {
                first_note = out[j][1];
                break;
            }
        }
    }

    api->set_param(inst, "seed", "200");
    for (int i = 0; i < 8 && second_note < 0; i++) {
        int n = api->tick(inst, 6000, 44100, out, lens, MIDI_FX_MAX_OUT_MSGS);
        for (int j = 0; j < n; j++) {
            if ((out[j][0] & 0xF0) == 0x90) {
                second_note = out[j][1];
                break;
            }
        }
    }

    CHECK(first_note >= 0);
    CHECK(second_note >= 0);
    CHECK(first_note != second_note);

    api->destroy_instance(inst);
}

static void test_set_get_bpm(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char  buf[64];

    /* default */
    api->get_param(inst, "bpm", buf, sizeof(buf));
    CHECK(atoi(buf) == 120);

    /* raw int */
    api->set_param(inst, "bpm", "140");
    api->get_param(inst, "bpm", buf, sizeof(buf));
    CHECK(atoi(buf) == 140);

    /* clamp low */
    api->set_param(inst, "bpm", "10");
    api->get_param(inst, "bpm", buf, sizeof(buf));
    CHECK(atoi(buf) == 20);

    /* clamp high */
    api->set_param(inst, "bpm", "999");
    api->get_param(inst, "bpm", buf, sizeof(buf));
    CHECK(atoi(buf) == 300);

    /* normalized float: 0.5 → midpoint of 20–300 = 160 */
    api->set_param(inst, "bpm", "0.5");
    api->get_param(inst, "bpm", buf, sizeof(buf));
    CHECK(atoi(buf) >= 155 && atoi(buf) <= 165);

    api->destroy_instance(inst);
}

static void test_save_load_roundtrip(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char  state[1024];
    char  buf[64];

    /* Set non-default values for every param */
    api->set_param(inst, "density",  "0.3500");
    api->set_param(inst, "chaos",    "0.8000");
    api->set_param(inst, "swing",    "0.3000");
    api->set_param(inst, "gate",     "0.6500");
    api->set_param(inst, "seed",     "4242");
    api->set_param(inst, "rate",     "1/8T");
    api->set_param(inst, "sync",     "move");
    api->set_param(inst, "steps",    "32");
    api->set_param(inst, "scale",    "dorian");
    api->set_param(inst, "root",     "-7");
    api->set_param(inst, "velocity", "0.6000");
    api->set_param(inst, "bpm",      "140");

    /* Save */
    int n = api->save_state(inst, state, sizeof(state));
    CHECK(n > 0);

    /* Reset to defaults */
    api->set_param(inst, "density",  "0.75");
    api->set_param(inst, "chaos",    "0.25");
    api->set_param(inst, "swing",    "0.0");
    api->set_param(inst, "gate",     "0.5");
    api->set_param(inst, "seed",     "1");
    api->set_param(inst, "rate",     "1/16");
    api->set_param(inst, "sync",     "internal");
    api->set_param(inst, "steps",    "16");
    api->set_param(inst, "scale",    "ionian");
    api->set_param(inst, "root",     "0");
    api->set_param(inst, "velocity", "0.75");
    api->set_param(inst, "bpm",      "120");

    /* Restore */
    api->load_state(inst, state, n);

    /* Verify each param round-tripped */
    api->get_param(inst, "density", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.35, 0.02);

    api->get_param(inst, "chaos", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.80, 0.02);

    api->get_param(inst, "swing", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.30, 0.02);

    api->get_param(inst, "gate", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.65, 0.02);

    api->get_param(inst, "seed", buf, sizeof(buf));
    CHECK((unsigned)atoi(buf) == 4242u);

    api->get_param(inst, "rate", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1/8T");

    api->get_param(inst, "sync", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "move");

    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 32);

    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "dorian");

    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == -7);

    api->get_param(inst, "velocity", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.60, 0.02);

    api->get_param(inst, "bpm", buf, sizeof(buf));
    CHECK(atoi(buf) == 140);

    api->destroy_instance(inst);
}

static void test_load_partial_state(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char  buf[64];

    /* Partial state — density, chaos, and scale only */
    const char *partial = "density=0.4000\nchaos=0.9000\nscale=minor_pent\n";
    api->load_state(inst, partial, (int)strlen(partial));

    api->get_param(inst, "density", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.40, 0.02);

    api->get_param(inst, "chaos", buf, sizeof(buf));
    CHECK_NEAR(atof(buf), 0.90, 0.02);

    api->get_param(inst, "scale", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "minor_pent");

    /* Other params should still be at their defaults */
    api->get_param(inst, "sync", buf, sizeof(buf));
    CHECK_EQ_STR(buf, "move");

    api->get_param(inst, "steps", buf, sizeof(buf));
    CHECK(atoi(buf) == 16);

    api->get_param(inst, "root", buf, sizeof(buf));
    CHECK(atoi(buf) == 0);

    api->destroy_instance(inst);
}

static void test_load_empty_state_no_crash(void)
{
    void *inst = api->create_instance(NULL, NULL);
    api->load_state(inst, "", 0);
    api->load_state(inst, NULL, 0);
    /* If we get here without crash, pass */
    CHECK(1);
    api->destroy_instance(inst);
}

static void test_save_state_small_buffer_no_crash(void)
{
    void *inst = api->create_instance(NULL, NULL);
    char state[8];

    int n = api->save_state(inst, state, sizeof(state));
    CHECK(n > 0);
    CHECK(state[sizeof(state) - 1] == '\0');

    api->destroy_instance(inst);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    /* Initialize the plugin API with a NULL host pointer.
     * All g_host accesses in the wrapper are guarded — this is safe. */
    api = move_midi_fx_init(NULL);
    if (!api) {
        fprintf(stderr, "FATAL: move_midi_fx_init returned NULL\n");
        return 1;
    }
    CHECK(api->api_version == 1);

    test_create_instance();
    test_get_param_returns_snprintf();
    test_set_get_density();
    test_set_get_chaos();
    test_set_get_swing();
    test_set_get_gate();
    test_set_get_seed();
    test_set_get_rate();
    test_set_get_sync();
    test_set_get_steps();
    test_set_get_scale();
    test_set_get_root();
    test_set_get_velocity();
    test_set_get_bpm();
    test_process_midi_passthrough();
    test_process_midi_note_on_passthrough();
    test_process_midi_transport_stop();
    test_tick_no_crash();
    test_move_sync_waits_for_midi_clock();
    test_move_sync_processes_transport_and_clock();
    test_rate_changes_internal_timing();
    test_fractional_move_rates_work();
    test_gate_changes_internal_note_length();
    test_gate_changes_move_clock_note_length();
    test_seed_changes_generated_loop();
    test_save_load_roundtrip();
    test_load_partial_state();
    test_load_empty_state_no_crash();
    test_save_state_small_buffer_no_crash();

    printf("MIDI FX tests: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
