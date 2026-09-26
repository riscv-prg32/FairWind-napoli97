# 2. The game loop and the screen state machine

## Learning objectives

- Model the flow of a program as a **finite-state machine** and implement it with an `enum`.
- Detect button **presses**, **releases** and **chords** from a bit mask using bitwise operators.
- Explain why *update* and *draw* are separated, and use a **dirty flag** to avoid unnecessary work.

## 2.1 A program as a set of screens

A player of FairWind moves through a fixed sequence of screens: title, mode selection, team selection, the team headquarters, the race briefing, (optionally) the multiplayer lobby, the race, the result, and after five races the final classification. At any moment the program is in exactly **one** of these states, and well-defined events (a button press, the end of a race) move it to another. This is a **finite-state machine (FSM)**.

In C an FSM is naturally written with an enumeration:

```c
typedef enum {
    ST_TITLE, ST_MODE, ST_TEAM, ST_MANAGER, ST_BRIEF,
    ST_LOBBY, ST_RACE, ST_RESULT, ST_SEASON
} screen_t;

static screen_t screen;           /* the current state */
```

An `enum` gives names to small integers (`ST_TITLE` is 0, `ST_MODE` is 1, …). The compiler does not stop you assigning any integer to `screen`, but named constants make the code read like its specification.

The transitions are:

| From | Event | To |
|---|---|---|
| `ST_TITLE` | A pressed | `ST_MODE` |
| `ST_MODE` | Up/Down | `ST_MODE` (toggle single/multiplayer) |
| `ST_MODE` | A pressed | `ST_TEAM` (a new campaign starts) |
| `ST_TEAM` | Left/Right | `ST_TEAM` (choose a syndicate) |
| `ST_TEAM` | A pressed | `ST_MANAGER` |
| `ST_MANAGER` | A on "RACE WEEKEND" | `ST_BRIEF` |
| `ST_BRIEF` | A, single player | `ST_RACE` |
| `ST_BRIEF` | A, multiplayer, room joined | `ST_LOBBY` |
| `ST_LOBBY` | everyone ready, or B | `ST_RACE` |
| `ST_RACE` | all yachts finished, or time limit | `ST_RESULT` |
| `ST_RESULT` | A pressed | `ST_MANAGER`, or `ST_SEASON` after race 5 |
| `ST_SEASON` | A pressed | `ST_TITLE` |

Draw this table as a graph on paper, with states as circles and transitions as arrows. You will find that you can check the code against the drawing line by line. This is the main benefit of thinking in states.

## 2.2 Implementing the machine

`fairwind_update` is a chain of `if … else if` on the current state. Each branch reads the input, possibly changes some variables, and possibly changes `screen`:

```c
if (screen == ST_TITLE && (p & PRG32_BTN_A)) screen = ST_MODE;
else if (screen == ST_MODE) {
    if (p & (PRG32_BTN_UP | PRG32_BTN_DOWN)) multiplayer = !multiplayer;
    if (p & PRG32_BTN_A) new_campaign();        /* sets screen = ST_TEAM */
}
else if (screen == ST_TEAM) {
    if (p & PRG32_BTN_LEFT)  team_sel = (team_sel + 3) % 4;
    if (p & PRG32_BTN_RIGHT) team_sel = (team_sel + 1) % 4;
    if (p & PRG32_BTN_A) { menu = 0; screen = ST_MANAGER; }
}
/* ... */
```

Notice `(team_sel + 3) % 4`. Going *left* from team 0 should wrap round to team 3. `(team_sel - 1) % 4` would give `-1` for `team_sel == 0`, because in C the remainder has the sign of the dividend. Adding 3 (that is, 4 − 1) before taking the remainder keeps the value non-negative. This is a common idiom for circular menus.

`fairwind_draw` has the same shape but only *reads* the state:

```c
if (screen == ST_TITLE)        draw_title();
else if (screen == ST_MODE)    draw_mode();
/* ... */
else if (screen == ST_RACE)    draw_race();
```

## 2.3 Why separate update and draw?

It would be possible to update and draw in one function. Keeping them apart brings three concrete benefits, all used in FairWind:

1. **Testing without a screen.** The harness can call `fairwind_update` millions of times and draw only occasionally. A season of five races runs in seconds.
2. **Determinism.** The state after *n* updates depends only on the inputs, never on how often the screen was painted. This is essential for multiplayer (Chapter 12).
3. **Skipping drawing.** If nothing has changed, `draw` can do nothing at all. On the PRG32 this saves the most expensive part of a frame, the transfer to the LCD (Chapter 11).

