/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* Arctic Scream VIBE's stage (VIBE FM SDK 23: a second, bigger picture the firmware draws itself, 960 x 540; the Web
 * Player opens with it in place of the device). The polar night, drawn by what plays:
 *   the sky      four northern lights, one a track in its colour: how high and bright each hangs, its level now
 *                (the engine's vis_lv); the curtains' folds drift on their own
 *   the moon     JAHKI's season (its colour, its Sami name); off: the polar night's pale moon
 *   the card     the selected track's sound's picture (the engine's pictures, moving), torn when it screams
 *   the tracks   T1..T4, their sounds' names, lit with their levels
 *   the lake     the sky and the mountains in the ice, rippling
 *   the shelf    the 16 pads as blocks of ice, lit as the VIBE's pads (the tracks, the slots, DICE's moods), and
 *                played with the pointer
 * The control flow's (vibe_on_stage, after vibe_on_draw): it reads the engine, never writes it; a block clicked goes
 * to the sound's flow as a pad would (vb_eq). Everything here also works without it: the display and the pads do it all. */

#define ST_W 960
#define ST_H 540
#define ST_HZ 318                               /* the horizon */
#define ST_LAKE 440                             /* the lake's end, the shelf's top */
static uint16_t st_fb[ST_W * ST_H];             /* the stage's picture, ours (RGB565) */
static uint8_t st_c[ST_W * ST_H * 3];           /* drawn in RGB first: the lights add up */
static int st_down = -1;                        /* the block held with the pointer (-1: none) */

static uint32_t st_hash(uint32_t x)             /* (a stable random number from x) */
{
    x ^= x >> 16, x *= 0x7feb352du, x ^= x >> 15, x *= 0x846ca68bu, x ^= x >> 16;
    return x;
}
static float st_noise(float x, uint32_t seed)   /* smooth noise 0..1 along x */
{
    int32_t i = (int32_t)x - (x < 0);
    float f = x - (float)i, a = (float)(st_hash((uint32_t)i * 2654435761u ^ seed) & 1023u) / 1023.0f,
          b = (float)(st_hash((uint32_t)(i + 1) * 2654435761u ^ seed) & 1023u) / 1023.0f;
    f = f * f * (3.0f - 2.0f * f);
    return a + (b - a) * f;
}
static float st_sin(float x)                    /* (a sine good enough for pictures: no libm here) */
{
    x -= 6.2831853f * (float)(int32_t)(x * 0.15915494f);
    if (x < 0) x += 6.2831853f;
    if (x > 3.1415927f) return -st_sin(x - 3.1415927f);
    return 16.0f * x * (3.1415927f - x) / (49.348022f - 4.0f * x * (3.1415927f - x));
}

static void st_set(int x, int y, uint32_t rgb)
{
    uint8_t *p;
    if ((unsigned)x >= ST_W || (unsigned)y >= ST_H) return;
    p = st_c + (y * ST_W + x) * 3;
    p[0] = (uint8_t)(rgb >> 16), p[1] = (uint8_t)(rgb >> 8), p[2] = (uint8_t)rgb;
}
static void st_add(int x, int y, uint32_t rgb, int a)   /* rgb x a / 256 added (light) */
{
    uint8_t *p;
    int c, v;
    if ((unsigned)x >= ST_W || (unsigned)y >= ST_H || a <= 0) return;
    p = st_c + (y * ST_W + x) * 3;
    for (c = 0; c < 3; c++) {
        v = p[c] + (int)((rgb >> (16 - 8 * c) & 255u) * (uint32_t)a >> 8);
        p[c] = (uint8_t)(v > 255 ? 255 : v);
    }
}
static void st_mix(int x, int y, uint32_t rgb, int a)   /* rgb over it, a / 256 of it */
{
    uint8_t *p;
    int c;
    if ((unsigned)x >= ST_W || (unsigned)y >= ST_H) return;
    p = st_c + (y * ST_W + x) * 3;
    for (c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] + (((int)(rgb >> (16 - 8 * c) & 255u) - p[c]) * a >> 8));
}
static void st_rect(int x, int y, int w, int h, uint32_t rgb)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) st_set(x + i, y + j, rgb);
}
static void st_rect_a(int x, int y, int w, int h, uint32_t rgb, int a)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) st_mix(x + i, y + j, rgb, a);
}
static int st_text(int x, int y, const char *s, int sc, uint32_t rgb)   /* the pixfont, sc x its pixels; -> the width */
{
    int x0 = x, r, b, i, j;
    for (; *s; s++, x += 8 * sc) {
        const uint8_t *g = PIXFONT[*s >= 32 && *s < 127 ? *s - 32 : 0];
        for (r = 0; r < 8; r++)
            for (b = 0; b < 8; b++)
                if (g[r] >> (7 - b) & 1u)
                    for (j = 0; j < sc; j++)
                        for (i = 0; i < sc; i++) st_set(x + b * sc + i, y + r * sc + j, rgb);
    }
    return x - x0;
}
static uint32_t st_565(uint16_t c)              /* RGB565 -> 0xRRGGBB */
{
    uint32_t r = c >> 11, g = c >> 5 & 63u, b = c & 31u;
    return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}
