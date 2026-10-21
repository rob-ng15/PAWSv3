// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2020 by Marcus Geelnard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      PAWSv3 system interface for sound (2x DMA PCM) and music (6x APU).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "z_zone.h"

#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "doomdef.h"

#include <PAWSlibrary.h>
#include "PAWSdefinitions.h"

extern unsigned char volatile *AUDIO_WAVEFORM;
extern unsigned char volatile *AUDIO_FREQUENCY;
extern unsigned short volatile *AUDIO_DURATION;
extern unsigned char volatile *AUDIO_VOLUME;

//
// SFX API (2x DMA Digital Audio Channels)
//

// The actual lengths of all sound effects.
static short s_sfx_lengths[NUMSFX];
static void poll_doom_music(void);

//
// This function loads the sound data from the WAD lump, for single sound.
//
static void* getsfx (const char* sfxname, short* len)
{
    char name[20];
    sprintf (name, "ds%s", sfxname);

    int sfxlump;
    if (W_CheckNumForName (name) == -1)
        sfxlump = W_GetNumForName ("dspistol");
    else
        sfxlump = W_GetNumForName (name);

    int size = W_LumpLength (sfxlump);

    unsigned char* sfx = (unsigned char*)W_CacheLumpNum (sfxlump, PU_STATIC);

    *len = (short)size;
    return (void *)sfx;
}

void I_InitSound (void)
{
    // Initialize external data (all sounds) at start, keep static.
    for (int i = 1; i < NUMSFX; i++)
    {
        if (!S_sfx[i].link)
        {
            S_sfx[i].data = getsfx (S_sfx[i].name, &s_sfx_lengths[i]);
        }
        else
        {
            S_sfx[i].data = S_sfx[i].link->data;
            s_sfx_lengths[i] =
            s_sfx_lengths[(S_sfx[i].link - S_sfx) /
            sizeof (sfxinfo_t)];
        }
    }
    I_InitMusic();
}

void I_ShutdownSound (void)
{
    pcmsample_stop( CHANNEL_PCM_BOTH );
    I_ShutdownMusic();
}

void I_SetSfxVolume (int volume)
{
    (void)volume;
}

int I_GetSfxLumpNum (sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf (namebuf, "ds%s", sfx->name);
    return W_GetNumForName (namebuf);
}

unsigned char __lastchannel = 1;
int I_StartSound (int id, int vol, int sep, int pitch, int priority)
{
    (void)vol; (void)sep; (void)pitch; (void)priority;

    if( !get_beep_active( ACTIVE_CHANNEL_LEFT_PCM ) ) {
        pcmsample_start( CHANNEL_PCM_LEFT, s_sfx_lengths[id] - 40, S_sfx[id].data + 0x18, KHz11025, FALSE );
        __lastchannel = 1;
    } else {
        if( !get_beep_active( ACTIVE_CHANNEL_RIGHT_PCM ) ) {
            pcmsample_start( CHANNEL_PCM_RIGHT, s_sfx_lengths[id] - 40, S_sfx[id].data + 0x18, KHz11025, FALSE );
            __lastchannel = 2;
        } else {
            __lastchannel = 3 - __lastchannel;
            pcmsample_stop( __lastchannel );
            pcmsample_start( __lastchannel, s_sfx_lengths[id] - 40, S_sfx[id].data + 0x18, KHz11025, FALSE );
        }
    }
    return id;
}

void I_StopSound (int handle)
{
    (void)handle;
}

void I_StopAllSounds (void)
{
    pcmsample_stop( CHANNEL_PCM_BOTH );
}

int I_SoundIsPlaying (int handle)
{
    (void)handle;
    return 0;
}

void I_UpdateSoundParams (int handle, int vol, int sep, int pitch)
{
    (void)handle; (void)vol; (void)sep; (void)pitch;
}

void I_UpdateSound (void)
{
    poll_doom_music();
}

void I_SubmitSound (void)
{
    poll_doom_music();
}

// =============================================================================
// MUSIC API (6-Channel Synchronous CSRtime() PAWSv3 APU Player - No Interrupts)
// =============================================================================

#define NUM_MUSIC_VOICES     6
#define MAX_TUNE_STEPS       4096
#define TUNE_END_STEP        0xFFFF

// 50 MHz CSRtime() cycles per 140 Hz MUS tick (50,000,000 / 140 = 357,143)
#define CYCLES_PER_MUS_TICK  357143UL

#define NOTE_REST            0x00
#define NOTE_SUSTAIN         0xFE

