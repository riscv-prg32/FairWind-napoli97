# 12‑Metre polar model

The runtime table is derived from the 2026 ORC certificate for **ANITA (12MR 2)**, an actual 12 MR yacht: 21.433 m LOA, 26,083 kg displacement, 120.67 m² mainsail, 78.87 m² jib, and 213.9 m² symmetric spinnaker. Source: <https://windregatta.com/en/orc/anita-ger-12mr-2-2026>.

Boat speeds are stored in tenths of a knot. The game uses the published 8, 12, and 16 knot TWS columns and the published 52°, 60°, 75°, 90°, 110°, 120°, 135°, and 150° TWA rows. The 40° point converts published beat VMG and beat angle to boat speed. The 180° endpoint conservatively uses published run VMG. Linear integer interpolation supplies intermediate TWA and TWS values.

| TWA | 8 kt TWS | 12 kt TWS | 16 kt TWS |
|---:|---:|---:|---:|
| 40° | 6.3 | 7.2 | 7.5 |
| 52° | 7.1 | 8.1 | 8.4 |
| 60° | 7.4 | 8.4 | 8.8 |
| 75° | 7.6 | 8.7 | 9.1 |
| 90° | 7.6 | 8.8 | 9.3 |
| 110° | 7.6 | 8.9 | 9.4 |
| 120° | 7.4 | 8.9 | 9.6 |
| 135° | 6.8 | 8.6 | 9.4 |
| 150° | 5.7 | 7.9 | 9.1 |
| 180° | 5.0 | 6.9 | 8.2 |

Two rows extend the table into the no-go zone: 0 kt at 28° TWA and roughly two thirds of the 40° speed at 34°, so a yacht pinching or luffing head to wind slows and coasts to a stop instead of creeping forward. Below 8 kt TWS the 8-knot column is scaled linearly; above 16 kt it is held.

| TWA | 8 kt TWS | 12 kt TWS | 16 kt TWS |
|---:|---:|---:|---:|
| 28° | 0.0 | 0.0 | 0.0 |
| 34° | 4.0 | 4.6 | 4.8 |

The ORC rows at and above 110° assume a kite. The runtime therefore applies a sail-plan efficiency, interpolated on the same TWA rows and blended by hoist progress while a kite goes up or comes down:

| TWA | Jib | Spinnaker | Gennaker |
|---:|---:|---:|---:|
| ≤60° | 100% | 40–45% | 50–80% |
| 75° | 100% | 60% | 102% |
| 90° | 100% | 88% | 106% |
| 110° | 90% | 100% | 104% |
| 120° | 84% | 100% | 100% |
| 135° | 78% | 100% | 95% |
| 150° | 74% | 100% | 90% |
| 180° | 72% | 100% | 82% |

Several further factors apply:
- **Trim.** A trim efficiency keeps full drive with about 18° angle of attack. It falls quadratically to zero as the sails luff and to 30% as they stall.
- **Syndicate.** Performance multiplies speed by 97–113% for the player (hull, sails, strategy) and 102–103% for the AI.
- **Momentum.** Speed approaches the target with 7–11 s acceleration and 14 s coasting time constants.

The ORC values represent a rated performance target, not an instrument-grade simulator. The behavioural harness checks the result: a trimmed beam reach in 12 kt settles at 8.7 kt, close-hauled at 7.2 kt, and nothing exceeds 10.5 kt in 16 kt of breeze.
