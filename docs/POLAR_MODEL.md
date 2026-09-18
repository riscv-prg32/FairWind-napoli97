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

The ORC values represent a rated performance target, not an instrument-grade simulator. Team and management upgrades apply small multipliers after polar interpolation. The spinnaker rule then penalizes downwind sailing without the sail and upwind sailing with it incorrectly deployed.