static unsigned short step_delay_ticks[MAX_TUNE_STEPS];
static unsigned char  tune_note[NUM_MUSIC_VOICES][MAX_TUNE_STEPS];
static unsigned short tune_duration[NUM_MUSIC_VOICES][MAX_TUNE_STEPS];
static unsigned char  tune_wave_vol[NUM_MUSIC_VOICES][MAX_TUNE_STEPS];

static unsigned short current_step  = 0;
static int            tick_accum    = 0;
static unsigned long  last_mus_time = 0;
static unsigned char  music_playing = 0;
static unsigned char  music_paused  = 0;
static unsigned char  music_looping = 1;
static unsigned char  music_volume  = 7;

// Interleave Left and Right APU channels for stereo balance
static const unsigned char hw_channel[NUM_MUSIC_VOICES] = {
    CHANNEL_LEFT_0,  CHANNEL_RIGHT_0,
    CHANNEL_LEFT_1,  CHANNEL_RIGHT_1,
    CHANNEL_LEFT_2,  CHANNEL_RIGHT_2
};

static inline void stop_paws_voice_inline(unsigned char ch)
{
    AUDIO_WAVEFORM[ch]  = 0;
    AUDIO_FREQUENCY[ch] = 0;
    AUDIO_DURATION[ch]  = 0;
}

static inline void start_paws_voice_inline(unsigned char ch, unsigned char wave,
                                           unsigned char note, unsigned short dur_ms,
                                           unsigned char vol)
{
    AUDIO_WAVEFORM[ch]  = wave;
    AUDIO_FREQUENCY[ch] = note;
    AUDIO_VOLUME[ch]    = vol;
    asm volatile ("" ::: "memory");
    AUDIO_DURATION[ch]  = dur_ms;
}

static void silence_all_music_voices(void)
{
    for (int ch = CHANNEL_LEFT_0; ch <= CHANNEL_RIGHT_2; ch++) {
        stop_paws_voice_inline(ch);
    }
}

// DOOM MUS Header
typedef struct __attribute__((packed)) {
    char           id[4];              // "MUS\x1a"
    unsigned short score_len;
    unsigned short score_start;
    unsigned short primary_channels;
    unsigned short secondary_channels;
    unsigned short instr_cnt;
    unsigned short dummy;
} mus_header_t;

typedef struct {
    unsigned char  active;
    unsigned char  triggered;
    unsigned char  changed;
    unsigned char  mus_chan;
    unsigned char  midi_note;
    unsigned char  paws_note;
    unsigned char  waveform;
    unsigned char  volume;
    unsigned char  priority;
    unsigned int   start_tick;
    unsigned int   auto_off_tick;
    unsigned short trigger_step;
} sim_voice_t;

static unsigned char gm_to_paws_waveform(unsigned char program)
{
    if (program >= 24 && program <= 31) return WAVE_SAW;      // Overdriven / Distortion Guitars
    if (program >= 32 && program <= 39) return WAVE_TRIANGLE; // Basses
    if (program >= 16 && program <= 23) return WAVE_WOOD;     // Organs
    if (program >= 40 && program <= 79) return WAVE_BRASS;    // Strings, Choir, Brass, Reeds
    if (program >= 80 && program <= 95) return WAVE_SQUARE;   // Synth Leads & Pads
    return WAVE_WOOD;                                         // Piano / Chromatic Percussion
}

static unsigned char midi_to_paws_note(unsigned char midi_note)
{
    int idx = ((int)midi_note - 36) * 2 + 3;
    while (idx < 3)   idx += 24;
    while (idx > 125) idx -= 24;
    return (unsigned char)idx;
}

static void map_drum_note(unsigned char midi_note, unsigned char *wave,
                          unsigned char *paws_note, unsigned int *drum_ticks)
{
    switch (midi_note) {
        case 35: case 36:
            *wave = WAVE_TRIANGLE; *paws_note = 7;  *drum_ticks = 7;  break;
        case 38: case 40:
            *wave = WAVE_NOISE;    *paws_note = 45; *drum_ticks = 8;  break;
        case 41: case 43: case 45: case 47: case 48: case 50:
            *wave = WAVE_SINE;     *paws_note = 15 + (midi_note - 41); *drum_ticks = 8; break;
        case 42: case 44:
            *wave = WAVE_NOISE;    *paws_note = 95; *drum_ticks = 3;  break;
        case 46: case 49: case 51: case 52: case 55: case 57:
            *wave = WAVE_NOISE;    *paws_note = 85; *drum_ticks = 12; break;
        default:
            *wave = WAVE_NOISE;    *paws_note = 65; *drum_ticks = 5;  break;
    }
}

