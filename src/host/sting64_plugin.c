/*
 * host/sting64_plugin.c — Schwung host wrapper for sting64
 *
 * Implements the midi_fx_api_v1_t interface for the sting64 engine.
 * Drives sting64_engine via tick(), handles param I/O, state save/load.
 *
 * Clock: internal BPM or Move MIDI clock. Move sync is driven from the
 * incoming MIDI transport/clock bytes (0xFA/0xFB/0xFC/0xF8), not from the
 * host clock callbacks, because those callbacks are unstable on some
 * firmware versions.
 *
 * Note lifecycle: single active note, flushed on 0xFC transport stop.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "midi_fx_api_v1.h"
#include "plugin_api_v1.h"
#include "../dsp/sting64_engine.h"

#define STING64_SYNC_INTERNAL 0
#define STING64_SYNC_MOVE     1
#define STING64_RATE_QUARTER_D      0
#define STING64_RATE_QUARTER        1
#define STING64_RATE_QUARTER_T      2
#define STING64_RATE_EIGHTH_D       3
#define STING64_RATE_EIGHTH         4
#define STING64_RATE_EIGHTH_T       5
#define STING64_RATE_SIXTEENTH_D    6
#define STING64_RATE_SIXTEENTH      7
#define STING64_RATE_SIXTEENTH_T    8
#define STING64_RATE_THIRTYSECOND_D 9
#define STING64_RATE_THIRTYSECOND   10
#define STING64_RATE_THIRTYSECOND_T 11
#define STING64_RATE_SIXTYFOURTH_D  12
#define STING64_RATE_SIXTYFOURTH    13
#define STING64_RATE_SIXTYFOURTH_T  14

#define STING64_MIDI_CLOCK_SUBDIV   4

static const host_api_v1_t *g_host = NULL;

/* ------------------------------------------------------------------ */
/* Per-instance state                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    StingEngine engine;

    /* timing */
    uint16_t bpm;                /* user-set BPM 20–300, default 120 */
    uint8_t  rate;               /* quarter to sixty-fourth, incl dotted/triplet */
    uint8_t  gate;               /* gate ratio 0–255, default ~0.5 */
    uint8_t  sync_mode;          /* internal BPM or Move MIDI clock */
    uint8_t  running;            /* tick-time running state */
    uint8_t  last_clock_status;  /* cached sync warning state */
    int      last_sample_rate;   /* detect sample rate changes */
    uint32_t frames_per_step;    /* recomputed from bpm + sample_rate */
    uint32_t frames_accum;       /* straight-time frame accumulator */
    uint16_t clock_phase_subclocks; /* external MIDI clock phase in 1/4-clock units */
    uint16_t step_subclocks;        /* step length in 1/4-clock units */

    /* swing */
    uint8_t  swing_pending;
    uint32_t swing_frames_left;
    uint16_t swing_subclocks_left;
    uint8_t  pending_note;

    /* gate */
    uint32_t note_off_frames;
    uint16_t note_off_subclocks;
    uint8_t  note_off_pending;

    /* active note */
    uint8_t  active_note;        /* 255 = none */
    uint8_t  active_channel;

} Sting64Instance;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int flush_active_note(Sting64Instance *inst,
                             uint8_t out_msgs[][3], int out_lens[],
                             int max_out, int count)
{
    if (inst->active_note != 255 && count < max_out) {
        out_msgs[count][0] = 0x80 | (inst->active_channel & 0x0F);
        out_msgs[count][1] = inst->active_note;
        out_msgs[count][2] = 0;
        out_lens[count]    = 3;
        count++;
        inst->active_note      = 255;
        inst->note_off_pending = 0;
        inst->note_off_frames  = 0;
        inst->note_off_subclocks = 0;
    }
    return count;
}

static float rate_notes_per_beat(uint8_t rate)
{
    switch (rate) {
    case STING64_RATE_QUARTER_D:
        return 2.0f / 3.0f;
    case STING64_RATE_QUARTER:
        return 1.0f;
    case STING64_RATE_QUARTER_T:
        return 1.5f;
    case STING64_RATE_EIGHTH_D:
        return 4.0f / 3.0f;
    case STING64_RATE_EIGHTH:
        return 2.0f;
    case STING64_RATE_EIGHTH_T:
        return 3.0f;
    case STING64_RATE_SIXTEENTH_D:
        return 8.0f / 3.0f;
    case STING64_RATE_SIXTEENTH:
        return 4.0f;
    case STING64_RATE_SIXTEENTH_T:
        return 6.0f;
    case STING64_RATE_THIRTYSECOND_D:
        return 16.0f / 3.0f;
    case STING64_RATE_THIRTYSECOND:
        return 8.0f;
    case STING64_RATE_THIRTYSECOND_T:
        return 12.0f;
    case STING64_RATE_SIXTYFOURTH_D:
        return 32.0f / 3.0f;
    case STING64_RATE_SIXTYFOURTH:
        return 16.0f;
    case STING64_RATE_SIXTYFOURTH_T:
        return 24.0f;
    default:
        return 4.0f;
    }
}

