# 12. Multiplayer and testing

## Learning objectives

- Pack several values into fixed-width integer fields with **shifts and masks**, and unpack them.
- Design a small network **protocol** that tolerates lost and late messages.
- Test a program **without its hardware**, with sanitizers, behavioural scenarios and fuzzing.

## Part A: Multiplayer

### 12.1 Snapshots, not commands

PRG32 multiplayer is a **snapshot** service. Several times per second each console publishes a small record describing its own player, and it can read the latest record of every other player in the room. There is no guarantee that every record arrives; there is only "the latest one I have heard".

FairWind therefore sends **state**, not commands: "my yacht is here, heading there, at this speed". It never sends "I pressed left", because a single lost "left" would make the two consoles disagree forever. A lost state is simply replaced by the next one. Each console **owns** its own yacht: it alone simulates it, and it alone judges its own penalties. The AI fills any slot not occupied by a human.

### 12.2 Fitting a yacht in 64 bits

The firmware relays four fields per player: `x`, `y`, `sprite`, `flags` (16 bits each) and 7 bits of `input`. Everything a remote console needs to *draw* our yacht correctly must fit:

| Field | Bits | Content |
|---|---|---|
| `x`, `y` | 16 + 16 | position in **decimetres** (±3.2 km fits in `int16_t`) |
| `sprite` | 9 | heading, the top 9 bits of the binary angle (0.7° steps) |
|  | 7 | sheet, 0–90° |
| `flags` | 3 | leg (next mark, 0–7) |
|  | 2 | team |
|  | 1 each | spinnaker set, started, finished, **racing**, gennaker set |
|  | 3 | kite hoist progress (eighths) |
|  | 1 | penalty pending |
| `input` | 7 | boat speed in 0.2-knot steps (0–25.4 kt) |

Packing uses shifts to move each value to its position and OR to combine them:

```c
uint16_t flags = (uint16_t)((me->leg & 7)
    | ((team_sel & 3) << 3)
    | (me->kite == KITE_SPIN ? 0x20 : 0)
    | (me->started  ? 0x40 : 0)
    | (me->finished ? 0x80 : 0)
    | 0x100                                   /* this is a racing snapshot */
    | (me->kite == KITE_GENN ? 0x200 : 0)
    | ((me->kite_prog >> 13) << 10)           /* 16-bit progress -> 3 bits */
    | (me->penalty ? 0x2000 : 0));
uint16_t sprite = (uint16_t)((me->heading >> 7) | ((unsigned)me->sheet << 9));
```

Unpacking reverses it, with AND masks to isolate each field:

```c
b->heading   = (uint16_t)((p.sprite & 511u) << 7);
b->sheet     = (uint8_t)(p.sprite >> 9);
b->leg       = p.flags & 7;
b->team      = (p.flags >> 3) & 3;
b->kite_prog = (uint16_t)(((p.flags >> 10) & 7u) * 9362u);   /* 7 x 9362 = 65534 */
```

The receiving console runs `update_rig` (Chapter 6) on remote yachts too. Their booms, sails and heel therefore agree with *its own* copy of the wind, which is identical to the sender's because the wind is a deterministic function of time (Chapter 5).

### 12.3 The lobby handshake

Before a race, players wait in a lobby and press A when ready. Lobby snapshots carry the team and a *ready* bit (0x40), but **not** the *racing* bit (0x100). A console starts its race when every visible peer is either ready or already racing:

```c
for (i = 0; i < peer_count; i++)
    if (!fairwind_net_peer(i, &peer) || !(peer.flags & 0x140)) all_ready = 0;
```

"Or already racing" matters. A peer that saw everyone ready one frame earlier has already started, and its snapshots no longer carry the ready bit. Without the racing bit, the last console to notice would wait forever: a **deadlock**, and a real bug fixed in version 3.1. Conversely, a late lobby snapshot must never be applied to a racing yacht, so a snapshot without the racing bit is ignored:

```c
if (!(p.flags & 0x100)) continue;      /* lobby snapshot: not a position */
```

Peers are bound to fleet slots by their **player id**, not by their position in the firmware's list. When a player leaves, only their yacht passes to the AI; everyone else keeps their slot and colour.