static unsigned char note_priority(unsigned char mus_chan, unsigned char prog, unsigned char midi_note)
{
    if (mus_chan == 15) return 70;
    if (prog >= 32 && prog <= 39) return 90;
    if (midi_note < 48) return 85;
    if (mus_chan == 0)  return 80;
    if (mus_chan <= 2)  return 75;
    if (mus_chan < 9)   return 60 - (mus_chan * 2);
    return 30;
}

static void finalize_voice_duration(sim_voice_t *vp, int v, unsigned int end_tick)
{
    if (vp->trigger_step >= MAX_TUNE_STEPS) return;
    unsigned int ticks = (end_tick > vp->start_tick) ? (end_tick - vp->start_tick) : 1;
    unsigned int dur_ms = (ticks * 1000U) / 140U;
    if (dur_ms < 15) dur_ms = 15;
    if (dur_ms > 60000U) dur_ms = 60000U;
    tune_duration[v][vp->trigger_step] = (unsigned short)dur_ms;
}

static void commit_step_slice(sim_voice_t *voices, unsigned int delay_ticks, unsigned short *step_count)
{
    unsigned short s = *step_count;
    if (s >= MAX_TUNE_STEPS - 2) return;

    if (delay_ticks == 0) delay_ticks = 1;
    step_delay_ticks[s] = (delay_ticks > 60000U) ? 60000U : (unsigned short)delay_ticks;

    for (int v = 0; v < NUM_MUSIC_VOICES; v++) {
        if (voices[v].active) {
            if (voices[v].triggered) {
                tune_note[v][s]        = voices[v].paws_note;
                tune_wave_vol[v][s]    = ((voices[v].waveform & 7) << 4) | (voices[v].volume & 7);
                tune_duration[v][s]    = 500;
                voices[v].trigger_step = s;
                voices[v].triggered    = 0;
            } else {
                tune_note[v][s]     = NOTE_SUSTAIN;
                tune_wave_vol[v][s] = 0;
                tune_duration[v][s] = 0;
            }
        } else {
            tune_note[v][s]     = voices[v].changed ? NOTE_REST : NOTE_SUSTAIN;
            tune_wave_vol[v][s] = 0;
            tune_duration[v][s] = 0;
            voices[v].changed   = 0;
        }
    }

    (*step_count)++;
}

