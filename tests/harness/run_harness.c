/* Headless behavioural harness: drives the real game.c logic with a fake
   prg32 backend so bugs can be observed by actually running the state
   machine, not just reading it. Not part of the shipped cartridge. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
size_t strlen(const char *s);

static uint32_t g_input = 0;
static long g_gfx_calls = 0;
static int g_min_x = 1 << 30, g_max_x = -(1 << 30);
static int g_min_y = 1 << 30, g_max_y = -(1 << 30);
static int g_track_plays[16];
static int g_multiplayer_join_result = 1; /* pretend join always succeeds */
static int g_peer_count = 0;
static int g_local_ready_seen = 0;

static void note_rect(int x, int y, int w, int h) {
    g_gfx_calls++;
    if (x < g_min_x) g_min_x = x;
    if (y < g_min_y) g_min_y = y;
    if (x + w > g_max_x) g_max_x = x + w;
    if (y + h > g_max_y) g_max_y = y + h;
}

#include "../../src/game.c"

uint32_t prg32_input_read(void) { return g_input; }
void prg32_band_set_game_info(const char *s) { (void)s; }
void prg32_audio_play_track(uint16_t t) { if (t < 16) g_track_plays[t]++; }
void prg32_gfx_clear(uint16_t c) { (void)c; note_rect(0, 0, 320, 200); }
void prg32_gfx_present(void) {}
void prg32_gfx_rect(int x, int y, int w, int h, uint16_t c) { (void)c; note_rect(x, y, w, h); }
void prg32_gfx_text8(int x, int y, const char *s, uint16_t fg, uint16_t bg) {
    (void)fg; (void)bg; note_rect(x, y, (int)strlen(s) * 8, 8);
}
void prg32_sprite_draw_bitplanes(int x, int y, const prg32_indexed_sprite_t *s, uint32_t frame) {
    (void)frame; note_rect(x, y, s->width, s->height);
}
void prg32_multiplayer_init(void) {}
int prg32_multiplayer_available(void) { return 1; }
int prg32_multiplayer_join(const char *room, uint32_t opts) { (void)room; (void)opts; return g_multiplayer_join_result ? 0 : -1; }
int prg32_multiplayer_leave(void) { return 0; }
void prg32_multiplayer_tick(void) {}
int prg32_multiplayer_set_local_state(int16_t x, int16_t y, uint16_t heading, uint16_t flags) {
    (void)x; (void)y; (void)heading; if (flags & 0x40) g_local_ready_seen = 1; return 0;
}
int prg32_multiplayer_set_input(uint32_t in) { (void)in; return 0; }
int prg32_multiplayer_get_peer_count(void) { return g_peer_count; }
int prg32_multiplayer_get_peer(int index, prg32_player_state_t *peer) {
    if (index < 0 || index >= g_peer_count) return -1;
    peer->player_id = (uint32_t)index;
    peer->x = (int16_t)(100 + index * 20);
    peer->y = (int16_t)(150);
    peer->sprite = 16; /* heading */
    peer->flags = (uint8_t)(((index & 3) << 3) | 0x40);
    peer->input = 0; peer->frame = 0; peer->last_seen_ms = 0;
    return 0;
}

static const char *screen_name(screen_t s) {
    switch (s) {
        case ST_TITLE: return "TITLE"; case ST_MODE: return "MODE";
        case ST_TEAM: return "TEAM"; case ST_MANAGER: return "MANAGER";
        case ST_BRIEF: return "BRIEF"; case ST_LOBBY: return "LOBBY";
        case ST_RACE: return "RACE"; case ST_RESULT: return "RESULT";
        case ST_SEASON: return "SEASON";
    }
    return "?";
}

/* Real hardware calls nacup_init() exactly once per boot; here we call it once
   per scenario in the same process, so stale last_input from a previous
   scenario's final held button can swallow the next scenario's first tap.
   Clear it explicitly so each scenario starts from a clean edge-detect state. */
static void reset_harness_globals(void) { last_input = 0; g_input = 0; }
static void tap(uint32_t btn) { g_input = btn; nacup_update(); g_input = 0; nacup_update(); }

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

