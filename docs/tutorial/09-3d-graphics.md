# 9. 3D graphics from scratch: cameras, projection, clipping and polygons

## Learning objectives

- Transform points from **world** to **camera** coordinates with a 2D rotation.
- Derive **perspective projection** from similar triangles.
- **Clip** polygons against planes with the Sutherland–Hodgman algorithm.
- Fill polygons with a **scanline** algorithm and order objects with the **painter's algorithm**.
- Build 3D models of yachts from a few points in their own coordinate frame.

## 9.1 The pipeline

FairWind contains a complete, if small, 3D renderer: about 150 lines of C, integers only. Every visible object goes through the same **pipeline**:

```
model coordinates  →  world coordinates  →  camera coordinates  →  clip  →  project  →  rasterise
   (a yacht's own        (metres on the        (relative to the           (to the     (to pixels)
    frame, in dm)         race field)           camera, in dm)              screen)
```

GPUs run exactly this pipeline, in hardware, millions of times per second. Writing it once in software is the best way to understand what they do.

## 9.2 The camera

The player's view is a **chase camera**. It sits `CAM_BACK` = 30 m behind the yacht and `CAM_H` = 6.5 m above the water, and looks along a heading `cam_h` that follows the yacht's heading smoothly:

```c
cam_h = (uint16_t)(cam_h + (int16_t)(me->heading - cam_h) * frame_ms / 100);  /* exponential smoothing */
```

`camera_setup` computes the camera's position once per frame. `cam_pt` then converts a world point (in decimetres) into camera coordinates:
- **x** to the right;
- **z** forward (the depth);
- **h** height relative to the eye.

```c
static v3 cam_pt(int32_t wx, int32_t wy, int32_t z) {
    v3 r;
    int32_t dx = wx - cam_wx, dy = wy - cam_wy;          /* translate */
    r.x = (dx * cam_c - dy * cam_s) >> 14;               /* rotate    */
    r.z = (dx * cam_s + dy * cam_c) >> 14;
    r.h = top_view_mode ? z : z - CAM_H;
    return r;
}
```

The two lines marked *rotate* are the 2D rotation matrix

```
| x' |   |  cos θ   −sin θ | | dx |
| z' | = |  sin θ    cos θ | | dy |
```

applied with the camera heading θ. Check it: a point straight ahead of the camera (dx = sin θ, dy = cos θ) gives x' = sin θ cos θ − cos θ sin θ = 0 and z' = sin²θ + cos²θ = 1. It is centred and in front, as expected.

## 9.3 Perspective projection

Why do distant things look small? Put a screen between your eye and the world. A point at depth *z* and height *h* is seen through the screen at height *h'*. By **similar triangles**, h'/f = h/z, where *f* is the distance from the eye to the screen (the **focal length**). So

```
screen_x = centre_x + x · f / z
screen_y = horizon  − h · f / z
```

In FairWind *f* = `FOCAL` = 150 pixels and the horizon is row 76:

```c
*sx = 160 + (int)(p->x * FOCAL / p->z);
*sy = HOR - (int)(p->h * FOCAL / p->z);
```

Points far away (large *z*) crowd towards the horizon, which is the whole of perspective in one division. A 150-pixel focal length over a 320-pixel screen gives a horizontal field of view of 2·atan(160/150) ≈ 94°.

The **top view**, used near marks and rivals, replaces the division by a constant scale: an *orthographic* projection. It adds a little of the height to the vertical position, so masts and sails still show their shape from above:

```c
*sx = 160 + (int)(p->x * TOP_S / 100);
*sy = TOP_CY - (int)((p->z * TOP_S + p->h * TOP_TILT) / 100);
```

## 9.4 Clipping: never divide by zero

The projection divides by *z*. A point behind the camera (z ≤ 0) would be projected upside-down, or cause a division by zero. A polygon that is partly behind the camera (the wake of our own yacht, or a shore wall passing alongside) must be **cut** at a *near plane* in front of the eye. It must also be cut at the left and right edges of the view, so that projected coordinates stay small enough for the integer arithmetic.

Each plane is described by a **signed distance** function, positive on the visible side:

```c
static int32_t plane_d(const v3 *p, int i) {
    return i == 0 ? p->z - NEAR_DM               /* near:  z ≥ 2 m                 */
         : i == 1 ? p->z * 16 + p->x * 15         /* left:  x ≥ −z·16/15            */
         :          p->z * 16 - p->x * 15;        /* right: x ≤  z·16/15 (≈ 94° FOV) */
}
```

The **Sutherland–Hodgman** algorithm (1974) clips a polygon against one plane at a time. It walks round the edges. For each edge (u → v), it keeps *u* if it is inside, and if the edge crosses the plane it adds the intersection point:

```c
for (i = m = 0; i < n; i++) {
    const v3 *u = &a[i], *v = &a[i + 1 == n ? 0 : i + 1];
    int32_t du = plane_d(u, p), dv = plane_d(v, p);
    if (du >= 0) b[m++] = *u;                                   /* keep inside vertex */
    if ((du >= 0) != (dv >= 0)) b[m++] = lerp3(u, v, du, dv);   /* edge crosses plane */
}
```

The intersection is at the fraction *t* = du / (du − dv) along the edge. `lerp3` computes it in fixed point (Q12), first halving both numbers while they are too large, so the multiplication cannot overflow. Clipping a convex polygon against a plane keeps it convex. The yacht outlines are not always convex, but the algorithm still produces a correct outline for them, because each plane is a straight cut.

