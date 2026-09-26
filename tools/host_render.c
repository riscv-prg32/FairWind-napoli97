/* Host renderer: runs the real game.c against a 320x200 framebuffer that
   resolves colours through the palette the cartridge loads, drives the player's
   yacht with the same AI helm the rivals use, and writes PPM frames.
   Built and run by tools/host_capture.py; not part of the cartridge. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "font8.h"

static uint16_t fb[200][320];
static uint32_t g_input;
/* The palette the cartridge loads; RGB565 colours resolve to an entry the
   way the firmware's prg32_gfx_index_for_rgb565 does. */
static uint16_t g_pal[256];
static uint16_t quant(uint16_t c) {
    static const uint16_t named[8] = {0x0000, 0xffff, 0xf800, 0x07e0, 0x001f, 0xffe0, 0x07ff, 0xf81f};
    unsigned r = ((c >> 11) & 31u) * 5u / 31u, g = ((c >> 5) & 63u) * 5u / 63u, b = (c & 31u) * 5u / 31u, i;
    for (i = 0; i < 8; i++) if (c == named[i]) return g_pal[i];
    return g_pal[16u + r * 36u + g * 6u + b];
}

#include "../src/game.c"

uint32_t prg32_input_read(void) { return g_input; }
void prg32_band_set_game_info(const char *s) { (void)s; }
void prg32_audio_play_track(uint16_t t) { (void)t; }
static void raw_rect(int x, int y, int w, int h, uint16_t c) {
    int i, j;
    for (j = y; j < y + h; j++) if (j >= 0 && j < 200) for (i = x; i < x + w; i++) if (i >= 0 && i < 320) fb[j][i] = c;
}
void prg32_gfx_rect(int x, int y, int w, int h, uint16_t c) { raw_rect(x, y, w, h, quant(c)); }
void prg32_gfx_pixel(int x, int y, uint16_t c) { prg32_gfx_rect(x, y, 1, 1, c); }
static uint16_t pal(uint8_t i) { return g_pal[i]; }
void prg32_palette_set(uint8_t i, uint16_t c) { g_pal[i] = c; }
void prg32_gfx_rect_indexed(int x, int y, int w, int h, uint8_t i) { raw_rect(x, y, w, h, pal(i)); }
void prg32_gfx_pixel_indexed(int x, int y, uint8_t i) { raw_rect(x, y, 1, 1, pal(i)); }
void prg32_gfx_clear_indexed(uint8_t i) { raw_rect(0, 0, 320, 200, pal(i)); }
uint32_t prg32_ticks_ms(void) { static uint32_t t; return t += 33; }
void prg32_gfx_clear(uint16_t c) { prg32_gfx_rect(0, 0, 320, 200, c); }
void prg32_gfx_present(void) {}
void prg32_gfx_text8(int x, int y, const char *s, uint16_t fg, uint16_t bg) {
    for (; *s; s++, x += 8) {
        unsigned ch = (unsigned char)*s; const uint8_t *g = g_font8[(ch < 32 || ch > 126 ? '?' : ch) - 32];
        for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) prg32_gfx_rect(x + c, y + r, 1, 1, (g[r] & (0x80 >> c)) ? fg : bg);
    }
}
void prg32_sprite_draw_bitplanes(int x, int y, const prg32_indexed_sprite_t *s, uint32_t frame_no) {
    unsigned rb = (s->width + 7u) / 8u, fsz = rb * s->height * s->bits_per_pixel;
    for (int yy = 0; yy < s->height; yy++) for (int xx = 0; xx < s->width; xx++) {
        unsigned v = 0;
        for (int p = 0; p < s->bits_per_pixel; p++) if (s->pixels[frame_no * fsz + (p * s->height + yy) * rb + xx / 8] & (0x80 >> (xx & 7))) v |= 1u << p;
        if ((int)v != s->transparent_index) prg32_gfx_rect(x + xx, y + yy, 1, 1, s->palette[v]);
    }
}
void prg32_multiplayer_init(void) {}
int prg32_multiplayer_available(void) { return 0; }
int prg32_multiplayer_join(const char *r, uint32_t o) { (void)r; (void)o; return -1; }
int prg32_multiplayer_leave(void) { return 0; }
void prg32_multiplayer_tick(void) {}
int prg32_multiplayer_set_local_state(int16_t x, int16_t y, uint16_t s, uint16_t f) { (void)x; (void)y; (void)s; (void)f; return 0; }
int prg32_multiplayer_set_input(uint32_t i) { (void)i; return 0; }
int prg32_multiplayer_get_peer_count(void) { return 0; }
int prg32_multiplayer_get_peer(int i, prg32_player_state_t *p) { (void)i; (void)p; return -1; }

static const char *outdir;
static int video;
static void shot(const char *name) {
    char path[512]; FILE *f; snprintf(path, sizeof path, "%s/%s.ppm", outdir, name);
    /* Stills are taken at arbitrary frames; on the console the throttled HUD
       would have refreshed within the last four frames, so refresh it now. */
    if (!video) hud_force = 1;
    fairwind_draw(); f = fopen(path, "wb"); if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n320 200\n255\n");
    for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) {
        uint16_t c = fb[y][x]; unsigned char px[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
        fwrite(px, 1, 3, f);
    }
    fclose(f); printf("%s\n", path);
}
/* Video mode (VIDEO=1): record highlight windows at the real 30 fps, each
   triggered once, as numbered PPM frames v00000.ppm... */