static uint32_t st_scale(uint32_t rgb, float k)
{
    uint32_t r = (uint32_t)((float)(rgb >> 16 & 255u) * k), g = (uint32_t)((float)(rgb >> 8 & 255u) * k),
             b = (uint32_t)((float)(rgb & 255u) * k);
    return (r > 255u ? 255u : r) << 16 | (g > 255u ? 255u : g) << 8 | (b > 255u ? 255u : b);
}

static float st_level(uint32_t t)               /* a track's level now, 0..1 (its peak, logarithmic: 64 .. 32768) */
{
    uint32_t p = vis_lv[t & 3u], n = 0;
    float f;
    if (p < 64u) return 0.0f;
    while (p >> (n + 1)) n++;
    f = ((float)n + (float)(p - (1u << n)) / (float)(1u << n) - 6.0f) / 9.0f;
    return f < 0 ? 0 : f > 1 ? 1 : f;
}

/* ---- the sky, the lights, the moon */
static const uint32_t ST_SEASON[8] = {0x1c2a6e, 0x9cc8f0, 0x3fb950, 0x7cd86a, 0xffcb47, 0xd9a441, 0xb05a3a, 0x6a6e9a};
static float st_lv[NT];                         /* the levels, smoothed for the picture */

static void st_sky(uint32_t ms)
{
    int x, y, i, k;
    float t = (float)ms * 0.001f;
    for (y = 0; y < ST_HZ; y++) {               /* the night: darker up high */
        float f = (float)y / (float)ST_HZ;
        uint32_t c = (uint32_t)(3 + 10 * f) << 16 | (uint32_t)(2 + 18 * f) << 8 | (uint32_t)(10 + 40 * f);
        for (x = 0; x < ST_W; x++) st_set(x, y, c);
    }
    for (i = 0; i < 260; i++) {                 /* the stars, twinkling */
        uint32_t h = st_hash((uint32_t)i * 7919u + 17u);
        int sx = (int)(h % ST_W), sy = (int)((h >> 10) % (ST_HZ - 40));
        float tw = 0.55f + 0.45f * st_sin(t * (0.6f + (float)(h >> 24 & 15u) * 0.15f) + (float)(h & 63u));
        uint32_t c = (h >> 20 & 7u) == 0u ? 0xa7c8ff : (h >> 20 & 7u) == 1u ? 0xffe0c0 : 0xdfe8f4;
        st_add(sx, sy, c, (int)(tw * (h >> 28 ? 150.0f : 255.0f)));
        if ((h >> 28) == 0u) {                  /* (one in sixteen a bright one, a cross) */
            st_add(sx - 1, sy, c, (int)(tw * 90)), st_add(sx + 1, sy, c, (int)(tw * 90));
            st_add(sx, sy - 1, c, (int)(tw * 90)), st_add(sx, sy + 1, c, (int)(tw * 90));
        }
    }
    for (k = NT - 1; k >= 0; k--) {             /* the four lights, T4 furthest back */
        float L = st_lv[k], amp = 0.3f + 1.1f * L, lift = 0.4f + 0.6f * L;
        uint32_t col = st_565(UI_ACCENTS[k]);
        if (trk[k].level <= 0 || (trk_quiet >> k & 1u)) amp = 0.08f, lift = 0.3f;
        for (x = 0; x < ST_W; x += 2) {
            float fx = (float)x;
            float base = 262.0f - (float)k * 30.0f + 30.0f * st_sin(fx * 0.0052f + t * 0.21f + (float)k * 1.7f) +
                         12.0f * st_sin(fx * 0.017f - t * 0.47f * (float)(k + 1));
            float ray = st_noise(fx * 0.05f + t * (0.35f + 0.12f * (float)k), 0x51u + (uint32_t)k), h, a0;
            ray = 0.2f + 0.8f * ray * ray;
            h = (70.0f + 200.0f * lift) * (0.45f + 0.55f * st_noise(fx * 0.011f - t * 0.2f, 0x77u + (uint32_t)k));
            a0 = 230.0f * amp * ray;
            for (y = 0; y < (int)h; y++) {      /* bright at the curtain's hem, fading upwards */
                float f = 1.0f - (float)y / h;
                int a = (int)(a0 * f * (0.35f + 0.65f * f) * (y < 4 ? 1.8f : 1.0f));
                int yy = (int)base - y;
                st_add(x, yy, col, a), st_add(x + 1, yy, col, a * 3 / 4);
                if (y < 30) st_add(x, yy + 30 - y / 2, col, a / 10);   /* (a little light under the hem) */
            }
            if (L > 0.5f) st_add(x, (int)base + 1, 0xffffff, (int)(200.0f * (L - 0.5f) * ray));   /* (a white hem: loud) */
        }
    }
}