static int convert_mus_to_tables(const void *mus_data)
{
    const mus_header_t *hdr = (const mus_header_t *)mus_data;
    if (memcmp(hdr->id, "MUS\x1a", 4) != 0) {
        step_delay_ticks[0] = TUNE_END_STEP;
        return 0;
    }

    const unsigned char *p   = (const unsigned char *)mus_data + hdr->score_start;
    const unsigned char *end = p + hdr->score_len;

    sim_voice_t    voices[NUM_MUSIC_VOICES];
    unsigned short step_count       = 0;
    unsigned char  chan_prog[16]     = {0};
    unsigned char  chan_vol[16];
    unsigned char  chan_last_vol[16];

    memset(voices, 0, sizeof(voices));
    memset(chan_vol, 110, sizeof(chan_vol));
    memset(chan_last_vol, 110, sizeof(chan_last_vol));

    unsigned int  cur_tick  = 0;
    unsigned char score_end = 0;

    while (p < end && !score_end && step_count < MAX_TUNE_STEPS - 2) {
        unsigned char event     = *p++;
        unsigned char last_flag = event & 0x80;
        unsigned char ev_type   = (event >> 4) & 0x07;
        unsigned char chan      = event & 0x0F;

        switch (ev_type) {
            case 0: { // Release Note
                if (p >= end) break;
                unsigned char note = *p++ & 0x7F;
                for (int v = 0; v < NUM_MUSIC_VOICES; v++) {
                    if (voices[v].active && voices[v].mus_chan == chan && voices[v].midi_note == note) {
                        if (!voices[v].triggered) {
                            finalize_voice_duration(&voices[v], v, cur_tick);
                        }
                        voices[v].active    = 0;
                        voices[v].triggered = 0;
                        voices[v].changed   = 1;
                    }
                }
                break;
            }

            case 1: { // Play Note
                if (p >= end) break;
                unsigned char b    = *p++;
                unsigned char note = b & 0x7F;
                if (b & 0x80) {
                    if (p < end) chan_last_vol[chan] = *p++ & 0x7F;
                }
                chan_vol[chan] = chan_last_vol[chan];

                unsigned char paws_vol = (chan_vol[chan] >> 4);
                if (paws_vol < 2 && chan_vol[chan] > 0) paws_vol = 2;

                unsigned char wave, paws_note;
                unsigned int  drum_ticks = 0;
                if (chan == 15) {
                    map_drum_note(note, &wave, &paws_note, &drum_ticks);
                } else {
                    wave      = gm_to_paws_waveform(chan_prog[chan]);
                    paws_note = midi_to_paws_note(note);
                }

                unsigned char prio = note_priority(chan, chan_prog[chan], note);

                int chosen = -1;
                if (chan == 15) {
                    chosen = 5;
                } else {
                    for (int v = 0; v < 5; v++) {
                        if (voices[v].active && voices[v].mus_chan == chan && voices[v].midi_note == note) {
                            chosen = v; break;
                        }
                    }
                    if (chosen < 0) {
                        for (int v = 0; v < 5; v++) {
                            if (!voices[v].active) { chosen = v; break; }
                        }
                    }
                    if (chosen < 0) {
                        int           best_v   = -1;
                        unsigned char min_prio = 255;
                        unsigned int  oldest_t = 0xFFFFFFFFU;
                        for (int v = 0; v < 5; v++) {
                            if (voices[v].priority < min_prio ||
                                (voices[v].priority == min_prio && voices[v].start_tick < oldest_t)) {
                                min_prio = voices[v].priority;
                            oldest_t = voices[v].start_tick;
                            best_v   = v;
                                }
                        }
                        if (best_v >= 0 && (prio >= min_prio || (cur_tick - oldest_t) >= 10)) {
                            chosen = best_v;
                        }
                    }
                }

                if (chosen >= 0) {
                    if (voices[chosen].active && !voices[chosen].triggered) {
                        finalize_voice_duration(&voices[chosen], chosen, cur_tick);
                    }
                    voices[chosen].active        = 1;
                    voices[chosen].triggered     = 1;
                    voices[chosen].changed       = 0;
                    voices[chosen].mus_chan      = chan;
                    voices[chosen].midi_note     = note;
                    voices[chosen].paws_note     = paws_note;
                    voices[chosen].waveform      = wave;
                    voices[chosen].volume        = paws_vol;
                    voices[chosen].priority      = prio;
                    voices[chosen].start_tick    = cur_tick;
                    voices[chosen].auto_off_tick = drum_ticks ? (cur_tick + drum_ticks) : 0;
                }
                break;
            }

            case 2: { // Pitch Bend
                if (p < end) p++;
                break;
            }

            case 3: { // System Event
                if (p >= end) break;
                unsigned char sys = *p++ & 0x7F;
                if (sys == 10 || sys == 11 || sys == 14) {
                    for (int v = 0; v < NUM_MUSIC_VOICES; v++) {
                        if (voices[v].active && voices[v].mus_chan == chan) {
                            if (!voices[v].triggered) {
                                finalize_voice_duration(&voices[v], v, cur_tick);
                            }
                            voices[v].active    = 0;
                            voices[v].triggered = 0;
                            voices[v].changed   = 1;
                        }
                    }
                }
                break;
            }

            case 4: { // Controller Change
                if (p + 1 >= end) { p = end; break; }
                unsigned char ctrl = *p++ & 0x7F;
                unsigned char val  = *p++ & 0x7F;
                if (ctrl == 0) {
                    chan_prog[chan] = val;
                } else if (ctrl == 3) {
                    chan_vol[chan]      = val;
                    chan_last_vol[chan] = val;
                }
                break;
            }

            case 6: { // End of Score
                score_end = 1;
                break;
            }

            case 7: {
                if (p < end) p++;
                break;
            }

            default:
                break;
        }

        if (last_flag && !score_end) {
            unsigned int  delay = 0;
            unsigned char db    = 0;
            do {
                if (p >= end) break;
                db    = *p++;
                delay = (delay << 7) | (db & 0x7F);
            } while (db & 0x80);

            if (delay > 0) {
                commit_step_slice(voices, delay, &step_count);
                cur_tick += delay;

                for (int v = 0; v < NUM_MUSIC_VOICES; v++) {
                    if (voices[v].active && voices[v].auto_off_tick > 0 &&
                        voices[v].auto_off_tick <= cur_tick) {
                        finalize_voice_duration(&voices[v], v, voices[v].auto_off_tick);
                    voices[v].active        = 0;
                    voices[v].triggered     = 0;
                    voices[v].changed       = 1;
                    voices[v].auto_off_tick = 0;
                        }
                }
            }
        }
    }

    for (int v = 0; v < NUM_MUSIC_VOICES; v++) {
        if (voices[v].active && !voices[v].triggered) {
            finalize_voice_duration(&voices[v], v, cur_tick + 14);
        }
    }

    step_delay_ticks[step_count] = TUNE_END_STEP;
    current_step = 0;
    tick_accum   = 0;

    return 1;
}

