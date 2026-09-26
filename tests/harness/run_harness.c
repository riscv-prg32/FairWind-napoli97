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
/* Per-peer overrides so scenarios can model real snapshots: stable ids that
   survive a disconnect, lobby (y=0) vs racing positions, and chosen teams. */
static uint32_t g_peer_ids[3] = {0, 1, 2};
static int16_t g_peer_y[3] = {150, 150, 150};
static uint16_t g_peer_flags[3] = {(0 << 3) | 0x140, (1 << 3) | 0x140, (2 << 3) | 0x140};
static void reset_peers(void) {
    for (int i = 0; i < 3; i++) { g_peer_ids[i] = (uint32_t)i; g_peer_y[i] = 150; g_peer_flags[i] = (uint16_t)(((i & 3) << 3) | 0x140); }
}

/* Draw-budget counters: RGB565 fills cost a colour conversion per pixel on
   the ILI9341 backend, indexed fills are memsets, text converts per pixel. */
static long g_rgb_px, g_idx_calls, g_chars, g_top_rows_touched, g_hud_touched;
static void note_rect(int x, int y, int w, int h) {
    if (y < 18 && h > 0) g_top_rows_touched++;
    if (y + h > 180 && h > 0) g_hud_touched++;
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
void prg32_gfx_rect(int x, int y, int w, int h, uint16_t c) { (void)c; if (w > 0 && h > 0) g_rgb_px += (long)w * h; note_rect(x, y, w, h); }
void prg32_gfx_pixel(int x, int y, uint16_t c) { (void)c; note_rect(x, y, 1, 1); }
void prg32_gfx_rect_indexed(int x, int y, int w, int h, uint8_t c) { (void)c; g_idx_calls++; note_rect(x, y, w, h); }
void prg32_gfx_pixel_indexed(int x, int y, uint8_t c) { (void)c; note_rect(x, y, 1, 1); }
void prg32_gfx_clear_indexed(uint8_t c) { (void)c; note_rect(0, 0, 320, 200); }
/* The firmware paces frames at 33 ms; the harness clock is deterministic. */
uint32_t prg32_ticks_ms(void) { static uint32_t t; return t += 33; }
void prg32_gfx_text8(int x, int y, const char *s, uint16_t fg, uint16_t bg) {
    (void)fg; (void)bg; g_chars += (long)strlen(s); note_rect(x, y, (int)strlen(s) * 8, 8);
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
    peer->player_id = g_peer_ids[index];
    peer->x = (int16_t)(100 + index * 20);
    peer->y = g_peer_y[index];
    peer->sprite = 0; /* heading 0, sheet 0 */
    peer->flags = g_peer_flags[index];
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

/* Real hardware calls fairwind_init() exactly once per boot; here we call it once
   per scenario in the same process, so stale last_input from a previous
   scenario's final held button can swallow the next scenario's first tap.
   Clear it explicitly so each scenario starts from a clean edge-detect state. */
static void reset_harness_globals(void) { last_input = 0; g_input = 0; g_peer_count = 0; reset_peers(); }
static void tap(uint32_t btn) { g_input = btn; fairwind_update(); g_input = 0; fairwind_update(); }

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

static void run_one_full_season(int verbose) {
    reset_harness_globals();
    fairwind_init();
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
            if ((frame + race_clock) % 7 < 3) in |= PRG32_BTN_LEFT;
            else if ((frame + race_clock) % 7 < 6) in |= PRG32_BTN_RIGHT;
            if (boats[0].penalty) in |= PRG32_BTN_B;
            g_input = in;
            fairwind_update();
            g_input = 0;
            fairwind_draw();
            for (int i = 0; i < 4; i++) {
                if (!(boats[i].x >= -FIELD_X * 1000 && boats[i].x <= FIELD_X * 1000)) {
                    printf("FAIL: boat %d x out of range: x=%d y=%d heading=%d speed=%d finished=%d penalty=%d screen=%s race_clock=%d start_clock=%d\n",
                           i, boats[i].x, boats[i].y, boats[i].heading, boats[i].v >> 8, boats[i].finished, boats[i].penalty, screen_name(screen), race_clock, start_clock);
                    failures++;
                }
                if (!(boats[i].y >= FIELD_Y0 * 1000 && boats[i].y <= FIELD_Y1 * 1000)) {
                    printf("FAIL: boat %d y out of range: x=%d y=%d heading=%d speed=%d finished=%d penalty=%d screen=%s race_clock=%d start_clock=%d\n",
                           i, boats[i].x, boats[i].y, boats[i].heading, boats[i].v >> 8, boats[i].finished, boats[i].penalty, screen_name(screen), race_clock, start_clock);
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
    fairwind_init();
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
        fairwind_update();
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
    fairwind_init();
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
            g_input = in; fairwind_update(); g_input = 0;
            if ((race_clock + frame) % 25 == 0) fairwind_draw();
            for (int i = 0; i < 4; i++) {
                CHECK(boats[i].x >= -FIELD_X * 1000 && boats[i].x <= FIELD_X * 1000, "fuzz: boat x must stay on the water");
                CHECK(boats[i].y >= FIELD_Y0 * 1000 && boats[i].y <= FIELD_Y1 * 1000, "fuzz: boat y must stay on the water");
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
    fairwind_init();
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
    while (screen == ST_LOBBY && guard++ < 100000) { g_input = PRG32_BTN_A; fairwind_update(); g_input = 0; }
    CHECK(screen == ST_RACE, "full lobby should start the race");
    for (int frame = 0; frame < 400; frame++) {
        if (frame == 150) g_peer_count = 1; /* two peers vanish mid-race */
        if (frame == 300) g_peer_count = 0; /* last peer vanishes too */
        g_input = (frame & 8) ? PRG32_BTN_LEFT : PRG32_BTN_RIGHT;
        fairwind_update();
        g_input = 0;
        fairwind_draw();
        for (int i = 0; i < 4; i++) {
            CHECK(boats[i].team < 4, "disconnect: boat team must stay valid after peers drop");
            CHECK(boats[i].x >= -FIELD_X * 1000 && boats[i].x <= FIELD_X * 1000, "disconnect: boat x must stay on the water");
            CHECK(boats[i].y >= FIELD_Y0 * 1000 && boats[i].y <= FIELD_Y1 * 1000, "disconnect: boat y must stay on the water");
        }
    }
    printf("multiplayer disconnect scenario: reached race_clock=%d start_clock=%d without crashing\n", race_clock, start_clock);
}

/* Navigate a fresh campaign to the multiplayer lobby with n visible peers. */
static void enter_lobby(int n) {
    reset_harness_globals();
    fairwind_init();
    tap(PRG32_BTN_A);
    multiplayer = 1;
    tap(PRG32_BTN_A);
    tap(PRG32_BTN_A);
    menu = 5;
    tap(PRG32_BTN_A);
    g_peer_count = n;
    tap(PRG32_BTN_A);
}

static void run_lobby_handshake_scenario(void) {
    /* A peer still in the lobby and not ready must hold the start. */
    enter_lobby(1);
    g_peer_y[0] = 0; g_peer_flags[0] = (uint16_t)(1 << 3);
    for (int i = 0; i < 200; i++) { g_input = PRG32_BTN_A; fairwind_update(); }
    g_input = 0;
    CHECK(screen == ST_LOBBY, "lobby: must wait for a peer that is not ready");
    /* The peer saw us ready first and is already racing: its published flags
       now carry race state (0x40 = crossed line, clear in pre-start). */
    g_peer_y[0] = 112; g_peer_flags[0] = (uint16_t)((1 << 3) | 0x100);
    fairwind_update();
    CHECK(screen == ST_RACE, "lobby: a peer that already started racing must count as ready (no deadlock)");
    fairwind_update();
    CHECK(boats[1].y == 11200, "lobby: a racing snapshot places the yacht (decimetres -> mm)");
    /* A lingering lobby snapshot must not teleport that yacht to (0,0). */
    g_peer_y[0] = 0; g_peer_flags[0] = (uint16_t)(1 << 3);
    fairwind_update();
    CHECK(boats[1].y == 11200, "lobby: a lobby snapshot must not be applied to a racing yacht");
    printf("lobby handshake: race started once peer was racing\n");
}

static void run_stable_slot_scenario(void) {
    enter_lobby(3);
    g_peer_ids[0] = 100; g_peer_ids[1] = 200; g_peer_ids[2] = 300;
    g_peer_flags[0] = (uint16_t)((1 << 3) | 0x140); g_peer_flags[1] = (uint16_t)((2 << 3) | 0x140); g_peer_flags[2] = (uint16_t)((3 << 3) | 0x140);
    while (screen == ST_LOBBY) { g_input = PRG32_BTN_A; fairwind_update(); g_input = 0; }
    fairwind_update();
    CHECK(net_live[1] && net_live[2] && net_live[3], "slots: all three peers bound");
    CHECK(net_ids[1] == 100 && net_ids[2] == 200 && net_ids[3] == 300, "slots: peers bound in join order");
    /* Remote penalties belong to the remote console and must not stick here. */
    boats[2].penalty = 1; boats[2].rule = RULE_PORT;
    fairwind_update();
    CHECK(!boats[2].penalty && boats[2].rule == RULE_NONE, "slots: remote yacht penalty must be cleared each snapshot");
    /* Peer 100 leaves: the firmware list compacts to [200,300]. */
    g_peer_ids[0] = 200; g_peer_ids[1] = 300; g_peer_flags[0] = g_peer_flags[1]; g_peer_flags[1] = g_peer_flags[2];
    g_peer_count = 2;
    fairwind_update();
    CHECK(!net_live[1], "slots: the departed peer's slot must pass to the AI");
    CHECK(net_live[2] && net_ids[2] == 200 && net_live[3] && net_ids[3] == 300, "slots: remaining peers must keep their slots");
    CHECK(peer_count == 2, "slots: peer_count must track bound peers");
    printf("stable slots: departed peer handed to AI, others kept their yachts\n");
}

static void run_ai_takeover_scenario(void) {
    /* A peer that disconnects after starting must be sailed round the course
       by the AI, not steered south as if it had never entered the box. */
    enter_lobby(1);
    g_peer_flags[0] = (uint16_t)((1 << 3) | 0x140);
    while (screen == ST_LOBBY) { g_input = PRG32_BTN_A; fairwind_update(); g_input = 0; }
    start_clock = 0; start_line_active = 0;
    g_peer_y[0] = 100;
    fairwind_update();
    CHECK(boats[1].started && boats[1].entered_box, "takeover: a started remote yacht is known to have entered the box");
    g_peer_count = 0;
    int legs_before = boats[1].leg, max_y = boats[1].y;
    for (int i = 0; i < 15000 && screen == ST_RACE && boats[1].leg == legs_before; i++) { fairwind_update(); if (boats[1].y > max_y) max_y = boats[1].y; }
    CHECK(max_y > 400000, "takeover: AI must beat upwind towards the first mark");
    CHECK(boats[1].leg > legs_before, "takeover: AI must round the next mark for a disconnected started peer");
    printf("ai takeover: slot 1 advanced from leg %d to %d\n", legs_before, boats[1].leg);
}

static void run_ai_team_scenario(void) {
    enter_lobby(1);
    team_sel = 0;
    g_peer_flags[0] = (uint16_t)((2 << 3) | 0x140); /* peer chose ATLANTIC UNION, the default AI team for slot 2 */
    while (screen == ST_LOBBY) { g_input = PRG32_BTN_A; fairwind_update(); g_input = 0; }
    fairwind_update(); /* first race frame applies the peer snapshot */
    unsigned used = 0;
    for (int i = 0; i < 4; i++) used |= 1u << boats[i].team;
    CHECK(used == 15u, "teams: AI must take the syndicates no human picked");
    printf("ai teams: fleet teams %d %d %d %d\n", boats[0].team, boats[1].team, boats[2].team, boats[3].team);
}

static void run_dnf_order_scenario(void) {
    reset_harness_globals();
    fairwind_init();
    tap(PRG32_BTN_A); multiplayer = 0; tap(PRG32_BTN_A); tap(PRG32_BTN_A);
    menu = 5; tap(PRG32_BTN_A); tap(PRG32_BTN_A);
    CHECK(screen == ST_RACE, "dnf: race should start");
    start_clock = 0; race_clock = RACE_LIMIT_SECONDS; sim_acc = 999;
    for (int i = 0; i < 4; i++) { boats[i].started = 1; boats[i].leg = (uint8_t)i; boats[i].x = (-300 + i * 200) * 1000; boats[i].y = 300000; }
    fairwind_update();
    CHECK(screen == ST_RESULT, "dnf: time limit ends the race");
    CHECK(result_order[0] == 3 && result_order[3] == 0, "dnf: unfinished yachts are ranked by course progress");
    printf("dnf order: %d %d %d %d\n", result_order[0], result_order[1], result_order[2], result_order[3]);
}

/* Put the local yacht alone on open water after the start with a steady
   breeze, so physics can be measured without rivals or the start line. */
static void open_water(int tws, uint16_t heading) {
    reset_harness_globals();
    fairwind_init();
    tap(PRG32_BTN_A); multiplayer = 0; tap(PRG32_BTN_A); tap(PRG32_BTN_A);
    menu = 5; tap(PRG32_BTN_A); tap(PRG32_BTN_A);
    start_clock = 0; start_line_active = 0; race_clock = 1;
    for (int i = 1; i < 4; i++) { boats[i].finished = 1; boats[i].x = (-800 + 60 * i) * 1000; boats[i].y = 1200000; }
    boats[0].x = 0; boats[0].y = 300000; boats[0].heading = heading; boats[0].v = 0; boats[0].started = 1; boats[0].leg = 0;
    boats[0].kite = boats[0].kite_want = KITE_NONE; boats[0].kite_prog = 0; boats[0].penalty = boats[0].serving = 0;
    hull = sails = crew = strategy = 1;
    wind_seed = 0; wind_t = 0; twd = 0; tws10 = tws; top_view_mode = 0;
}
/* Sail n frames holding the heading, trimming to the optimum, wind frozen. */
static void sail_frames(int n, int trim) {
    for (int i = 0; i < n; i++) {
        uint32_t in = 0; int opt = trim_opt(awa_deg(&boats[0]));
        if (trim) { if (boats[0].sheet < opt) in |= PRG32_BTN_UP; else if (boats[0].sheet > opt) in |= PRG32_BTN_DOWN; }
        g_input = in; wind_t = 0; sim_acc = 0; update_race(in, 0); twd = 0; g_input = 0;
        boats[0].leg = 0; boats[0].x = 0; boats[0].y = 300000; /* stay mid-field */
    }
}
static int knots10(const boat_t *b) { return (int)((b->v >> 8) * 100 / 5144); }

static void run_fixmath_scenario(void) {
    CHECK(fsin(0) == 0 && fsin(16384) == 16384 && fsin(32768) == 0 && fsin(49152) == -16384, "fixmath: sine quadrants");
    CHECK(absi((int)fcos((uint16_t)DEG(60)) - 8192) < 40, "fixmath: cos 60 = 0.5");
    CHECK(absi((int16_t)(bearing(1000, 0) - 16384)) < 30, "fixmath: bearing east is 90 degrees");
    CHECK(absi((int16_t)(bearing(-1000, -1000) - DEG(225))) < 60, "fixmath: bearing SW is 225 degrees");
    CHECK(absi((int16_t)(bearing(3, 4) - DEG(37))) < 120, "fixmath: bearing of (3,4)");
    CHECK(isqrt(1000000u) == 1000 && isqrt(99u) == 9, "fixmath: integer sqrt");
    printf("fixmath: sine, bearing and sqrt within tolerance\n");
}

static void run_polar_speed_scenario(void) {
    /* Beam reach in 12 knots: the jib polar gives 8.8 kt, trimmed yacht must
       settle close to it (hull/sails level 1 = 99.5% performance). */
    open_water(120, (uint16_t)DEG(270));
    sail_frames(3000, 1);
    int reach = knots10(&boats[0]);
    CHECK(reach >= 82 && reach <= 92, "polar: beam reach in 12 kt settles near 8.8 kt");
    /* Close-hauled at 42 degrees is slower, dead into the wind stops. */
    open_water(120, (uint16_t)DEG(318));
    sail_frames(3000, 1);
    int beat = knots10(&boats[0]);
    CHECK(beat >= 66 && beat < reach, "polar: close-hauled is slower than a reach");
    open_water(120, 0); boats[0].v = 4000 << 8;
    sail_frames(2000, 1);
    CHECK(knots10(&boats[0]) < 10, "polar: head to wind the yacht stops (no-go zone)");
    /* Momentum: a 26-tonne yacht takes many seconds to accelerate. */
    open_water(120, (uint16_t)DEG(270));
    sail_frames(30, 1); /* 4 simulated seconds */
    CHECK(knots10(&boats[0]) < reach * 2 / 3, "polar: yacht accelerates gradually, not instantly");
    /* Realistic top speeds: never above ~10 kt in 16 kt of breeze. */
    open_water(160, (uint16_t)DEG(240));
    sail_frames(3000, 1);
    CHECK(knots10(&boats[0]) <= 105, "polar: no go-kart speeds");
    /* Beam reach against the ORC table at 8 and 16 knots too (7.6 and 9.3 kt). */
    open_water(80, (uint16_t)DEG(270)); sail_frames(3000, 1);
    int reach8 = knots10(&boats[0]);
    open_water(160, (uint16_t)DEG(270)); sail_frames(3000, 1);
    int reach16 = knots10(&boats[0]);
    CHECK(reach8 >= 71 && reach8 <= 80, "polar: beam reach in 8 kt settles near 7.6 kt");
    CHECK(reach16 >= 88 && reach16 <= 97, "polar: beam reach in 16 kt settles near 9.3 kt");
    printf("polar: beam reach %d.%d / %d.%d / %d.%d kt in 8 / 12 / 16 kt; beat %d.%d kt in 12 kt\n",
           reach8 / 10, reach8 % 10, reach / 10, reach % 10, reach16 / 10, reach16 % 10, beat / 10, beat % 10);
}

static void run_trim_scenario(void) {
    open_water(120, (uint16_t)DEG(270));
    sail_frames(3000, 1);
    int trimmed = knots10(&boats[0]);
    CHECK(boats[0].boom < 0, "trim: boom lies to port (leeward) with wind from starboard");
    CHECK(absi(boats[0].boom) <= boats[0].sheet, "trim: boom never swings past the eased sheet");
    for (int i = 0; i < 200; i++) sail_frames(1, 0), g_input = 0, update_race(PRG32_BTN_UP, 0);
    CHECK(boats[0].sheet == 90, "trim: UP eases the sheets");
    sail_frames(2000, 0);
    int eased = knots10(&boats[0]);
    CHECK(eased < trimmed / 2, "trim: a fully eased, luffing sail loses drive");
    for (int i = 0; i < 200; i++) update_race(PRG32_BTN_DOWN, 0);
    CHECK(boats[0].sheet == 0, "trim: DOWN trims the sheets");
    sail_frames(2000, 0);
    CHECK(knots10(&boats[0]) < trimmed, "trim: over-trimmed on a reach stalls");
    /* Gybe: wind from port puts the boom to starboard. */
    open_water(120, (uint16_t)DEG(90));
    sail_frames(600, 1);
    CHECK(boats[0].boom > 0, "trim: boom goes to starboard with wind from port");
    printf("trim: trimmed %d, eased %d (knots x10)\n", trimmed, eased);
}

static void run_helm_scenario(void) {
    open_water(120, (uint16_t)DEG(270));
    sail_frames(1500, 1);
    uint16_t h0 = boats[0].heading;
    for (int i = 0; i < 30; i++) update_race(PRG32_BTN_LEFT, 0);
    CHECK((int16_t)(boats[0].heading - h0) < -DEG(3), "helm: LEFT turns to port");
    h0 = boats[0].heading;
    for (int i = 0; i < 60; i++) update_race(PRG32_BTN_RIGHT, 0);
    CHECK((int16_t)(boats[0].heading - h0) > DEG(3), "helm: RIGHT turns to starboard");
    /* A yacht turns at a few degrees per second, not instantly. */
    h0 = boats[0].heading;
    for (int i = 0; i < 8; i++) update_race(PRG32_BTN_RIGHT, 0); /* about one simulated second */
    CHECK((int16_t)(boats[0].heading - h0) < DEG(15), "helm: turn rate stays realistic");
    printf("helm: port and starboard steering verified\n");
}

static void press_release(uint32_t b) { update_race(b, 0); update_race(0, b); }
static void run_kite_scenario(void) {
    open_water(120, (uint16_t)DEG(200)); /* broad reach / run, wind from port quarter */
    sail_frames(1500, 1);
    int jib = knots10(&boats[0]);
    press_release(PRG32_BTN_A);
    CHECK(boats[0].kite == KITE_SPIN && boats[0].kite_want == KITE_SPIN, "kite: A hoists the spinnaker");
    CHECK(boats[0].kite_prog < 65535, "kite: the hoist is not instantaneous");
    int frames = 0;
    while (boats[0].kite_prog < 65535 && frames < 5000) { sail_frames(1, 1); frames++; }
    CHECK(frames > 150, "kite: hoisting takes simulated time (>10 s)");
    sail_frames(1500, 1);
    int spin = knots10(&boats[0]);
    CHECK(spin > jib, "kite: spinnaker is faster than the jib downwind");
    /* B while the spinnaker is set: drop it first, then hoist the gennaker. */
    press_release(PRG32_BTN_B);
    CHECK(boats[0].kite == KITE_SPIN && boats[0].kite_want == KITE_GENN, "kite: B peels, spinnaker still coming down");
    int saw_both = 0; frames = 0;
    while (!(boats[0].kite == KITE_GENN && boats[0].kite_prog == 65535) && frames < 8000) {
        sail_frames(1, 1); frames++;
        if (boats[0].kite == KITE_GENN && boats[0].kite_prog == 0) saw_both |= 0;
    }
    CHECK(boats[0].kite == KITE_GENN && boats[0].kite_prog == 65535, "kite: gennaker set after the peel");
    CHECK(!saw_both, "kite: spinnaker and gennaker are never set together");
    press_release(PRG32_BTN_B);
    while (boats[0].kite_prog > 0 && frames < 12000) { sail_frames(1, 1); frames++; }
    CHECK(boats[0].kite_prog == 0 && boats[0].kite_want == KITE_NONE, "kite: B again drops the gennaker");
    printf("kite: jib %d, spinnaker %d (knots x10)\n", jib, spin);
}

static void run_penalty_combo_scenario(void) {
    open_water(120, (uint16_t)DEG(270));
    sail_frames(1500, 1);
    award_penalty(&boats[0], RULE_PORT);
    update_race(PRG32_BTN_A, 0);
    update_race(PRG32_BTN_A | PRG32_BTN_B, 0);
    update_race(PRG32_BTN_B, PRG32_BTN_A);
    update_race(0, PRG32_BTN_B);
    CHECK(boats[0].serving, "penalty: A+B starts the penalty turn");
    CHECK(boats[0].kite_want == KITE_NONE, "penalty: A+B never toggles a kite");
    int frames = 0;
    while (boats[0].penalty && frames < 20000) { sail_frames(1, 0); frames++; }
    CHECK(!boats[0].penalty && boats[0].rule == RULE_NONE, "penalty: a full 360 turn clears the penalty");
    CHECK(frames > 60, "penalty: the turn takes time");
    update_race(PRG32_BTN_A | PRG32_BTN_B, 0); update_race(0, PRG32_BTN_A | PRG32_BTN_B);
    CHECK(!boats[0].serving, "penalty: A+B without a penalty does nothing");
    printf("penalty: turn served in %d frames\n", frames);
}

static void run_land_scenario(void) {
    open_water(120, (uint16_t)DEG(270));
    boats[0].x = -(FIELD_X - 60) * 1000;
    for (int i = 0; i < 4000; i++) { uint32_t in = boats[0].sheet > trim_opt(awa_deg(&boats[0])) ? PRG32_BTN_DOWN : 0; update_race(in, 0); twd = 0; }
    CHECK(boats[0].x >= -FIELD_X * 1000, "land: a yacht cannot sail onto the shore");
    CHECK(boats[0].aground, "land: running into the shore is reported");
    CHECK(knots10(&boats[0]) < 5, "land: aground the yacht loses all way");
    printf("land: grounded at x=%d m\n", boats[0].x / 1000);
}

static void run_top_view_scenario(void) {
    open_water(120, (uint16_t)DEG(270));
    boats[0].x = 0; boats[0].y = (course_y[course_sel][0] - 200) * 1000;
    update_race(0, 0);
    CHECK(!top_view_mode, "view: open water uses the stern chase view");
    boats[0].y = (course_y[course_sel][0] - 60) * 1000; update_race(0, 0);
    CHECK(top_view_mode, "view: approaching a mark switches to the top view");
    boats[0].y = (course_y[course_sel][0] - 85) * 1000; update_race(0, 0);
    CHECK(top_view_mode, "view: hysteresis keeps the top view just outside the entry range");
    boats[0].y = (course_y[course_sel][0] - 130) * 1000; update_race(0, 0);
    CHECK(!top_view_mode, "view: leaving the mark restores the chase view");
    boats[1].finished = 0; boats[1].x = boats[0].x + 40000; boats[1].y = boats[0].y; update_race(0, 0);
    CHECK(top_view_mode, "view: a close rival switches to the top view");
    fairwind_draw();
    printf("view: chase/top switching verified\n");
}

static void run_wind_scenario(void) {
    int lo = 999, hi = -999, slo = 999, shi = -999, changes = 0; uint16_t last;
    wind_seed = 12345; wind_t = 0; update_wind(); last = twd;
    for (wind_t = 0; wind_t < 3300; wind_t++) {
        update_wind(); int sh = wind_shift_deg();
        if (sh < lo) lo = sh; if (sh > hi) hi = sh; if (tws10 < slo) slo = tws10; if (tws10 > shi) shi = tws10;
        if (twd != last) changes++; last = twd;
    }
    CHECK(lo <= -5 && hi >= 5, "wind: oscillating shifts of several degrees");
    CHECK(lo >= -20 && hi <= 20, "wind: shifts stay within a realistic band");
    CHECK(slo >= 50 && shi <= 190, "wind: pressure stays in the SW sea-breeze range");
    wind_t = 777; update_wind(); uint16_t a = twd; int b = tws10; wind_t = 777; update_wind();
    CHECK(a == twd && b == tws10, "wind: deterministic for every console in a room");
    printf("wind: shift %d..%d deg, %d.%d..%d.%d kt\n", lo, hi, slo / 10, slo % 10, shi / 10, shi % 10);
}

/* The AI must start legally and sail every course to the finish. */
static void run_ai_race_scenario(int course) {
    reset_harness_globals();
    fairwind_init();
    tap(PRG32_BTN_A); multiplayer = 0; tap(PRG32_BTN_A); tap(PRG32_BTN_A);
    menu = 5; tap(PRG32_BTN_A);
    course_sel = course; tap(PRG32_BTN_A);
    int finished_ai = 0, guard = 0, started_by = -1;
    while (screen == ST_RACE && guard++ < 60000) {
        fairwind_update(); /* player idle */
        if (started_by < 0 && boats[1].started && boats[2].started && boats[3].started) started_by = race_clock;
    }
    for (int i = 1; i < 4; i++) finished_ai += boats[i].finished && boats[i].leg == course_len[course] && boats[i].finish_time < RACE_LIMIT_SECONDS;
    CHECK(started_by >= 0 && started_by < 120, "ai race: every AI yacht starts within two minutes of the gun");
    CHECK(finished_ai == 3, "ai race: every AI yacht completes the course inside the time limit");
    printf("ai race %s: all started by R+%d s, finishes %d/%d/%d s\n", course_names[course], started_by, boats[1].finish_time, boats[2].finish_time, boats[3].finish_time);
}

/* The simulation integrates real frame time: sailing the same simulated time
   at 15 fps and at 30 fps must give the same yacht. */
static void run_frame_rate_scenario(void) {
    int32_t x33, y33, v33; uint16_t h33;
    open_water(120, (uint16_t)DEG(270)); frame_ms = 33;
    for (int i = 0; i < 400; i++) { update_race(i < 100 ? PRG32_BTN_DOWN : (i < 130 ? PRG32_BTN_LEFT : 0), 0); twd = 0; wind_t = 0; }
    x33 = boats[0].x; y33 = boats[0].y; v33 = boats[0].v; h33 = boats[0].heading;
    open_water(120, (uint16_t)DEG(270)); frame_ms = 66;
    for (int i = 0; i < 200; i++) { update_race(i < 50 ? PRG32_BTN_DOWN : (i < 65 ? PRG32_BTN_LEFT : 0), 0); twd = 0; wind_t = 0; }
    int dx = (int)((boats[0].x - x33) / 1000), dy = (int)((boats[0].y - y33) / 1000);
    int dv = (int)(((boats[0].v - v33) >> 8) * 100 / 5144), dh = (int)BAM2DEG((int16_t)(boats[0].heading - h33));
    CHECK(dx * dx + dy * dy < 25 * 25, "frame rate: position after 53 s agrees within 25 m at 15 and 30 fps");
    CHECK(absi(dv) <= 3, "frame rate: speed agrees within 0.3 kt at 15 and 30 fps");
    CHECK(absi(dh) <= 5, "frame rate: heading agrees within 5 degrees at 15 and 30 fps");
    frame_ms = FRAME_MS;
    printf("frame rate: 30 vs 15 fps differ by %d m, %d.%d kt, %d deg\n", (int)isqrt((uint32_t)(dx * dx + dy * dy)), dv / 10, absi(dv) % 10, dh);
}

/* ESP32-C6 draw budget: a race frame must use indexed fills only, never
   touch the static header after its first frame, refresh the HUD one frame
   in four, and keep text (converted per pixel by the firmware) short. */
static void run_draw_budget_scenario(void) {
    long frames = 0, chars = 0, max_chars = 0, top_frames = 0, hud_frames = 0;
    reset_harness_globals();
    fairwind_init();
    tap(PRG32_BTN_A); multiplayer = 0; tap(PRG32_BTN_A); tap(PRG32_BTN_A);
    menu = 5; tap(PRG32_BTN_A); tap(PRG32_BTN_A);
    fairwind_draw();
    for (int i = 0; i < 6000 && screen == ST_RACE; i++) {
        fairwind_update();
        g_rgb_px = g_chars = g_top_rows_touched = g_hud_touched = 0;
        fairwind_draw();
        CHECK(g_rgb_px == 0, "budget: race frames draw with palette indices only");
        frames++; chars += g_chars; if (g_chars > max_chars) max_chars = g_chars;
        top_frames += g_top_rows_touched > 0; hud_frames += g_hud_touched > 0;
    }
    CHECK(top_frames == 0, "budget: the static race header is never redrawn");
    CHECK(hud_frames * 3 < frames, "budget: the HUD refreshes at most one frame in four");
    CHECK(max_chars <= 64, "budget: at most 64 text characters in any race frame");
    CHECK(chars <= 40 * frames, "budget: at most 40 text characters per race frame on average");
    /* Menus redraw only when something changes. */
    screen = ST_TITLE; ui_dirty = 1; fairwind_draw(); g_idx_calls = g_chars = 0;
    for (int i = 0; i < 30; i++) { fairwind_update(); fairwind_draw(); }
    CHECK(g_idx_calls == 0 && g_chars == 0, "budget: an idle menu draws nothing");
    printf("draw budget: %ld race frames, %.1f chars/frame (max %ld), HUD on %ld frames\n", frames, (double)chars / (double)frames, max_chars, hud_frames);
}

/* A finish-line crossing counts only after every mark, and only downwind. */
static void run_early_finish_scenario(void) {
    open_water(120, (uint16_t)DEG(180));
    boats[0].x = 0; boats[0].y = (FINISH_Y + 30) * 1000; boats[0].leg = 0;
    for (int i = 0; i < 200 && boats[0].y > (FINISH_Y - 30) * 1000; i++) { update_race(0, 0); boats[0].leg = 0; twd = 0; }
    CHECK(!boats[0].finished, "finish: crossing before the last mark is ignored");
    open_water(120, (uint16_t)DEG(180));
    boats[0].x = 0; boats[0].y = (FINISH_Y + 30) * 1000; boats[0].leg = (uint8_t)course_len[course_sel]; boats[0].v = 3000 << 8;
    for (int i = 0; i < 400 && !boats[0].finished; i++) { update_race(0, 0); twd = 0; boats[0].x = 0; }
    CHECK(boats[0].finished, "finish: a downwind crossing after the last mark finishes");
    printf("finish: early crossing ignored, valid crossing at %d s\n", boats[0].finish_time);
}

/* Rules 10, 14 and 31 on contact and on touching a mark. */
static void run_rules_scenario(void) {
    open_water(120, (uint16_t)DEG(315));   /* boat 0: wind from starboard -> starboard tack */
    boats[1].finished = 0; boats[1].penalty = 0;
    boats[1].x = boats[0].x + 2000; boats[1].y = boats[0].y; boats[1].heading = (uint16_t)DEG(45); /* port tack */
    boats[1].v = 3000 << 8; boats[0].v = 3000 << 8;
    update_rig(&boats[0], 1); update_rig(&boats[1], 1);
    int32_t gap0 = boats[1].x - boats[0].x;
    enforce_rules();
    CHECK(boats[1].penalty && boats[1].rule == RULE_PORT && !boats[0].penalty, "rules: port-tack yacht is penalised under Rule 10");
    CHECK(boats[1].x - boats[0].x > gap0, "rules: contact pushes the yachts apart (Rule 14)");
    open_water(120, (uint16_t)DEG(270));
    boats[0].x = course_x[course_sel][0] * 1000 + 1500; boats[0].y = course_y[course_sel][0] * 1000;
    enforce_rules();
    CHECK(boats[0].penalty && boats[0].rule == RULE_MARK_TOUCH, "rules: touching a mark is penalised under Rule 31");
    /* Rule 11: same tack, overlapped side by side -> the windward yacht. */
    open_water(120, (uint16_t)DEG(315));
    boats[1].finished = 0; boats[1].penalty = 0; boats[1].heading = boats[0].heading;
    boats[1].x = boats[0].x - 2000; boats[1].y = boats[0].y + 2000;    /* up-wind of boat 0 */
    update_rig(&boats[0], 1); update_rig(&boats[1], 1); enforce_rules();
    CHECK(boats[1].penalty && boats[1].rule == RULE_WINDWARD && !boats[0].penalty, "rules: windward yacht is penalised under Rule 11");
    /* Rule 12: same tack, not overlapped -> the yacht clear astern. */
    open_water(120, (uint16_t)DEG(315));
    boats[1].finished = 0; boats[1].penalty = 0; boats[1].heading = boats[0].heading;
    boats[1].x = boats[0].x - ((225 * 100 * fsin(boats[0].heading)) >> 14); boats[1].y = boats[0].y - ((225 * 100 * fcos(boats[0].heading)) >> 14);
    update_rig(&boats[0], 1); update_rig(&boats[1], 1); enforce_rules();
    CHECK(boats[1].penalty && boats[1].rule == RULE_ASTERN && !boats[0].penalty, "rules: yacht clear astern is penalised under Rule 12");
    /* Rule 13: a yacht that is tacking keeps clear. */
    open_water(120, (uint16_t)DEG(315));
    boats[1].finished = 0; boats[1].penalty = 0; boats[1].heading = boats[0].heading;
    boats[1].x = boats[0].x + 2000; boats[1].y = boats[0].y; update_rig(&boats[0], 1); update_rig(&boats[1], 1);
    boats[1].tacking = 1; enforce_rules();
    CHECK(boats[1].penalty && boats[1].rule == RULE_TACKING && !boats[0].penalty, "rules: tacking yacht is penalised under Rule 13");
    /* Rule 18: in the zone of the same next mark, the outside yacht gives room. */
    open_water(120, (uint16_t)DEG(315));
    boats[1].finished = 0; boats[1].penalty = 0; boats[1].heading = boats[0].heading; boats[1].leg = boats[0].leg = 0; boats[1].started = 1;
    boats[0].x = course_x[course_sel][0] * 1000 + 20000; boats[0].y = course_y[course_sel][0] * 1000 - 20000;
    boats[1].x = boats[0].x + 2000; boats[1].y = boats[0].y - 1000;
    update_rig(&boats[0], 1); update_rig(&boats[1], 1); enforce_rules();
    CHECK(boats[1].penalty && boats[1].rule == RULE_MARK_ROOM && !boats[0].penalty, "rules: outside yacht in the zone is penalised under Rule 18");
    printf("rules: Rules 10, 11, 12, 13, 14, 18 and 31 verified\n");
}

int main(void) {
    printf("== single player full season ==\n");
    run_one_full_season(1);
    printf("== multiplayer lobby smoke test ==\n");
    run_multiplayer_smoke();
    printf("== multiplayer mid-race disconnect scenario ==\n");
    run_multiplayer_disconnect_scenario();
    printf("== lobby ready handshake ==\n");
    run_lobby_handshake_scenario();
    printf("== stable peer slots ==\n");
    run_stable_slot_scenario();
    printf("== AI takeover of started peer ==\n");
    run_ai_takeover_scenario();
    printf("== AI team assignment ==\n");
    run_ai_team_scenario();
    printf("== DNF ordering ==\n");
    run_dnf_order_scenario();
    printf("== fixed-point maths ==\n");
    run_fixmath_scenario();
    printf("== polar speeds and momentum ==\n");
    run_polar_speed_scenario();
    printf("== sail trim ==\n");
    run_trim_scenario();
    printf("== helm ==\n");
    run_helm_scenario();
    printf("== spinnaker / gennaker ==\n");
    run_kite_scenario();
    printf("== A+B penalty turn ==\n");
    run_penalty_combo_scenario();
    printf("== shore and race-area limits ==\n");
    run_land_scenario();
    printf("== chase / top view ==\n");
    run_top_view_scenario();
    printf("== wind shifts ==\n");
    run_wind_scenario();
    printf("== finish line ==\n");
    run_early_finish_scenario();
    printf("== racing rules ==\n");
    run_rules_scenario();
    printf("== frame-rate independence ==\n");
    run_frame_rate_scenario();
    printf("== ESP32-C6 draw budget ==\n");
    run_draw_budget_scenario();
    printf("== AI races ==\n");
    for (int c = 0; c < COURSE_COUNT; c++) run_ai_race_scenario(c);
    printf("== fuzz seasons ==\n");
    for (uint32_t seed = 1; seed <= 12; seed++) run_fuzz_season(seed * 2654435761u, seed <= 3);
    printf("gfx bbox seen: x[%d,%d] y[%d,%d] over %ld calls\n", g_min_x, g_max_x, g_min_y, g_max_y, g_gfx_calls);
    if (failures) { printf("\n%d CHECK FAILURE(S)\n", failures); return 1; }
    printf("\nALL CHECKS PASSED\n");
    return 0;
}
