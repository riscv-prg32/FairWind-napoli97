# 5. Time and wind: simulated time, frame time and deterministic randomness

## Learning objectives

- Distinguish **real time**, **frame time** and **simulated time**, and convert between them.
- Make a simulation independent of the frame rate by integrating the measured frame time.
- Generate "random-looking" but **reproducible** behaviour with sums of sines and a pseudo-random generator.

## 5.1 Three clocks

A race of twelve-metre yachts lasts about twenty minutes. Nobody wants to hold a console for twenty minutes of real time per race, five races a season. Nor do we want the yachts to move like go-karts. FairWind therefore keeps three notions of time:

| Clock | Unit | Source |
|---|---|---|
| Real time | milliseconds | `prg32_ticks_ms()`, the firmware's timer |
| Frame time | milliseconds per frame | difference between two successive real-time readings |
| Simulated time | milliseconds or seconds of race time | frame time × time scale |

The **time scale** says how many simulated seconds pass per real second:

```c
#define FAST_TIME_SCALE 20      /* long waiting before the start box opens */
#define RACE_TIME_SCALE 4       /* from two minutes before the start       */

static int time_scale(void) { return start_clock > 120 ? FAST_TIME_SCALE : RACE_TIME_SCALE; }
```

At 4× a yacht sailing at 8 knots (4.1 m/s) covers 16.5 m of water per real second, about three-quarters of its own length. On screen it looks right, and a race takes five to six minutes of play. The first eight minutes of the ten-minute starting sequence, in which nothing much happens, run at 20×, and the HUD shows `x20`.

## 5.2 Integrating the frame time

The PRG32 firmware tries to call the game every 33 ms. If a frame takes longer (a busy moment, a slow screen transfer) the next call simply comes later. A naive simulation that advances "one step per frame" would then run *slower* when the machine is busy. Worse, two consoles in a multiplayer race would disagree about where the yachts are.

FairWind measures the real duration of every frame and advances the simulation by exactly that much:

```c
/* in fairwind_update */
uint32_t now = prg32_ticks_ms();
frame_ms = last_ticks ? clamp((int)(now - last_ticks), 10, 66) : FRAME_MS;
last_ticks = now;

/* simulated milliseconds that pass during this frame */
static int32_t sim_ms(void) { return (int32_t)time_scale() * frame_ms; }
```

Two details deserve attention:

- `now - last_ticks` is computed in **unsigned** arithmetic, so it stays correct even when the millisecond counter wraps round after 49 days. This is the same trick as the binary angles of Chapter 4.
- The measured time is **clamped** to 10–66 ms. A pause (say the firmware showed a menu, or this is the first frame after loading) must not make the yachts jump hundreds of metres in one step.

Every rate in the physics is then expressed *per simulated second* and multiplied by `sim_ms()`. For example, the boom swings at 45 degrees per simulated second:

```c
step = (int)(45 * sm / 1000);          /* sm = sim_ms() */
```

This is the **explicit Euler method**: new value = old value + rate × Δt. It is the simplest numerical integration scheme you will meet in a numerical-methods course. It is accurate enough when Δt is small compared with the time constants of the system; here Δt ≈ 0.13 s and the yacht's time constants are around 10 s.

The harness verifies the result. The scenario `run_frame_rate_scenario` sails the same manoeuvre once at 30 frames per second and once at 15, and compares the yachts after 53 simulated seconds. They differ by about **one metre** and 0.0 knots.

## 5.3 Whole seconds: an accumulator

Some things happen once per simulated second: the race clock ticks, the start signals sound, the wind is recomputed. With a variable frame time the number of simulated milliseconds per frame is not a divisor of 1,000, so FairWind **accumulates** them:

```c
for (sim_acc += sim_ms(); sim_acc >= 1000; ) {
    sim_acc -= 1000;
    wind_t++;                      /* seconds since the start of the sequence */
    update_wind();
    if (start_clock > 0) {
        start_clock--;             /* countdown: 600 .. 0 seconds */
        if (start_clock == 300 || start_clock == 240 || start_clock == 60) play_start_signal(0);
        else if (start_clock == 0) play_start_signal(1);
    } else
        race_clock++;
}
```

