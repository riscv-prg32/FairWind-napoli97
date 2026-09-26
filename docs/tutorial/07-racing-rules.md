# 7. Racing: the start, the marks, the finish and the rules

## Learning objectives

- Detect a **line crossing** between two frames with a sign test.
- Detect **collisions** between elongated objects using sampled points and squared distances.
- Encode a set of rules with **priorities** so that exactly one decision is made.
- Use **hysteresis flags** so that one event is counted once.

## 7.1 The course coordinate system

All race geometry is expressed in a frame aligned with the mean wind:
- **y** points upwind, into the south-westerly;
- **x** points to starboard when looking upwind;
- the origin is the middle of the start line.

| Object | Coordinates (metres) |
|---|---|
| Start line | y = 0, from x = −110 (pin buoy) to x = +110 (committee boat) |
| Pre-start box | −150 ≤ x ≤ 150, −200 ≤ y ≤ 0 |
| Windward mark | (0, 800) |
| Finish line | y = −80, −90 ≤ x ≤ 90 |

Choosing coordinates that match the problem makes most tests one-dimensional. "Upwind of the line" is simply `y > 0`.

## 7.2 The starting sequence

The start follows the match-racing sequence of the America's Cup:

| Time before the start | Signal | Game behaviour |
|---|---|---|
| 10 min | Warning | Yachts wait on the course side (y ≥ 15 m is enforced) |
| 5 min | Class flag | |
| 4 min | P flag up | Racing rules become active |
| 2 min | Box opens | Each yacht must **enter the box** through its own gate |
| 1 min | P flag down | |
| 0 | Start | Cross the line from the box side towards the course |

**Entering the box** means crossing the start line *downwards* (from y ≥ 0 to y < 0) on the yacht's own half: port-entry teams on the left, starboard-entry teams on the right. The key technique is to compare the position **before** and **after** this frame's movement:

```c
int32_t old_y = b->y;               /* remembered before moving */
/* ... move the yacht ... */
if (start_clock <= 120 && !b->entered_box && old_y >= 0 && b->y < 0 &&
    ((!(b->team & 1) && b->x >= -BOX_HALF * 1000 && b->x < 0) ||
     ( (b->team & 1) && b->x > 0 && b->x <= BOX_HALF * 1000)))
    b->entered_box = 1;
```

The crossing happened if the sign of `y` changed between the two samples. Testing a single position (`y < 0`) is not enough: a yacht that sailed into the box around the end of the line, without crossing it, must not count.

**Starting** is the mirror image. After the gun, a yacht that is ready (it entered the box and is below the line) must cross the line upwards between the two ends:

```c
if (b->entered_box && b->start_ready && old_y < 0 && b->y >= 0 &&
    b->x >= -LINE_HALF * 1000 && b->x <= LINE_HALF * 1000)
    b->started = 1;
```

A yacht that is above the line at the gun (*over early*) is not `start_ready`. It must first dip back below the line, and only then can its crossing count. No extra code is needed: `start_ready` is set only while `y < 0`.

## 7.3 Marks and the finish

Each yacht has a `leg`, the index of its next mark. When it passes within 40 m of that mark, the leg advances:

```c
int32_t dx = b->x / 1000 - course_x[course_sel][b->leg],
        dy = b->y / 1000 - course_y[course_sel][b->leg];
if (dx * dx + dy * dy < MARK_CAPTURE * MARK_CAPTURE) b->leg++;
```

Comparing **squared** distances avoids a square root: for non-negative numbers, a < b exactly when a² < b². Use this whenever you only need to compare distances.

The finish is a crossing of the finish line **downwards** (the last leg is a run), and it counts only after every mark:

```c
else if (b->started && b->leg >= course_len[course_sel] &&
         old_y >= FINISH_Y * 1000 && b->y < FINISH_Y * 1000 &&
         b->x >= -FINISH_HALF * 1000 && b->x <= FINISH_HALF * 1000) {
    b->finished = 1;
    b->finish_time = (uint16_t)(race_clock + b->penalty * 180);  /* +3 min if unserved */
}
```

## 7.4 Collision detection

A yacht is long and thin: 21 m by 3.6 m. Approximating it by a circle would either miss bow-to-side contacts or invent contacts between boats sailing side by side. FairWind samples **five points** along each hull's centre line:

```c
static void hull_points(const boat_t *b, int32_t *px, int32_t *py) {
    int32_t s = fsin(b->heading), c = fcos(b->heading), l;
    int k;
    for (k = 0; k < 5; k++) {
        l = (k - 2) * 47;                          /* -9.4 m .. +9.4 m, decimetres */
        px[k] = b->x / 100 + ((l * s) >> 14);
        py[k] = b->y / 100 + ((l * c) >> 14);
    }
}
```

