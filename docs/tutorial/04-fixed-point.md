# 4. Numbers without floating point: fixed-point arithmetic

## Learning objectives

- Represent real quantities (positions, speeds, angles) as scaled integers, and choose the scale.
- Compute sine, cosine, arctangent and square root with integers only.
- Reason about **overflow** before it happens, and order operations to avoid it.

## 4.1 Why not `float`?

The ESP32-C6 core implements the RISC-V base integer instructions plus multiply/divide (M), atomics (A) and compressed instructions (C), but **no floating-point unit**. Every `float` addition would call a software routine costing tens of instructions, and the cartridge has no C library to provide those routines anyway. So FairWind, like console games of the 1990s, does all its arithmetic in integers.

The idea is simple: choose a **unit small enough** that the quantities you need are whole numbers, and a **type large enough** that the biggest value fits.

| Quantity | Unit (scale) | Type | Range | Resolution |
|---|---|---|---|---|
| Position | millimetre | `int32_t` | ±2,147 km | 1 mm |
| Speed | mm/s × 256 | `int32_t` | ±8,388 m/s | 0.004 mm/s |
| Heading, wind angle | 1/65536 of a turn | `uint16_t` / `int16_t` | one full turn | 0.0055° |
| Sine, cosine | × 16384 ("Q14") | `int32_t` | −1…+1 | 0.00006 |
| Wind speed | 0.1 knot | `int` | | 0.05 m/s |
| Camera space | decimetre | `int32_t` | | 10 cm |

Speed is stored multiplied by 256 because the yacht accelerates very gradually (Chapter 6). In one frame the change in speed can be a fraction of a mm/s. Without the extra 8 fractional bits it would round to zero and the yacht would never reach its target speed. This is the general lesson: **the scale is chosen by the smallest change you must represent, not by the value itself.**

## 4.2 Binary angles

Degrees have an awkward property: after 359 comes 0, so every angle computation needs a "wrap-around" correction. FairWind measures angles in **binary angle measurement units (BAM)**: a full turn is 65,536 units, exactly the range of a 16-bit integer.

```c
#define DEG(d)     ((int32_t)(d) * 182)       /* degrees -> BAM (182.04)  */
#define BAM2DEG(a) ((int32_t)(a) * 45 / 8192) /* BAM -> degrees           */
```

Unsigned 16-bit arithmetic wraps modulo 65,536, which is exactly the wrap-around of a circle. Two consequences are used everywhere:

- **Turning** is just addition: `heading = (uint16_t)(heading + delta);` never needs a correction.
- **The signed difference** between two angles is obtained by reinterpreting the unsigned difference as signed:

```c
b->twa = (int16_t)(twd - b->heading);   /* in -32768..32767, i.e. -180..+180 degrees */
```

If the wind comes from 10° and the yacht heads 350°, then `twd - heading` wraps to 20° of BAM. That is the correct answer: the wind is 20° to starboard. In degrees you would need an explicit `if` to get it.

## 4.3 Sine and cosine from a table

`src/fixmath.h` stores one quarter of a sine wave, sampled at 65 points, as Q14 integers (the value multiplied by 16,384):

```c
static const int16_t sin_quarter[65] = {0, 402, 804, 1205, /* ... */ 16379, 16384};
```

The other three quarters follow from symmetry: sin(180° − x) = sin x and sin(180° + x) = −sin x. Between table entries the function **interpolates linearly**, using the low 8 bits of the angle:

```c
static int32_t fsin(uint16_t a) {
    unsigned q = a >> 14;              /* quadrant 0..3                      */
    unsigned t = a & 16383u;           /* angle within the quadrant          */
    unsigned i;
    int32_t v;
    if (q & 1u) t = 16384u - t;        /* 2nd and 4th quadrants: mirror      */
    i = t >> 8;                        /* table index 0..64                  */
    v = i >= 64u ? 16384
                 : sin_quarter[i] + (((sin_quarter[i + 1] - sin_quarter[i]) *
                                      (int32_t)(t & 255u)) >> 8);
    return q & 2u ? -v : v;            /* 3rd and 4th quadrants: negate      */
}
static int32_t fcos(uint16_t a) { return fsin((uint16_t)(a + 16384u)); }
```

A 130-byte table and a dozen instructions give sine and cosine to better than 0.01%. Multiplying by a Q14 value and shifting right by 14 bits scales a quantity by a sine:

```c
b->x += (step * fsin(b->heading)) >> 14;   /* east component of the step */
```

## 4.4 The inverse problem: which way does a vector point?

The game often needs the **direction** of a vector (dx, dy), for example the bearing from a yacht to a mark, or the angle of the apparent wind. The mathematical tool is `atan2`. FairWind computes it with the **CORDIC** algorithm (COordinate Rotation DIgital Computer, J. Volder, 1959), invented for exactly this situation: machines that can add and shift but not multiply cheaply.