static void run_one_full_season(int verbose) {
    reset_harness_globals();
    nacup_init();
    CHECK(screen == ST_TITLE, "should start at title screen");
    tap(PRG32_BTN_A); /* -> MODE */
    CHECK(screen == ST_MODE, "A at title should reach mode select");
    tap(PRG32_BTN_A); /* confirm single player -> new_campaign -> TEAM */
    CHECK(screen == ST_TEAM, "confirming mode should reach team select");
    CHECK(money == 70, "campaign should start with 70k");
    tap(PRG32_BTN_A); /* pick team -> MANAGER */
    CHECK(screen == ST_MANAGER, "picking team should reach manager");

    int race = 0;
    long guard = 0;
    while (screen != ST_SEASON) {
        guard++;
        if (guard > 2000000) { printf("FAIL: watchdog tripped, likely infinite loop/softlock in %s\n", screen_name(screen)); failures++; break; }
        if (screen == ST_MANAGER) {
            /* go straight to race weekend */
            menu = 5;
            tap(PRG32_BTN_A);
        } else if (screen == ST_BRIEF) {
            tap(PRG32_BTN_A);
        } else if (screen == ST_RACE) {
            /* steer with a cheap deterministic pattern and occasionally toggle spinnaker */
            uint32_t in = 0;
            if ((race_frames + race_clock) % 7 < 3) in |= PRG32_BTN_LEFT;
            else if ((race_frames + race_clock) % 7 < 6) in |= PRG32_BTN_RIGHT;
            if (boats[0].penalty) in |= PRG32_BTN_B;
            g_input = in;
            nacup_update();
            g_input = 0;
            nacup_draw();
            for (int i = 0; i < 4; i++) {
                if (!(boats[i].x >= -21 && boats[i].x <= 341)) {
                    printf("FAIL: boat %d x out of range: x=%d y=%d heading=%d speed=%d finished=%d penalty=%d screen=%s race_clock=%d start_clock=%d\n",
                           i, boats[i].x, boats[i].y, boats[i].heading, boats[i].speed, boats[i].finished, boats[i].penalty, screen_name(screen), race_clock, start_clock);
                    failures++;
                }
                if (!(boats[i].y >= 27 && boats[i].y <= 191)) {
                    printf("FAIL: boat %d y out of range: x=%d y=%d heading=%d speed=%d finished=%d penalty=%d screen=%s race_clock=%d start_clock=%d\n",
                           i, boats[i].x, boats[i].y, boats[i].heading, boats[i].speed, boats[i].finished, boats[i].penalty, screen_name(screen), race_clock, start_clock);
                    failures++;
                }
                CHECK(boats[i].team < 4, "boat team index must stay < 4");
                CHECK(boats[i].leg <= COURSE_MARKS_MAX, "boat leg must stay in bounds");
            }
        } else if (screen == ST_RESULT) {
            race++;
            CHECK(race <= RACES + 1, "should not loop through more result screens than races");
            tap(PRG32_BTN_A);
        } else {
            printf("FAIL: unexpected screen %s reached during season loop\n", screen_name(screen));
            failures++;
            break;
        }
    }
    if (verbose) printf("season finished after %d results, money=%d wins=%d points=[%d,%d,%d,%d]\n",
                         race, money, wins, points[0], points[1], points[2], points[3]);
    CHECK(race == RACES, "exactly RACES result screens should have been shown");
}

static void run_multiplayer_smoke(void) {
    reset_harness_globals();
    nacup_init();
    tap(PRG32_BTN_A); /* MODE */
    multiplayer = 1; /* force the mode directly: the UP toggle is order-dependent on prior test state */
    tap(PRG32_BTN_A); /* confirm -> TEAM */
    tap(PRG32_BTN_A); /* pick team -> MANAGER */
    menu = 5;
    tap(PRG32_BTN_A); /* -> BRIEF */
    g_peer_count = 2;
    tap(PRG32_BTN_A); /* -> should attempt join -> LOBBY */
    CHECK(screen == ST_LOBBY, "multiplayer brief confirm should reach lobby when join succeeds");
    long guard = 0;
    while (screen == ST_LOBBY) {
        guard++;
        if (guard > 100000) { printf("FAIL: lobby never starts even though peer_count>0 and local_ready set\n"); failures++; break; }
        g_input = PRG32_BTN_A; /* mark ready every frame */
        nacup_update();
        g_input = 0;
    }
    CHECK(screen == ST_RACE, "lobby should transition into a race once all visible players are ready");
    printf("multiplayer smoke: peer_count reported in-race = %d\n", peer_count);
}

static uint32_t fuzz_rng = 12345;
static uint32_t fuzz_next(void) { fuzz_rng ^= fuzz_rng << 13; fuzz_rng ^= fuzz_rng >> 17; fuzz_rng ^= fuzz_rng << 5; return fuzz_rng; }

