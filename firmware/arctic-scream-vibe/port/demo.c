/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* The demos A1..A6 on K1..K6: the FM-1's web page's demos (port/demos.h, tools/gen_demos.py), played as that page plays
 * them (arctic-drones web/site.js): an action every 40 ms, an editor frame into the fake FM-1's SysEx in (the engine
 * takes it in samples_poll, as from USB), a panic, keys tapped 120 ms (latched). Then HOME says which one plays. */
#include "demos.h"

static int dm_cur = -1, dm_shown = -1;          /* the demo being loaded; the one HOME names (-1: none) */
static uint32_t dm_pos, dm_next, dm_up;         /* the next action; when; the tapped keys let go then (0: none held) */

static void dm_start(uint32_t d)
{
    if (d >= DM_N) return;
    dm_cur = dm_shown = (int)d, dm_pos = 0, dm_next = fm1_ms, dm_up = 0;
}

static void dm_step(void)                       /* (each millisecond of the engine: main.c, before app_step) */
{
    const uint8_t *a;
    uint32_t n;
    if (dm_cur < 0) return;
    if (dm_up) {
        if (fm1_ms < dm_up) return;
        vb_keys(vp_fkeys), dm_up = 0, dm_next = fm1_ms + 60u;   /* (the keys up: the pads' own again) */
        return;
    }
    if (fm1_ms < dm_next || vb_sx_in_ready) return;   /* (the last frame not taken yet) */
    a = DEMOS[dm_cur].act + dm_pos;
    switch (a[0]) {
    case DA_SX:
        n = a[1] | (uint32_t)a[2] << 8;
        memcpy(vb_sx_in, a + 3, n);
        vb_sx_in_n = n, vb_sx_out_n = 0, vb_sx_in_ready = 1;
        dm_pos += 3u + n;
        break;
    case DA_PANIC:
        for (n = 0; n < NT; n++) nq_put(NQ_RELEASE, n, 0);
        dm_pos += 1u;
        break;
    case DA_KEYS:
        vb_keys(vp_fkeys | ((uint32_t)a[1] | (uint32_t)a[2] << 8 | (uint32_t)a[3] << 16 | (uint32_t)a[4] << 24));
        dm_up = fm1_ms + 120u;
        dm_pos += 5u;
        break;
    default:                                    /* (DA_END: loaded, it plays on) */
        dm_cur = -1;
        return;
    }
    dm_next = fm1_ms + 40u;
}
