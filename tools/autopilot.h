/* Autopilot for tools and measurements, never for the shipped game: it asks
   helm_ai what it would do with the player's yacht and presses the buttons a
   human would press to follow it. Include after game.c. */
#ifndef FAIRWIND_AUTOPILOT_H
#define FAIRWIND_AUTOPILOT_H
static inline uint32_t autopilot_input(void) {
    static boat_t shadow;
    static uint8_t kite_hold;
    boat_t s = boats[0];
    uint32_t in = 0;
    s.ai_clock = shadow.ai_clock; s.ai_tack = shadow.ai_tack;
    s.ai_last_tack = shadow.ai_last_tack; s.ai_above = shadow.ai_above;
    s.rudder = 0;                    /* read the helm's intent, not its slew */
    helm_ai(&s);
    shadow = s;
    if (boats[0].penalty && !boats[0].serving) return PRG32_BTN_A | PRG32_BTN_B;
    if (s.rudder <= -10) in |= PRG32_BTN_LEFT; else if (s.rudder >= 10) in |= PRG32_BTN_RIGHT;
    if (s.sheet > boats[0].sheet + 1) in |= PRG32_BTN_UP; else if (s.sheet + 1 < boats[0].sheet) in |= PRG32_BTN_DOWN;
    if (kite_hold) { kite_hold--; return in; }  /* release: kites toggle on release */
    if (s.kite_want != boats[0].kite_want) {
        kite_hold = 3;
        return in | ((s.kite_want == KITE_SPIN || (s.kite_want == KITE_NONE && boats[0].kite_want == KITE_SPIN)) ? PRG32_BTN_A : PRG32_BTN_B);
    }
    return in;
}
/* Walk the menus into a race weekend: A through title, mode and team, down
   to RACE WEEKEND in Team HQ, A at the briefing and results. */
static inline uint32_t autopilot_menus(uint32_t frame_no) {
    if (screen == ST_MANAGER && menu < 5) return (frame_no & 8) ? PRG32_BTN_DOWN : 0;
    return (frame_no % 24) < 3 ? PRG32_BTN_A : 0;
}
#endif
