/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch
 * Arctic Scream on VIBE FM: a fake FM-1 under the firmware, as web/fm1_web.c is one in a browser (arctic-drones), whose
 * design it keeps: that file follows Felucca's browser build, web/emu/felucca_web.c of hugelton/Felucca (Copyright (C)
 * 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0), which follows X0X's, web/emu/x0x_web.c of
 * charlesvestal/fm1-x0x (GPL-3.0). */
/* platform.c includes this file (FM1_PLATFORM_FILE, set by main.c) instead of the hardware; the app (engine/grimm.c)
 * runs unchanged on it. VIBE FM has no loop that never returns: main.c calls app_start once and app_step for each
 * millisecond of sound the host asks for. One app_step is one call of platform_poll(): one millisecond of the device,
 * the audio halves due by then (app_audio) into a ring at 44.1 kHz, which main.c resamples to the host's rate.
 * Not here: USB (MIDI out goes nowhere yet), updates, the rescue, crashes. */
#include <stdint.h>
#include <string.h>

#ifndef APP_NAME
#define APP_NAME "DINGHY"
#endif
#ifndef APP_VER
#define APP_VER "0.0"
#endif
#define FS 44100u
#define HALF_FRAMES 256u


static volatile uint32_t fm1_ms;
#define FM1_TICKS_PER_US 24u
static uint32_t vb_ticks;                       /* 24 MHz, as the FM-1's TIMER4: here counted from the frames rendered */
static uint32_t fm1_ticks(void) { return vb_ticks; }

/* ---- the panel: keys (bit k = key k, 0 = F3), buttons (bit = BTN_ id), encoders (ENC_ id), LEDs (fm1_led_key ids:
 * 0..13 the buttons, 14 + k the keys) */
static struct { uint32_t notes, buttons; } fm1_in;
static uint32_t vb_pressed, vb_released, vb_notes_pressed, vb_led_btn, vb_led_key;
static uint32_t vb_key_mask, vb_key_tap;     /* the keys held; pressed and let go since the last poll */
static int32_t vb_enc_steps[7];
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = vb_pressed;
    if (released) *released = vb_released;
    vb_pressed = vb_released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void) { uint32_t n = vb_notes_pressed; vb_notes_pressed = 0; return n; }
static int32_t fm1_enc_take(uint32_t e) { int32_t s = vb_enc_steps[e % 7u]; vb_enc_steps[e % 7u] = 0; return s; }
static void fm1_led_key(uint32_t id, int on)
{
    uint32_t *m = id < 14u ? &vb_led_btn : &vb_led_key, bit = id < 14u ? id : id - 14u;
    if (id >= 14u + 27u) return;
    if (on) *m |= 1u << bit;
    else *m &= ~(1u << bit);
}

static void fm1_led_glow(uint32_t id, int on) { (void)id; (void)on; }   /* (the glow: not shown here) */

/* ---- the screen: 240 x 240 RGB565, as host.c keeps it (gfx.c sends blits byte-swapped, as the ST7789 wants) */
static uint16_t fb[240 * 240];
static uint32_t vb_draws;
static void lcd_sync(void) {}
static int vb_lit = 1;
static void lcd_light(int on) { vb_lit = on; }  /* (SCREEN OFF: the VIBE's picture dark) */
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = c;
    vb_draws++;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240u + x + i] = (uint16_t)(c >> 8 | c << 8);
            }
    vb_draws++;
}
#define CV_MAX (240u * 64u)
#include "../engine/gfx.c"                       /* (engine/, beside this folder) */

/* ---- settings: up to 256 bytes, kept in RAM (main.c: in the host's store later); as platform.c, a
 * load only succeeds with the length that was saved */
static uint8_t vb_set[256];
static int32_t vb_set_len = -1;
static uint32_t vb_set_writes;
static int settings_load(void *p, uint32_t n)
{
    if (vb_set_len < 0 || (uint32_t)vb_set_len != n || n > sizeof vb_set) return -1;
    memcpy(p, vb_set, n);
    return 0;
}
static int settings_save(const void *p, uint32_t n)
{
    if (n > sizeof vb_set) return -1;
    memcpy(vb_set, p, n);
    vb_set_len = (int32_t)n;
    vb_set_writes++;
    return 0;
}

