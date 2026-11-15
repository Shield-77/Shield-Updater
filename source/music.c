/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Shield-77.
 */
#include "music.h"

#include <3ds.h>
#include <tremor/ivorbisfile.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#define MUSIC_CHANNEL 0
#define MUSIC_BUFFERS 4
#define MUSIC_FRAMES 8192
#define OUTPUT_RATE 22050
#define CROSSFADE_FRAMES (OUTPUT_RATE / 2)
#define SOURCE_FRAMES 4096
#define DECODE_CHUNK 1024
#define TRACK_COUNT 4

typedef enum { CODEC_OGG, CODEC_MP3 } MusicCodec;

typedef struct {
    MusicCodec codec;
    OggVorbis_File ogg;
    mp3dec_ex_t mp3;
    mp3dec_io_t mp3_io;
    FILE *mp3_file;
    bool opened;
    int channels;
    int rate;
    uint32_t source_step_q16;
    int16_t source[SOURCE_FRAMES * 2];
    size_t source_frames;
    uint32_t source_position_q16;
    unsigned read_errors;
    bool eof;
} TrackDecoder;

typedef struct {
    ndspWaveBuf wave;
    int16_t *pcm;
} MusicBuffer;

static const char *const track_paths[TRACK_COUNT] = {
    "romfs:/music/01-cat-caffe.mp3",
    "romfs:/music/02-chillloopable.mp3",
    "romfs:/music/03-chill-lofi-loop.ogg",
    "romfs:/music/05-blue-boi.mp3"
};

static int playlist_order[TRACK_COUNT];
static MusicBuffer buffers[MUSIC_BUFFERS];
static TrackDecoder decoders[2];
static int16_t fade_tail[CROSSFADE_FRAMES * 2];
static int16_t fade_head[CROSSFADE_FRAMES * 2];
static int16_t decoded_chunk[DECODE_CHUNK * 2];
static Thread worker;
static volatile bool stop_requested;
static bool dsp_ready;
static int current_slot;
static int current_track;
static int playlist_position;
static int next_slot;
static int next_track;
static int next_playlist_position;
static size_t tail_frames;
static size_t tail_cursor;
static size_t transition_frames;
static size_t transition_offset;
static bool transition_ready;
static int32_t current_gain_q15 = 32768;
static uint32_t random_state;

