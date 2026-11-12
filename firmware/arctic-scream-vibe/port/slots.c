/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* Long samples (what the FM-1 cannot): Arctic Scream's four sample slots, each ~95 s here (the FM-1: 6.6 s), filled on
 * the VIBE itself, never by a computer:
 *   REC     the sound in (as heard, through the VOICE; or dry) recorded into a slot (live_in.c records, here it is kept)
 *   LOAD    a WAV from the host's media (the VIBE's card, a USB stick, the plugin's folder; SDK 13), any rate, 8 / 16 /
 *           24 / 32 bit or float, mono or more (mixed down), read a piece at a time and resampled to the slots' 22050 Hz
 *   DROP    a WAV let go on the plugin's window (SDK 14): onto pads 5..8 its slot, else the SAMPLE page's
 * A slot is written as samples.c's upload writes it (samples.c: the "GSMP" header, IMA ADPCM from predictor 0, the
 * header last), then samples.c checks and indexes it as any slot. The encoder is the standard IMA ADPCM choice
 * (tools/sampleio.py ima_encode, look=False). Never from vibe_on_audio. */

#define SL_MAX SMP_MAX_N                        /* a slot's samples at 22050 Hz (~95 s) */
static int16_t sl_buf[SL_MAX];                  /* a recording or a file, before it is encoded (shared: one at a time) */
static uint32_t sl_n;

static uint32_t sl_wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); return 2u; }
static uint32_t sl_wr32(uint8_t *p, uint32_t v) { sl_wr16(p, v); sl_wr16(p + 2, v >> 16); return 4u; }

static void sl_write(uint32_t k, const char *name, const int16_t *x, uint32_t n)   /* slot k := x (n samples) */
{
    uint8_t *h = fake_flash + k * SMP_SLOT_SIZE, *d = h + SMP_DATA_OFF;
    int32_t pred = 0, idx = 0, peak = 1;
    uint32_t i, len;
    int64_t g;
    if (n > SL_MAX) n = SL_MAX;
    for (i = 0; i < n; i++) { int32_t a = x[i] < 0 ? -x[i] : x[i]; if (a > peak) peak = a; }
    g = (int64_t)29204 * 65536 / peak;          /* (to -1 dBFS: as loud as a slot can be) */
    memset(h, 0xFF, SMP_SLOT_SIZE);
    smp[k].ready = 0, smp[k].state = SS_EMPTY;
    for (i = 0; i < n; i++) {                   /* IMA ADPCM, 4 bit, low nibble first */
        int32_t s = (int32_t)(x[i] * g >> 16), step = IMA_STEP[idx], diff = s - pred, vd = step >> 3, code = 0;
        if (diff < 0) code = 8, diff = -diff;
        if (diff >= step) code |= 4, diff -= step, vd += step;
        if (diff >= step >> 1) code |= 2, diff -= step >> 1, vd += step >> 1;
        if (diff >= step >> 2) code |= 1, vd += step >> 2;
        pred = code & 8 ? pred - vd : pred + vd;
        pred = pred > 32767 ? 32767 : pred < -32768 ? -32768 : pred;
        idx += IMA_IDX[code & 7];
        idx = idx < 0 ? 0 : idx > 88 ? 88 : idx;
        if (i & 1u) d[i >> 1] = (uint8_t)((d[i >> 1] & 15u) | (uint32_t)code << 4);
        else d[i >> 1] = (uint8_t)code;
    }
    len = (n + 1u) >> 1;
    memset(h, 0, SMP_HDR_LEN);                  /* the header (tools/sampleio.py user_slot, one zone) */
    sl_wr32(h, SMP_MAGIC), sl_wr16(h + 4, 1u), h[6] = 1u;
    for (i = 0; i < 8u && name[i]; i++) {
        char c = name[i] >= 'a' && name[i] <= 'z' ? (char)(name[i] - 32) : name[i];
        h[8 + i] = (uint8_t)(c >= 32 && c < 127 ? c : '_');
    }
    sl_wr32(h + 16, len), sl_wr32(h + 20, ~smp_crc(0xFFFFFFFFu, d, len));
    sl_wr32(h + 32, 0), sl_wr32(h + 36, n), sl_wr32(h + 40, 0), sl_wr32(h + 44, n - 1u), sl_wr32(h + 48, SMP_RATE_Q16);
    sl_wr16(h + 52, 60u * 16u), sl_wr16(h + 54, 0), h[56] = 0, h[57] = 0, h[58] = 127, h[59] = 0;
    smp_look(k);                                /* (samples.c checks and indexes it, then it plays) */
    vb_dirty |= VD_SLOT0 << k;
}