## 2.4 Reading buttons: bit masks and edges

`prg32_input_read()` returns a 32-bit integer in which each button is one bit:

```c
#define PRG32_BTN_LEFT  (1u << 0)
#define PRG32_BTN_RIGHT (1u << 1)
#define PRG32_BTN_UP    (1u << 2)
#define PRG32_BTN_DOWN  (1u << 3)
#define PRG32_BTN_A     (1u << 4)
#define PRG32_BTN_B     (1u << 5)
```

`in & PRG32_BTN_A` is non-zero while A is **held**. In a menu we usually want to react once per **press**, not thirty times per second while the finger is down. Keeping the previous frame's mask in `last_input` and using the bitwise operators gives both edges:

```c
uint32_t p = in & ~last_input;   /* pressed:  1 now, 0 before */
uint32_t r = last_input & ~in;   /* released: 0 now, 1 before */
```

| `last_input` bit | `in` bit | `in & ~last_input` | `last_input & ~in` | meaning |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | idle |
| 0 | 1 | **1** | 0 | just pressed |
| 1 | 1 | 0 | 0 | held |
| 1 | 0 | 0 | **1** | just released |

The race uses **held** buttons for steering and sheet trimming. It uses **releases** for the spinnaker (A) and gennaker (B), because of the next problem.

## 2.5 Chords: A+B without accidents

The game uses the chord **A+B** for a penalty turn, while A alone toggles the spinnaker and B alone the gennaker. A human never presses two buttons in exactly the same 33 ms frame. If A reacted on *press*, the spinnaker would be hoisted in the frame before B arrived. The solution is to act on *release*, and to remember whether a chord happened while the buttons were down:

```c
if ((in & (PRG32_BTN_A | PRG32_BTN_B)) == (PRG32_BTN_A | PRG32_BTN_B)) {
    ab_combo = 1;                          /* both held: a chord        */
    begin_penalty_turn(me);
} else {
    if ((released & PRG32_BTN_A) && !ab_combo)
        me->kite_want = me->kite_want == KITE_SPIN ? KITE_NONE : KITE_SPIN;
    if ((released & PRG32_BTN_B) && !ab_combo)
        me->kite_want = me->kite_want == KITE_GENN ? KITE_NONE : KITE_GENN;
}
if (!(in & (PRG32_BTN_A | PRG32_BTN_B))) ab_combo = 0;   /* all released */
```

`ab_combo` is a one-bit piece of state that lives across frames. It is set when both buttons are down and cleared only when *both* are up, so the releases that end a chord are ignored. `start_race` also sets `ab_combo = 1`: the A press that left the briefing screen must not hoist a spinnaker when the finger lifts in the first race frame. Bugs like this are found by thinking about *edges between states*, which is exactly what the FSM view encourages.

## 2.6 The dirty flag

Menus are static: the title screen looks the same until a button is pressed. Redrawing it thirty times per second wastes energy and, on the PRG32, keeps the LCD bus busy. FairWind marks the menu as **dirty** when something may have changed, and draws only then:

```c
/* in fairwind_update */
if (p) ui_dirty = 1;                        /* any press may change a menu */

/* in fairwind_draw */
if (screen != ST_RACE) {
    if (!ui_dirty && (int)screen == drawn_screen &&
        !(screen == ST_LOBBY && frame % 15 == 0))
        return;                             /* nothing new: draw nothing */
    ui_dirty = 0;
    prg32_gfx_clear_indexed(ci(SEA));
}
drawn_screen = (int)screen;
```

Three conditions force a redraw:
- **input:** a button was pressed;
- **a new screen:** `drawn_screen` differs from `screen`;
- **the lobby clock:** twice a second, the lobby redraws, because remote players can join without any local input.

This pattern (*invalidate on change, redraw on demand*) is used by every windowing system you have ever used.

## Check your understanding

1. What would happen if `fairwind_draw` changed `screen`? Which of the three benefits of §2.3 would be lost?
2. Compute `(0 - 1) % 4` and `(0 + 3) % 4` in C.
3. Why is `ab_combo` cleared only when *both* A and B are released, not when either is?

## Exercises

- ★ Add a transition: pressing B on `ST_MODE` returns to `ST_TITLE`. Remember to mark the menu dirty.
- ★★ The harness function `tap()` presses and releases a button in two frames. Write a harness test that verifies that holding A across the briefing-to-race transition does **not** hoist a spinnaker.
- ★★ Replace the `if … else if` chain in `fairwind_update` with a table of function pointers, one `update` and one `draw` function per state. Compare readability and code size (`riscv32-esp-elf-size`).