static void st_snow(uint32_t ms)                /* snow drifting down, faster with the tempo */
{
    int i;
    float t = (float)ms * 0.001f, v = 8.0f + (float)clamp(seq_bpm, 20, 300) * 0.12f;
    for (i = 0; i < 180; i++) {
        uint32_t h = st_hash((uint32_t)i * 2246822519u + 5u);
        float sp = v * (0.5f + (float)(h & 255u) / 255.0f), y = (float)((h >> 8) % ST_LAKE) + t * sp;
        int yy = (int)y % ST_LAKE, xx = (int)((float)((h >> 16) % ST_W) + 14.0f * st_sin(t * 0.7f + (float)(h & 31u)));
        int a = 90 + (int)(h >> 26) * 2, big = (h >> 29) == 0u;
        st_add(xx, yy, 0xdfeaf6, a), st_add(xx + 1, yy, 0xdfeaf6, a), st_add(xx, yy + 1, 0xdfeaf6, a), st_add(xx + 1, yy + 1, 0xdfeaf6, a);
        if (big) st_add(xx - 1, yy, 0xdfeaf6, a / 2), st_add(xx + 2, yy + 1, 0xdfeaf6, a / 2), st_add(xx, yy - 1, 0xdfeaf6, a / 2),
                 st_add(xx + 1, yy + 2, 0xdfeaf6, a / 2);
    }
}

static void st_moon(uint32_t ms)
{
    int on = yr_on(), x, y, cx = 902, cy = 64, r = 26;
    uint32_t c = on ? ST_SEASON[(uint32_t)clamp(yr_season, 0, 7)] : 0xd8e2ee;
    const char *nm = on ? YR_NAME[(uint32_t)clamp(yr_season, 0, YR_NS - 1)] : "POLAR NIGHT";
    (void)ms;
    for (y = -r * 3; y <= r * 3; y++)           /* its halo, then the disc */
        for (x = -r * 3; x <= r * 3; x++) {
            int d2 = x * x + y * y;
            if (d2 <= r * r) st_mix(cx + x, cy + y, st_scale(c, 0.85f + 0.15f * (float)(-x - y) / (float)(2 * r)), 255);
            else if (d2 < 9 * r * r) st_add(cx + x, cy + y, c, (int)(40.0f * (float)r * (float)r / (float)d2));
        }
    st_text(cx - r - 14 - 16 * (int)strlen(nm), cy - 8, nm, 2, c);
}

static void st_mountains(void)
{
    int x, y;
    for (x = 0; x < ST_W; x++) {
        float fx = (float)x;
        int far = ST_HZ - (int)(22.0f + 46.0f * st_noise(fx * 0.006f, 3u) + 16.0f * st_noise(fx * 0.03f, 4u));
        int near = ST_HZ - (int)(8.0f + 30.0f * st_noise(fx * 0.009f + 40.0f, 5u) + 8.0f * st_noise(fx * 0.05f, 6u));
        for (y = far; y < ST_HZ; y++) st_set(x, y, y < far + 2 ? 0x2a3650 : 0x0b1124);
        for (y = near; y < ST_HZ; y++) st_set(x, y, y < near + 2 ? 0x3c4a66 : 0x060a16);
    }
}