Lines are clipped the same way, simply by moving whichever end is outside onto the plane.

## 9.5 Filling polygons: the scanline algorithm

A projected polygon is a list of screen points. To fill it, consider each screen row (*scanline*) in turn. Find where the polygon's edges cross that row, sort the crossings from left to right, and fill between the first and second, the third and fourth, and so on. This is the **even-odd rule**, and it handles concave shapes such as a sail with a curved leech.

The cost to watch is the crossing computation, x = x₀ + (y − y₀)·(x₁ − x₀)/(y₁ − y₀), which contains a division. Doing that division for every edge on every row would be slow on a microcontroller. So FairWind computes each edge's **slope once**, in 16.16 fixed point (16 integer bits, 16 fractional bits). A scanline then needs only a multiplication:

```c
/* once per edge: top row, bottom row, x at the top, and dx/dy in 16.16 */
ey0[m] = ys[top];  ey1[m] = ys[bottom];
ex[m]  = (int32_t)xs[top] * 65536 + 32768;                  /* + 0.5 for rounding */
es[m]  = ((int32_t)xs[bottom] - xs[top]) * 65536 / (ey1[m] - ey0[m]);

/* per scanline y: every edge that spans y crosses at */
cx[nx++] = (int)((ex[i] + es[i] * (y - ey0[i])) >> 16);
```

The crossings are sorted with an insertion sort, the best sort for a handful of elements, and each span becomes one call to the firmware's `prg32_gfx_rect_indexed`.

## 9.6 Visibility: the painter's algorithm

Which yacht is in front? FairWind uses the **painter's algorithm**: draw the objects from the farthest to the nearest, so nearer ones paint over farther ones, as a painter paints the sky before the trees. Objects are sorted by the camera depth *z* of their centre:

```c
for (i = 1; i < n; i++) {                              /* insertion sort, far first */
    t = it[i];
    for (j = i; j > 0 && it[j-1].z < t.z; j--) it[j] = it[j-1];
    it[j] = t;
}
```

Within one yacht the same idea decides the order of the mainsail and the headsail. From behind, the main is nearer and is drawn last; seen from ahead it is the other way round. The painter's algorithm fails for objects that interpenetrate or overlap cyclically. Yachts on the water rarely do, which is why a game on a microcontroller can do without a depth buffer. A 320×200 buffer of 16-bit depths would need 125 KiB, twice the whole cartridge.

## 9.7 Modelling a yacht

A yacht is modelled in its own frame: forward is +y, starboard is +x, up is z, all in decimetres. The hull is a seven-point outline:

```c
static const int8_t  hull_lx[7] = {0, 17, 18,  13,  -13, -18, -17};
static const int16_t hull_ly[7] = {106, 55, -30, -98, -98, -30, 55};
```

It is drawn three times:
1. at water level in deep red (the topsides);
2. at deck height in the team colour (the sheer line);
3. shrunk to 72% in pale teak (the deck).

`bpt` converts a model point to camera space. It **heels** the point to leeward in proportion to its height, rotates it by the yacht's heading and places it at the yacht's position:

```c
static v3 bpt(int lx, int ly, int z) {
    int32_t wx, wy;
    lx += z * bb_->heel / 57;                           /* heel: small-angle approximation */
    wx = bb_->x / 100 + ((lx * bc_ + ly * bs_) >> 14);  /* rotate by heading, translate    */
    wy = bb_->y / 100 + ((ly * bc_ - lx * bs_) >> 14);
    return cam_pt(wx, wy, z);
}
```

The **mainsail** is a five-point polygon:
- the tack at the foot of the mast;
- the head at the masthead;
- a middle point on the leech, pushed to leeward by the sail's *belly*;
- the clew at the end of the boom;
- a belly point on the foot.

The boom's end follows the boom angle of Chapter 6, so what you see on screen *is* the trim state. When the sail is luffing, the belly alternates sides every few frames and the cloth changes shade: the flutter. The spinnaker and gennaker are eight-point outlines that grow from the bow as they are hoisted, scaled by the hoist progress.

## 9.8 The horizon and the Bay

The distant scenery is not 3D at all. The venue panorama (Vesuvius, Capri, Sorrento…) is an image 320 pixels wide covering 122° of horizon. That is exactly 150 pixels per radian, the same scale as the perspective projection near the centre. So the panorama can simply be **copied with a horizontal offset** proportional to the camera heading:

```c
int off = (int)((int16_t)(32768u - cam_h)) * FOCAL / 10430;   /* 10430 BAM per radian */
```

It is placed dead downwind, towards Naples. Procedural low hills fill the rest of the land sector, and the open sea lies upwind to the south-west. This cylindrical "sky box" is the same technique used by 1990s racing games.

## Check your understanding

1. Why must polygons be clipped *before* projection, not after?
2. What does the even-odd rule do with a polygon whose outline crosses itself?
3. Estimate the screen height, in pixels, of a 25 m mast 500 m away.

## Exercises

- ★ Change `FOCAL` to 110 (a wider lens). What happens to the apparent size of the rivals and to the field of view?
- ★★ Add a roll of the hull with the waves: offset every model point's height by a sine of time and position.
- ★★★ Implement back-face culling for the shore walls: skip a wall whose normal points away from the camera. Measure the saving with the profiler of Chapter 11.
