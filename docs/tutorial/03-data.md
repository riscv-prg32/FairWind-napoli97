# 3. Data: structs, tables and the campaign

## Learning objectives

- Design `struct` types that group related data, and choose integer widths deliberately.
- Use `const` tables (one- and two-dimensional arrays) to separate **data** from **code**.
- Implement the campaign economy (sponsors, prizes, upgrades) with plain integer arithmetic.

## 3.1 Everything is a table

Look at the top of `game.c` and you will find many lines that contain no logic at all:

```c
static const team_t teams[4] = {
    {"PARTENOPE 12",   "ITA", CYAN,   C6(0,4,5), 6, 7, 7},
    {"LIBECCIO CORSE", "ITA", RED,    C6(5,1,1), 7, 6, 6},
    {"ATLANTIC UNION", "USA", WHITE,  C6(4,2,5), 7, 7, 5},
    {"SOUTHERN CROSS", "AUS", 0xffe0, C6(5,4,0), 6, 6, 8}
};
static const sponsor_t sponsors[4] = {
    {"GULF MARINE", 18, 7, 0}, {"VESUVIO TECH", 26, 11, 1},
    {"CAPRI LUX", 38, 16, 2},  {"PARTENOPE GLOBAL", 55, 24, 3}
};
static const char race_names[RACES][16] = {
    "SANTA LUCIA", "VESUVIUS CUP", "CAPRI PASSAGE", "SORRENTO RACE", "NAPLES FINAL"
};
```

This is **data-driven design**: the game's content lives in tables, and the code is written once to work on any row. To add a fifth syndicate you would change a table and a constant, not a dozen `if` statements. The `const` qualifier is important on a microcontroller: constant tables are placed with the code and are never copied into writable memory.

## 3.2 Designing a struct

A `struct` gathers the fields that describe one thing. The syndicate type is simple:

```c
typedef struct {
    char name[18], code[4];      /* fixed-size strings: no malloc          */
    uint16_t color, kite;        /* menu colour, spinnaker colour (RGB565) */
    uint8_t speed, turn, crew;   /* performance ratings for the AI         */
} team_t;
```

The yacht is the central type of the game, and it deserves a careful look:

```c
typedef struct {
    int32_t x, y;                 /* position, millimetres                */
    int32_t v;                    /* speed, mm/s multiplied by 256         */
    int32_t aws;                  /* apparent wind speed, mm/s             */
    int32_t tack_start, ai_last_tack;   /* simulated seconds              */
    uint32_t turned;              /* penalty-turn progress, binary angles  */
    uint16_t heading;             /* binary angle: 65536 = 360 degrees     */
    uint16_t kite_prog;           /* kite hoist progress, 0..65535         */
    uint16_t finish_time, ai_clock;
    int16_t rudder;               /* -100 (hard to port) .. +100           */
    int16_t boom;                 /* degrees, + = starboard                */
    int16_t twa, awa;             /* true / apparent wind angle, binary    */
    int16_t hacc;                 /* sub-unit heading accumulator          */
    int8_t tack, heel, serve_dir, ai_tack;
    uint8_t team, leg, finished, penalty, serving, kite, kite_want, sheet,
            started, start_ready, entered_box, tacking, rule, mark_touch,
            touching, aground, ai_above;
} boat_t;
```

Several principles are at work here:

- **Units are part of the type.** Every comment states a unit. A position in *millimetres* stored in a 32-bit integer can represent ±2,147 km, far more than the 2 km race field, and still resolves the few millimetres a slowly drifting yacht moves in one frame. Chapter 4 explains how each unit was chosen.
- **The smallest sufficient width.** A flag needs one byte, not four. With four yachts the saving is small, but the habit matters, and a smaller struct is also faster to copy.
- **Signedness follows meaning.** `rudder` and `boom` have a side, so they are signed. `sheet` (0–90 degrees eased) and `leg` (which mark is next) are not.
- **Order by size.** Placing the 4-byte fields first, then the 2-byte fields, then the bytes avoids padding. Try `printf("%zu\n", sizeof(boat_t));` on your computer, then shuffle the fields and print it again.