static uint16_t step_subclocks_for_rate(uint8_t rate)
{
    switch (rate) {
    case STING64_RATE_QUARTER_D:
        return 144;
    case STING64_RATE_QUARTER:
        return 96;
    case STING64_RATE_QUARTER_T:
        return 64;
    case STING64_RATE_EIGHTH_D:
        return 72;
    case STING64_RATE_EIGHTH:
        return 48;
    case STING64_RATE_EIGHTH_T:
        return 32;
    case STING64_RATE_SIXTEENTH_D:
        return 36;
    case STING64_RATE_SIXTEENTH:
        return 24;
    case STING64_RATE_SIXTEENTH_T:
        return 16;
    case STING64_RATE_THIRTYSECOND_D:
        return 18;
    case STING64_RATE_THIRTYSECOND:
        return 12;
    case STING64_RATE_THIRTYSECOND_T:
        return 8;
    case STING64_RATE_SIXTYFOURTH_D:
        return 9;
    case STING64_RATE_SIXTYFOURTH:
        return 6;
    case STING64_RATE_SIXTYFOURTH_T:
        return 4;
    default:
        return 24;
    }
}

static void recompute_timing(Sting64Instance *inst, int sample_rate, float bpm)
{
    if (sample_rate <= 0) sample_rate = 44100;
    if (bpm < 20.0f) bpm = 20.0f;
    if (bpm > 300.0f) bpm = 300.0f;
    float steps_per_second = (bpm / 60.0f) * rate_notes_per_beat(inst->rate);
    inst->frames_per_step  = (uint32_t)(sample_rate / steps_per_second + 0.5f);
    if (inst->frames_per_step < 1) inst->frames_per_step = 1;
    inst->step_subclocks   = step_subclocks_for_rate(inst->rate);
    inst->last_sample_rate = sample_rate;
}

static float current_bpm(const Sting64Instance *inst)
{
    return (float)inst->bpm;
}

static int update_running_state(Sting64Instance *inst)
{
    if (inst->sync_mode == STING64_SYNC_MOVE)
        return inst->running;

    inst->last_clock_status = MOVE_CLOCK_STATUS_STOPPED;
    inst->running = 1;
    return 1;
}

static uint32_t swing_offset_frames(const Sting64Instance *inst)
{
    float ratio = inst->engine.swing / 255.0f;
    return (uint32_t)(inst->frames_per_step * ratio * 0.5f + 0.5f);
}

static uint16_t swing_offset_subclocks(const Sting64Instance *inst)
{
    float ratio = inst->engine.swing / 255.0f;
    uint16_t delay = (uint16_t)(inst->step_subclocks * ratio * 0.5f + 0.5f);
    if (delay >= inst->step_subclocks)
        delay = (uint16_t)(inst->step_subclocks - 1);
    return delay;
}

static int is_swung_step(const Sting64Instance *inst)
{
    return (inst->engine.step_pos & 1) == 1;
}

static int emit_note_on(Sting64Instance *inst, uint8_t note,
                        uint8_t out_msgs[][3], int out_lens[],
                        int max_out, int count)
{
    if (count >= max_out) return count;

    uint8_t vel = inst->engine.velocity;
    if (vel == 0) vel = 1;

    out_msgs[count][0] = 0x90 | (inst->active_channel & 0x0F);
    out_msgs[count][1] = note;
    out_msgs[count][2] = vel;
    out_lens[count]    = 3;
    count++;

    inst->active_note      = note;
    inst->note_off_pending = 1;
    /* gate: 0.0..1.0 of the step duration */
    if (inst->sync_mode == STING64_SYNC_MOVE) {
        inst->note_off_subclocks =
            (uint16_t)(((uint32_t)inst->step_subclocks * inst->gate + 127u) / 255u);
        if (inst->note_off_subclocks < 1) inst->note_off_subclocks = 1;
        inst->note_off_frames = 0;
    } else {
        inst->note_off_frames =
            (uint32_t)(((uint64_t)inst->frames_per_step * inst->gate + 127u) / 255u);
        if (inst->note_off_frames < 1) inst->note_off_frames = 1;
        inst->note_off_subclocks = 0;
    }

    return count;
}

static void reset_transport_phase(Sting64Instance *inst, int reset_step)
{
    if (!inst) return;

    if (reset_step)
        inst->engine.step_pos = 0;

    inst->frames_accum = 0;
    inst->clock_phase_subclocks = 0;
    inst->swing_pending = 0;
    inst->swing_frames_left = 0;
    inst->swing_subclocks_left = 0;
    inst->note_off_pending = 0;
    inst->note_off_frames = 0;
    inst->note_off_subclocks = 0;
}