static void st_lake(uint32_t ms)                /* the sky and the mountains in the ice */
{
    int x, y;
    float t = (float)ms * 0.001f;
    for (y = ST_HZ; y < ST_LAKE; y++) {
        int d = y - ST_HZ, sy = ST_HZ - 1 - d * 2;   /* (foreshortened: the mirror twice as fast) */
        int off = (int)(3.0f * st_sin((float)y * 0.45f + t * 2.6f) * (float)(d + 8) / 60.0f);
        float k = 0.6f - 0.3f * (float)d / (float)(ST_LAKE - ST_HZ);
        if (sy < 0) sy = 0;
        for (x = 0; x < ST_W; x++) {
            int xx = x + off;
            const uint8_t *s = st_c + (sy * ST_W + (xx < 0 ? 0 : xx >= ST_W ? ST_W - 1 : xx)) * 3;
            uint32_t r = (uint32_t)(s[0] * k) + 6u, g = (uint32_t)(s[1] * k) + 12u, b = (uint32_t)(s[2] * k) + 22u;
            st_set(x, y, r << 16 | g << 8 | b);
        }
        if (d > 6 && (st_hash((uint32_t)y * 31u) & 7u) == 0u)   /* (cracks in the ice, a faint line here and there) */
            for (x = (int)(st_hash((uint32_t)y) % 600u); x < ST_W; x += 1 + (int)(st_hash((uint32_t)(x + y)) % 3u))
                if (st_noise((float)x * 0.02f, (uint32_t)y) > 0.62f) st_add(x, y, 0x6a8aa8, 40);
    }
}

/* ---- the card: the selected track's sound's picture, 3 x its cells */
#define ST_CX 584
#define ST_CY 92
static float st_card_l;
static int st_card_row;
static void st_card_line(uint32_t y, const uint16_t *col)
{
    int x, j, i, tear = 0, gy = ST_CY + (int)y * 3;
    if (st_card_l > 0.6f && (st_hash((uint32_t)y * 97u + (uint32_t)st_card_row) & 15u) == 0u)   /* screaming: torn */
        tear = (int)(st_hash((uint32_t)y + (uint32_t)st_card_row) % 25u) - 12;
    for (x = 0; x < (int)PIC_W; x++) {
        uint32_t c = st_565(col[x]);
        for (j = 0; j < 3; j++)
            for (i = 0; i < 3; i++) st_set(ST_CX + x * 3 + i + tear, gy + j, c);
    }
}
static void st_card(uint32_t ms)
{
    char nm[24];
    uint32_t acc = st_565(UI_ACCENTS[sel & 3u]);
    int w = (int)PIC_W * 3, h = (int)PIC_H * 3;
    st_card_l = st_lv[sel & 3u], st_card_row = (int)(ms / 70u);
    st_rect_a(ST_CX - 10, ST_CY - 10, w + 20, h + 20, 0x000000, 150);   /* (its shadow) */
    pic_anim_rows(pic_of_track(sel), ms, 0, st_card_line);
    st_rect(ST_CX - 3, ST_CY - 3, w + 6, 2, acc), st_rect(ST_CX - 3, ST_CY + h + 1, w + 6, 2, acc);
    st_rect(ST_CX - 3, ST_CY - 3, 2, h + 6, acc), st_rect(ST_CX + w + 1, ST_CY - 3, 2, h + 6, acc);
    if (st_card_l > 0.6f) {                     /* screaming: its ghosts, ice and plasma, pulled apart */
        int d = (int)((st_card_l - 0.6f) * 30.0f);
        st_rect_a(ST_CX - d, ST_CY, w, h, 0x22d3ee, 18), st_rect_a(ST_CX + d, ST_CY, w, h, 0xff1a8c, 18);
    }
    put_s(put_u(put_s(nm, "T"), sel + 1u), " ");
    trk_sname(sel, nm + strlen(nm));
    if (!nm[3]) put_s(nm + 3, sel_engine()->name);
    st_rect_a(ST_CX - 3, ST_CY + h + 6, 16 * (int)strlen(nm) + 16, 26, 0x000000, 170);
    st_text(ST_CX + 5, ST_CY + h + 11, nm, 2, acc);
}