/* ---- REC: live_in.c hands each engine frame (44.1 kHz) to sl_rec_put while recording; two make one slot sample */
static volatile int sl_rec;                     /* recording into slot sl_rec - 1 (0: not) */
static int sl_rec_dry;                          /* the input dry, not through the VOICE */
static int32_t sl_rec_half;
static uint32_t sl_rec_odd;
static void sl_rec_put(float x)                 /* (audio: from live_take) */
{
    int32_t v = (int32_t)(x * 32767.0f);
    if (!(sl_rec_odd ^= 1u)) {                  /* (pairs averaged: 44.1 -> 22.05 kHz) */
        if (sl_n < SL_MAX) sl_buf[sl_n++] = (int16_t)((sl_rec_half + v) >> 1);
    } else
        sl_rec_half = v;
}
static void sl_rec_start(uint32_t k) { sl_n = 0, sl_rec_odd = 0, sl_rec = (int)k + 1; }
static void sl_rec_stop(void)
{
    uint32_t k = (uint32_t)sl_rec - 1u;
    if (!sl_rec) return;
    sl_rec = 0;
    if (sl_n >= 2205u) sl_write(k, "REC", sl_buf, sl_n);   /* (shorter than 0.1 s: nothing) */
}

/* ---- LOAD: a WAV from the media, a piece a frame (vibe_on_draw, VIBE_MEDIA_EV_READY) */
enum { WL_IDLE, WL_HEAD, WL_DATA };
static int wl_state, wl_slot, wl_fmt, wl_bits, wl_ch, wl_err;
static char wl_path[VIBE_MEDIA_PATH], wl_name[9], wl_msg[48];
static uint64_t wl_off, wl_end;                 /* the next byte to read, the data's end */
static uint64_t wl_t, wl_dt;                    /* the next output sample's time in input frames (Q32), a step */
static uint64_t wl_in;                          /* input frames taken so far */
static float wl_prev, wl_lp[3];
static uint8_t wl_b[32768];

static void wl_start(const char *path, uint32_t slot)
{
    const char *s = path, *e;
    uint32_t i = 0;
    while (*s) s++;
    while (s > path && s[-1] != '/') s--;
    for (e = s; *e && *e != '.' && i < 8u; e++) wl_name[i++] = *e;
    wl_name[i] = 0;
    for (i = 0; path[i] && i < VIBE_MEDIA_PATH - 1u; i++) wl_path[i] = path[i];
    wl_path[i] = 0;
    wl_slot = (int)slot, wl_ch = 0, wl_state = WL_HEAD, wl_off = 0, wl_err = 0, sl_n = 0, wl_in = 0, wl_t = 0, wl_prev = 0;
    wl_lp[0] = wl_lp[1] = wl_lp[2] = 0;
    strcpy(wl_msg, "LOADING");
}

static void wl_fail(const char *m) { wl_state = WL_IDLE, wl_err = 1; strcpy(wl_msg, m); }

static int wl_head(void)                        /* the RIFF's chunks up to "data": 1 done, 0 not yet (ask again) */
{
    int r = vibe_media_read(wl_path, wl_off, wl_b, 12), sr;
    uint32_t n;
    if (wl_off == 0) {
        if (r == VIBE_MEDIA_PENDING) return 0;
        if (r < 12 || memcmp(wl_b, "RIFF", 4) || memcmp(wl_b + 8, "WAVE", 4)) { wl_fail("NOT A WAV"); return 0; }
        wl_off = 12;
    }
    for (;;) {                                  /* a chunk's head: its id, its length */
        r = vibe_media_read(wl_path, wl_off, wl_b, 8);
        if (r == VIBE_MEDIA_PENDING) return 0;
        if (r < 8) { wl_fail("NO DATA IN THE WAV"); return 0; }
        n = rd32(wl_b + 4);
        if (!memcmp(wl_b, "fmt ", 4)) {
            r = vibe_media_read(wl_path, wl_off + 8u, wl_b, 40);
            if (r == VIBE_MEDIA_PENDING) return 0;
            if (r < 16) { wl_fail("A BROKEN WAV"); return 0; }
            wl_fmt = (int)rd16(wl_b), wl_ch = (int)rd16(wl_b + 2), sr = (int)rd32(wl_b + 4), wl_bits = (int)rd16(wl_b + 14);
            if (wl_fmt == 0xFFFE && r >= 26) wl_fmt = (int)rd16(wl_b + 24);   /* (EXTENSIBLE: its subformat) */
            if ((wl_fmt != 1 && wl_fmt != 3) || wl_ch < 1 || wl_ch > 8 || sr < 4000 ||
                (wl_fmt == 1 && wl_bits != 8 && wl_bits != 16 && wl_bits != 24 && wl_bits != 32) || (wl_fmt == 3 && wl_bits != 32)) {
                wl_fail("THIS WAV: NOT READ");
                return 0;
            }
            wl_dt = ((uint64_t)sr << 32) / 22050u;
        } else if (!memcmp(wl_b, "data", 4)) {
            if (!wl_ch) { wl_fail("A BROKEN WAV"); return 0; }
            wl_off += 8u, wl_end = wl_off + n;
            return 1;
        }
        wl_off += 8u + n + (n & 1u);
    }
}