static int run_step_boundary(Sting64Instance *inst, int clock_mode,
                             uint8_t out_msgs[][3], int out_lens[],
                             int max_out, int count)
{
    int should_play = sting64_engine_should_play_step(&inst->engine);
    int swung = is_swung_step(inst);

    if (should_play) {
        uint8_t note = sting64_engine_pick_note(&inst->engine);

        if (clock_mode) {
            uint16_t delay = swung ? swing_offset_subclocks(inst) : 0;
            if (delay > 0) {
                inst->pending_note = note;
                inst->swing_pending = 1;
                inst->swing_subclocks_left = delay;
            } else {
                count = flush_active_note(inst, out_msgs, out_lens, max_out, count);
                count = emit_note_on(inst, note, out_msgs, out_lens, max_out, count);
            }
        } else {
            uint32_t delay = swung ? swing_offset_frames(inst) : 0;
            if (delay > 0) {
                inst->pending_note = note;
                inst->swing_pending = 1;
                inst->swing_frames_left = delay;
            } else {
                count = flush_active_note(inst, out_msgs, out_lens, max_out, count);
                count = emit_note_on(inst, note, out_msgs, out_lens, max_out, count);
            }
        }
    }

    inst->engine.step_pos = (inst->engine.step_pos + 1) %
                            sting64_engine_get_steps(&inst->engine);
    return count;
}

static int process_clock_tick(Sting64Instance *inst,
                              uint8_t out_msgs[][3], int out_lens[],
                              int max_out)
{
    int count = 0;

    if (!inst || !inst->running)
        return 0;

    if (inst->note_off_pending && inst->active_note != 255) {
        if (inst->note_off_subclocks > STING64_MIDI_CLOCK_SUBDIV)
            inst->note_off_subclocks -= STING64_MIDI_CLOCK_SUBDIV;
        else
            inst->note_off_subclocks = 0;
        if (inst->note_off_subclocks == 0)
            count = flush_active_note(inst, out_msgs, out_lens, max_out, count);
    }

    if (inst->swing_pending) {
        if (inst->swing_subclocks_left > STING64_MIDI_CLOCK_SUBDIV)
            inst->swing_subclocks_left -= STING64_MIDI_CLOCK_SUBDIV;
        else
            inst->swing_subclocks_left = 0;
        if (inst->swing_subclocks_left == 0) {
            inst->swing_pending = 0;
            count = flush_active_note(inst, out_msgs, out_lens, max_out, count);
            count = emit_note_on(inst, inst->pending_note, out_msgs, out_lens,
                                 max_out, count);
        }
    }

    inst->clock_phase_subclocks += STING64_MIDI_CLOCK_SUBDIV;
    while (inst->clock_phase_subclocks >= inst->step_subclocks) {
        inst->clock_phase_subclocks -= inst->step_subclocks;
        count = run_step_boundary(inst, 1, out_msgs, out_lens, max_out, count);
    }

    return count;
}

/* ------------------------------------------------------------------ */
/* create_instance                                                      */
/* ------------------------------------------------------------------ */

static void *create_instance(const char *module_dir, const char *config_json)
{
    (void)module_dir;
    (void)config_json;

    Sting64Instance *inst = calloc(1, sizeof(Sting64Instance));
    if (!inst) return NULL;

    sting64_engine_init(&inst->engine);

    inst->bpm              = 120;
    inst->rate             = STING64_RATE_SIXTEENTH;
    inst->gate             = 128;
    inst->sync_mode        = STING64_SYNC_MOVE; /* matches module.json default */
    inst->running          = 0;
    inst->last_clock_status = MOVE_CLOCK_STATUS_STOPPED;
    inst->last_sample_rate = 44100;
    inst->frames_per_step  = 5513;   /* 120 BPM @ 44100 Hz fallback */
    inst->frames_accum     = 0;
    inst->clock_phase_subclocks = 0;
    inst->step_subclocks  = step_subclocks_for_rate(inst->rate);
    inst->swing_pending    = 0;
    inst->swing_frames_left = 0;
    inst->swing_subclocks_left = 0;
    inst->note_off_pending  = 0;
    inst->note_off_frames   = 0;
    inst->note_off_subclocks = 0;
    inst->active_note      = 255;    /* sentinel: no active note */
    inst->active_channel   = 0;

    return inst;
}

/* ------------------------------------------------------------------ */
/* destroy_instance                                                     */
/* ------------------------------------------------------------------ */

static void destroy_instance(void *instance)
{
    free(instance);
}

/* ------------------------------------------------------------------ */
/* process_midi                                                          */
/* ------------------------------------------------------------------ */

