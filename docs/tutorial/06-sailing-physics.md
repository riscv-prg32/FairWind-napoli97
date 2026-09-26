# 6. Sailing physics: apparent wind, polars, trim and momentum

## Learning objectives

- Compute the **apparent wind** by vector subtraction in a rotating frame of reference.
- Use **bilinear interpolation** in a measured table (a *polar*) instead of a physical formula.
- Model a heavy body's acceleration as a **first-order system** with a time constant.
- Implement small **state machines** inside the physics (boom, kites, tacking).

## 6.1 Modelling strategy

A sailing yacht is a genuinely hard physical system: aerodynamics of sails, hydrodynamics of hull and keel, heel, waves. A game cannot solve the equations of fluid dynamics thirty times per second on a microcontroller, and does not need to. FairWind follows the approach of professional race-analysis software:

1. use **measured data** for the steady-state behaviour: how fast does this boat go at each wind angle and speed? This is the *polar*;
2. use **simple dynamic models** for the transitions: how quickly does it get there? How much speed does a tack cost?

This division between *what* (data) and *how fast* (dynamics) is a powerful general idea in simulation.

## 6.2 Frames of reference and the apparent wind

The wind blows at speed TWS (true wind speed) from direction TWD. A yacht moving through the air feels the **apparent wind**, the vector difference between the air's velocity and its own. Try it on a bicycle: on a calm day you still feel a headwind. It is the apparent wind that fills the sails.

The calculation is easiest in the **boat's frame of reference**: an axis *f* pointing forward and an axis *s* pointing to starboard. Let TWA be the true wind angle, the direction the wind comes from measured clockwise from the bow. The wind's "from" vector has components *(TWS·cos TWA, TWS·sin TWA)*. The boat's motion adds a headwind equal to its speed *v* on the forward axis. So:

```
w_f = TWS · cos(TWA) + v          (forward component of the apparent wind)
w_s = TWS · sin(TWA)              (sideways component)
AWA = atan2(w_s, w_f)             (apparent wind angle)
AWS = sqrt(w_f² + w_s²)           (apparent wind speed)
```

In FairWind (`update_rig`), with the fixed-point tools of Chapter 4:

```c
b->twa = (int16_t)(twd - b->heading);                       /* binary angle */
wf = ((tws * fcos((uint16_t)b->twa)) >> 14) + (b->v >> 8);  /* mm/s         */
ws =  (tws * fsin((uint16_t)b->twa)) >> 14;
b->awa = (int16_t)bearing(ws, wf);
b->aws = isqrt((uint32_t)(wf * wf + ws * ws));
```

**Worked example.** A 12-knot breeze (6.17 m/s) at TWA 90° (a beam reach), with the yacht doing 8.7 knots (4.48 m/s):

- w_f = 6.17·cos 90° + 4.48 = 4.48 m/s
- w_s = 6.17·sin 90° = 6.17 m/s
- AWA = atan2(6.17, 4.48) ≈ 54°
- AWS ≈ 7.6 m/s

The sails feel a wind well *forward* of the beam and stronger than the true wind. This is why fast boats sheet their sails in tighter than the true wind angle suggests, and our trim model (§6.5) works with the apparent angle for that reason.

The sign of TWA also tells us the **tack**: wind from starboard (TWA > 0) is starboard tack.

## 6.3 The polar table

Naval architects measure or compute a yacht's **velocity prediction** for many wind speeds and angles and publish it as a *polar*. FairWind uses published ORC data for a real 12-Metre, *ANITA* (see `docs/POLAR_MODEL.md`), in tenths of a knot:

```c
static const uint8_t polar_angle[13] = {0, 28, 34, 40, 52, 60, 75, 90, 110, 120, 135, 150, 180};
static const uint8_t polar8[13]  = {0, 0, 40, 63, 71, 74, 76, 76, 76, 74, 68, 57, 50};  /*  8 kt */
static const uint8_t polar12[13] = {0, 0, 46, 72, 81, 84, 87, 88, 89, 89, 86, 79, 69};  /* 12 kt */
static const uint8_t polar16[13] = {0, 0, 48, 75, 84, 88, 91, 93, 94, 96, 94, 91, 82};  /* 16 kt */
```

