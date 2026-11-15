/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* What the FM-1 keeps in its flash, the host keeps (vibe_store_*: in the DAW's project, per instance, in the plugin; on
 * the VIBE's card; in the browser's IndexedDB). The engine writes its fake flash (port/fm1_vibe.c) as on the FM-1;
 * what it wrote goes to the host a moment later, or at once when the host asks (VIBE_EV_SAVE: a DAW saving its project):
 *   save.bin       the six save sectors (store.c: the working project, the sounds, slots A..D, the undo)
 *   settings.bin   SYSTEM's settings, as many bytes as were saved
 *   sample-K.bin   sample slot K (1..4): its header and its data, as long as the header says (an empty slot: none)
 * Never from vibe_on_audio: the control flow copies what the sound's flow wrote (fm1_vibe.c: vb_wseq, vb_gen), whole,
 * and keeps the copy. */

static uint32_t vs_t;                           /* fm1_ms of the last write to the host */
static uint32_t vs_done[VG_N];                  /* each part's count as the host last kept it */
static uint8_t vs_img[SMP_SLOT_SIZE];           /* a copy, as it was between two writes */

static void vs_load(void)                       /* (vibe_on_init, before app_start) what the host kept, back */
{
    char name[16] = "sample-1.bin";
    uint32_t k;
    int n;
    (void)save_flash(0);                        /* (its first call erases it: before it is filled) */
    if (vibe_store_size("save.bin") == (int)sizeof fake_save) vibe_store_read("save.bin", fake_save, (int)sizeof fake_save);
    n = vibe_store_read("settings.bin", vb_set, (int)sizeof vb_set);
    if (n > 0) vb_set_len = n;
    for (k = 0; k < SMP_SLOTS; k++) {
        name[7] = (char)('1' + k);
        n = vibe_store_size(name);
        if (n > (int)SMP_DATA_OFF && n <= (int)SMP_SLOT_SIZE) vibe_store_read(name, fake_flash + k * SMP_SLOT_SIZE, n);
    }
    vb_wclose();
    for (k = 0; k < VG_N; k++) vs_done[k] = vb_gen[k];
}

static int vs_copy(uint32_t part, int *len)    /* part's bytes into vs_img, whole (1), or not now (0: a write going on) */
{
    uint32_t q = vb_wseq, n;
    if (q & 1u) return 0;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    if (part == VG_SAVE) n = sizeof fake_save, memcpy(vs_img, fake_save, n);
    else if (part == VG_SET) n = vb_set_len > 0 ? (uint32_t)vb_set_len : 0u, memcpy(vs_img, vb_set, n);
    else {
        const uint8_t *h = fake_flash + (part - VG_SLOT0) * SMP_SLOT_SIZE;
        uint32_t d = rd32(h + 16);
        n = rd32(h) == SMP_MAGIC && d > 0u && d <= SMP_MAX_DATA ? SMP_DATA_OFF + d : 0u;   /* (a slot with a header: as
                                                 * long as it says; empty, or being written: none) */
        memcpy(vs_img, h, n);
    }
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    *len = (int)n;
    return vb_wseq == q;
}

static void vs_flush(int now)                   /* what changed, to the host: now, or 1 s after the last time */
{
    char name[16] = "sample-1.bin";
    uint32_t k, g;
    int n, tries;
    for (k = 0; k < VG_N && vb_gen[k] == vs_done[k]; k++)
        ;
    if (k == VG_N || (!now && fm1_ms - vs_t < 1000u)) return;
    vs_t = fm1_ms;
    for (k = 0; k < VG_N; k++) {
        if ((g = vb_gen[k]) == vs_done[k]) continue;
        for (tries = now ? (k >= VG_SLOT0 ? 4 : 64) : 1; tries > 0 && !vs_copy(k, &n); tries--)   /* (a DAW saving: at once, between the
                                                 * sound's writes; else again next time) */
            ;
        if (!tries) continue;
        if (k == VG_SAVE) vibe_store_write("save.bin", vs_img, n);
        else if (k == VG_SET) { if (n > 0) vibe_store_write("settings.bin", vs_img, n); }
        else {
            name[7] = (char)('1' + k - VG_SLOT0);
            if (n > 0) vibe_store_write(name, vs_img, n);
            else vibe_store_delete(name);
        }
        vs_done[k] = g;
    }
}