static int process_midi(void *instance,
                        const uint8_t *in_msg, int in_len,
                        uint8_t out_msgs[][3], int out_lens[],
                        int max_out)
{
    Sting64Instance *inst = (Sting64Instance *)instance;
    int count = 0;

    if (in_len < 1) return 0;

    uint8_t status = in_msg[0];
    uint8_t type   = status & 0xF0;
    uint8_t ch     = status & 0x0F;

    if (inst->sync_mode == STING64_SYNC_MOVE) {
        if (status == 0xFA) { /* Start */
            count = flush_active_note(inst, out_msgs, out_lens, max_out, count);
            inst->running = 1;
            inst->last_clock_status = MOVE_CLOCK_STATUS_RUNNING;
            reset_transport_phase(inst, 1);
            return run_step_boundary(inst, 1, out_msgs, out_lens, max_out, count);
        }

        if (status == 0xFB) { /* Continue */
            inst->running = 1;
            inst->last_clock_status = MOVE_CLOCK_STATUS_RUNNING;
            inst->clock_phase_subclocks = 0;
            return 0;
        }

        if (status == 0xFC) { /* Stop */
            count = flush_active_note(inst, out_msgs, out_lens, max_out, count);
            inst->running = 0;
            inst->last_clock_status = MOVE_CLOCK_STATUS_STOPPED;
            reset_transport_phase(inst, 1);
            return count;
        }

        if (status == 0xF8) { /* MIDI Clock */
            if (!inst->running) {
                inst->running = 1;
                inst->last_clock_status = MOVE_CLOCK_STATUS_RUNNING;
            }
            return process_clock_tick(inst, out_msgs, out_lens, max_out);
        }
    }

    /* Transport stop — flush active note */
    if (status == 0xFC) {
        count = flush_active_note(inst, out_msgs, out_lens, max_out, count);
        inst->running         = 0;
        inst->frames_accum     = 0;
        inst->swing_pending    = 0;
        inst->note_off_pending = 0;
        return count;
    }

    /* Note-on — latch MIDI channel */
    if (type == 0x90 && in_len >= 3 && in_msg[2] > 0)
        inst->active_channel = ch;

    /* Pass through */
    if (count < max_out) {
        memcpy(out_msgs[count], in_msg,
               (in_len > 3 ? 3 : (size_t)in_len));
        out_lens[count] = in_len;
        count++;
    }

    return count;
}

/* ------------------------------------------------------------------ */
/* tick                                                                  */
/* ------------------------------------------------------------------ */

static int tick(void *instance,
                int nframes, int sample_rate,
                uint8_t out_msgs[][3], int out_lens[],
                int max_out)
{
    Sting64Instance *inst = (Sting64Instance *)instance;
    int count = 0;
    int was_running = inst->running;

    if (inst->sync_mode == STING64_SYNC_MOVE)
        return 0;

    update_running_state(inst);
    recompute_timing(inst, sample_rate, current_bpm(inst));

    if (inst->frames_per_step == 0 || nframes <= 0) return count;

    if (!inst->running) {
        count = flush_active_note(inst, out_msgs, out_lens, max_out, count);
        inst->frames_accum      = 0;
        inst->swing_pending     = 0;
        inst->note_off_pending  = 0;
        return count;
    }

    if (!was_running && inst->running) {
        inst->engine.step_pos    = 0;
        inst->frames_accum       = 0;
        inst->swing_pending      = 0;
        inst->note_off_pending   = 0;
    }

    uint32_t frames_left = (uint32_t)nframes;

    while (frames_left > 0 && count < max_out) {

        /* Service note-off */
        if (inst->note_off_pending && inst->active_note != 255) {
            if (inst->note_off_frames <= frames_left) {
                frames_left -= inst->note_off_frames;
                inst->note_off_frames = 0;
                count = flush_active_note(inst, out_msgs, out_lens,
                                          max_out, count);
            } else {
                inst->note_off_frames -= frames_left;
                frames_left = 0;
                break;
            }
        }

        /* Service pending swung step */
        if (inst->swing_pending) {
            if (inst->swing_frames_left <= frames_left) {
                frames_left -= inst->swing_frames_left;
                inst->swing_frames_left = 0;
                inst->swing_pending     = 0;
                count = flush_active_note(inst, out_msgs, out_lens,
                                          max_out, count);
                count = emit_note_on(inst, inst->pending_note,
                                     out_msgs, out_lens, max_out, count);
            } else {
                inst->swing_frames_left -= frames_left;
                frames_left = 0;
                break;
            }
            continue;
        }

        /* Advance accumulator */
        uint32_t to_next = inst->frames_per_step - inst->frames_accum;
        if (frames_left < to_next) {
            inst->frames_accum += frames_left;
            frames_left = 0;
            break;
        }

        frames_left -= to_next;
        inst->frames_accum = 0;

        count = run_step_boundary(inst, 0, out_msgs, out_lens, max_out, count);
    }

    return count;
}

/* ------------------------------------------------------------------ */
/* Parameter parsing                                                    */
/* ------------------------------------------------------------------ */

static uint8_t parse_norm_float(const char *s)
{
    if (!s) return 0;
    float v = (float)atof(s);
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return (uint8_t)(v * 255.0f + 0.5f);
}

static uint8_t parse_swing(const char *s)
{
    if (!s) return 0;
    float v = (float)atof(s);
    if (v < 0.0f) v = 0.0f;
    if (v > 0.5f) v = v * 0.5f;
    float norm = v / 0.5f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint8_t)(norm * 255.0f + 0.5f);
}

