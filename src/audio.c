#include "dxball.h"

#include <fluidsynth.h>
#include <stdio.h>
#include <string.h>

static const char *k_sf2[] = {
    "/usr/share/soundfonts/FluidR3_GM.sf2",
    "/usr/share/soundfonts/FluidR3_GS.sf2",
    "/usr/share/sounds/sf2/FluidR3_GM.sf2"
};

static fluid_synth_t *synth_of(Game *g) {
    return (fluid_synth_t *)g->fluid_synth;
}

void audio_play(Game *g, int slot, int x) {
    int i;
    int pan;
    int vol_l;
    int vol_r;
    Voice *v;

    if (slot < 0 || slot >= MAX_SOUNDS || g->sounds[slot].pcm == NULL) {
        return;
    }
    pan = (int)((double)x * 1.5625 - 500.0);
    if (pan < -500) {
        pan = -500;
    }
    if (pan > 500) {
        pan = 500;
    }
    vol_l = (256 * (500 - pan)) / 1000;
    vol_r = (256 * (500 + pan)) / 1000;
    if (g->audio == 0) {
        return;
    }
    SDL_LockAudioDevice(g->audio);
    v = &g->voices[0];
    for (i = 0; i < MAX_VOICES; i++) {
        if (!g->voices[i].pcm) {
            v = &g->voices[i];
            break;
        }
        if (g->voices[i].pos > v->pos) {
            v = &g->voices[i];
        }
    }
    v->pcm = g->sounds[slot].pcm;
    v->frames = g->sounds[slot].frames;
    v->pos = 0;
    v->vol_l = vol_l;
    v->vol_r = vol_r;
    SDL_UnlockAudioDevice(g->audio);
}

static void midi_all_off(Game *g) {
    fluid_synth_t *s = synth_of(g);
    int ch;
    if (s == NULL) {
        return;
    }
    for (ch = 0; ch < 16; ch++) {
        fluid_synth_all_sounds_off(s, ch);
        fluid_synth_all_notes_off(s, ch);
    }
}

static void midi_apply(Game *g, uint32_t msg) {
    fluid_synth_t *s = synth_of(g);
    uint32_t typ = msg >> 24;
    uint32_t parm = msg & 0x00ffffff;
    int st;
    int d1;
    int d2;
    int ch;

    if (s == NULL) {
        return;
    }
    if (typ == 1) {
        if (parm >= 1000) {
            g->midi_tempo = parm;
        }
        return;
    }
    if (typ != 0) {
        return;
    }
    st = parm & 0xff;
    d1 = (parm >> 8) & 0xff;
    d2 = (parm >> 16) & 0xff;
    ch = st & 0x0f;
    switch (st & 0xf0) {
    case 0x80:
        fluid_synth_noteoff(s, ch, d1);
        break;
    case 0x90:
        if (d2 == 0) {
            fluid_synth_noteoff(s, ch, d1);
        } else {
            fluid_synth_noteon(s, ch, d1, d2);
        }
        break;
    case 0xb0:
        fluid_synth_cc(s, ch, d1, d2);
        if (d1 == 120 || d1 == 123) {
            midi_all_off(g);
        }
        break;
    case 0xc0:
        fluid_synth_program_change(s, ch, d1);
        break;
    case 0xd0:
        fluid_synth_channel_pressure(s, ch, d1);
        break;
    case 0xe0:
        fluid_synth_pitch_bend(s, ch, d1 | (d2 << 7));
        break;
    default:
        break;
    }
}

static void midi_run_events(Game *g) {
    while (g->midi_pos < g->midi_nev &&
           g->midi_ev[g->midi_pos].tick <= g->midi_tick) {
        midi_apply(g, g->midi_ev[g->midi_pos].msg);
        g->midi_pos++;
    }
    if (g->midi_pos >= g->midi_nev && g->midi_nev > 0) {
        g->midi_pos = 0;
        g->midi_tick = 0;
        midi_all_off(g);
        midi_run_events(g);
    }
}

static void midi_advance_frames(Game *g, int frames) {
    uint32_t us_per_tick;
    uint32_t add;
    if (!g->midi_on || g->midi_nev <= 0 || g->midi_ppqn == 0 || frames <= 0) {
        return;
    }
    us_per_tick = g->midi_tempo / g->midi_ppqn;
    if (us_per_tick == 0) {
        us_per_tick = 1;
    }
    /* midi_frac counts microseconds * MIX_RATE so tick rate stays exact. */
    g->midi_frac += (uint32_t)frames * 1000000u;
    add = us_per_tick * (uint32_t)MIX_RATE;
    while (g->midi_frac >= add) {
        g->midi_frac -= add;
        g->midi_tick++;
        midi_run_events(g);
    }
}