// Synchronous music advancement using the 50MHz CSRtime() counter
static void poll_doom_music(void)
{
    if (!music_playing || music_paused) return;

    unsigned long now     = (unsigned long)CSRtime();
    unsigned long elapsed = now - last_mus_time;
    if (elapsed < CYCLES_PER_MUS_TICK) return;

    unsigned int ticks = (unsigned int)(elapsed / CYCLES_PER_MUS_TICK);
    last_mus_time += (unsigned long)ticks * CYCLES_PER_MUS_TICK;

    // Clamp if a level load paused the main loop for a long time
    if (ticks > 35) ticks = 35;
    tick_accum -= (int)ticks;

    int max_iters = 16;
    while (tick_accum <= 0 && --max_iters >= 0) {
        unsigned short s = current_step;
        if (s >= MAX_TUNE_STEPS) s = 0;

        unsigned short delay = step_delay_ticks[s];

        if (delay == TUNE_END_STEP) {
            if (music_looping) {
                current_step = 0;
                tick_accum   = 0;
                s            = 0;
                delay        = step_delay_ticks[0];
                if (delay == TUNE_END_STEP || delay == 0) {
                    music_playing = 0;
                    silence_all_music_voices();
                    return;
                }
            } else {
                music_playing = 0;
                silence_all_music_voices();
                return;
            }
        }

        if (delay == 0) delay = 1;

        for (int v = 0; v < NUM_MUSIC_VOICES; v++) {
            unsigned char n = tune_note[v][s];
            if (n == NOTE_SUSTAIN) {
                continue;
            } else if (n == NOTE_REST) {
                stop_paws_voice_inline(hw_channel[v]);
            } else {
                unsigned char wv   = tune_wave_vol[v][s];
                unsigned char wave = (wv >> 4) & 7;
                unsigned char vol  = ((wv & 7) * music_volume) >> 3;
                if (vol == 0 && music_volume > 0) vol = 1;

                start_paws_voice_inline(hw_channel[v], wave, n, tune_duration[v][s], vol);
            }
        }

        tick_accum  += (int)delay;
        current_step = s + 1;
    }
}

void I_InitMusic (void)
{
    IRQ_OFF(IRQ_VBLANK, TRUE);
    music_playing = 0;
    silence_all_music_voices();

    step_delay_ticks[0] = TUNE_END_STEP;
    current_step = 0;
    tick_accum   = 0;
}

void I_ShutdownMusic (void)
{
    music_playing = 0;
    silence_all_music_voices();
}

void I_SetMusicVolume (int volume)
{
    if (volume > 15) volume >>= 3;
    music_volume = (volume > 0) ? ((volume >> 1) & 7) : 0;
    if (volume > 0 && music_volume == 0) music_volume = 1;
}

void I_PlaySong (int handle, int looping)
{
    (void)handle;
    silence_all_music_voices();

    current_step  = 0;
    tick_accum    = 0;
    last_mus_time = (unsigned long)CSRtime();
    music_looping = (unsigned char)looping;
    music_paused  = 0;
    music_playing = 1;

    poll_doom_music();
}

void I_PauseSong (int handle)
{
    (void)handle;
    music_paused = 1;
    silence_all_music_voices();
}

void I_ResumeSong (int handle)
{
    (void)handle;
    last_mus_time = (unsigned long)CSRtime();
    music_paused  = 0;
}

void I_StopSong (int handle)
{
    (void)handle;
    music_playing = 0;
    silence_all_music_voices();
}

void I_UnRegisterSong (int handle)
{
    (void)handle;
    I_StopSong(handle);
}

int I_RegisterSong (void* data)
{
    I_StopSong(1);
    if (data != NULL) {
        convert_mus_to_tables(data);
    }
    return 1;
}

int I_QrySongPlaying (int handle)
{
    (void)handle;
    poll_doom_music();
    return music_playing;
}