static int looks_normalized_float(const char *s, double v)
{
    if (!s) return 0;
    if (v < 0.0 || v > 1.0) return 0;
    return strchr(s, '.') != NULL ||
           strchr(s, 'e') != NULL ||
           strchr(s, 'E') != NULL;
}

static int8_t parse_root(const char *s)
{
    if (!s) return 0;
    double d = atof(s);
    int v;

    if (looks_normalized_float(s, d)) {
        double mapped = -24.0 + d * 48.0;
        v = (int)(mapped + (mapped >= 0.0 ? 0.5 : -0.5));
    } else {
        v = (int)(d + (d >= 0.0 ? 0.5 : -0.5));
    }

    if (v < -24) v = -24;
    if (v >  24) v =  24;
    return (int8_t)v;
}

static uint8_t parse_steps(const char *s)
{
    double dv;
    int v;

    if (!s) return 16;

    dv = atof(s);
    if (looks_normalized_float(s, dv)) {
        v = (int)(1.0 + dv * (STING64_MAX_STEPS - 1) + 0.5);
    } else {
        v = (int)(dv + (dv >= 0.0 ? 0.5 : -0.5));
    }

    if (v < 1) v = 1;
    if (v > STING64_MAX_STEPS) v = STING64_MAX_STEPS;
    return (uint8_t)v;
}

static uint32_t parse_seed(const char *s)
{
    double dv;
    long long v;

    if (!s) return 1;

    dv = atof(s);
    if (looks_normalized_float(s, dv)) {
        if (dv < 0.0) dv = 0.0;
        if (dv > 1.0) dv = 1.0;
        return (uint32_t)(dv * 65535.0 + 0.5);
    }

    v = atoll(s);
    if (v < 0) v = 0;
    if (v > 65535) v = 65535;
    return (uint32_t)v;
}

static uint8_t parse_rate(const char *s)
{
    double dv;
    int iv;

    if (!s) return STING64_RATE_SIXTEENTH;

    if (strcmp(s, "1/4D") == 0 || strcmp(s, "quarter_dotted") == 0)
        return STING64_RATE_QUARTER_D;
    if (strcmp(s, "1/4") == 0 || strcmp(s, "quarter") == 0)
        return STING64_RATE_QUARTER;
    if (strcmp(s, "1/4T") == 0 || strcmp(s, "quarter_triplet") == 0)
        return STING64_RATE_QUARTER_T;
    if (strcmp(s, "1/8D") == 0 || strcmp(s, "eighth_dotted") == 0)
        return STING64_RATE_EIGHTH_D;
    if (strcmp(s, "1/8") == 0 || strcmp(s, "eighth") == 0)
        return STING64_RATE_EIGHTH;
    if (strcmp(s, "1/8T") == 0 || strcmp(s, "eighth_triplet") == 0)
        return STING64_RATE_EIGHTH_T;
    if (strcmp(s, "1/16D") == 0 || strcmp(s, "sixteenth_dotted") == 0)
        return STING64_RATE_SIXTEENTH_D;
    if (strcmp(s, "1/16") == 0 || strcmp(s, "sixteenth") == 0)
        return STING64_RATE_SIXTEENTH;
    if (strcmp(s, "1/16T") == 0 || strcmp(s, "sixteenth_triplet") == 0)
        return STING64_RATE_SIXTEENTH_T;
    if (strcmp(s, "1/32D") == 0 || strcmp(s, "thirty-second_dotted") == 0)
        return STING64_RATE_THIRTYSECOND_D;
    if (strcmp(s, "1/32") == 0 || strcmp(s, "thirty-second") == 0)
        return STING64_RATE_THIRTYSECOND;
    if (strcmp(s, "1/32T") == 0 || strcmp(s, "thirty-second_triplet") == 0)
        return STING64_RATE_THIRTYSECOND_T;
    if (strcmp(s, "1/64D") == 0 || strcmp(s, "sixty-fourth_dotted") == 0)
        return STING64_RATE_SIXTYFOURTH_D;
    if (strcmp(s, "1/64") == 0 || strcmp(s, "sixty-fourth") == 0)
        return STING64_RATE_SIXTYFOURTH;
    if (strcmp(s, "1/64T") == 0 || strcmp(s, "sixty-fourth_triplet") == 0)
        return STING64_RATE_SIXTYFOURTH_T;

    dv = atof(s);
    if (looks_normalized_float(s, dv)) {
        int idx = (int)(dv * 15.0);
        if (idx > 14) idx = 14;
        return (uint8_t)idx;
    }

    iv = (int)(dv + (dv >= 0.0 ? 0.5 : -0.5));
    if (iv == 0) return STING64_RATE_QUARTER_D;
    if (iv == 4) return STING64_RATE_QUARTER;
    if (iv == 8) return STING64_RATE_EIGHTH;
    if (iv == 16) return STING64_RATE_SIXTEENTH;
    if (iv == 32) return STING64_RATE_THIRTYSECOND;
    if (iv == 64) return STING64_RATE_SIXTYFOURTH;
    if (iv >= 0 && iv <= 14) return (uint8_t)iv;
    return STING64_RATE_SIXTEENTH;
}