Read a row: in 12 knots of wind, at 90° to the wind, the yacht sails at 8.8 knots. Below 28° the speed is zero. No sailing boat can sail directly into the wind; this is the *no-go zone*.

Between the tabulated values the game **interpolates linearly**, first along the angle (`row_at`), then between the wind-speed columns (`polar_speed10`). Two successive linear interpolations make a *bilinear* interpolation:

```c
static int row_at(const uint8_t *p, int a) {             /* a in degrees, 0..180 */
    int i;
    for (i = 1; i < 13 && a > polar_angle[i]; i++) {}    /* find the bracket     */
    if (i >= 13) return p[12];
    return p[i-1] + (p[i] - p[i-1]) * (a - polar_angle[i-1])
                                    / (polar_angle[i] - polar_angle[i-1]);
}
static int polar_speed10(int twa, int tws) {             /* tws in 0.1 knot      */
    int lo, hi;
    if (tws <= 80)  return row_at(polar8, twa) * tws / 80;   /* light air: scale */
    if (tws >= 160) return row_at(polar16, twa);
    if (tws <= 120) { lo = row_at(polar8, twa);  hi = row_at(polar12, twa); return lo + (hi - lo) * (tws - 80)  / 40; }
    lo = row_at(polar12, twa); hi = row_at(polar16, twa);        return lo + (hi - lo) * (tws - 120) / 40;
}
```

The general formula for linear interpolation between (x₀, y₀) and (x₁, y₁) is y = y₀ + (y₁ − y₀)(x − x₀)/(x₁ − x₀). Multiply before dividing, as the code does, so that integer division does not throw the fraction away.

## 6.4 Sails as efficiency factors

The polar assumes the right sails are set. FairWind has three sail plans:
- the **jib** (always available);
- the **spinnaker**, symmetric, for running downwind;
- the **gennaker**, asymmetric, for reaching.

Each has an efficiency table on the same angle rows:

```c
static const uint8_t plan_jib[13]  = {100,100,100,100,100,100,100,100, 90, 84, 78, 74, 72};
static const uint8_t plan_spin[13] = { 40, 40, 40, 40, 40, 45, 60, 88,100,100,100,100,100};
static const uint8_t plan_genn[13] = { 50, 50, 50, 55, 65, 80,102,106,104,100, 95, 90, 82};
```

The ORC rows above 90° already assume a kite, so the jib alone loses up to 28% downwind. A spinnaker set on a reach collapses (40–60%), and the gennaker is best between 75° and 110°. While a kite is being hoisted the efficiency is **blended** by the hoist progress, so a half-hoisted spinnaker gives half of its benefit:

```c
return j + (int)((k - j) * (int32_t)b->kite_prog / 65535);   /* j: jib, k: kite */
```

## 6.5 Trimming: the angle of attack

A sail works like an aircraft wing. It needs a small **angle of attack** to the apparent wind, about 15–20°. With less, the front of the sail collapses and flaps (*luffing*); with more, the airflow separates (*stall*). The player controls the **sheet**, the maximum angle the boom may swing out. The wind then pushes the boom out to leeward until the sheet stops it, or until the sail streams like a flag if the sheet is looser than the wind angle:

```c
side   = b->awa >= 0 ? -1 : 1;          /* leeward side: opposite the wind */
lim    = b->sheet < awa ? b->sheet : awa;
target = side * lim;                    /* where the boom wants to be      */
```

The best boom angle keeps 18° of attack, and the efficiency falls away on either side:

```c
static int trim_opt(int awa) { return clamp(awa - 18, 2, 85); }
static int trim_eff(const boat_t *b) {
    int err = absi(b->boom) - trim_opt(awa_deg(b));
    if (err > 0) return clamp(100 - err * err * 100 / 324,  0, 100);  /* eased: luffing  */
    return        clamp(100 - err * err * 100 / 2025, 30, 100);       /* trimmed: stall  */
}
```

The two parabolas are deliberately asymmetric. Easing 18° too far (324 = 18²) kills all drive, because the sail is flapping. Over-trimming by as much as 45° (2025 = 45²) still leaves 30%, because a stalled sail still pushes. The HUD's trim bar shows the green optimum and the white boom, and the words `LUFF`, `GOOD` and `STALL`.