CORDIC rotates the vector in a sequence of ever smaller steps. At step *i* the vector rotates by ±atan(2⁻ⁱ), always towards the x axis, and the steps taken are added up. Because tan(θᵢ) = 2⁻ⁱ, each rotation needs only shifts and additions:

```c
static const int16_t cordic_atan[14] = {8192, 4836, 2555, 1297, 651, 326, 163,
                                        81, 41, 20, 10, 5, 3, 1}; /* atan(2^-i) in BAM */

static uint16_t bearing(int32_t dx, int32_t dy) {  /* 0 = +y, clockwise */
    int32_t x = dy, y = dx, nx;
    uint16_t a = 0;
    int i;
    if (!x && !y) return 0;
    /* keep |x|,|y| in a range where the shifts neither overflow nor lose precision */
    while (x > (1 << 24) || x < -(1 << 24) || y > (1 << 24) || y < -(1 << 24)) { x >>= 1; y >>= 1; }
    while (x < (1 << 14) && x > -(1 << 14) && y < (1 << 14) && y > -(1 << 14)) { x *= 2; y *= 2; }
    if (x < 0) { x = -x; y = -y; a = 32768u; }        /* rotate by 180 degrees */
    for (i = 0; i < 14; i++) {
        if (y > 0) { nx = x + (y >> i); y -= x >> i; a = (uint16_t)(a + cordic_atan[i]); }
        else       { nx = x - (y >> i); y += x >> i; a = (uint16_t)(a - cordic_atan[i]); }
        x = nx;
    }
    return a;
}
```

Note the convention. Headings are measured **clockwise from north** (+y), like a compass. So the "x" of the textbook algorithm is our *dy* and the "y" is our *dx*. After 14 iterations the error is about one BAM, 0.005°.

## 4.5 Square root by bits

Distances (`sqrt(dx*dx + dy*dy)`) need a square root. The classic digit-by-digit method finds one bit of the result per iteration, from the most significant down:

```c
static int32_t isqrt(uint32_t n) {
    uint32_t r = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (int32_t)r;
}
```

It is the binary version of the long-division square root you may have learned at school. Sixteen iterations of shifts, comparisons and subtractions give an exact integer result.

## 4.6 Overflow: the silent enemy

In C, overflowing a **signed** integer is *undefined behaviour*: the compiler may assume it never happens, and the results can be absurd. Unsigned overflow is defined (it wraps), which is why angles are unsigned. Everything else must be kept in range **by design**. FairWind's comments are full of small calculations like this one, from the physics code:

```c
/* rudder (±100) × turn rate (≤1200) × 182 / 100          ≤ 218,400
   × simulated ms this frame (≤ 1,320)                     ≤ 2.9 × 10^8   fits in int32
   / 100 × crew factor (≤ 108) / 100 × 256 / 1000                          */
q = (int32_t)b->rudder * (300 + 900 * vv / 4100) * 182 / 100;
q = q * sm / 100 * (92 + 2 * cr) / 100 * 256 / 1000 + b->hacc;
```

The recipe is always the same:

1. write down the **largest** possible value of each operand;
2. multiply them in the order the code does;
3. if a product can exceed 2³¹ ≈ 2.1 × 10⁹, **divide earlier** (losing a little precision) or change units.

Division placement is a trade-off. Dividing early prevents overflow but discards low bits; dividing late keeps precision but risks overflow. Where precision matters and the numbers are small, the code multiplies first; where the numbers can be large, it divides first.

The test harness is compiled with `-fsanitize=undefined`, which aborts on any signed overflow. During development it caught two real bugs. One was an overflow in the ripple animation, where a frame counter grew without bound and was eventually multiplied by a sine. The other was a left shift of a negative number in the line-drawing code, which is also undefined behaviour in C; the fix multiplies by 65,536 instead.

## Check your understanding

1. What is `(int16_t)(DEG(10) - DEG(350))` in degrees? And `(int16_t)(DEG(350) - DEG(10))`?
2. Why does `fsin` interpolate with `>> 8`? What is the size of one table step in BAM?
3. Give the largest value of `step * fsin(heading)` if `step` can reach 2,000 mm. Does it fit in `int32_t`?

## Exercises

- ★ Write a host program that compares `fsin` with the C library `sin` for all 65,536 angles and prints the largest error.
- ★★ Implement `fatan2` with a small table and a division instead of CORDIC. Compare accuracy and instruction count (compile with `-Os` for RISC-V and count instructions with `objdump`).
- ★★★ Change positions from millimetres to centimetres. Which lines of the physics must change? What breaks, and why? (Hint: a slow yacht's movement per frame.)