static uint8_t parse_sync(const char *s)
{
    double dv;
    int iv;

    if (!s) return STING64_SYNC_INTERNAL;

    if (strcmp(s, "internal") == 0)
        return STING64_SYNC_INTERNAL;
    if (strcmp(s, "move") == 0)
        return STING64_SYNC_MOVE;

    dv = atof(s);
    if (looks_normalized_float(s, dv))
        return dv >= 0.5 ? STING64_SYNC_MOVE : STING64_SYNC_INTERNAL;

    iv = (int)(dv + (dv >= 0.0 ? 0.5 : -0.5));
    return iv > 0 ? STING64_SYNC_MOVE : STING64_SYNC_INTERNAL;
}

static const char *sync_name(uint8_t mode)
{
    return mode == STING64_SYNC_MOVE ? "move" : "internal";
}

static const char *rate_name(uint8_t rate)
{
    switch (rate) {
    case STING64_RATE_QUARTER_D:
        return "1/4D";
    case STING64_RATE_QUARTER:
        return "1/4";
    case STING64_RATE_QUARTER_T:
        return "1/4T";
    case STING64_RATE_EIGHTH_D:
        return "1/8D";
    case STING64_RATE_EIGHTH:
        return "1/8";
    case STING64_RATE_EIGHTH_T:
        return "1/8T";
    case STING64_RATE_SIXTEENTH_D:
        return "1/16D";
    case STING64_RATE_SIXTEENTH:
        return "1/16";
    case STING64_RATE_SIXTEENTH_T:
        return "1/16T";
    case STING64_RATE_THIRTYSECOND_D:
        return "1/32D";
    case STING64_RATE_THIRTYSECOND:
        return "1/32";
    case STING64_RATE_THIRTYSECOND_T:
        return "1/32T";
    case STING64_RATE_SIXTYFOURTH_D:
        return "1/64D";
    case STING64_RATE_SIXTYFOURTH:
        return "1/64";
    case STING64_RATE_SIXTYFOURTH_T:
        return "1/64T";
    default:
        return "1/16";
    }
}

static uint8_t parse_scale(const char *s)
{
    if (!s) return 0;
    double dv = atof(s);

    if (strcmp(s, "ionian") == 0 || strcmp(s, "major") == 0)
        return STING64_SCALE_IONIAN;
    if (strcmp(s, "aeolian") == 0 || strcmp(s, "minor") == 0 ||
        strcmp(s, "natural_minor") == 0)
        return STING64_SCALE_AEOLIAN;
    if (strcmp(s, "dorian") == 0)
        return STING64_SCALE_DORIAN;
    if (strcmp(s, "mixolydian") == 0)
        return STING64_SCALE_MIXOLYDIAN;
    if (strcmp(s, "phrygian") == 0)
        return STING64_SCALE_PHRYGIAN;
    if (strcmp(s, "lydian") == 0)
        return STING64_SCALE_LYDIAN;
    if (strcmp(s, "locrian") == 0)
        return STING64_SCALE_LOCRIAN;
    if (strcmp(s, "major_pent") == 0 || strcmp(s, "pent") == 0)
        return STING64_SCALE_MAJOR_PENT;
    if (strcmp(s, "minor_pent") == 0)
        return STING64_SCALE_MINOR_PENT;
    if (strcmp(s, "major_blues") == 0)
        return STING64_SCALE_MAJOR_BLUES;
    if (strcmp(s, "minor_blues") == 0 || strcmp(s, "blues") == 0)
        return STING64_SCALE_MINOR_BLUES;
    if (strcmp(s, "harmonic_minor") == 0 || strcmp(s, "harm_minor") == 0)
        return STING64_SCALE_HARMONIC_MINOR;
    if (strcmp(s, "melodic_minor") == 0 || strcmp(s, "mel_minor") == 0)
        return STING64_SCALE_MELODIC_MINOR;
    if (strcmp(s, "phrygian_dominant") == 0)
        return STING64_SCALE_PHRYGIAN_DOMINANT;
    if (strcmp(s, "double_harmonic") == 0)
        return STING64_SCALE_DOUBLE_HARMONIC;
    if (strcmp(s, "whole_tone") == 0)
        return STING64_SCALE_WHOLE_TONE;
    if (strcmp(s, "diminished_wh") == 0)
        return STING64_SCALE_DIMINISHED_WH;
    if (strcmp(s, "diminished_hw") == 0)
        return STING64_SCALE_DIMINISHED_HW;
    if (strcmp(s, "chromatic") == 0)
        return STING64_SCALE_CHROMATIC;

    if (looks_normalized_float(s, dv)) {
        int idx = (int)(dv * STING64_SCALE_COUNT);
        if (idx >= STING64_SCALE_COUNT)
            idx = STING64_SCALE_COUNT - 1;
        return (uint8_t)idx;
    }

    float fv = (float)dv;
    int   iv = (int)(fv + 0.5f);
    if (iv >= 0 && iv < STING64_SCALE_COUNT)
        return (uint8_t)iv;
    return STING64_SCALE_IONIAN;
}

