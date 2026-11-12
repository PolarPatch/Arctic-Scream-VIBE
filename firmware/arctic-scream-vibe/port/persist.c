/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* What the FM-1 keeps in its flash, the host keeps (vibe_store_*: in the DAW's project, per instance, in the plugin; on
 * the VIBE's card; in the browser's IndexedDB). The engine writes its fake flash (port/fm1_vibe.c) as on the FM-1;
 * what it wrote goes to the host a moment later, or at once when the host asks (VIBE_EV_SAVE: a DAW saving its project):
 *   save.bin       the six save sectors (store.c: the working project, the sounds, slots A..D, the undo)
 *   settings.bin   SYSTEM's settings, as many bytes as were saved
 *   sample-K.bin   sample slot K (1..4): its header and its data, as long as the header says (an empty slot: none)
 * Never from vibe_on_audio. */

static uint32_t vs_t;                           /* fm1_ms of the last write to the host */

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
    vb_dirty = 0;
}

static void vs_flush(int now)                   /* what changed, to the host: now, or 1 s after the last time */
{
    char name[16] = "sample-1.bin";
    uint32_t k;
    if (!vb_dirty || (!now && fm1_ms - vs_t < 1000u)) return;
    vs_t = fm1_ms;
    if (vb_dirty & VD_SAVE) vibe_store_write("save.bin", fake_save, (int)sizeof fake_save);
    if ((vb_dirty & VD_SET) && vb_set_len > 0) vibe_store_write("settings.bin", vb_set, vb_set_len);
    for (k = 0; k < SMP_SLOTS; k++)
        if (vb_dirty & VD_SLOT0 << k) {
            const uint8_t *h = fake_flash + k * SMP_SLOT_SIZE;
            uint32_t len = rd32(h + 16);
            name[7] = (char)('1' + k);
            if (rd32(h) == SMP_MAGIC && len > 0u && len <= SMP_MAX_DATA)   /* (a slot with a header: as long as it says) */
                vibe_store_write(name, h, (int)(SMP_DATA_OFF + len));
            else
                vibe_store_delete(name);        /* (empty, or being written: none until its header is) */
        }
    vb_dirty = 0;
}