Two yachts touch if any pair of points (5 × 5 = 25 pairs) is closer than 4 m. The points are 4.7 m apart, so a bow can at most slip 2.35 m between two samples of the other hull. That is less than the contact distance, so no real contact is missed. This is an example of **choosing the sampling density from the tolerance**.

On contact, Rule 14 ("avoid contact") applies to both boats: they bounce 2.5 m apart along the line joining their centres, and any boat still moving loses half its speed.

## 7.5 Who was wrong? Rules as a priority list

The Racing Rules of Sailing define which boat must *keep clear*. When two yachts touch, FairWind decides who is penalised with an ordered list of tests. The **first** test that applies decides:

```c
if (a->tacking != b->tacking)                     { o = a->tacking ? a : b;       r = RULE_TACKING;   }  /* Rule 13 */
else if (a->leg == b->leg && (da < MARK_ZONE * MARK_ZONE || db < MARK_ZONE * MARK_ZONE))
                                                  { o = da <= db ? b : a;         r = RULE_MARK_ROOM; }  /* Rule 18 */
else if (a->tack != b->tack)                      { o = a->tack < 0 ? a : b;      r = RULE_PORT;      }  /* Rule 10 */
else {
    along = (dx * fsin(a->heading) + dy * fcos(a->heading)) >> 14;
    if (absi(along) < 210)                        { o = windward_score(a) >= windward_score(b) ? a : b;
                                                    r = RULE_WINDWARD; }                                 /* Rule 11 */
    else                                          { o = along > 0 ? a : b;        r = RULE_ASTERN;    }  /* Rule 12 */
}
award_penalty(o, r);
```

Some notes on the geometry:

- **`along`** is the projection of the vector from `a` to `b` on `a`'s heading: a **dot product** with the unit heading vector. If the other boat is more than one hull length (21 m, 210 dm) ahead or behind, the boats are not overlapped, and Rule 12 (clear astern keeps clear) applies.
- **`windward_score`** is the projection of a position on the wind direction. The boat with the larger value is further upwind, and Rule 11 makes the *windward* boat keep clear.
- **Mark room** (Rule 18) applies inside the three-length zone, 63 m from the next mark: the boat farther from the mark must give room.

Real umpires weigh more factors. The point of the priority list is that it **always gives exactly one answer**, in a fixed and documented order, so the player can learn it.

## 7.6 Counting once: hysteresis flags

Touching a mark is a Rule 31 penalty. A yacht's hull stays near the buoy for several frames, so a naive test would award a penalty every frame. The `touching` flag has **hysteresis**: it is set below 2.5 m and cleared only beyond 6 m.

```c
int32_t d = buoy_clearance(&boats[i]);                 /* squared, decimetres */
if (d < 25 * 25 && !boats[i].touching) { boats[i].touching = 1; boats[i].mark_touch = 1; }
else if (d > 60 * 60) boats[i].touching = 0;
```

Hysteresis appears again in Chapter 8 (tacking decisions) and Chapter 9 (switching to the top view at 70 m but back only at 100 m). Wherever a noisy quantity crosses a threshold, a single threshold makes the state flicker. Two thresholds do not.

## 7.7 The penalty turn

A penalised yacht must turn a full circle. The yacht is steered hard over, towards the wind first, and the absolute heading changes are **accumulated** until they reach one full turn:

```c
if (b->serving) {
    b->turned += (uint32_t)absi((int)(q >> 8));   /* binary angle turned this frame */
    if (b->turned >= 65536u) {                    /* one full circle              */
        b->penalty = b->serving = 0;
        b->rule = RULE_NONE;
    }
}
```

The player starts the turn with A+B (Chapter 2). The AI starts it as soon as it is in clear water (Chapter 8).

## Check your understanding

1. A yacht moves 1.4 m in one frame. Can the entry test of §7.2 miss a crossing? Could a test on `y` alone detect it?
2. Why is the rule order Tacking → Mark room → Port/Starboard → Windward → Astern, and not the reverse? Find an example where the order changes the verdict.
3. Why compare squared distances in `buoy_clearance`?

## Exercises

- ★ Add the rule name to the result screen for the player's last penalty.
- ★★ Implement Rule 31 for the committee boat as well as the buoys (it is also a mark of the line). Add a harness scenario.
- ★★★ Replace the five-point hull by a segment-to-segment distance computation, the closest points of two segments. Compare accuracy and instruction count.
