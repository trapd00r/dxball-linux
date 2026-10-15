#include "dxball.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_all(const char *path, uint8_t **out, size_t *n) {
    FILE *f;
    long sz;

    f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    rewind(f);
    *out = malloc((size_t)sz);
    if (*out == NULL) {
        fclose(f);
        return -1;
    }
    if (fread(*out, 1, (size_t)sz, f) != (size_t)sz) {
        free(*out);
        fclose(f);
        return -1;
    }
    fclose(f);
    *n = (size_t)sz;
    return 0;
}

int load_pcx(const char *path, PcxImage *out) {
    uint8_t *data;
    size_t n;
    int xmin, ymin, xmax, ymax, w, h, bpl, x, y, i, o;
    uint8_t bits, planes;
    uint8_t *src;
    uint8_t *pixels;

    memset(out, 0, sizeof(*out));
    data = NULL;
    if (read_all(path, &data, &n) != 0 || n < 128 + 768) {
        free(data);
        return -1;
    }
    if (data[0] != 0x0A) {
        free(data);
        return -1;
    }
    xmin = data[4] | (data[5] << 8);
    ymin = data[6] | (data[7] << 8);
    xmax = data[8] | (data[9] << 8);
    ymax = data[10] | (data[11] << 8);
    bits = data[3];
    planes = data[65];
    bpl = data[66] | (data[67] << 8);
    w = xmax - xmin + 1;
    h = ymax - ymin + 1;
    if (bits != 8 || planes != 1 || w <= 0 || h <= 0) {
        free(data);
        return -1;
    }
    pixels = calloc((size_t)w * (size_t)h, 1);
    if (pixels == NULL) {
        free(data);
        return -1;
    }
    src = data + 128;
    i = 0;
    o = 0;
    for (y = 0; y < h; y++) {
        x = 0;
        while (x < bpl) {
            uint8_t b;
            int run;
            uint8_t v;
            if ((size_t)i >= n - 768) {
                free(pixels);
                free(data);
                return -1;
            }
            b = src[i++];
            if ((b & 0xC0) == 0xC0) {
                run = b & 0x3F;
                v = src[i++];
            } else {
                run = 1;
                v = b;
            }
            while (run-- > 0 && x < bpl) {
                if (x < w) {
                    pixels[o + x] = v;
                }
                x++;
            }
        }
        o += w;
    }
    memcpy(out->pal, data + n - 768, 768);
    free(data);
    out->w = w;
    out->h = h;
    out->pixels = pixels;
    return 0;
}

void free_pcx(PcxImage *img) {
    free(img->pixels);
    img->pixels = NULL;
}

int load_sbk(const char *path, SpriteBank *out) {
    uint8_t *data;
    size_t n;
    uint32_t count, off, i, y;

    memset(out, 0, sizeof(*out));
    data = NULL;
    if (read_all(path, &data, &n) != 0 || n < 4) {
        free(data);
        return -1;
    }
    memcpy(&count, data, 4);
    /* FUN_00404610 loads file sprites at index 1, leaving slot 0 empty. */
    if (count + 1 > MAX_SPRITES) {
        free(data);
        return -1;
    }
    off = 4;
    out->count = (int)count + 1;
    for (i = 0; i < count; i++) {
        uint32_t w, h, extra;
        uint8_t ch;
        uint8_t *pix;
        if (off + 13 > n) {
            free(data);
            return -1;
        }
        memcpy(&w, data + off, 4);
        off += 4;
        memcpy(&h, data + off, 4);
        off += 4;
        ch = data[off++];
        memcpy(&extra, data + off, 4);
        off += 4;
        if (off + w * h > n || w == 0 || h == 0 || w > 1024 || h > 1024) {
            free(data);
            return -1;
        }
        pix = malloc((size_t)w * (size_t)h);
        if (pix == NULL) {
            free(data);
            return -1;
        }
        /* File stores the first row as the bottom of the DirectDraw surface. */
        for (y = 0; y < h; y++) {
            memcpy(pix + (size_t)(h - 1 - y) * w, data + off + (size_t)y * w, w);
        }
        off += w * h;
        out->sprites[i + 1].w = (int)w;
        out->sprites[i + 1].h = (int)h;
        out->sprites[i + 1].ch = ch;
        out->sprites[i + 1].extra = (int)extra;
        out->sprites[i + 1].pixels = pix;
    }
    free(data);
    return 0;
}

void free_sbk(SpriteBank *bank) {
    int i;
    for (i = 0; i < bank->count; i++) {
        free(bank->sprites[i].pixels);
        bank->sprites[i].pixels = NULL;
    }
    bank->count = 0;
}

int load_bds(const char *path, uint8_t boards[BOARD_COUNT][BOARD_ROWS][BOARD_COLS]) {
    uint8_t *data;
    size_t n;
    int b, r;

    if (read_all(path, &data, &n) != 0) {
        return -1;
    }
    if (n != (size_t)BOARD_COUNT * BOARD_ROWS * BOARD_COLS) {
        free(data);
        return -1;
    }
    for (b = 0; b < BOARD_COUNT; b++) {
        for (r = 0; r < BOARD_ROWS; r++) {
            memcpy(boards[b][r],
                   data + (size_t)b * BOARD_ROWS * BOARD_COLS + (size_t)r * BOARD_COLS,
                   BOARD_COLS);
        }
    }
    free(data);
    return 0;
}