static const char *scale_name(uint8_t idx)
{
    static const char *names[] = {
        "ionian",
        "aeolian",
        "dorian",
        "mixolydian",
        "phrygian",
        "lydian",
        "locrian",
        "major_pent",
        "minor_pent",
        "major_blues",
        "minor_blues",
        "harmonic_minor",
        "melodic_minor",
        "phrygian_dominant",
        "double_harmonic",
        "whole_tone",
        "diminished_wh",
        "diminished_hw",
        "chromatic"
    };
    if (idx >= STING64_SCALE_COUNT)
        idx = STING64_SCALE_IONIAN;
    return names[idx];
}

/* ------------------------------------------------------------------ */
/* set_param                                                             */
/* ------------------------------------------------------------------ */

static void set_param(void *instance, const char *key, const char *val)
{
    Sting64Instance *inst = (Sting64Instance *)instance;
    if (!key || !val) return;

    if (strcmp(key, "density") == 0) {
        inst->engine.density = parse_norm_float(val);
        sting64_engine_invalidate_sequence(&inst->engine);
        return;
    }
    if (strcmp(key, "chaos") == 0) {
        inst->engine.chaos = parse_norm_float(val);
        sting64_engine_invalidate_sequence(&inst->engine);
        return;
    }
    if (strcmp(key, "swing") == 0) {
        inst->engine.swing = parse_swing(val);
        return;
    }
    if (strcmp(key, "gate") == 0) {
        inst->gate = parse_norm_float(val);
        return;
    }
    if (strcmp(key, "seed") == 0) {
        inst->engine.seed = parse_seed(val);
        sting64_engine_invalidate_sequence(&inst->engine);
        return;
    }
    if (strcmp(key, "rate") == 0) {
        inst->rate = parse_rate(val);
        recompute_timing(inst, inst->last_sample_rate, (float)inst->bpm);
        reset_transport_phase(inst, 0);
        return;
    }
    if (strcmp(key, "sync") == 0) {
        inst->sync_mode = parse_sync(val);
        inst->running = 0;
        inst->last_clock_status = inst->sync_mode == STING64_SYNC_MOVE
                                ? MOVE_CLOCK_STATUS_UNAVAILABLE
                                : MOVE_CLOCK_STATUS_STOPPED;
        reset_transport_phase(inst, 1);
        return;
    }
    if (strcmp(key, "steps") == 0) {
        inst->engine.steps_count = parse_steps(val);
        int steps = sting64_engine_get_steps(&inst->engine);
        if (inst->engine.step_pos >= steps)
            inst->engine.step_pos = inst->engine.step_pos % steps;
        sting64_engine_invalidate_sequence(&inst->engine);
        return;
    }
    if (strcmp(key, "scale") == 0) {
        inst->engine.scale_index = parse_scale(val);
        sting64_engine_invalidate_sequence(&inst->engine);
        return;
    }
    if (strcmp(key, "root") == 0) {
        inst->engine.root = parse_root(val);
        sting64_engine_invalidate_sequence(&inst->engine);
        return;
    }
    if (strcmp(key, "velocity") == 0) {
        float v = (float)atof(val);
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        inst->engine.velocity = (uint8_t)(v * 127.0f + 0.5f);
        return;
    }
    if (strcmp(key, "bpm") == 0) {
        float v = (float)atof(val);
        /* Accept both raw BPM (20–300) and normalized (0–1 → 20–300) */
        if (v > 0.0f && v <= 1.0f) v = 20.0f + v * 280.0f;
        if (v < 20.0f)  v = 20.0f;
        if (v > 300.0f) v = 300.0f;
        inst->bpm = (uint16_t)(v + 0.5f);
        recompute_timing(inst, inst->last_sample_rate, (float)inst->bpm);
        return;
    }
}

/* ------------------------------------------------------------------ */
/* get_param                                                             */
/* ------------------------------------------------------------------ */