static float wl_frame(const uint8_t *p)         /* one input frame, mixed to mono, -1..1 */
{
    float s = 0;
    int c;
    for (c = 0; c < wl_ch; c++, p += wl_bits / 8) {
        if (wl_fmt == 3) { union { uint32_t u; float f; } q; q.u = rd32(p); s += q.f; }
        else if (wl_bits == 8) s += ((float)p[0] - 128.0f) * (1.0f / 128.0f);
        else if (wl_bits == 16) s += (float)(int16_t)rd16(p) * (1.0f / 32768.0f);
        else if (wl_bits == 24) s += (float)((int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8) * (1.0f / 8388608.0f);
        else s += (float)(int32_t)rd32(p) * (1.0f / 2147483648.0f);
    }
    return s / (float)wl_ch;
}

static void wl_step(void)                       /* (vibe_on_draw, READY) as much as the host has ready */
{
    int k, fb, r;
    if (wl_state == WL_HEAD && !wl_head()) return;
    if (wl_state == WL_HEAD) wl_state = WL_DATA;
    fb = wl_bits / 8 * wl_ch;
    for (k = 0; k < 8 && wl_state == WL_DATA; k++) {   /* (up to 8 x 32 KB a call) */
        uint64_t want = wl_end - wl_off;
        int i, nf;
        if (want > sizeof wl_b) want = sizeof wl_b;
        want -= want % (uint64_t)fb;
        r = want ? vibe_media_read(wl_path, wl_off, wl_b, (int)want) : 0;
        if (r == VIBE_MEDIA_PENDING) return;
        if (r < 0) { wl_fail("THE FILE IS GONE"); return; }
        nf = r / fb;
        for (i = 0; i < nf && sl_n < SL_MAX; i++) {   /* each input frame: low-passed (below 22.05 kHz's 11 kHz when the
                                                 * input is faster), then the output samples between it and the last */
            float x = wl_frame(wl_b + i * fb);
            if (wl_dt > (3ull << 31)) {         /* (faster than 33 kHz: two one-poles at ~9 kHz of its rate) */
                float a = 9000.0f * 6.2832f / ((float)(wl_dt >> 16) * 22050.0f / 65536.0f);
                a = a > 1.0f ? 1.0f : a;
                wl_lp[0] += (x - wl_lp[0]) * a, wl_lp[1] += (wl_lp[0] - wl_lp[1]) * a, x = wl_lp[1];
            }
            while ((wl_t >> 32) <= wl_in && sl_n < SL_MAX) {   /* (between frame wl_in - 1 and wl_in) */
                float f = (float)(uint32_t)wl_t * (1.0f / 4294967296.0f), y;
                y = (wl_t >> 32) == wl_in ? x : wl_prev + (x - wl_prev) * f;
                y = y > 1.0f ? 1.0f : y < -1.0f ? -1.0f : y;
                sl_buf[sl_n++] = (int16_t)(y * 32767.0f);
                wl_t += wl_dt;
            }
            wl_prev = x, wl_in++;
        }
        wl_off += (uint64_t)nf * (uint64_t)fb;
        if (!r || wl_off >= wl_end || sl_n >= SL_MAX) {
            wl_state = WL_IDLE;
            if (sl_n < 64u) { wl_fail("EMPTY"); return; }
            sl_write((uint32_t)wl_slot, wl_name, sl_buf, sl_n);
            strcpy(wl_msg, sl_n >= SL_MAX ? "LOADED (CUT AT 95 S)" : "LOADED");
            return;
        }
    }
}

/* ---- the media's folders, for the SAMPLE page's list */
#define ML_N 64
static char ml_cur[VIBE_MEDIA_PATH], ml_lines[ML_N][96], ml_src[1024];
static int ml_n, ml_sel, ml_top, ml_want = 1, ml_none;
static const char *ml_field(const char *l, int k) { while (k-- > 0) { while (*l && *l != '\t') l++; if (*l) l++; } return l; }
static void ml_split(char *b, int n)
{
    char *p = b;
    b[n] = 0, ml_n = 0;
    while (*p && ml_n < ML_N) {
        char *nl = p;
        int len, i;
        while (*nl && *nl != '\n') nl++;
        len = (int)(nl - p) < 95 ? (int)(nl - p) : 95;
        for (i = 0; i < len; i++) ml_lines[ml_n][i] = p[i];
        ml_lines[ml_n++][len] = 0;
        p = *nl ? nl + 1 : nl;
    }
}
static void ml_step(void)
{
    static char b[4096];
    int r;
    if (!ml_want) return;
    if (!ml_cur[0]) {
        r = vibe_media_sources(ml_src, (int)sizeof ml_src - 1);
        if (r < 0) { ml_none = 1, ml_want = 0, ml_n = 0; return; }
        ml_none = 0, ml_want = 0;
        ml_split(ml_src, r);
        return;
    }
    r = vibe_media_list(ml_cur, 0, b, (int)sizeof b - 1);
    if (r == VIBE_MEDIA_PENDING) return;
    ml_want = 0;
    if (r < 0) { ml_cur[0] = 0, ml_want = 1; return; }
    ml_split(b, r);
}
static int ml_is_wav(const char *s) { const char *e = s; while (*e) e++; return e - s > 4 && (e[-1] | 32) == 'v' && (e[-2] | 32) == 'a' && (e[-3] | 32) == 'w' && e[-4] == '.'; }
static void ml_open(uint32_t slot)              /* ENTER: into a source or a folder, or a WAV into the slot */
{
    char p[VIBE_MEDIA_PATH];
    int i, n = 0;
    if (ml_sel >= ml_n) return;
    if (!ml_cur[0]) {
        for (i = 0; ml_lines[ml_sel][i] && ml_lines[ml_sel][i] != '\t' && i < 47; i++) ml_cur[i] = ml_lines[ml_sel][i];
        ml_cur[i] = 0, ml_want = 1, ml_sel = ml_top = 0;
        return;
    }
    while (ml_cur[n] && n < VIBE_MEDIA_PATH - 2) p[n] = ml_cur[n], n++;
    p[n++] = '/';
    for (i = 0; ml_field(ml_lines[ml_sel], 2)[i] && n < VIBE_MEDIA_PATH - 1; i++) p[n++] = ml_field(ml_lines[ml_sel], 2)[i];
    p[n] = 0;
    if (ml_lines[ml_sel][0] == 'd') { strcpy(ml_cur, p), ml_want = 1, ml_sel = ml_top = 0; }
    else if (ml_is_wav(p)) wl_start(p, slot);
    else strcpy(wl_msg, "ONLY WAV FILES");
}
static void ml_up(void)
{
    int n = (int)strlen(ml_cur);
    while (n > 0 && ml_cur[n - 1] != '/') n--;
    ml_cur[n > 0 ? n - 1 : 0] = 0;
    ml_want = 1, ml_sel = ml_top = 0;
}

static void sl_drops(uint32_t slot)             /* (SDK 14) files let go on the plugin: pads 5..8 their slot */
{
    char p[VIBE_MEDIA_PATH];
    int pad = -1, x, y;
    if (wl_state != WL_IDLE) return;
    if (vibe_media_dropped(p, (int)sizeof p, &pad, &x, &y) == 1) {
        if (ml_is_wav(p)) wl_start(p, pad >= 4 && pad < 8 ? (uint32_t)pad - 4u : slot);
        else strcpy(wl_msg, "ONLY WAV FILES");
    }
}

static void sl_work(uint32_t slot)              /* (vibe_on_draw, VIBE_EV_MEDIA) the media's work, as far as it goes */
{
    int i;
    for (i = 0; i < 4; i++) {
        int s = wl_state, w = ml_want;
        uint64_t o = wl_off;
        ml_step();
        if (wl_state != WL_IDLE) wl_step();
        else sl_drops(slot);
        if (s == wl_state && w == ml_want && o == wl_off) break;
    }
}