The remainder stays in `sim_acc`, so no time is ever lost to rounding. At 20× a frame can be worth more than a simulated second, which is why this is a `for`/`while` loop and not an `if`.

## 5.4 Wind that shifts, but not randomly

Real sea breezes are never steady. In the Gulf of Naples the summer breeze blows from the south-west and **oscillates** by several degrees, with periods of a few minutes. Sailors win races by noticing these shifts (Chapter 8). The game needs shifts that are:

- **realistic**: a few degrees, slowly varying;
- **different** between races;
- **identical** on every console of a multiplayer race, without sending wind data over the network.

The solution is to make the wind a **pure function of time and a seed**:

```c
static void update_wind(void) {
    uint32_t s = wind_seed;
    int32_t  t = wind_t, trend = (int32_t)((s >> 19) % 11u) - 5;   /* -5..+5 degrees */
    int32_t shift = ((DEG(7) * fsin((uint16_t)(t * 156 + (int32_t)(s >> 3))))  >> 14)
                  + ((DEG(4) * fsin((uint16_t)(t * 345 + (int32_t)(s >> 11)))) >> 14)
                  + trend * DEG(1) * t / 3300;
    twd = (uint16_t)(PREVAILING_SW_HEADING + shift);
    tws10 = 80 + 20 * (int)(s % 5u)                                 /* 8..16 knots base  */
          + (int)((15 * fsin((uint16_t)(t * 504  + (int32_t)(s >> 5))))  >> 14)
          + (int)(( 8 * fsin((uint16_t)(t * 1191 + (int32_t)(s >> 13)))) >> 14);
}
```

The shift in direction is the sum of three terms:

1. a ±7° oscillation. Its period is 65,536 / 156 ≈ 420 s, seven minutes;
2. a ±4° oscillation with a period of about 190 s;
3. a slow **persistent** veer or back of up to ±5° over the whole race.

Different bits of the seed set the phases, so the two oscillations are out of step and the pattern does not repeat within a race. Summing sines of different periods is a standard way of making smooth, natural-looking variation. Section 4.3's `fsin` makes it cheap.

Wind *speed* is built the same way: a base of 8–16 knots chosen by the seed, plus two faster "pressure pulses".

The seed is derived from the course, and in single player also from the race number:

```c
uint32_t s = 0x9e3779b9u * (uint32_t)(course_sel + 1)
           ^ (multiplayer ? 0u : (uint32_t)race_no * 0x85ebca6bu);
s ^= s >> 15; s *= 0x2c1b3c6du; s ^= s >> 12;          /* mix the bits */
wind_seed = s;
```

The constants are "hash multipliers": large odd numbers with well-mixed bits. Multiplying and xor-shifting spreads a small input (0, 1, 2) over all 32 bits of the seed, so neighbouring courses get unrelated winds. In multiplayer the race number is left out, because two players may be at different points of their own campaigns but must sail the same breeze.

## 5.5 Pseudo-random numbers for the AI

The AI rivals need a little noise so that they do not steer with inhuman precision. FairWind uses a 32-bit **xorshift** generator (G. Marsaglia, 2003):

```c
static uint32_t rng = 0x97ca1997u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
```

Three shifts and three exclusive-ors produce a sequence that visits every non-zero 32-bit value before repeating. It is not suitable for cryptography, but it is ideal for games: fast, tiny and **reproducible**. The same starting value gives the same sequence, so a bug seen once in a test can be reproduced exactly.

## Check your understanding

1. At 4×, how many simulated milliseconds pass in a 33 ms frame? In a 50 ms frame?
2. Why is the frame time clamped to at most 66 ms? What could happen without the clamp?
3. Why can the wind be computed independently on each console, while the yachts' positions must be sent over the network?

## Exercises

- ★ Change `RACE_TIME_SCALE` to 3 and run the AI race scenario of the harness. How do the finishing times (in simulated seconds) change? And the real duration?
- ★★ Plot the wind shift over a whole race: write a small host program that calls `update_wind()` for `wind_t` = 0…3,300 and prints `wind_shift_deg()`. Plot it with any tool. Identify the two periods and the trend.
- ★★★ Replace explicit Euler with the *semi-implicit* (symplectic) Euler method for position and speed. Show with the frame-rate scenario that the difference between 15 and 30 fps shrinks or grows, and explain why.
