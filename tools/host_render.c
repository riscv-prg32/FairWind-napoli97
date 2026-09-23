/* Host renderer: runs the real game.c against a 320x200 framebuffer that
   quantises colours like the ILI9341's 6x6x6 palette, drives the player's
   yacht with the same AI helm the rivals use, and writes PPM frames.
   Built and run by tools/host_capture.py; not part of the cartridge. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "font8.h"

static uint16_t fb[200][320];
static uint32_t g_input;
static uint16_t quant(uint16_t c) {
    unsigned r = ((c >> 11) & 31u) * 5u / 31u, g = ((c >> 5) & 63u) * 5u / 63u, b = (c & 31u) * 5u / 31u;
    if (c == 0xffff || c == 0xf800 || c == 0x07e0 || c == 0x001f || c == 0xffe0 || c == 0x07ff || c == 0xf81f) return c;
    return (uint16_t)(((r * 31u / 5u) << 11) | ((g * 63u / 5u) << 5) | (b * 31u / 5u));
}

#include "../src/game.c"

uint32_t prg32_input_read(void) { return g_input; }
void prg32_band_set_game_info(const char *s) { (void)s; }
void prg32_audio_play_track(uint16_t t) { (void)t; }
void prg32_gfx_rect(int x, int y, int w, int h, uint16_t c) {
    int i, j; c = quant(c);
    for (j = y; j < y + h; j++) if (j >= 0 && j < 200) for (i = x; i < x + w; i++) if (i >= 0 && i < 320) fb[j][i] = c;
}
void prg32_gfx_pixel(int x, int y, uint16_t c) { prg32_gfx_rect(x, y, 1, 1, c); }
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
static void shot(const char *name) {
    char path[512]; FILE *f; snprintf(path, sizeof path, "%s/%s.ppm", outdir, name);
    fairwind_draw(); f = fopen(path, "wb"); if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n320 200\n255\n");
    for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) {
        uint16_t c = fb[y][x]; unsigned char px[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
        fwrite(px, 1, 3, f);
    }
    fclose(f); printf("%s\n", path);
}
static void tap(uint32_t b) { g_input = b; fairwind_update(); g_input = 0; fairwind_update(); }
/* Autopilot: let helm_ai decide on a shadow copy, then press the buttons a
   human would to follow it. */
static uint32_t autopilot(void) {
    static boat_t shadow; static uint32_t kite_hold; boat_t s = boats[0]; uint32_t in = 0;
    s.ai_clock = shadow.ai_clock; s.ai_tack = shadow.ai_tack; s.ai_last_tack = shadow.ai_last_tack;
    s.team = boats[0].team; s.rudder = 0; helm_ai(&s); shadow = s;
    if (boats[0].penalty && !boats[0].serving) return PRG32_BTN_A | PRG32_BTN_B;
    if (s.rudder <= -10) in |= PRG32_BTN_LEFT; else if (s.rudder >= 10) in |= PRG32_BTN_RIGHT;
    if (s.sheet > boats[0].sheet + 1) in |= PRG32_BTN_UP; else if (s.sheet + 1 < boats[0].sheet) in |= PRG32_BTN_DOWN;
    if (kite_hold) { kite_hold--; return in; }
    if (s.kite_want != boats[0].kite_want) {
        uint32_t k = (s.kite_want == KITE_SPIN || (s.kite_want == KITE_NONE && boats[0].kite_want == KITE_SPIN)) ? PRG32_BTN_A : PRG32_BTN_B;
        g_input = in | k; fairwind_update(); kite_hold = 2;
    }
    return in;
}
int main(int argc, char **argv) {
    int course = argc > 2 ? atoi(argv[2]) : 0, venue = argc > 3 ? atoi(argv[3]) : 1, n = 0, last_top = 0, last_kite = 0, last_leg = 0, got_run = 0, got_top = 0, got_beat = 0;
    long f; char name[64];
    outdir = argc > 1 ? argv[1] : ".";
    fairwind_init(); shot("01-title");
    tap(PRG32_BTN_A); shot("02-mode");
    tap(PRG32_BTN_A); tap(PRG32_BTN_A); menu = 2; strategy = 3; shot("03-team-hq");
    menu = 5; tap(PRG32_BTN_A); race_no = venue; course_sel = course; shot("04-race-briefing");
    tap(PRG32_BTN_A);
    for (f = 0; screen == ST_RACE && f < 200000; f++) {
        g_input = getenv("IDLE") ? 0 : autopilot(); fairwind_update(); g_input = 0;
        if (screen != ST_RACE) break;
        if (getenv("TRACE") && f % 150 == 0) {
            printf("f=%ld T-%d R%d twd=%d tws=%d |", f, start_clock, race_clock, (int)BAM2DEG((int16_t)twd), tws10);
            for (int i = 0; i < 4; i++) printf(" [%d] %d,%d h%d v%d L%d e%d r%d s%d sh%d ru%d k%d%s", i, boats[i].x / 1000, boats[i].y / 1000, (int)BAM2DEG(boats[i].heading), (int)((boats[i].v >> 8) * 100 / 5144), boats[i].leg, boats[i].entered_box, boats[i].start_ready, boats[i].started, boats[i].sheet, boats[i].rudder, boats[i].kite, boats[i].penalty ? "P" : "");
            printf("\n");
        }
        if (f == 30) shot("05-prestart");
        /* Store frames: first fully set kite in the chase view, first top
           view at a mark, and a beat with rivals ahead. */
        if (!got_run && boats[0].kite && boats[0].kite_prog == 65535 && !top_view_mode && boats[0].heel == 0 && race_clock % 20 == 0 && race_frames == 0) { shot("store-run"); got_run = 1; }
        if (!got_top && top_view_mode && boats[0].started && boats[0].leg > 0) { got_top = 1; shot("store-top"); }
        if (!got_beat && boats[0].started && !top_view_mode && race_clock >= 150 && race_frames == 0) { got_beat = 1; shot("store-beat"); }
        if (start_clock == 90 && race_frames == 0) shot("06-entering-box");
        if (start_clock == 0 && race_frames == 0 && race_clock == 0) shot("07-start");
        if (top_view_mode != last_top && n < 60) { snprintf(name, sizeof name, "t%03d-top%d-leg%d", n++, top_view_mode, boats[0].leg); shot(name); }
        if (boats[0].kite_prog == 65535 && boats[0].kite != last_kite && n < 60) { snprintf(name, sizeof name, "k%03d-kite%d", n++, boats[0].kite); shot(name); }
        if (boats[0].leg != last_leg && n < 60) { snprintf(name, sizeof name, "m%03d-leg%d", n++, boats[0].leg); shot(name); }
        if (f % 900 == 450 && n < 60) { snprintf(name, sizeof name, "p%03d-clock%d", n++, race_clock); shot(name); }
        last_top = top_view_mode; last_kite = boats[0].kite_prog == 65535 ? boats[0].kite : last_kite; last_leg = boats[0].leg;
    }
    printf("race ended after %ld frames: clock=%d rank=%d finish=%d/%d/%d/%d legs=%d/%d/%d/%d\n", f, race_clock, player_rank,
           boats[0].finish_time, boats[1].finish_time, boats[2].finish_time, boats[3].finish_time, boats[0].leg, boats[1].leg, boats[2].leg, boats[3].leg);
    shot("99-result");
    return 0;
}