void midi_init(Game *g) {
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    unsigned i;
    int id;

    g->fluid_settings = NULL;
    g->fluid_synth = NULL;
    settings = new_fluid_settings();
    if (settings == NULL) {
        return;
    }
    fluid_settings_setnum(settings, "synth.sample-rate", (double)MIX_RATE);
    fluid_settings_setstr(settings, "audio.driver", "file");
    fluid_settings_setint(settings, "synth.reverb.active", 1);
    fluid_settings_setint(settings, "synth.chorus.active", 1);
    fluid_settings_setnum(settings, "synth.gain", 0.55);
    synth = new_fluid_synth(settings);
    if (synth == NULL) {
        delete_fluid_settings(settings);
        return;
    }
    id = FLUID_FAILED;
    for (i = 0; i < sizeof(k_sf2) / sizeof(k_sf2[0]); i++) {
        id = fluid_synth_sfload(synth, k_sf2[i], 1);
        if (id != FLUID_FAILED) {
            fprintf(stderr, "midi: %s\n", k_sf2[i]);
            break;
        }
    }
    if (id == FLUID_FAILED) {
        fprintf(stderr, "midi: no GM/GS soundfont found, title music off\n");
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        return;
    }
    g->fluid_settings = settings;
    g->fluid_synth = synth;
}

void midi_free(Game *g) {
    midi_stop(g);
    if (g->fluid_synth != NULL) {
        delete_fluid_synth((fluid_synth_t *)g->fluid_synth);
        g->fluid_synth = NULL;
    }
    if (g->fluid_settings != NULL) {
        delete_fluid_settings((fluid_settings_t *)g->fluid_settings);
        g->fluid_settings = NULL;
    }
}

void midi_start(Game *g, int track) {
    char path[640];
    static const char *names[] = {
        "12FLIGHT.MDS", "ACKER-GS.MDS", "BRAIN.MDS",
        "ETHNO_PA.MDS", "FREEBEE.MDS", "GMFIGARO.MDS"
    };
    int n;

    if (g->fluid_synth == NULL) {
        g->midi_on = 0;
        return;
    }
    if (track < 0) {
        track = 0;
    }
    track %= 6;
    if (g->midi_on && g->midi_track == track && g->midi_nev > 0) {
        return;
    }
    snprintf(path, sizeof(path), "%s/%s", g->data_dir, names[track]);
    n = load_mids(path, g->midi_ev, MAX_MIDI_EV, &g->midi_ppqn);
    if (n < 1) {
        g->midi_on = 0;
        g->midi_nev = 0;
        return;
    }
    if (g->audio != 0) {
        SDL_LockAudioDevice(g->audio);
    }
    midi_all_off(g);
    g->midi_nev = n;
    g->midi_pos = 0;
    g->midi_tick = 0;
    g->midi_frac = 0;
    g->midi_tempo = 500000;
    g->midi_on = 1;
    g->midi_track = track;
    midi_run_events(g);
    if (g->audio != 0) {
        SDL_UnlockAudioDevice(g->audio);
    }
}

void midi_stop(Game *g) {
    if (g->audio != 0) {
        SDL_LockAudioDevice(g->audio);
    }
    g->midi_on = 0;
    midi_all_off(g);
    if (g->audio != 0) {
        SDL_UnlockAudioDevice(g->audio);
    }
}

void audio_cb(void *userdata, Uint8 *stream, int len) {
    Game *g = userdata;
    int16_t *out = (int16_t *)stream;
    int frames = len / 4;
    int i, v;
    int acc_l, acc_r, s, mix;
    int16_t midi_buf[4096];
    int have_midi;
    fluid_synth_t *syn;

    memset(stream, 0, (size_t)len);
    if (frames < 1) {
        return;
    }
    if (frames > 2048) {
        frames = 2048;
    }
    syn = synth_of(g);
    have_midi = (g->midi_on && syn != NULL);
    i = 0;
    while (i < frames) {
        int chunk = frames - i;
        int j;
        if (chunk > 32) {
            chunk = 32;
        }
        if (have_midi) {
            midi_advance_frames(g, chunk);
            fluid_synth_write_s16(syn, chunk, midi_buf, 0, 2, midi_buf, 1, 2);
        }
        for (j = 0; j < chunk; j++) {
            acc_l = 0;
            acc_r = 0;
            if (have_midi) {
                acc_l += midi_buf[j * 2];
                acc_r += midi_buf[j * 2 + 1];
            }
            for (v = 0; v < MAX_VOICES; v++) {
                Voice *vo = &g->voices[v];
                if (vo->pcm == NULL || vo->pos >= vo->frames) {
                    vo->pcm = NULL;
                    continue;
                }
                s = vo->pcm[vo->pos * 2];
                acc_l += (s * vo->vol_l) >> 8;
                s = vo->pcm[vo->pos * 2 + 1];
                acc_r += (s * vo->vol_r) >> 8;
                vo->pos++;
            }
            mix = acc_l;
            if (mix > 32767) {
                mix = 32767;
            }
            if (mix < -32768) {
                mix = -32768;
            }
            out[(i + j) * 2] = (int16_t)mix;
            mix = acc_r;
            if (mix > 32767) {
                mix = 32767;
            }
            if (mix < -32768) {
                mix = -32768;
            }
            out[(i + j) * 2 + 1] = (int16_t)mix;
        }
        i += chunk;
    }
}
