/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* HOME on the VIBE: the FM-1's HOME (engine/ui.c), drawn again at the display's own 480 x 320, out to its edges: the
 * same header, night sky, four waves and knob slabs, in its pixfont (Press Start 2P's 8 px grid, SIL OFL 1.1) and its
 * palette, from what ui.c keeps (its header's texts ui_hb / ui_hl / ui_hr, its slabs ui_sl, its waves wv_d, its stars).
 * The slabs packed tight, so the four tracks have the room. */

static uint16_t *hm_fb;
static void hm_rect(int x, int y, int w, int h, uint16_t c)
{
    int i, j;
    if (x < 0) w += x, x = 0;
    if (y < 0) h += y, y = 0;
    if (x + w > VIBE_SCREEN_W) w = VIBE_SCREEN_W - x;
    if (y + h > VIBE_SCREEN_H) h = VIBE_SCREEN_H - y;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) hm_fb[(y + j) * VIBE_SCREEN_W + x + i] = c;
}
static int hm_text(int x, int y, const char *s, int sc, uint16_t c)   /* the pixfont, sc x its pixels; -> the width */
{
    int x0 = x;
    for (; *s; s++, x += 8 * sc) {
        const uint8_t *g = PIXFONT[*s >= 32 && *s < 127 ? *s - 32 : 0];
        int r, b;
        for (r = 0; r < 8; r++)
            for (b = 0; b < 8; b++)
                if (g[r] >> (7 - b) & 1u) hm_rect(x + b * sc, y + r * sc, sc, sc, c);
    }
    return x - x0;
}
static uint32_t hm_888(uint16_t c)              /* RGB565 -> 0xRRGGBB (for vg_line's glow) */
{
    uint32_t r = c >> 11, g = c >> 5 & 63u, b = c & 31u;
    return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}

static int hm_card_top, hm_card_bot;
static uint32_t hm_pic, hm_dim;                 /* (2.1) the picture hm_card_row draws; dimmed (SKY) */
static void hm_card_row(uint32_t y, const uint8_t *cells)   /* (2.1) a row of the sound's picture: 4 x 4 pixels a cell,
                                                 * 464 wide; its middle rows in the field, the rest cut */
{
    const uint16_t *pal = PIC_PAL[hm_pic % NPIC];
    int yy = (hm_card_top + hm_card_bot) / 2 - (int)PIC_H * 2 + (int)y * 4, x;
    if (yy + 4 <= hm_card_top || yy >= hm_card_bot) return;
    for (x = 0; x < (int)PIC_W; x++) {
        int y0 = yy < hm_card_top ? hm_card_top : yy, y1 = yy + 4 > hm_card_bot ? hm_card_bot : yy + 4;
        hm_rect(8 + x * 4, y0, 4, y1 - y0, hm_dim ? ui_mix(UI_K, pal[cells[x]], 7, 16) : pal[cells[x]]);
    }
}

#define HM_HEAD 26                              /* the header's height */
#define HM_SLAB 52                              /* the slabs' height (the FM-1's 72, packed) */
static void vp_home_full(uint16_t *vfb)
{
    int t, i, k, top = HM_HEAD + 4, bot = VIBE_SCREEN_H - HM_SLAB - 3, lane = (bot - top) / 4;
    hm_fb = vfb;
    hm_rect(0, 0, VIBE_SCREEN_W, VIBE_SCREEN_H, UI_K);
    if (!sk_ok) sk_init();
    if (pc_mode == PC_SKY) {                    /* (2.1) SKY: the selected track's sound's picture, dimmed */
        hm_card_top = top, hm_card_bot = bot, hm_pic = pic_of_track(sel), hm_dim = 1;
        pic_rows(hm_pic, hm_card_row);
        hm_dim = 0;
    } else
    for (i = 0; i < (int)NSTAR; i++) {          /* the sky: ui.c's stars, spread over the whole width (twice: the left
                                                 * half and, mirrored, the right) */
        uint16_t c = sk_col((uint32_t)i);
        int x = sk_x[i] * 3, y = top + sk_y[i] * (bot - top - 6) / 44;
        hm_rect(x, y, 3, 3, c);
        hm_rect(VIBE_SCREEN_W - 3 - x, top + (bot - top - 6) - (y - top), 3, 3, sk_col((uint32_t)(NSTAR - 1 - i)));
    }
    hm_rect(0, 0, 76, HM_HEAD - 2, ui_hd);      /* the header: its button's block, the bar with the track and tempo */
    hm_text(6, 4, ui_hb, 2, UI_K);
    hm_rect(78, 0, VIBE_SCREEN_W - 78, HM_HEAD - 2, UI_D0);
    if (dm_shown >= 0 && fm1_ms >= hd_until) { /* a demo: its name in the header, what it is under it */
        char h[24];
        uint32_t n = 0;
        const char *q;
        for (q = DEMOS[dm_shown].id; *q && n < 4u; ) h[n++] = *q++;
        h[n++] = ' ';
        for (q = DEMOS[dm_shown].name; *q && n < 23u; ) h[n++] = *q++;
        h[n] = 0;
        hm_text(86, 4, h, 2, UI_W);
        hm_text(24, top + 4, DEMOS[dm_shown].sub, 1, UI_M0);
    } else
        hm_text(86, 4, ui_hl, 2, UI_W);
    if (fm1_ms - vp_data_t < 1500u)             /* DATA just turned: it is PRESETS (its print says DATA: the host's) */
        hm_text(VIBE_SCREEN_W - 6 - 16 * 7, 4, "PRESETS", 2, ui_hd);
    else
        hm_text(VIBE_SCREEN_W - 6 - 16 * (int)strlen(ui_hr), 4, ui_hr, 2, UI_L0);
    {                                           /* the field's corners */
        int x0 = 4, y0 = top, x1 = VIBE_SCREEN_W - 5, y1 = bot;
        hm_rect(x0, y0, 14, 3, ui_acc), hm_rect(x0, y0, 3, 14, ui_acc);
        hm_rect(x1 - 13, y1 - 2, 14, 3, ui_acc), hm_rect(x1 - 2, y1 - 13, 3, 14, ui_acc);
    }
    if (pic_card_on()) {                        /* (2.1) PRESETS turned: the sound's picture, big, its name on it */
        hm_card_top = top, hm_card_bot = bot, hm_pic = pc_id, hm_dim = 0;
        pic_rows(pc_id, hm_card_row);
        hm_rect(10, top + 6, 16 * (int)strlen(pc_name) + 12, 22, UI_K);
        hm_text(16, top + 9, pc_name, 2, UI_W);
        t = (int)NT;                            /* (no waves over it) */
    }
    for (t = pic_card_on() ? (int)NT : 0; t < (int)NT; t++) {                  /* the four tracks: the number, the wave across the width */
        int c = top + lane / 2 + t * lane, flat = 1;
        uint16_t col = wv_col((uint32_t)t), dim = ui_mix(UI_K, col, 1, 3);
        float wx[SC_N], wy[SC_N];
        char lab[2] = {(char)('1' + t), 0};
        if ((uint32_t)t == sel) hm_rect(12, c - 10, 3, 20, col);
        hm_text(20, c - 7, lab, 2, (uint32_t)t == sel ? UI_W : UI_M);
        for (i = 0; i < (int)SC_N; i++) {
            int32_t v = wv_d[t][i];
            if (v) flat = 0;
            wx[i] = 44.0f + (float)i * ((float)(VIBE_SCREEN_W - 56) / (float)(SC_N - 1u));
            wy[i] = (float)c - (float)(lane / 2 - 2) * (float)v / (float)((v < 0 ? -v : v) + WV_KNEE);
        }
        if (flat)
            for (k = 44; k < VIBE_SCREEN_W - 12; k += 8) hm_rect(k, c, 4, 2, UI_D);
        else {
            vg_begin(vfb);
            for (i = 1; i < (int)SC_N; i++) vg_line(wx[i - 1], wy[i - 1], wx[i], wy[i], 5.0f, hm_888(dim));
            for (i = 1; i < (int)SC_N; i++) vg_line(wx[i - 1], wy[i - 1], wx[i], wy[i], 2.0f, hm_888(col));
        }
    }
    for (k = 0; k < 4; k++) {                   /* the slabs: the knob's name, its 8 segments, its value, packed */
        const slab_t *S = &ui_sl[k];
        int x = 3 + k * 119, y = VIBE_SCREEN_H - HM_SLAB, j;
        uint16_t fc = S->acc ? ui_acc : UI_KC[k];
        hm_rect(x, y, 116, HM_SLAB, UI_D0);
        hm_rect(x, y, 116, 2, UI_KC[k]);
        hm_text(x + 4, y + (strlen(S->lab) > 6 ? 9 : 5), S->lab, strlen(S->lab) > 6 ? 1 : 2, UI_L0);
        for (j = 0; j < 8; j++) hm_rect(x + 4 + j * 14, y + 23, 12, 7, j < S->fill ? fc : UI_K);
        hm_text(x + 4, y + 33, S->val, 2, S->dim ? UI_M0 : fc);
    }
}