int load_wav(const char *path, Sound *out) {
    SDL_AudioSpec spec;
    Uint8 *pcm;
    Uint32 bytes;
    SDL_AudioCVT cvt;

    memset(out, 0, sizeof(*out));
    if (SDL_LoadWAV(path, &spec, &pcm, &bytes) == NULL) {
        return -1;
    }
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq,
                          AUDIO_S16SYS, 2, MIX_RATE) < 0) {
        SDL_FreeWAV(pcm);
        return -1;
    }
    if (cvt.len_mult < 1) {
        cvt.len_mult = 1;
    }
    cvt.len = (int)bytes;
    cvt.buf = malloc((size_t)cvt.len * (size_t)cvt.len_mult);
    if (cvt.buf == NULL) {
        SDL_FreeWAV(pcm);
        return -1;
    }
    memcpy(cvt.buf, pcm, bytes);
    SDL_FreeWAV(pcm);
    if (cvt.needed) {
        if (SDL_ConvertAudio(&cvt) < 0) {
            free(cvt.buf);
            return -1;
        }
    } else {
        cvt.len_cvt = cvt.len;
    }
    out->pcm = (int16_t *)cvt.buf;
    out->frames = cvt.len_cvt / (int)(sizeof(int16_t) * 2);
    if (out->frames < 1) {
        free(out->pcm);
        out->pcm = NULL;
        return -1;
    }
    return 0;
}

void free_sound(Sound *snd) {
    free(snd->pcm);
    snd->pcm = NULL;
    snd->frames = 0;
}

int save_bds(const char *path, uint8_t boards[BOARD_COUNT][BOARD_ROWS][BOARD_COLS]) {
    FILE *f;
    int b, r;

    f = fopen(path, "wb");
    if (f == NULL) {
        return -1;
    }
    for (b = 0; b < BOARD_COUNT; b++) {
        for (r = 0; r < BOARD_ROWS; r++) {
            if (fwrite(boards[b][r], 1, BOARD_COLS, f) != BOARD_COLS) {
                fclose(f);
                return -1;
            }
        }
    }
    fclose(f);
    return 0;
}

int load_hiscores(const char *path, HiScore *out) {
    FILE *f;
    int i;

    memset(out, 0, sizeof(HiScore) * HISCORE_N);
    f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }
    for (i = 0; i < HISCORE_N; i++) {
        if (fread(out[i].name, 1, HISCORE_NAME, f) != HISCORE_NAME) {
            fclose(f);
            return -1;
        }
        out[i].name[HISCORE_NAME - 1] = 0;
        if (fread(&out[i].score, 4, 1, f) != 1) {
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    return 0;
}

int save_hiscores(const char *path, const HiScore *in) {
    FILE *f;
    int i;

    f = fopen(path, "wb");
    if (f == NULL) {
        return -1;
    }
    for (i = 0; i < HISCORE_N; i++) {
        char name[HISCORE_NAME];
        memset(name, 0, HISCORE_NAME);
        snprintf(name, HISCORE_NAME, "%s", in[i].name);
        fwrite(name, 1, HISCORE_NAME, f);
        fwrite(&in[i].score, 4, 1, f);
    }
    fclose(f);
    return 0;
}

int load_mids(const char *path, MidiEv *ev, int max, uint32_t *ppqn) {
    uint8_t *data;
    size_t n;
    size_t i;
    uint32_t tick;
    int nev;
    uint32_t data_off;
    uint32_t data_sz;

    if (read_all(path, &data, &n) != 0) {
        return -1;
    }
    if (n < 48 || memcmp(data, "RIFF", 4) != 0) {
        free(data);
        return -1;
    }
    data_off = 0;
    data_sz = 0;
    *ppqn = 192;
    i = 12;
    while (i + 8 <= n) {
        uint32_t sz;
        memcpy(&sz, data + i + 4, 4);
        if (memcmp(data + i, "fmt ", 4) == 0 && sz >= 4) {
            memcpy(ppqn, data + i + 8, 4);
            if (*ppqn == 0) {
                *ppqn = 192;
            }
        }
        if (memcmp(data + i, "data", 4) == 0) {
            data_off = (uint32_t)i + 8;
            data_sz = sz;
            break;
        }
        i += 8 + sz;
        if (sz & 1) {
            i++;
        }
    }
    if (data_off == 0 || data_off + 4 > n) {
        free(data);
        return -1;
    }
    if (data_off + data_sz > n) {
        data_sz = (uint32_t)(n - data_off);
    }
    /* data: DWORD nBuffers, then per buffer DWORD startTick, DWORD packedSize,
       then packedSize bytes of (delta, dwEvent) pairs. FUN_00401210. */
    {
        uint32_t nbuf;
        uint32_t b;
        size_t end = data_off + data_sz;
        memcpy(&nbuf, data + data_off, 4);
        i = data_off + 4;
        tick = 0;
        nev = 0;
        for (b = 0; b < nbuf && i + 8 <= end && nev < max; b++) {
            uint32_t packed;
            size_t pend;
            memcpy(&packed, data + i + 4, 4);
            i += 8;
            pend = i + packed;
            if (pend > end) {
                pend = end;
            }
            while (i + 8 <= pend && nev < max) {
                uint32_t delta;
                uint32_t msg;
                memcpy(&delta, data + i, 4);
                memcpy(&msg, data + i + 4, 4);
                tick += delta;
                ev[nev].tick = tick;
                ev[nev].msg = msg;
                nev++;
                i += 8;
            }
            i = pend;
        }
    }
    free(data);
    return nev;
}