static int rec_left, vframes;
static void vshot(void) { char name[32]; snprintf(name, sizeof name, "v%05d", vframes++); shot(name); }
static void trigger(int *done, int cond, int frames) { if (video && !*done && cond && !rec_left) { *done = 1; rec_left = frames; } }
static void tap(uint32_t b) { g_input = b; fairwind_update(); g_input = 0; fairwind_update(); }
#include "autopilot.h"
int main(int argc, char **argv) {
    int course = argc > 2 ? atoi(argv[2]) : 0, venue = argc > 3 ? atoi(argv[3]) : 1, n = 0, last_top = 0, last_kite = 0, last_leg = 0, got_run = 0, got_top = 0, got_beat = 0;
    long f; char name[64];
    outdir = argc > 1 ? argv[1] : ".";
    video = getenv("VIDEO") != 0;
    fairwind_init(); shot("01-title");
    if (video) for (int i = 0; i < 75; i++) vshot();
    tap(PRG32_BTN_A); shot("02-mode");
    tap(PRG32_BTN_A); tap(PRG32_BTN_A); menu = 2; strategy = 3; shot("03-team-hq");
    menu = 5; tap(PRG32_BTN_A); race_no = venue; course_sel = course; shot("04-race-briefing");
    tap(PRG32_BTN_A);
    for (f = 0; screen == ST_RACE && f < 200000; f++) {
        g_input = getenv("IDLE") ? 0 : autopilot_input(); fairwind_update(); g_input = 0;
        if (screen != ST_RACE) break;
        if (video) {
            static int t1, t2, t3, t4, t5, t6;
            trigger(&t1, f == 40, 240);
            trigger(&t2, start_clock == 8, 270);
            trigger(&t3, race_clock == 150, 240);
            trigger(&t4, top_view_mode && boats[0].leg >= 1, 240);
            trigger(&t5, boats[0].kite_want == KITE_SPIN, 360);
            trigger(&t6, boats[0].leg >= course_len[course_sel] && boats[0].y < (FINISH_Y + 45) * 1000, 150);
            if (rec_left) { rec_left--; vshot(); }
            continue;
        }
        if (getenv("TRACE") && f % 150 == 0) {
            printf("f=%ld T-%d R%d twd=%d tws=%d |", f, start_clock, race_clock, (int)BAM2DEG((int16_t)twd), tws10);
            for (int i = 0; i < 4; i++) printf(" [%d] %d,%d h%d v%d L%d e%d r%d s%d sh%d ru%d k%d%s", i, boats[i].x / 1000, boats[i].y / 1000, (int)BAM2DEG(boats[i].heading), (int)((boats[i].v >> 8) * 100 / 5144), boats[i].leg, boats[i].entered_box, boats[i].start_ready, boats[i].started, boats[i].sheet, boats[i].rudder, boats[i].kite, boats[i].penalty ? "P" : "");
            printf("\n");
        }
        if (f == 30) shot("05-prestart");
        /* Store frames: first fully set kite in the chase view, first top
           view at a mark, and a beat with rivals ahead. */
        if (!got_run && boats[0].kite && boats[0].kite_prog == 65535 && !top_view_mode && boats[0].heel == 0 && race_clock % 20 == 0 && sim_acc < sim_ms()) { shot("store-run"); got_run = 1; }
        if (!got_top && top_view_mode && boats[0].started && boats[0].leg > 0) { got_top = 1; shot("store-top"); }
        if (!got_beat && boats[0].started && !top_view_mode && race_clock >= 150 && sim_acc < sim_ms()) { got_beat = 1; shot("store-beat"); }
        if (start_clock == 90 && sim_acc < sim_ms()) shot("06-entering-box");
        if (start_clock == 0 && sim_acc < sim_ms() && race_clock == 0) shot("07-start");
        if (top_view_mode != last_top && n < 60) { snprintf(name, sizeof name, "t%03d-top%d-leg%d", n++, top_view_mode, boats[0].leg); shot(name); }
        if (boats[0].kite_prog == 65535 && boats[0].kite != last_kite && n < 60) { snprintf(name, sizeof name, "k%03d-kite%d", n++, boats[0].kite); shot(name); }
        if (boats[0].leg != last_leg && n < 60) { snprintf(name, sizeof name, "m%03d-leg%d", n++, boats[0].leg); shot(name); }
        if (f % 900 == 450 && n < 60) { snprintf(name, sizeof name, "p%03d-clock%d", n++, race_clock); shot(name); }
        last_top = top_view_mode; last_kite = boats[0].kite_prog == 65535 ? boats[0].kite : last_kite; last_leg = boats[0].leg;
    }
    printf("race ended after %ld frames: clock=%d rank=%d finish=%d/%d/%d/%d legs=%d/%d/%d/%d\n", f, race_clock, player_rank,
           boats[0].finish_time, boats[1].finish_time, boats[2].finish_time, boats[3].finish_time, boats[0].leg, boats[1].leg, boats[2].leg, boats[3].leg);
    shot("99-result");
    if (video) for (int i = 0; i < 75; i++) vshot();
    return 0;
}