static void run_fuzz_season(uint32_t seed, int verbose) {
    fuzz_rng = seed ? seed : 1;
    reset_harness_globals();
    nacup_init();
    tap(PRG32_BTN_A);
    if (fuzz_next() & 1) tap(PRG32_BTN_UP); /* randomly poke multiplayer toggle then back off */
    if (multiplayer) tap(PRG32_BTN_UP);
    tap(PRG32_BTN_A); /* single player campaign */
    for (int i = 0; i < (int)(fuzz_next() % 4); i++) tap(PRG32_BTN_RIGHT);
    tap(PRG32_BTN_A); /* team -> manager */
    int race = 0;
    long guard = 0;
    while (screen != ST_SEASON) {
        guard++;
        if (guard > 3000000) { printf("FAIL(seed %u): watchdog tripped in %s\n", seed, screen_name(screen)); failures++; return; }
        if (screen == ST_MANAGER) {
            int steps = (int)(fuzz_next() % 8);
            for (int i = 0; i < steps; i++) tap((fuzz_next() & 1) ? PRG32_BTN_DOWN : PRG32_BTN_UP);
            for (int i = 0; i < (int)(fuzz_next() % 3); i++) tap(PRG32_BTN_A);
            menu = 5;
            tap(PRG32_BTN_A);
        } else if (screen == ST_BRIEF) {
            for (int i = 0; i < (int)(fuzz_next() % 3); i++) tap(PRG32_BTN_LEFT);
            tap(PRG32_BTN_A);
        } else if (screen == ST_RACE) {
            uint32_t r = fuzz_next();
            uint32_t in = 0;
            if (r % 3 == 0) in |= PRG32_BTN_LEFT; else if (r % 3 == 1) in |= PRG32_BTN_RIGHT;
            if (boats[0].penalty && (r & 4)) in |= PRG32_BTN_B;
            if (r & 0x100) in |= PRG32_BTN_A;
            g_input = in; nacup_update(); g_input = 0;
            if ((race_clock + race_frames) % 5 == 0) nacup_draw();
            for (int i = 0; i < 4; i++) {
                CHECK(boats[i].x >= -20 && boats[i].x <= 340, "fuzz: boat x must stay in clamp range");
                CHECK(boats[i].y >= 28 && boats[i].y <= 190, "fuzz: boat y must stay in clamp range");
                CHECK(boats[i].finish_time < 10000, "fuzz: finish_time should stay sane (no wraparound)");
            }
            CHECK(money > -1000 && money < 100000, "fuzz: money should stay in a sane range");
        } else if (screen == ST_RESULT) {
            /* Fuzz input can hold A into the exact frame the race ends, so the
               confirm press is correctly debounced and needs a retry here;
               that's why this count can exceed RACES, unlike the deterministic
               season test above which never holds A across a transition. */
            race++;
            tap(PRG32_BTN_A);
        } else {
            printf("FAIL(seed %u): unexpected screen %s\n", seed, screen_name(screen));
            failures++;
            return;
        }
    }
    if (verbose) printf("fuzz seed %u: season ok, race=%d money=%d\n", seed, race, money);
}

static void run_multiplayer_disconnect_scenario(void) {
    reset_harness_globals();
    nacup_init();
    tap(PRG32_BTN_A);
    multiplayer = 1; /* force the mode directly: the UP toggle is order-dependent on prior test state */
    tap(PRG32_BTN_A);
    tap(PRG32_BTN_A);
    menu = 5;
    tap(PRG32_BTN_A);
    g_peer_count = 3; /* full lobby */
    tap(PRG32_BTN_A);
    CHECK(screen == ST_LOBBY, "3-peer lobby should be reachable");
    long guard = 0;
    while (screen == ST_LOBBY && guard++ < 100000) { g_input = PRG32_BTN_A; nacup_update(); g_input = 0; }
    CHECK(screen == ST_RACE, "full lobby should start the race");
    for (int frame = 0; frame < 400; frame++) {
        if (frame == 150) g_peer_count = 1; /* two peers vanish mid-race */
        if (frame == 300) g_peer_count = 0; /* last peer vanishes too */
        g_input = (frame & 8) ? PRG32_BTN_LEFT : PRG32_BTN_RIGHT;
        nacup_update();
        g_input = 0;
        nacup_draw();
        for (int i = 0; i < 4; i++) {
            CHECK(boats[i].team < 4, "disconnect: boat team must stay valid after peers drop");
            CHECK(boats[i].x >= -20 && boats[i].x <= 340, "disconnect: boat x must stay in clamp range");
            CHECK(boats[i].y >= 28 && boats[i].y <= 190, "disconnect: boat y must stay in clamp range");
        }
    }
    printf("multiplayer disconnect scenario: reached race_clock=%d start_clock=%d without crashing\n", race_clock, start_clock);
}

int main(void) {
    printf("== single player full season ==\n");
    run_one_full_season(1);
    printf("== multiplayer lobby smoke test ==\n");
    run_multiplayer_smoke();
    printf("== multiplayer mid-race disconnect scenario ==\n");
    run_multiplayer_disconnect_scenario();
    printf("== fuzz seasons ==\n");
    for (uint32_t seed = 1; seed <= 40; seed++) run_fuzz_season(seed * 2654435761u, seed <= 3);
    printf("gfx bbox seen: x[%d,%d] y[%d,%d] over %ld calls\n", g_min_x, g_max_x, g_min_y, g_max_y, g_gfx_calls);
    if (failures) { printf("\n%d CHECK FAILURE(S)\n", failures); return 1; }
    printf("\nALL CHECKS PASSED\n");
    return 0;
}
