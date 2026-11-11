#!/usr/bin/env python3
"""Compose FairWind's music and sound instruments into audio.json.

Every piece is original. The tracker plays one track at a time and gives
channel n the instrument n, so tracks use channels 0..7 with the music
instruments 0..7. Effects are played by the game itself with
prg32_audio_note_on_pan() on instruments 8 and up, and only while no song
plays: during a race the score falls silent and the sea, the wind, the
winches, the sails and the committee boat take over.

Track map (src/game.c, SND_*):
  0 title    "Libeccio", a 6/8 tarantella-style theme in A minor
  1 hq       "Banchina", the team-HQ and briefing loop
  2 podium   a short fanfare for a podium finish
  3 result   a short minor cadence for 4th place
  4 season   "Coppa", the championship finale loop
  5 lobby    a waiting loop for the multiplayer lobby

Tracker events wait their `delta` (in 1/16 notes) *after* they run.
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "audio.json"

TRI, SAW, PULSE, NOISE = 0, 1, 2, 3


def synth(wave, pw=8, cutoff=15, res=0):
    return 0x8000 | ((res & 3) << 10) | ((cutoff & 15) << 6) | ((pw & 15) << 2) | (wave & 3)


def inst(sid, vol, pan, a, d, s, r):
    return {"sample_id": sid, "default_volume": vol, "default_pan": pan,
            "attack": a, "decay": d, "sustain": s, "release": r}


INSTRUMENTS = [
    # 0..7: music (tracker channel n plays instrument n)
    inst(synth(PULSE, 5, 13), 210, -12, 1, 40, 110, 30),    # 0 mandolin lead
    inst(synth(TRI, 8, 13), 200, 34, 4, 60, 140, 50),       # 1 counter melody
    inst(synth(SAW, 8, 5, 1), 225, 0, 1, 50, 150, 30),      # 2 bass
    inst(synth(PULSE, 8, 6), 150, -44, 30, 80, 120, 70),    # 3 pad
    inst(synth(PULSE, 3, 10), 170, 46, 0, 26, 0, 16),       # 4 guitar
    inst(synth(NOISE, 8, 15), 120, 24, 0, 5, 0, 5),         # 5 shaker
    inst(synth(NOISE, 8, 4, 2), 220, -22, 0, 22, 0, 14),    # 6 tamburello
    inst(synth(TRI, 8, 15), 190, 10, 0, 110, 0, 90),        # 7 bell
    # 8..: effects, played by the game on voices it owns
    inst(synth(NOISE, 8, 8, 2), 255, 0, 120, 0, 255, 120),  # 8 wind in the rig
    inst(synth(NOISE, 8, 3, 1), 255, 0, 70, 0, 255, 90),    # 9 water rushing past the hull
    inst(synth(NOISE, 8, 5, 1), 230, 0, 110, 150, 40, 130), # 10 swell washing by
    inst(synth(SAW, 8, 6, 1), 235, 0, 8, 0, 235, 50),       # 11 committee-boat horn
    inst(synth(NOISE, 8, 14), 150, 0, 0, 3, 0, 2),          # 12 winch pawl click
    inst(synth(NOISE, 8, 11, 1), 170, 0, 0, 10, 0, 8),      # 13 luffing cloth
    inst(synth(NOISE, 8, 2, 3), 255, 0, 0, 34, 0, 24),      # 14 thump: boom slam, contact
    inst(synth(PULSE, 8, 9), 160, 0, 0, 0, 210, 10),        # 15 penalty buzzer
    inst(synth(TRI, 8, 15), 210, 0, 0, 120, 0, 110),        # 16 mark-rounding chime
    inst(synth(PULSE, 4, 14), 120, 0, 0, 6, 0, 4),          # 17 menu click
    inst(synth(NOISE, 8, 12), 255, 0, 0, 44, 0, 70),        # 18 start gun
    inst(synth(SAW, 8, 9), 150, 0, 4, 60, 40, 40),          # 19 halyard whoosh (kite up)
]

NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def n(name):
    """'A4' -> 69, 'G#5', 'Bb3'."""
    pitch = NOTE[name[0]]
    rest = name[1:]
    if rest.startswith("#"):
        pitch += 1
        rest = rest[1:]
    elif rest.startswith("b"):
        pitch -= 1
        rest = rest[1:]
    return 12 * (int(rest) + 1) + pitch


class Track:
    def __init__(self, tempo):
        self.tempo = tempo
        self.events = []          # (time, order, command, arg0, arg1)
        self.length = 0

    def voice(self, ch, notes, start=0, gate=1):
        """notes: list of (name or None, ticks). Each note is re-struck; it is
        released `gate` ticks before the next one."""
        t = start
        for name, ticks in notes:
            if name:
                self.events.append((t, 1, "NOTE_ON", ch, n(name)))
                self.events.append((t + max(1, ticks - gate), 0, "NOTE_OFF", ch, 0))
            t += ticks
        self.length = max(self.length, t)
        return t

    def render(self, loop=True, length=None):
        length = length or self.length
        ev = sorted(self.events, key=lambda e: (e[0], e[1]))
        out = [{"delta": 0, "command": "SET_TEMPO", "arg0": self.tempo}]
        for i, (t, _, cmd, a0, a1) in enumerate(ev):
            nxt = ev[i + 1][0] if i + 1 < len(ev) else length
            wait = max(0, nxt - t)
            out.append({"delta": min(wait, 255), "command": cmd, "arg0": a0, "arg1": a1})
            wait -= min(wait, 255)
            while wait > 0:  # long rests: no-op volume events carry the wait
                out.append({"delta": min(wait, 255), "command": "NOTE_OFF", "arg0": 7, "arg1": 0})
                wait -= min(wait, 255)
        if loop:
            out.append({"delta": 0, "command": "JUMP", "arg0": 1, "arg1": 0})
        else:
            out.append({"delta": 0, "command": "END", "arg0": 0, "arg1": 0})
        return out


def parse(s):
    """'E5:2 A5:2 r:4' -> [('E5',2),('A5',2),(None,4)]"""
    out = []
    for tok in s.split():
        name, ticks = tok.split(":")
        out.append((None if name == "r" else name, int(ticks)))
    return out


def tremolo(notes, step=1):
    """Mandolin tremolo: notes of 4+ ticks are re-struck every `step` ticks."""
    out = []
    for name, ticks in notes:
        if name and ticks >= 4:
            out += [(name, step)] * (ticks // step)
        else:
            out.append((name, ticks))
    return out


# ------------------------------------------------------------ 0 title --
# 6/8: a bar is 12 ticks (a dotted crotchet is 6).
A1 = parse("E5:2 A5:2 B5:2 C6:4 B5:2 A5:2 G5:2 A5:2 E5:6 F5:2 E5:2 D5:2 C5:4 D5:2 E5:6 r:2 E5:2 G#5:2 "
           "A5:2 B5:2 C6:2 D6:4 C6:2 B5:2 A5:2 G5:2 A5:4 E5:2 F5:2 D5:2 B4:2 G#4:2 B4:2 D5:2 A4:8 r:4")
B1 = parse("G5:2 C6:2 E6:2 D6:4 C6:2 B5:2 C6:2 D6:2 G5:6 A5:2 B5:2 C6:2 F5:4 A5:2 G5:6 r:2 G5:2 B5:2 "
           "C6:2 B5:2 A5:2 E6:4 D6:2 C6:2 B5:2 A5:2 G#5:4 B5:2 A5:2 E5:2 C5:2 B4:2 D5:2 G#5:2 A5:8 r:4")
CH_A = ["A", "A", "D", "E", "A", "A", "E", "A"]
CH_B = ["C", "G", "F", "G", "A", "E", "E", "A"]
CHORD = {"A": ("A2", "C4", "E4"), "D": ("D3", "F4", "A4"), "E": ("E2", "G#4", "B4"),
         "C": ("C3", "E4", "G4"), "G": ("G2", "B3", "D4"), "F": ("F2", "A3", "C4")}


def tarantella_bars(chords):
    bass, gtr, pad = [], [], []
    for c in chords:
        root, third, fifth = CHORD[c]
        up = n(root) + 7
        fifth_low = [k for k, v in NOTE.items() if v == up % 12]
        fname = (fifth_low[0] if fifth_low else root[0]) + str(up // 12 - 1)
        bass += [(root, 3), (None, 3), (fname, 3), (None, 3)]
        gtr += [(None, 2), (third, 2), (fifth, 2), (None, 2), (third, 2), (fifth, 2)]
        pad += [(third[:-1] + "5", 12)]
    return bass, gtr, pad


title = Track(156)
t0 = 0
for phrase, chords, counter in ((A1, CH_A, False), (A1, CH_A, True), (B1, CH_B, True), (A1, CH_A, False)):
    end = title.voice(0, tremolo(phrase), t0)
    bass, gtr, pad = tarantella_bars(chords)
    title.voice(2, bass, t0)
    title.voice(4, gtr, t0)
    title.voice(3, pad, t0, gate=2)
    if counter:
        title.voice(1, [(CHORD[c][1][:-1] + "5", 6) for c in chords for _ in range(2)], t0, gate=1)
    shaker = [("A6", 2)] * (6 * len(chords))
    title.voice(5, shaker, t0)
    title.voice(6, [("C3", 6)] * (2 * len(chords)), t0)
    t0 = end

# --------------------------------------------------------------- 1 hq --
hq = Track(104)
HQ_M = parse("F#5:4 A5:4 D6:6 C#6:2 B5:4 A5:4 F#5:8 G5:4 B5:4 E6:6 D6:2 C#6:4 A5:4 E5:8 "
             "F#5:4 A5:4 D6:6 E6:2 F#6:4 E6:4 D6:4 B5:4 G5:4 A5:4 B5:4 C#6:4 D6:12 r:4")
HQ_CH = [("D3", "F#4", "A4"), ("D3", "F#4", "A4"), ("G2", "B3", "D4"), ("A2", "C#4", "E4"),
         ("D3", "F#4", "A4"), ("B2", "D4", "F#4"), ("G2", "B3", "D4"), ("D3", "F#4", "A4")]
end = hq.voice(0, HQ_M, 0)
hq.voice(2, [(c[0], 4) for c in HQ_CH for _ in range(4)], 0)
hq.voice(4, [(x, 2) for c in HQ_CH for x in (c[1], c[2], c[1], c[2], c[1], c[2], c[1], c[2])], 0)
hq.voice(3, [(c[2][:-1] + "5", 16) for c in HQ_CH], 0, gate=2)
hq.voice(5, [("A6", 4)] * 32, 2)

# ----------------------------------------------------------- 2 podium --
podium = Track(132)
podium.voice(0, parse("C5:2 E5:2 G5:2 C6:6 G5:2 C6:4 E6:12"), 0)
podium.voice(1, parse("G4:2 C5:2 E5:2 G5:6 E5:2 G5:4 G5:12"), 0)
podium.voice(2, parse("C3:6 G2:6 C3:4 C2:12"), 0)
podium.voice(7, parse("r:16 C7:4 G6:4 C7:8"), 0)
podium.voice(6, parse("C3:2 C3:2 C3:2 C3:6 r:4 C3:4"), 0)

# ----------------------------------------------------------- 3 result --
result = Track(108)
result.voice(0, parse("E5:4 D5:4 C5:4 B4:8 A4:12"), 0)
result.voice(2, parse("A2:8 F2:4 E2:8 A2:12"), 0)
result.voice(3, parse("C5:8 A4:4 G#4:8 A4:12"), 0, gate=2)

# ----------------------------------------------------------- 4 season --
season = Track(120)
S_M = parse("C5:4 E5:4 G5:4 C6:8 B5:2 A5:2 G5:8 E5:4 G5:4 F5:4 A5:4 C6:6 B5:2 A5:4 G5:12 r:4 "
            "C5:4 E5:4 G5:4 C6:8 D6:2 E6:2 F6:8 E6:4 D6:4 C6:4 B5:4 D6:4 C6:16 r:4")
S_CH = ["C", "C", "F", "G", "C", "F", "G", "C"]
SC = {"C": ("C3", "E4", "G4"), "F": ("F2", "A3", "C4"), "G": ("G2", "B3", "D4")}
season.voice(0, S_M, 0)
season.voice(1, [(SC[c][1][:-1] + "5", 8) for c in S_CH for _ in range(2)], 0)
season.voice(2, [(SC[c][0], 4) for c in S_CH for _ in range(4)], 0)
season.voice(6, [("C3", 4), (None, 4), ("C3", 2), ("C3", 2), (None, 4)] * 8, 0)
season.voice(7, [("C7", 16)] * 8, 0)

# ------------------------------------------------------------ 5 lobby --
lobby = Track(96)
lobby.voice(2, parse("A2:8 E2:8 F2:8 E2:8"), 0)
lobby.voice(4, [(x, 2) for x in ("C4", "E4", "A4", "E4") * 2 + ("B3", "E4", "G#4", "E4") * 2
                + ("C4", "F4", "A4", "F4") * 2 + ("B3", "E4", "G#4", "E4") * 2], 0)
lobby.voice(7, parse("E6:16 r:8 B5:8"), 0)
lobby.voice(5, [("A6", 4)] * 8, 2)

tracks = [title.render(), hq.render(), podium.render(loop=False), result.render(loop=False),
          season.render(), lobby.render()]
for i, t in enumerate(tracks):
    assert len(t) < 4000, (i, len(t))
    for e in t:
        assert 0 <= e["delta"] <= 255
        if e["command"] == "NOTE_ON":
            assert e["arg0"] <= 7
OUT.write_text(json.dumps({"instruments": INSTRUMENTS, "tracks": [{"events": t} for t in tracks]},
                          separators=(",", ":")) + "\n")
print(f"wrote {OUT}: {len(INSTRUMENTS)} instruments, tracks of "
      + ", ".join(str(len(t)) for t in tracks) + " events")