The boom does not jump to its target. It moves at 45° per simulated second, and at 180° per second if it is crossing the centre line with the wind behind it. That is a *gybe*, when the boom slams across. It is a two-speed rate limiter, the simplest possible model of the sail's own inertia.

## 6.6 Momentum: a first-order system

Target speed = polar × sail plan × trim × performance. But a 26-tonne keelboat does not reach its target instantly. The simplest model of a system that "approaches a target" is the **first-order lag**:

```
dv/dt = (v_target − v) / τ
```

where τ (tau) is the **time constant**: after τ seconds the boat has covered 63% of the gap, after 3τ about 95%. Integrated with the Euler method of Chapter 5:

```c
tau = target > v ? 11 - cr / 2 : 14;                   /* seconds: accelerate / coast */
b->v += ((target << 8) - b->v) / (10 * tau) * sm / 100;
```

A better crew (`cr`) accelerates faster (τ from 11 down to 7 s), and every yacht coasts with τ = 14 s. This single line produces most of the "feel" of the boat:

- **Tacking costs speed.** The bow passes through the no-go zone, the target drops to zero for a few seconds, and the boat coasts down.
- **Luffing at the start line slows the boat gradually**, as real crews do.
- **A penalty turn** loses time in proportion to its duration.

## 6.7 Steering and moving

The rudder turns the boat faster when the boat moves faster, because the water flow over the blade is stronger. The game uses a rate from 3°/s at rest to 12°/s at 8 knots and above. The rudder also brakes a little:

```c
b->v -= (b->v / 1000) * absi(b->rudder) * sm / 5000;           /* ≤ 2 % per s */
q = (int32_t)b->rudder * (300 + 900 * vv / 4100) * 182 / 100;  /* 100 × BAM/s */
q = q * sm / 100 * (92 + 2 * cr) / 100 * 256 / 1000 + b->hacc;
b->heading = (uint16_t)(b->heading + (q >> 8));
b->hacc    = (int16_t)(q & 255);
```

`hacc` keeps the fraction of a binary angle that did not fit into this frame's turn, and adds it to the next. Without it, a small rudder angle at low speed would round to zero and the boat would never turn. This is the same idea as the time accumulator of Chapter 5, and it is a recurring pattern in fixed-point code.

Finally the position advances along the heading, and a small **leeway drift** pushes the boat downwind at 1.5% of the wind speed, so a stopped yacht slowly drifts backwards:

```c
step = v * sm / 1000;                                       /* mm this frame */
b->x += (step * fsin(b->heading)) >> 14;
b->y += (step * fcos(b->heading)) >> 14;
```

## 6.8 The shore

The race area is a rectangle, 1.8 km across and 1.7 km upwind. Naples lies beyond its downwind edge, the Sorrento cliffs to port and Posillipo to starboard; the upwind edge is the race-area limit. If a move would leave the rectangle, `clamp_field` puts the yacht back on the boundary, and the speed is halved every frame while it stays there. The yacht stops dead against the shore and must steer away, which is what "the boats cannot sail on the background" means in practice.

## 6.9 Checking the model

The harness measures the model against the polar:

| Situation (12 kt true wind) | Polar | Simulated |
|---|---|---|
| Beam reach, trimmed | 8.8 kt | 8.7 kt |
| Close-hauled (42°) | 7.3 kt | 7.2 kt |
| Head to wind | 0 kt | < 1 kt (coasting) |
| Sheets fully eased on a reach | – | 0 kt (luffing) |
| Run: jib vs spinnaker | – | 5.5 vs 7.5 kt |

The 1% difference on the reach is the base performance of an undeveloped yacht (99.5%).

## Check your understanding

1. At TWA 180° (dead downwind) with the boat at 6 knots in 12 knots of wind, what are AWA and AWS?
2. Why does the trim model use the *apparent* rather than the *true* wind angle?
3. After how many seconds does a yacht with τ = 10 s reach 95% of its target speed?

## Exercises

- ★ Change the stall floor from 30% to 50%. Does the close-hauled speed change? Why not?
- ★★ Add a *heel penalty*: above 20° of heel, reduce the target by 1% per degree. Where is the heel computed? Write a harness test for strong wind.
- ★★★ Replace the first-order model with a force balance: drive force from the polar at the current speed, drag growing with v². Tune it so the harness's steady-state speeds are unchanged, and compare the acceleration curves.