/* ---- the title, the tracks */
static void st_title(uint32_t ms)
{
    static const uint32_t GH[3] = {0x22d3ee, 0xff1a8c, 0xa78bfa};
    int g, x = 28, y = 26;
    uint32_t tick = ms / 90u;
    for (g = 0; g < 3; g++) {                   /* now and then a ghost torn off it, as the web page's titles */
        uint32_t h = st_hash(tick * 3u + (uint32_t)g);
        if ((h & 7u) == 0u) {
            int band = (int)(h >> 8 & 31u), dx = (int)(h >> 16 & 7u) - 3;
            int j, i;
            for (j = band; j < band + 6 && j < 32; j++)
                for (i = 0; i < 13 * 32; i++) {
                    const char *s = "ARCTIC SCREAM";
                    const uint8_t *gl = PIXFONT[s[i / 32] - 32];
                    if (gl[j / 4] >> (7 - (i % 32) / 4) & 1u) st_add(x + i + dx * 3, y + j, GH[g], 200);
                }
        }
    }
    st_text(x, y, "ARCTIC SCREAM", 4, 0xeaf6fa);
    for (g = 0; g < 4; g++) st_rect(x + g * 106, y + 42, 98, 3, st_565(UI_ACCENTS[g]));
    {
        char b[32];
        int n = 0;
        if (dm_shown >= 0) {
            const char *q;
            for (q = DEMOS[dm_shown].id; *q && n < 4; ) b[n++] = *q++;
            b[n++] = ' ';
            for (q = DEMOS[dm_shown].name; *q && n < 28; ) b[n++] = *q++;
            b[n] = 0;
            st_text(x, y + 58, b, 2, 0xeaf6fa);
        } else
            st_text(x, y + 58, "THE POLAR NIGHT", 2, 0x7fa8b8);
        put_s(put_u(b, (uint32_t)seq_bpm), " BPM");
        n = st_text(x, y + 82, b, 2, 0x7fa8b8);
        if (playing) st_text(x + n + 20, y + 82, "PLAYING", 2, 0x3fb950);
        if (rec || song_rec) st_text(x + n + 150, y + 82, "REC", 2, 0xf85149);
    }
}
static void st_tracks(void)                     /* on the ice: T1..T4, their sounds, their levels */
{
    int t;
    for (t = 0; t < (int)NT; t++) {
        char nm[24];
        int x = 28 + (t & 1) * 470, y = ST_HZ + 30 + (t >> 1) * 44;
        float L = st_lv[t];
        uint32_t c = st_565(UI_ACCENTS[t]), on = trk[t].level > 0 && !(trk_quiet >> t & 1u);
        put_s(put_u(put_s(nm, "T"), (uint32_t)t + 1u), " ");
        trk_sname((uint32_t)t, nm + 3);
        if (!nm[3]) put_s(nm + 3, ENGINES[(uint32_t)clamp(trk[t].eng, 0, NENG - 1)]->name);
        nm[22] = 0;
        if ((uint32_t)t == sel) st_rect(x - 10, y, 4, 18, c);
        st_text(x, y, nm, 2, on ? ((uint32_t)t == sel ? 0xeaf6fa : st_scale(c, 0.9f)) : 0x45606e);
        st_rect(x, y + 22, 420, 2, 0x16222e);
        st_rect(x, y + 22, (int)(420.0f * L), 2, c);
        st_rect_a(x, y + 24, (int)(420.0f * L), 6, c, 40);   /* (its light in the ice) */
    }
}

