/* Profiling cartridge: the real game with the player's yacht on autopilot,
   timing fairwind_update and fairwind_draw with the firmware's microsecond
   clock and reporting on the serial console. Under QEMU with -icount the
   microseconds are thousands of executed RV32 instructions. Built by
   tools/profile/qemu_profile.py; never shipped. */
#define prg32_input_read profile_input_read
#ifdef FAIRWIND_NULL_GFX
/* Null graphics: count what the game asks the firmware to draw, draw nothing,
   so the timers measure the cartridge's own work only. */
#include "prg32.h"
static uint32_t n_rect, n_rect_px, n_rows, n_pix, n_chars, n_rgb;
#define prg32_gfx_rect_indexed(x, y, w, h, c) null_rect((x), (y), (w), (h))
#define prg32_gfx_pixel_indexed(x, y, c) (n_pix++)
#define prg32_gfx_text8(x, y, s, f, b) null_text(s)
#define prg32_gfx_rect(x, y, w, h, c) (n_rgb++)
static void null_rect(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; } if (x + w > 320) w = 320 - x; if (y + h > 200) h = 200 - y;
    if (w > 0 && h > 0) { n_rect++; n_rows += (uint32_t)h; n_rect_px += (uint32_t)(w * h); }
}
static void null_text(const char *s) { while (*s++) n_chars++; }
#endif
#define fairwind_update game_update
#define fairwind_draw game_draw
#include "../../src/game.c"
#undef fairwind_update
#undef fairwind_draw
#include "../autopilot.h"
uint64_t prg32_perf_now_us(void);

static uint32_t prof_frame, prof_n, prof_upd, prof_drw, prof_upd_max, prof_drw_max, prof_top;
uint32_t profile_input_read(void) {
    prof_frame++;
    return screen == ST_RACE ? autopilot_input() : autopilot_menus(prof_frame);
}
static void put_dec(uint32_t n) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = (char)('0' + n % 10u); n /= 10u; } while (n && i); prg32_console_write(&b[i]); }
static void put_kv(const char *k, uint32_t v) { prg32_console_write(k); put_dec(v); }

/* Calibration: exact copies of the ILI9341 backend's hot loops (see PRG32
   components/prg32/prg32_display_ili9341.c), compiled at -Og like the
   firmware, timed once under the instruction counter. */
#define CAL __attribute__((optimize("Og"), noinline))
static uint8_t cal_fb[320];
static uint16_t cal_out[320], cal_pal[256];
CAL static uint8_t cal_index(uint16_t color) {
    static const uint16_t named[8] = {0x0000, 0xffff, 0xf800, 0x07e0, 0x001f, 0xffe0, 0x07ff, 0xf81f};
    for (uint8_t i = 0; i < 8; ++i) if (color == named[i]) return i;
    unsigned r = ((color >> 11) & 31u) * 5u / 31u, g = ((color >> 5) & 63u) * 5u / 63u, b = (color & 31u) * 5u / 31u;
    return (uint8_t)(16u + r * 36u + g * 6u + b);
}
CAL static void cal_text_char(const uint8_t *glyph, uint16_t fg, uint16_t bg) {
    for (int row = 0; row < 8; ++row) {
        uint8_t bits = glyph[row];
        if ((unsigned)row >= 200u) continue;
        for (int col = 0; col < 8; ++col) {
            if ((unsigned)col >= 320u) continue;
            cal_fb[row * 8 + col] = cal_index((bits & (1u << (7 - col))) ? fg : bg);
        }
    }
}
CAL static void cal_render_row(const uint8_t *src, int width, uint16_t *out) { for (int x = 0; x < width; ++x) out[x] = cal_pal[src[x]]; }
static uint32_t cal_us(void) { return (uint32_t)prg32_perf_now_us(); }
static void calibrate(void) {
    static const uint8_t glyph[8] = {0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00};   /* 'A': 22 of 64 pixels set */
    uint32_t t0, rows, navy, black, gold;
    int i;
    t0 = cal_us(); for (i = 0; i < 200; i++) cal_render_row(cal_fb, 320, cal_out); rows = cal_us() - t0;
    t0 = cal_us(); for (i = 0; i < 100; i++) cal_text_char(glyph, 0xffff, 0x0010); navy = cal_us() - t0;
    t0 = cal_us(); for (i = 0; i < 100; i++) cal_text_char(glyph, 0xffff, 0x0000); black = cal_us() - t0;
    t0 = cal_us(); for (i = 0; i < 100; i++) cal_text_char(glyph, 0xfda0, 0x0000); gold = cal_us() - t0;
    /* virtual microseconds x 1000 / 16 ns = instructions */
    put_kv("CAL present_row_px_x100=", rows * 1000u / 16u * 100u / 64000u);
    put_kv(" char_white_on_navy=", navy * 1000u / 16u / 100u);
    put_kv(" char_white_on_black=", black * 1000u / 16u / 100u);
    put_kv(" char_gold_on_black=", gold * 1000u / 16u / 100u);
    prg32_console_write("\n");
}
void fairwind_update(void) {
    uint32_t t0 = (uint32_t)prg32_perf_now_us(), dt;
    static int last_screen = -1;
    game_update();
    dt = (uint32_t)prg32_perf_now_us() - t0;
    if ((int)screen != last_screen && screen == ST_RESULT) {
        put_kv("RESULT race=", (uint32_t)race_no + 1u); put_kv(" course=", (uint32_t)course_sel); put_kv(" rank=", player_rank);
        put_kv(" times=", boats[0].finish_time); put_kv("/", boats[1].finish_time); put_kv("/", boats[2].finish_time); put_kv("/", boats[3].finish_time);
        prg32_console_write("\n");
    }
    last_screen = (int)screen;
    if (screen == ST_RACE) { prof_upd += dt; if (dt > prof_upd_max) prof_upd_max = dt; }
}
void fairwind_draw(void) {
    uint32_t t0 = (uint32_t)prg32_perf_now_us(), dt;
    game_draw();
    dt = (uint32_t)prg32_perf_now_us() - t0;
    static int calibrated;
    if (screen == ST_RACE && !calibrated) { calibrated = 1; calibrate(); }
    if (screen != ST_RACE) {
#ifdef FAIRWIND_NULL_GFX
        n_rect = n_rows = n_rect_px = n_pix = n_chars = n_rgb = 0;
#endif
        return;
    }
    prof_drw += dt; if (dt > prof_drw_max) prof_drw_max = dt;
    prof_top += top_view_mode; prof_n++;
    if (prof_n == 64) {
        put_kv("PROF upd_avg=", prof_upd / prof_n); put_kv(" upd_max=", prof_upd_max);
        put_kv(" drw_avg=", prof_drw / prof_n); put_kv(" drw_max=", prof_drw_max);
        put_kv(" top=", prof_top); put_kv(" start=", (uint32_t)start_clock); put_kv(" race=", (uint32_t)race_clock);
        put_kv(" leg=", boats[0].leg);
#ifdef FAIRWIND_NULL_GFX
        put_kv(" rects=", n_rect / prof_n); put_kv(" rows=", n_rows / prof_n); put_kv(" px=", n_rect_px / prof_n);
        put_kv(" pixels=", n_pix / prof_n); put_kv(" chars=", n_chars / prof_n); put_kv(" rgb=", n_rgb / prof_n);
        n_rect = n_rows = n_rect_px = n_pix = n_chars = n_rgb = 0;
#endif
        prg32_console_write("\n");
        prof_n = prof_upd = prof_drw = prof_upd_max = prof_drw_max = prof_top = 0;
    }
}