## Part B: Testing

### 12.4 Testing without the console

Running every test on the console would be slow, and impossible in a continuous-integration system. FairWind is tested on the developer's computer by **replacing the hardware**. `tests/harness/run_harness.c` includes `game.c` and supplies fake versions of every firmware function:

```c
static uint32_t g_input;                                  /* the test sets the buttons */
uint32_t prg32_input_read(void) { return g_input; }
uint32_t prg32_ticks_ms(void) { static uint32_t t; return t += 33; }  /* a perfect 30 fps clock */
void prg32_gfx_rect_indexed(int x, int y, int w, int h, uint8_t c) { g_idx_calls++; note_rect(x, y, w, h); }
```

This technique is called **test doubles** (stubs, fakes, mocks). Because `game.c` is included rather than linked, the tests can also read and set its `static` variables directly, for example to put a yacht in a chosen position.

### 12.5 Sanitizers

The harness is compiled with the AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cc -std=c11 -Wall -Wextra -O1 -fsanitize=address,undefined ...
```

They insert checks around every memory access and every arithmetic operation that C leaves undefined. The program stops with a precise report at the first out-of-bounds array access, use of freed memory, signed overflow or invalid shift. The code on the console runs without these checks, so every such bug must be found on the host. Chapter 4 lists two that were.

### 12.6 Behavioural scenarios

Each scenario sets up a situation, runs the game, and checks a **property** that a sailor or a player would recognise:

| Scenario | Property checked |
|---|---|
| Polar | A trimmed beam reach in 12 kt settles at 8.8 ± 0.5 kt; close-hauled is slower; head to wind stops. |
| Trim | The boom lies to leeward; eased sails luff and lose drive; over-trimmed sails stall. |
| Helm | Left turns to port, right to starboard, at a realistic rate. |
| Kites | A hoists the spinnaker over > 10 s; B peels to the gennaker; both are never set. |
| Penalty | A+B serves a 360° turn; A+B without a penalty does nothing. |
| Land | A yacht driven into the shore stops and is reported aground. |
| View | The top view switches in at 70 m and out at 100 m. |
| Wind | Shifts stay within ±20°; the wind is deterministic. |
| Frame rate | 15 and 30 fps give the same yacht after 53 s. |
| Draw budget | Race frames use indexed fills only, keep the header static, refresh the HUD one frame in four, and stay under 64 text characters. |
| Rules | Rules 10, 14 and 31 are applied; an early finish crossing is ignored. |
| AI races | On every course, every AI yacht starts and finishes. |
| Multiplayer | Lobby handshake, slot binding, disconnects and AI takeover. |

Notice the style of the checks: *properties*, not exact numbers. "Close-hauled is slower than a reach" survives a retuned polar; "close-hauled is 7.23 knots" would break every time the model improves.

### 12.7 Fuzzing

The fuzz scenario plays whole seasons with **random** buttons, a dozen seeds, and checks invariants that must hold whatever the input:
- yachts stay on the water;
- times never wrap round;
- money stays sane;
- the season ends.

Random testing reaches input combinations nobody thought of, such as a button held down across the exact frame a race ends, which edge detection must (and does) ignore.

### 12.8 Tests as documentation

Read `tests/harness/run_harness.c` alongside this tutorial. Each scenario is a short, executable statement of how the game is *supposed* to behave. When you change the game, the harness tells you which of those statements you have broken. Then you decide whether the code or the statement is wrong.

## Check your understanding

1. Why does FairWind send positions rather than button presses?
2. What is the largest speed the 7-bit `input` field can carry? Is that enough for a 12-Metre?
3. Why are the harness's checks written as inequalities rather than exact values?

## Exercises

- ★ Write the unpacking of the *started*, *finished* and *penalty* bits, and check your answer against `net_update`.
- ★★ Add a harness scenario in which a peer's snapshots stop for two seconds and then resume. Check that its yacht is taken over by the AI and then handed back.
- ★★★ Add **interpolation** of remote yachts: store the last two snapshots with their arrival times and draw the yacht at a position blended between them. Measure the visual difference with the host renderer.