static int get_param(void *instance, const char *key, char *buf, int buf_len)
{
    Sting64Instance *inst = (Sting64Instance *)instance;
    if (!key || !buf || buf_len <= 0) return -1;

    if (strcmp(key, "density") == 0)
        return snprintf(buf, buf_len, "%.4f",
                        inst->engine.density / 255.0f);
    if (strcmp(key, "chaos") == 0)
        return snprintf(buf, buf_len, "%.4f",
                        inst->engine.chaos / 255.0f);
    if (strcmp(key, "swing") == 0)
        return snprintf(buf, buf_len, "%.4f",
                        (inst->engine.swing / 255.0f) * 0.5f);
    if (strcmp(key, "gate") == 0)
        return snprintf(buf, buf_len, "%.4f", inst->gate / 255.0f);
    if (strcmp(key, "seed") == 0)
        return snprintf(buf, buf_len, "%u", (unsigned)inst->engine.seed);
    if (strcmp(key, "rate") == 0)
        return snprintf(buf, buf_len, "%s", rate_name(inst->rate));
    if (strcmp(key, "sync") == 0)
        return snprintf(buf, buf_len, "%s", sync_name(inst->sync_mode));
    if (strcmp(key, "sync_warn") == 0) {
        if (inst->sync_mode == STING64_SYNC_MOVE) {
            if (inst->last_clock_status == MOVE_CLOCK_STATUS_UNAVAILABLE)
                return snprintf(buf, buf_len, "Waiting for Move MIDI clock");
            if (inst->last_clock_status == MOVE_CLOCK_STATUS_STOPPED)
                return snprintf(buf, buf_len, "transport stopped");
        }
        return snprintf(buf, buf_len, "%s", "");
    }
    if (strcmp(key, "steps") == 0)
        return snprintf(buf, buf_len, "%d",
                        sting64_engine_get_steps(&inst->engine));
    if (strcmp(key, "scale") == 0)
        return snprintf(buf, buf_len, "%s",
                        scale_name(inst->engine.scale_index));
    if (strcmp(key, "root") == 0)
        return snprintf(buf, buf_len, "%d",
                        (int)inst->engine.root);
    if (strcmp(key, "velocity") == 0)
        return snprintf(buf, buf_len, "%.4f",
                        inst->engine.velocity / 127.0f);
    if (strcmp(key, "bpm") == 0)
        return snprintf(buf, buf_len, "%d",
                        (int)inst->bpm);

    return -1;
}

/* ------------------------------------------------------------------ */
/* save_state / load_state                                               */
/* ------------------------------------------------------------------ */

static int save_state(void *instance, char *buf, int buf_len)
{
    Sting64Instance *inst = (Sting64Instance *)instance;
    int total = 0;
    char sink[1];

#define APPEND_STATE(...) do { \
        int avail = total < buf_len ? buf_len - total : 0; \
        int wrote = snprintf(avail > 0 ? buf + total : sink, \
                             (size_t)(avail > 0 ? avail : 0), \
                             __VA_ARGS__); \
        if (wrote < 0) return total; \
        total += wrote; \
    } while (0)

    if (!buf || buf_len <= 0) return 0;

    APPEND_STATE("density=%.4f\n", inst->engine.density / 255.0f);
    APPEND_STATE("chaos=%.4f\n", inst->engine.chaos / 255.0f);
    APPEND_STATE("swing=%.4f\n", (inst->engine.swing / 255.0f) * 0.5f);
    APPEND_STATE("gate=%.4f\n", inst->gate / 255.0f);
    APPEND_STATE("seed=%u\n", (unsigned)inst->engine.seed);
    APPEND_STATE("rate=%s\n", rate_name(inst->rate));
    APPEND_STATE("sync=%s\n", sync_name(inst->sync_mode));
    APPEND_STATE("steps=%d\n", sting64_engine_get_steps(&inst->engine));
    APPEND_STATE("scale=%s\n", scale_name(inst->engine.scale_index));
    APPEND_STATE("root=%d\n", (int)inst->engine.root);
    APPEND_STATE("velocity=%.4f\n", inst->engine.velocity / 127.0f);
    APPEND_STATE("bpm=%d\n", (int)inst->bpm);

#undef APPEND_STATE

    return total;
}

static void load_state(void *instance, const char *buf, int buf_len)
{
    Sting64Instance *inst = (Sting64Instance *)instance;
    if (!buf || buf_len <= 0) return;

    /* Flush held note before applying new state */
    uint8_t  tmp_msgs[MIDI_FX_MAX_OUT_MSGS][3];
    int      tmp_lens[MIDI_FX_MAX_OUT_MSGS];
    flush_active_note(inst, tmp_msgs, tmp_lens, MIDI_FX_MAX_OUT_MSGS, 0);
    inst->swing_pending    = 0;
    inst->note_off_pending = 0;

    char tmp[512];
    int  copy_len = buf_len < (int)sizeof(tmp) - 1
                    ? buf_len : (int)sizeof(tmp) - 1;
    memcpy(tmp, buf, (size_t)copy_len);
    tmp[copy_len] = '\0';

    char *line = tmp;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            set_param(instance, line, eq + 1);
        }
        line = nl ? nl + 1 : NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Plugin vtable and entry point                                         */
/* ------------------------------------------------------------------ */

static midi_fx_api_v1_t g_api = {
    .api_version      = MIDI_FX_API_VERSION,
    .create_instance  = create_instance,
    .destroy_instance = destroy_instance,
    .process_midi     = process_midi,
    .tick             = tick,
    .set_param        = set_param,
    .get_param        = get_param,
    .save_state       = save_state,
    .load_state       = load_state,
};

midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host)
{
    g_host = host;
    return &g_api;
}