/* ---- the shelf: the pads as blocks of ice, lit as the VIBE's pads */
#define ST_TW 112
#define ST_TH 40
#define ST_TX ((ST_W - (8 * ST_TW + 7 * 6)) / 2)
static void st_tile_xy(int p, int *x, int *y) { *x = ST_TX + (p % 8) * (ST_TW + 6), *y = ST_LAKE + 9 + (p / 8) * (ST_TH + 6); }
static void st_shelf(void)
{
    static const char *const LAB[8] = {"T1", "T2", "T3", "T4", "A", "B", "C", "D"};
    int p, x, y, i;
    for (y = ST_LAKE; y < ST_H; y++)
        for (x = 0; x < ST_W; x++) st_set(x, y, y == ST_LAKE ? 0x5a7a96 : 0x0a1420);
    for (p = 0; p < 16; p++) {
        uint32_t c;
        float l;
        const char *lab = p < 8 ? LAB[p] : VP_MOOD[p - 8];
        if (vp_btn & ~(1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) {   /* (a button held: the FM-1's keys under it) */
            int k = p < 8 ? WK[p] : (vp_btn >> BTN_FX & 1u) ? BK[p - 8] : WK2[p - 8];
            c = 0xffffff, l = (vb_led_key >> k & 1u) ? 0.9f : 0.12f, lab = "";
        } else if (p < 4) c = VP_TRK[p], l = (uint32_t)p == sel ? 1.0f : 0.25f;
        else if (p < 8) c = 0x22d3ee, l = slot_used((uint32_t)p - 4u) ? (pj_slot == p - 4 ? 0.9f : 0.4f) : 0.05f;
        else c = VP_TRK[sel & 3u], l = 0.15f + 0.05f * (float)(p - 8);
        if (st_down == p) l = 1.0f;
        st_tile_xy(p, &x, &y);
        for (i = 1; i < 10 && l > 0.5f; i++) { /* lit: its light around it and up the ice */
            int a = (int)(70.0f * l / (float)i), j;
            for (j = 0; j < ST_TW; j++) st_add(x + j, y - i, c, a), st_add(x + j, y + ST_TH - 1 + i, c, a / 2);
            for (j = 0; j < ST_TH; j++) st_add(x - i, y + j, c, a / 2), st_add(x + ST_TW - 1 + i, y + j, c, a / 2);
        }
        if (l > 0.5f && p < 8)
            for (i = 10; i < 70; i++)
                for (int j = 10; j < ST_TW - 10; j += 2) st_add(x + j, y - i, c, (int)(70.0f * l * (1.0f - (float)i / 70.0f)));
        st_rect(x, y, ST_TW, ST_TH, 0x112233);
        for (i = 0; i < ST_TH; i++) st_rect_a(x, y + i, ST_TW, 1, 0xcfe6f5, 22 - i / 2);   /* (the ice: lighter at its top) */
        st_rect_a(x, y, ST_TW, ST_TH, c, (int)(30.0f + 170.0f * l));
        st_rect(x, y, ST_TW, 2, st_scale(0xcfe6f5, 0.3f + 0.6f * l));   /* (the ice's top edge catches the light) */
        for (i = 0; i < 3; i++)                 /* (a crack in each block) */
            st_add(x + 14 + (int)(st_hash((uint32_t)(p * 3 + i)) % 80u), y + 6 + i * 9, 0xcfe6f5, 30);
        st_text(x + 8, y + ST_TH - 14, lab, 1, l > 0.5f ? 0x060410 : 0xcfe6f5);
    }
}

static void st_event(const vibe_event *e)       /* (control) the pointer on the stage: a block pressed is a pad */
{
    int x = VIBE_STAGE_X(e), y = VIBE_STAGE_Y(e), p, tx, ty, hit = -1;
    vibe_event pe;
    if (e->id == VIBE_STAGE_DOWN) {
        for (p = 0; p < 16; p++) {
            st_tile_xy(p, &tx, &ty);
            if (x >= tx && x < tx + ST_TW && y >= ty && y < ty + ST_TH) hit = p;
        }
        if (hit < 0) return;
        memset(&pe, 0, sizeof pe);
        pe.type = VIBE_EV_PAD, pe.id = (uint8_t)hit, pe.down = 1, pe.value = 0.8f;
        vb_eq_put(&pe), st_down = hit;
    } else if (e->id == VIBE_STAGE_UP && st_down >= 0) {
        memset(&pe, 0, sizeof pe);
        pe.type = VIBE_EV_PAD, pe.id = (uint8_t)st_down;
        vb_eq_put(&pe), st_down = -1;
    }
}

void vibe_on_stage(uint16_t *fb, int w, int h)
{
    uint32_t ms = fm1_ms, t, i;
    if (w != ST_W || h != ST_H) return;
    for (t = 0; t < NT; t++) {                  /* (the levels eased: a curtain rises fast, sinks slowly) */
        float L = st_level(t);
        st_lv[t] += (L - st_lv[t]) * (L > st_lv[t] ? 0.5f : 0.08f);
    }
    st_sky(ms);
    st_moon(ms);
    st_mountains();
    st_lake(ms);
    st_tracks();
    st_card(ms);
    st_title(ms);
    st_shelf();
    st_snow(ms);
    for (i = 0; i < (uint32_t)(ST_W * ST_H); i++) {
        const uint8_t *p = st_c + i * 3u;
        fb[i] = (uint16_t)((p[0] >> 3) << 11 | (p[1] >> 2) << 5 | p[2] >> 3);
    }
}