All four yachts live in one array, `static boat_t boats[4];`. Slot 0 is always the local player, which is why `&boats[0]` appears so often. A yacht is reset at the start of each race by zeroing the whole struct and then setting the few fields that must not be zero:

```c
memset(b, 0, sizeof *b);
b->team = team;
b->x = side * (LINE_HALF + 60 + 20 * (team >> 1)) * 1000;   /* mm */
b->y = (90 + 30 * (team >> 1)) * 1000;
b->sheet = 90;                           /* sails eased, luffing: stopped */
```

## 3.3 Two-dimensional tables: the courses

The three courses are lists of marks, stored as two parallel 2D arrays in metres:

```c
static const uint8_t course_len[COURSE_COUNT] = {3, 5, 6};
static const int16_t course_x[COURSE_COUNT][COURSE_MARKS_MAX] = {
    {0,    0, 0,   0,    0, 0},      /* windward / run   */
    {0, -360, 0,   0,    0, 0},      /* Olympic triangle */
    {0,  360, 0,   0, -360, 0}};     /* 1992 IACC Z      */
static const int16_t course_y[COURSE_COUNT][COURSE_MARKS_MAX] = {
    {800, 120, 800,   0,   0,   0},
    {800, 450, 120, 800, 120,   0},
    {800, 450, 120, 800, 450, 800}};
```

`course_x[c][i]` is the across-wind coordinate of mark `i` of course `c`. The next mark of a yacht is always `course_x[course_sel][b->leg]`. Unused entries are zero and are never read, because `course_len` bounds every loop. This is the **sentinel-free** way to store ragged lists in a fixed array: carry the length alongside.

## 3.4 The campaign economy

Between races the player manages money. The rules fit in two short functions.

**Scoring a race** sorts the fleet by finishing time with a simple exchange sort (four elements do not need anything cleverer), awards points, and pays the player:

```c
static void score_race(void) {
    int i, j;
    for (i = 0; i < 4; i++) result_order[i] = (uint8_t)i;
    for (i = 0; i < 4; i++)
        for (j = i + 1; j < 4; j++)
            if (boats[result_order[j]].finish_time < boats[result_order[i]].finish_time) {
                uint8_t t = result_order[i];
                result_order[i] = result_order[j];
                result_order[j] = t;
            }
    for (i = 0; i < 4; i++) {
        points[boats[result_order[i]].team] += 4 - i;
        if (result_order[i] == 0) player_rank = (uint8_t)(i + 1);
    }
    last_prize = 30 - (player_rank - 1) * 7;
    if (player_rank == 1) { wins++; last_prize += sponsors[sponsor].win_bonus; }
    money += last_prize + sponsors[sponsor].fee;
    if (sponsor < 3 && wins >= sponsors[sponsor + 1].min_wins) sponsor++;
}
```

Notice that the sort permutes an **index array** (`result_order`) and leaves the yachts where they are. Slot 0 must stay the player, so moving the structs themselves would break the rest of the game.

**Development** costs grow linearly with the level already reached, and selling returns half the price:

```c
static int upgrade_cost(int level) { return 8 + level * 7; }
```

The ratings then enter the physics as small percentage multipliers (Chapter 6). For example, the player's performance factor is `97 + hull + sails + strategy / 2` percent. A fully developed yacht (level 8 in hull and sails, 8 in strategy) sails at 117% of its base polar, while the AI teams sail at 102–103%.

## Check your understanding

1. Why are `teams` and `sponsors` declared `const`? What would change in memory if they were not?
2. Why does `score_race` sort `result_order` instead of `boats`?
3. `finish_time` is `uint16_t`. What is the longest race it can record, in simulated seconds? Is that enough (see `RACE_LIMIT_SECONDS`)?

## Exercises

- ★ Add a sixth sponsor tier that requires five wins. What other constants must change?
- ★★ Add a fourth course, "Short windward/leeward", with two marks. Update `COURSE_COUNT`, `course_len`, `course_names` and the two coordinate tables, and run the harness: the AI race scenario tests every course automatically.
- ★★★ Replace the parallel arrays `course_x`/`course_y` with an array of `struct { int16_t x, y; }`. Measure the change in code size and argue which layout is clearer.