static uint32_t next_random(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void shuffle_playlist(int avoid_first) {
    for (int i = 0; i < TRACK_COUNT; ++i) playlist_order[i] = i;
    for (int i = TRACK_COUNT - 1; i > 0; --i) {
        int other = (int)(next_random() % (uint32_t)(i + 1));
        int temp = playlist_order[i];
        playlist_order[i] = playlist_order[other];
        playlist_order[other] = temp;
    }
    if (TRACK_COUNT > 1 && playlist_order[0] == avoid_first) {
        int other = 1 + (int)(next_random() % (uint32_t)(TRACK_COUNT - 1));
        int temp = playlist_order[0];
        playlist_order[0] = playlist_order[other];
        playlist_order[other] = temp;
    }
}

static size_t mp3_read_callback(void *buffer, size_t size, void *user_data) {
    FILE *file = (FILE *)user_data;
    return fread(buffer, 1, size, file);
}

static int mp3_seek_callback(uint64_t position, void *user_data) {
    FILE *file = (FILE *)user_data;
    if (position > (uint64_t)LONG_MAX) return -1;
    return fseek(file, (long)position, SEEK_SET);
}

static void close_track(TrackDecoder *track) {
    if (!track->opened) return;
    if (track->codec == CODEC_OGG) ov_clear(&track->ogg);
    else {
        mp3dec_ex_close(&track->mp3);
        if (track->mp3_file) fclose(track->mp3_file);
    }
    memset(track, 0, sizeof(*track));
}

static bool open_track(TrackDecoder *track, int index) {
    memset(track, 0, sizeof(*track));
    const char *path = track_paths[index];
    size_t path_len = strlen(path);
    if (path_len >= 4 && strcmp(path + path_len - 4, ".ogg") == 0) {
        FILE *file = fopen(path, "rb");
        if (!file) return false;
        if (ov_open(file, &track->ogg, NULL, 0) != 0) {
            fclose(file);
            return false;
        }
        vorbis_info *info = ov_info(&track->ogg, -1);
        if (!info || (info->channels != 1 && info->channels != 2) ||
            info->rate < 8000 || info->rate > 48000) {
            ov_clear(&track->ogg);
            return false;
        }
        track->codec = CODEC_OGG;
        track->channels = info->channels;
        track->rate = (int)info->rate;
    } else {
        track->mp3_file = fopen(path, "rb");
        if (!track->mp3_file) return false;
        track->mp3_io.read = mp3_read_callback;
        track->mp3_io.read_data = track->mp3_file;
        track->mp3_io.seek = mp3_seek_callback;
        track->mp3_io.seek_data = track->mp3_file;
        if (mp3dec_ex_open_cb(&track->mp3, &track->mp3_io, MP3D_DO_NOT_SCAN) != 0) {
            mp3dec_ex_close(&track->mp3);
            fclose(track->mp3_file);
            memset(track, 0, sizeof(*track));
            return false;
        }
        if ((track->mp3.info.channels != 1 && track->mp3.info.channels != 2) ||
            track->mp3.info.hz < 8000 || track->mp3.info.hz > 48000) {
            mp3dec_ex_close(&track->mp3);
            fclose(track->mp3_file);
            memset(track, 0, sizeof(*track));
            return false;
        }
        track->codec = CODEC_MP3;
        track->channels = track->mp3.info.channels;
        track->rate = track->mp3.info.hz;
    }
    track->source_step_q16 = (uint32_t)(((uint64_t)track->rate << 16) / OUTPUT_RATE);
    track->opened = true;
    return true;
}

static bool refill_source(TrackDecoder *track) {
    size_t consumed = (size_t)(track->source_position_q16 >> 16);
    if (consumed > 0) {
        if (consumed > track->source_frames) consumed = track->source_frames;
        size_t remaining = track->source_frames - consumed;
        if (remaining)
            memmove(track->source, track->source + consumed * (size_t)track->channels,
                    remaining * (size_t)track->channels * sizeof(int16_t));
        track->source_frames = remaining;
        track->source_position_q16 -= (uint32_t)(consumed << 16);
    }
    if (track->eof || track->source_frames >= SOURCE_FRAMES) return false;

    size_t free_frames = SOURCE_FRAMES - track->source_frames;
    int16_t *dst = track->source + track->source_frames * (size_t)track->channels;
    if (track->codec == CODEC_OGG) {
        int bitstream = 0;
        size_t bytes = free_frames * (size_t)track->channels * sizeof(int16_t);
        if (bytes > 32768) bytes = 32768;
        long count = ov_read(&track->ogg, (char *)dst, (int)bytes, &bitstream);
        if (count > 0) {
            track->source_frames += (size_t)count /
                                    ((size_t)track->channels * sizeof(int16_t));
            track->read_errors = 0;
            return true;
        }
        if (count == 0 || ++track->read_errors > 5) track->eof = true;
        return false;
    }

    size_t samples = mp3dec_ex_read(&track->mp3, (mp3d_sample_t *)dst,
                                    free_frames * (size_t)track->channels);
    if (samples > 0) {
        track->source_frames += samples / (size_t)track->channels;
        track->read_errors = 0;
        return true;
    }
    track->eof = true;
    return false;
}

static size_t decode_resampled(TrackDecoder *track, int16_t *out, size_t frames) {
    size_t written = 0;
    while (written < frames && !stop_requested) {
        if (track->source_frames == 0 && !track->eof) {
            if (!refill_source(track) && track->source_frames == 0 && track->eof) break;
        }
        size_t at = (size_t)(track->source_position_q16 >> 16);
        if (at >= track->source_frames) {
            if (track->eof) break;
            if (!refill_source(track) && at >= track->source_frames && track->eof) break;
            continue;
        }
        if (at + 1 >= track->source_frames && !track->eof) {
            size_t before = track->source_frames;
            refill_source(track);
            if (track->source_frames == before && !track->eof) continue;
            at = (size_t)(track->source_position_q16 >> 16);
            if (at >= track->source_frames && track->eof) break;
        }

        size_t next = at + 1 < track->source_frames ? at + 1 : at;
        uint32_t fraction = track->source_position_q16 & 0xffffu;
        for (int channel = 0; channel < 2; ++channel) {
            int source_channel = track->channels == 1 ? 0 : channel;
            int32_t a = track->source[at * (size_t)track->channels + (size_t)source_channel];
            int32_t b = track->source[next * (size_t)track->channels + (size_t)source_channel];
            out[written * 2 + (size_t)channel] = (int16_t)(a +
                (int32_t)(((int64_t)(b - a) * fraction) >> 16));
        }
        ++written;
        track->source_position_q16 += track->source_step_q16;
    }
    return written;
}

static bool prime_tail(void) {
    tail_frames = 0;
    tail_cursor = 0;
    while (tail_frames < CROSSFADE_FRAMES && !stop_requested) {
        size_t request = CROSSFADE_FRAMES - tail_frames;
        if (request > DECODE_CHUNK) request = DECODE_CHUNK;
        size_t count = decode_resampled(&decoders[current_slot], decoded_chunk, request);
        if (!count) break;
        memcpy(fade_tail + tail_frames * 2, decoded_chunk, count * 2 * sizeof(int16_t));
        tail_frames += count;
    }
    return tail_frames > 0;
}

static bool begin_transition(void) {
    if (transition_ready) return true;
    int first_position = playlist_position + 1;
    if (first_position >= TRACK_COUNT) {
        shuffle_playlist(current_track);
        first_position = 0;
    }
    for (int attempt = 0; attempt < TRACK_COUNT; ++attempt) {
        next_playlist_position = (first_position + attempt) % TRACK_COUNT;
        next_track = playlist_order[next_playlist_position];
        next_slot = 1 - current_slot;
        close_track(&decoders[next_slot]);
        if (!open_track(&decoders[next_slot], next_track)) continue;
        size_t head_frames = 0;
        while (head_frames < CROSSFADE_FRAMES && !stop_requested) {
            size_t request = CROSSFADE_FRAMES - head_frames;
            if (request > DECODE_CHUNK) request = DECODE_CHUNK;
            size_t count = decode_resampled(&decoders[next_slot],
                                            fade_head + head_frames * 2, request);
            if (!count) break;
            head_frames += count;
        }
        if (!head_frames) {
            close_track(&decoders[next_slot]);
            continue;
        }
        transition_frames = head_frames < tail_frames ? head_frames : tail_frames;
        transition_offset = 0;
        transition_ready = true;
        return true;
    }
    return false;
}

static void finish_transition(void) {
    close_track(&decoders[current_slot]);
    current_slot = next_slot;
    current_track = next_track;
    playlist_position = next_playlist_position;
    tail_frames = 0;
    tail_cursor = 0;
    transition_frames = 0;
    transition_offset = 0;
    transition_ready = false;
    prime_tail();
}

static size_t read_playlist(int16_t *out, size_t requested) {
    size_t written = 0;
    while (written < requested && !stop_requested) {
        if (!transition_ready) {
            size_t request = requested - written;
            if (request > DECODE_CHUNK) request = DECODE_CHUNK;
            if (tail_frames < CROSSFADE_FRAMES &&
                request > CROSSFADE_FRAMES - tail_frames)
                request = CROSSFADE_FRAMES - tail_frames;
            size_t count = decode_resampled(&decoders[current_slot], decoded_chunk, request);
            if (count) {
                for (size_t i = 0; i < count; ++i) {
                    if (tail_frames < CROSSFADE_FRAMES) {
                        size_t at = (tail_cursor + tail_frames) % CROSSFADE_FRAMES;
                        fade_tail[at * 2] = decoded_chunk[i * 2];
                        fade_tail[at * 2 + 1] = decoded_chunk[i * 2 + 1];
                        ++tail_frames;
                    } else {
                        out[written * 2] = fade_tail[tail_cursor * 2];
                        out[written * 2 + 1] = fade_tail[tail_cursor * 2 + 1];
                        fade_tail[tail_cursor * 2] = decoded_chunk[i * 2];
                        fade_tail[tail_cursor * 2 + 1] = decoded_chunk[i * 2 + 1];
                        tail_cursor = (tail_cursor + 1) % CROSSFADE_FRAMES;
                        ++written;
                    }
                }
                continue;
            }
            if (!begin_transition()) break;
        }

        size_t remaining = transition_frames - transition_offset;
        size_t count = requested - written;
        if (count > remaining) count = remaining;
        for (size_t i = 0; i < count; ++i) {
            size_t fade_at = transition_offset + i;
            size_t tail_at = (tail_cursor + fade_at) % CROSSFADE_FRAMES;
            int32_t a = fade_tail[tail_at * 2];
            int32_t b = fade_tail[tail_at * 2 + 1];
            int32_t c = fade_head[fade_at * 2];
            int32_t d = fade_head[fade_at * 2 + 1];
            int32_t blend_q15 = (int32_t)(((uint64_t)(fade_at + 1) * 32768) /
                                          (uint64_t)transition_frames);
            int32_t old_q15 = 32768 - blend_q15;
            out[(written + i) * 2] = (int16_t)((a * old_q15 + c * blend_q15) >> 15);
            out[(written + i) * 2 + 1] = (int16_t)((b * old_q15 + d * blend_q15) >> 15);
        }
        written += count;
        transition_offset += count;
        if (transition_offset >= transition_frames) finish_transition();
    }
    return written;
}

static void protect_output(int16_t *pcm, size_t samples) {
    int32_t peak = 0;
    for (size_t i = 0; i < samples; ++i) {
        int32_t value = pcm[i];
        int32_t magnitude = value < 0 ? -value : value;
        if (magnitude > peak) peak = magnitude;
    }
    int32_t target_gain = 32768;
    if (peak > 0 && (int64_t)peak * target_gain > (int64_t)30000 * 32768)
        target_gain = (int32_t)((int64_t)30000 * 32768 / peak);
    if (target_gain < current_gain_q15) current_gain_q15 = target_gain;
    else current_gain_q15 += (target_gain - current_gain_q15) / 4;
    for (size_t i = 0; i < samples; ++i)
        pcm[i] = (int16_t)(((int64_t)pcm[i] * current_gain_q15) >> 15);
}

static bool fill_buffer(MusicBuffer *buffer) {
    size_t frames = read_playlist(buffer->pcm, MUSIC_FRAMES);
    if (!frames) return false;
    protect_output(buffer->pcm, frames * 2);
    size_t bytes = frames * 2 * sizeof(int16_t);
    DSP_FlushDataCache(buffer->pcm, bytes);
    buffer->wave.data_vaddr = buffer->pcm;
    buffer->wave.nsamples = frames;
    buffer->wave.looping = false;
    ndspChnWaveBufAdd(MUSIC_CHANNEL, &buffer->wave);
    return true;
}

static void music_worker(void *unused) {
    (void)unused;
    while (!stop_requested) {
        bool filled = false;
        for (unsigned i = 0; i < MUSIC_BUFFERS && !stop_requested; ++i) {
            if (buffers[i].wave.status == NDSP_WBUF_DONE) {
                if (!fill_buffer(&buffers[i])) { stop_requested = true; break; }
                filled = true;
            }
        }
        svcSleepThread(filled ? 1000000LL : 5000000LL);
    }
}

bool music_start(void) {
    if (worker) return true;
    stop_requested = false;
    current_gain_q15 = 32768;
    current_slot = 0;
    current_track = -1;
    playlist_position = -1;
    next_slot = 1;
    tail_frames = tail_cursor = 0;
    transition_frames = transition_offset = 0;
    transition_ready = false;
    random_state = (uint32_t)osGetTime();
    if (!random_state) random_state = 0x9e3779b9u;
    shuffle_playlist(-1);
    if (R_FAILED(ndspInit())) return false;
    dsp_ready = true;
    bool found_track = false;
    for (int attempt = 0; attempt < TRACK_COUNT; ++attempt) {
        playlist_position = attempt;
        current_track = playlist_order[attempt];
        if (open_track(&decoders[current_slot], current_track) && prime_tail()) {
            found_track = true;
            break;
        }
        close_track(&decoders[current_slot]);
    }
    if (!found_track) {
        music_stop();
        return false;
    }

    for (unsigned i = 0; i < MUSIC_BUFFERS; ++i) {
        buffers[i].pcm = linearAlloc(MUSIC_FRAMES * 2 * sizeof(int16_t));
        if (!buffers[i].pcm) { music_stop(); return false; }
        memset(&buffers[i].wave, 0, sizeof(buffers[i].wave));
        buffers[i].wave.status = NDSP_WBUF_DONE;
    }

    ndspChnReset(MUSIC_CHANNEL);
    ndspChnSetFormat(MUSIC_CHANNEL, NDSP_FORMAT_STEREO_PCM16);
    ndspChnSetRate(MUSIC_CHANNEL, (float)OUTPUT_RATE);
    ndspChnSetInterp(MUSIC_CHANNEL, NDSP_INTERP_LINEAR);
    float mix[12] = {0};
    mix[0] = mix[1] = 1.0f;
    ndspChnSetMix(MUSIC_CHANNEL, mix);

    for (unsigned i = 0; i < MUSIC_BUFFERS; ++i)
        if (!fill_buffer(&buffers[i])) { music_stop(); return false; }
    /* minimp3 keeps its decode scratch buffers on the caller's stack (~20 KiB).
       The worker also opens and primes the next track during crossfade, so a
       16 KiB stack corrupts memory exactly at the first MP3 transition. */
    worker = threadCreate(music_worker, NULL, 64 * 1024, 0x28, -2, false);
    if (!worker) { music_stop(); return false; }
    return true;
}

void music_stop(void) {
    stop_requested = true;
    if (worker) {
        threadJoin(worker, U64_MAX);
        threadFree(worker);
        worker = NULL;
    }
    if (dsp_ready) {
        ndspChnWaveBufClear(MUSIC_CHANNEL);
        ndspChnReset(MUSIC_CHANNEL);
    }
    close_track(&decoders[0]);
    close_track(&decoders[1]);
    for (unsigned i = 0; i < MUSIC_BUFFERS; ++i) {
        if (buffers[i].pcm) {
            linearFree(buffers[i].pcm);
            buffers[i].pcm = NULL;
        }
        memset(&buffers[i].wave, 0, sizeof(buffers[i].wave));
    }
    if (dsp_ready) {
        ndspExit();
        dsp_ready = false;
    }
}