/* ---- MIDI: in from the host (vb_midi), out nowhere yet; no MIDI clock here (platform.c's ring stays empty) */
static uint32_t rt_now(void) { return 0; }
static uint8_t mq[64][3];
static uint32_t mq_w, mq_r;
static int midi_get(uint8_t *st, uint8_t *d1, uint8_t *d2)
{
    if (mq_r == mq_w) return 0;
    *st = mq[mq_r % 64u][0]; *d1 = mq[mq_r % 64u][1]; *d2 = mq[mq_r % 64u][2];
    mq_r++;
    return 1;
}
static void midi_send(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static uint32_t midi_room(void) { return 64u; }
static int32_t batt_raw(void) { return -1; }   /* (no battery in a browser: no icon) */
static int usb_powered(void) { return 0; }
static uint32_t usb_drops(void) { return 0u; }   /* (no USB audio here) */
static uint32_t stk_main(void) { return 0u; }   /* (1.5.2: the stacks are measured on the FM-1 only) */
static uint32_t stk_irq(void) { return 0u; }
#define STK_MAIN_SZ 0u
#define STK_IRQ_SZ 0u
static int usb_recording(void) { return 0; }
static int usb_feeding(void) { return 0; }
static void usb_feed(const int32_t *x, uint32_t n) { (void)x; (void)n; }

/* Arctic Scream: the sample area (platform.c's, 288 KiB), here RAM, erased as a new FM-1's: at boot the firmware writes its four
 * factory samples in (samples.c), as on the device. (2.0, Arctic Scream's web extension) The slots are
 * longer than the FM-1's: 1 MiB each, ~95 s (the FM-1: 72 KiB, 6.6 s); INFO says so, and the pages cut to it */
#define SMP_SLOT_SIZE (1024u * 1024u)
#define SAMPLE_AREA_LEN (4u * SMP_SLOT_SIZE)
static uint8_t fake_flash[SAMPLE_AREA_LEN];
static const uint8_t *sample_flash(void) { return fake_flash; }
static int sample_flash_erase(uint32_t off)
{
    uint32_t i;
    if ((off & 0xFFFu) || off >= SAMPLE_AREA_LEN) return -8;
    for (i = 0; i < 4096u; i++) fake_flash[off + i] = 0xFF;
    return 0;
}
static int sample_flash_write(uint32_t off, const void *p, uint32_t n)   /* NOR: a write only clears bits */
{
    const uint8_t *s = p;
    uint32_t i;
    if (off >= SAMPLE_AREA_LEN || n > SAMPLE_AREA_LEN - off) return -8;
    for (i = 0; i < n; i++) fake_flash[off + i] &= s[i];
    return 0;
}
/* Arctic Scream: the save sectors (store.c), here RAM, erased at the start: nothing saved */
#define SAVE_NSECT 6u
static uint8_t fake_save[SAVE_NSECT][4096];
static const uint8_t *save_flash(uint32_t i)
{
    static int init;
    if (!init) { memset(fake_save, 0xFF, sizeof fake_save); init = 1; }
    return i < SAVE_NSECT ? fake_save[i] : 0;
}
static int save_flash_erase(uint32_t i)
{
    if (i >= SAVE_NSECT) return -8;
    (void)save_flash(0);
    memset(fake_save[i], 0xFF, 4096u);
    return 0;
}
static int save_flash_write(uint32_t i, uint32_t off, const void *p, uint32_t n)   /* NOR: a write only clears bits */
{
    const uint8_t *s = p;
    uint32_t k;
    if (i >= SAVE_NSECT || off >= 4096u || n > 4096u - off) return -8;
    (void)save_flash(0);
    for (k = 0; k < n; k++) fake_save[i][off + k] &= s[k];
    return 0;
}
/* SysEx: the editor page's frames in (as USB would bring them: 7D .. without F0 / F7), the replies out (F0 .. F7) */
static uint8_t vb_sx_in[640], vb_sx_out[4096];
static uint32_t vb_sx_in_n, vb_sx_in_ready, vb_sx_out_n;
static int sysex_get(const uint8_t **p, uint32_t *n)
{
    if (!vb_sx_in_ready) return 0;
    if (!vb_sx_in_n || vb_sx_in[0] != 0x7Du) { vb_sx_in_ready = 0; return 0; }
    *p = vb_sx_in, *n = vb_sx_in_n;
    return 1;
}
static void sysex_done(void) { vb_sx_in_ready = 0; }
static int ann_wire_send(const uint8_t *p, uint32_t n) { (void)p; (void)n; return -1; }   /* (1.5: no reader here yet) */
static int po_wire_send(const uint8_t *p, uint32_t n) { (void)p; (void)n; return -1; }    /* (1.5.2: PANEL OUT, none here) */
static int sysex_send(const uint8_t *p, uint32_t n)
{
    if (vb_sx_out_n + n > sizeof vb_sx_out) return -1;
    memcpy(vb_sx_out + vb_sx_out_n, p, n);
    vb_sx_out_n += n;
    return 0;
}


static void app_audio(int32_t *out, uint32_t n);

/* ---- the sound: a ring of stereo floats at 44.1 kHz, filled by platform_poll, drained by main.c */
#define RING 4096u
static float ring[RING * 2u];
static uint32_t ring_w, ring_r, vb_owed;
static int32_t vb_knob = 700, vb_knob_avg = 512 * 16, vb_master_q12 = 2048;   /* MASTER: 0..1023, as the ADC */
static int32_t fm1_master(void) { return vb_master_q12; }   /* (the app's output stage asks) */

static void platform_poll(void)                 /* one call = one millisecond */
{
    static int32_t blk[2u * HALF_FRAMES];
    uint32_t i, k10;
    fm1_in.notes = vb_key_mask | vb_key_tap;  /* a key tapped between two polls is down for this one */
    vb_key_tap = 0;
    fm1_ms++;
    vb_ticks += 24000u;
    vb_knob_avg += (vb_knob * 16 - vb_knob_avg) / 8;    /* platform.c: the knob, smoothed, squared */
    k10 = (uint32_t)(vb_knob_avg / 16);
    vb_master_q12 = (int32_t)((k10 * k10) >> 8);
    vb_owed += FS;                              /* the frames this millisecond is worth, in halves as the ISR renders */
    while (vb_owed >= 1000u * HALF_FRAMES) {
        vb_owed -= 1000u * HALF_FRAMES;
        memset(blk, 0, sizeof blk);
        app_audio(blk, HALF_FRAMES);
        for (i = 0; i < 2u * HALF_FRAMES; i++) {
            int32_t v = blk[i];                 /* (MASTER applied by the app, fm1_master) */
            v = v > 32767 ? 32767 : v < -32767 ? -32767 : v;
            ring[(ring_w * 2u + i) % (RING * 2u)] = (float)v / 65536.0f;   /* -6 dBFS, as the web player */
        }
        ring_w += HALF_FRAMES;
    }
}

/* ---- what main.c hands in: the panel, MIDI */
static void vb_keys(uint32_t mask)
{
    mask &= (1u << 27) - 1u;
    vb_notes_pressed |= mask & ~vb_key_mask;
    vb_key_tap |= mask & ~vb_key_mask;
    vb_key_mask = mask;
    fm1_in.notes = mask | vb_key_tap;
}
static void vb_buttons(uint32_t mask)
{
    mask &= (1u << 14) - 1u;
    vb_pressed |= mask & ~fm1_in.buttons;
    vb_released |= fm1_in.buttons & ~mask;
    fm1_in.buttons = mask;
}
static void vb_enc(uint32_t e, int32_t n) { if (e < 7u) vb_enc_steps[e] += n; }
static void vb_midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st < 0x80u || st >= 0xF0u || mq_w - mq_r >= 64u) return;
    mq[mq_w % 64u][0] = (uint8_t)st; mq[mq_w % 64u][1] = (uint8_t)(d1 & 127u); mq[mq_w % 64u][2] = (uint8_t)(d2 & 127u);
    mq_w++;
}
