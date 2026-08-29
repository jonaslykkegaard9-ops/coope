#!/usr/bin/env python3
"""aphex_beat.py - generates an Aphex Twin style drill'n'bass beat.

Outputs:
  aphex_beat.mid - 3-track MIDI (drums / acid bass / music-box lead) for
                   importing into a DAW such as FL Studio (drag & drop).
  aphex_beat.wav - rendered stereo preview of how the MIDI is meant to sound
                   (44.1 kHz, 16-bit).

16 bars at 170 BPM in A minor:
  bars 1-4   breakbeat + 303 acid line
  bars 5-8   music-box lead enters, bar 8 ends in an accelerating snare roll
  bars 9-10  breakdown - drums thin out, acid + delay carry it
  bars 11-16 full chaos, acid pattern B, lead up an octave, final mega-roll

The composition is deterministic (seeded RNG) so re-running the script
reproduces the exact same beat.

Run: pip install numpy && python3 aphex_beat.py
"""

import math
import random
import struct
import wave

import numpy as np

# ---------------------------------------------------------------- timebase
BPM = 170.0
PPQ = 480                 # MIDI ticks per quarter note
T16 = PPQ // 4            # ticks per 16th step
BARS = 16
STEPS = 16                # 16th steps per bar
SR = 44100

SEC_PER_TICK = (60.0 / BPM) / PPQ

rnd = random.Random(0xAFE1)
noise_rng = np.random.default_rng(0xD1BA55)

# GM drum notes
KICK, SNARE, HC, HO = 36, 38, 42, 46
TOM_L, TOM_M, TOM_H, CRASH = 45, 47, 50, 49

# Event lists shared by the MIDI writer and the audio renderer.
drums = []   # (tick, note, vel, pan 0..1, delay send 0..1)
acid = []    # (tick, dur_ticks, midi_note, vel, slide)
lead = []    # (tick, dur_ticks, midi_note, vel)


def hit(tick, note, vel, pan=0.5, send=0.06):
    drums.append((int(tick), note, int(vel), pan, send))


# ---------------------------------------------------------- drum programming
def accel_roll(tick0, span_ticks, n_hits, ratio=0.93):
    """Positions for a roll whose hits get closer together (accelerando)."""
    unit = span_ticks * (1.0 - ratio) / (1.0 - ratio ** n_hits)
    pos, out = float(tick0), []
    for k in range(n_hits):
        out.append(int(pos))
        pos += unit * ratio ** k
    return out


def program_drums():
    for b in range(BARS):
        bar0 = b * STEPS * T16
        thin = b in (8, 9)          # breakdown bars
        mega = b in (7, 15)         # bar-ending mega roll
        occupied = set()

        # decide rush zones first so the base pattern can steer around them
        rushes = []
        if mega:
            zone = range(8, 16)
            occupied.update(zone)
            ticks = accel_roll(bar0 + 8 * T16, 8 * T16, 26, ratio=0.90)
            for i, tk in enumerate(ticks):
                frac = i / (len(ticks) - 1)
                vel = int(50 + 77 * frac ** 1.5)
                pan = 0.5 + (0.42 - 0.38 * frac) * (1 if i % 2 else -1)
                note = SNARE
                if i >= len(ticks) - 6:          # walk up toms into the crash
                    note = (TOM_L, TOM_L, TOM_M, TOM_M, TOM_H, TOM_H)[i - len(ticks) + 6]
                rushes.append((tk, note, vel, pan, 0.35))
        elif not thin and rnd.random() < 0.75:
            for _ in range(rnd.choice((1, 1, 2))):
                start = rnd.choice((3, 6, 7, 11, 13, 14))
                length = rnd.choice((1, 1, 2))
                if any(s in occupied for s in range(start, start + length)):
                    continue
                occupied.update(range(start, start + length))
                hits = rnd.choice((3, 4, 4, 6, 8)) * length
                span = length * T16
                up = rnd.random() < 0.6
                for i in range(hits):
                    frac = i / max(hits - 1, 1)
                    vel = int(45 + 70 * (frac if up else 1 - frac))
                    pan = 0.15 if i % 2 else 0.85
                    rushes.append((bar0 + start * T16 + int(span * i / hits),
                                   SNARE, vel, pan, 0.35))

        # base breakbeat
        for s in range(STEPS):
            tk = bar0 + s * T16
            if s in occupied:
                continue
            if not thin:
                if s in (0, 10):
                    hit(tk, KICK, 118)
                elif s == 3 and rnd.random() < 0.30:
                    hit(tk, KICK, 104)
                elif s == 6 and rnd.random() < 0.25:
                    hit(tk, KICK, 104)
                elif s == 13 and rnd.random() < 0.35:
                    hit(tk, KICK, 108)
                if s in (4, 12):
                    hit(tk, SNARE, 112)
                elif s in (2, 7, 9, 11, 15) and rnd.random() < 0.30:
                    hit(tk, SNARE, rnd.randint(30, 55))  # ghost notes
            else:
                if s in (4, 12) and rnd.random() < 0.5:
                    hit(tk, SNARE, rnd.randint(35, 60))

            # hats
            if thin:
                if s % 2 == 0:
                    hit(tk, HC, 46, pan=0.62)
            elif rnd.random() > 0.10:
                vel = 72 if s % 4 == 0 else (48 if s % 2 == 0 else 30)
                hit(tk, HC, vel + rnd.randint(-6, 6), pan=0.62)
            if s == 14 and b % 2 == 1 and not thin and s not in occupied:
                hit(tk, HO, 78, pan=0.38)

        for tk, note, vel, pan, send in rushes:
            hit(tk, note, vel, pan, send)

        if b == 8:                              # crash into the breakdown
            hit(bar0, CRASH, 100, pan=0.4, send=0.2)

    # final downbeat: kick + crash ringing out
    end = BARS * STEPS * T16
    hit(end, KICK, 120)
    hit(end, CRASH, 110, pan=0.5, send=0.25)


# ------------------------------------------------------------- 303 acid line
A1 = 33  # MIDI A1, root of the line

PAT_A = dict(
    note=[0, 0, 12, 0, 3, 0, 10, 12, 0, 0, 15, 12, 7, 3, 2, 0],
    acc=[1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1],
    slide=[0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0])
PAT_B = dict(
    note=[0, 12, 0, 10, 8, 0, 7, 3, 0, 12, 15, 0, 17, 15, 12, 10],
    acc=[1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1],
    slide=[0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0])


def program_acid():
    for b in range(BARS):
        pat = PAT_A if b < 8 else PAT_B
        trans = 12 if b >= 14 else 0            # scream register for the climax
        bar0 = b * STEPS * T16
        for s in range(STEPS):
            tick = bar0 + s * T16
            slide = bool(pat["slide"][s])
            dur = T16 + 12 if slide else int(T16 * 0.55)   # overlap = slide
            vel = 127 if pat["acc"][s] else 88
            acid.append((tick, dur, A1 + pat["note"][s] + trans, vel, slide))


# ------------------------------------------------------- music-box lead line
A4 = 69
N = None
PHRASE = [0, N, 7, N, 3, N, 2, N, -2, N, N, 0, N, N, 3, 2,
          0, N, -4, N, -2, N, 2, N, 7, N, N, 8, 7, N, 3, N]


def program_lead():
    # 2-bar phrases starting at bars 5, 7, 11, 13, 15 (1-indexed);
    # the last two are up an octave for the climax
    for start_bar, trans in ((4, 0), (6, 0), (10, 0), (12, 12), (14, 12)):
        bar0 = start_bar * STEPS * T16
        for slot, deg in enumerate(PHRASE):
            if deg is None:
                continue
            tick = bar0 + slot * 2 * T16        # 8th-note grid
            vel = 100 if slot % 8 == 0 else 86
            lead.append((tick, int(2 * T16 * 1.8), A4 + deg + trans, vel))
    # let the last phrase resolve on a long high A
    lead.append((15 * STEPS * T16 + 24 * T16, 8 * T16, A4 + 12, 105))


# ---------------------------------------------------------------- MIDI file
def vlq(n):
    out = bytearray([n & 0x7F])
    n >>= 7
    while n:
        out.insert(0, 0x80 | (n & 0x7F))
        n >>= 7
    return bytes(out)


def track_chunk(events):
    """events: list of (tick, sort_priority, bytes) -> one MTrk chunk."""
    events.sort(key=lambda e: (e[0], e[1]))
    data, last = bytearray(), 0
    for tick, _, msg in events:
        data += vlq(tick - last) + msg
        last = tick
    data += vlq(0) + b"\xff\x2f\x00"
    return b"MTrk" + struct.pack(">I", len(data)) + bytes(data)


def note_pair(events, ch, tick, dur, note, vel):
    events.append((tick, 1, bytes((0x90 | ch, note, vel))))
    events.append((tick + dur, 0, bytes((0x80 | ch, note, 0))))


def write_midi(path):
    tempo = int(60_000_000 / BPM)
    meta = [(0, 0, b"\xff\x51\x03" + struct.pack(">I", tempo)[1:]),
            (0, 0, b"\xff\x58\x04\x04\x02\x18\x08")]

    d_ev = [(0, 0, b"\xff\x03\x05drums")]
    for tick, note, vel, _pan, _send in drums:
        note_pair(d_ev, 9, tick, 30, note, vel)

    a_ev = [(0, 0, b"\xff\x03\x04acid"), (0, 0, bytes((0xC0, 38)))]  # Synth Bass 1
    for tick, dur, note, vel, _slide in acid:
        note_pair(a_ev, 0, tick, dur, note, vel)

    l_ev = [(0, 0, b"\xff\x03\x04lead"), (0, 0, bytes((0xC1, 10)))]  # Music Box
    for tick, dur, note, vel in lead:
        note_pair(l_ev, 1, tick, dur, note, vel)

    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 1, 4, PPQ))
        for ev in (meta, d_ev, a_ev, l_ev):
            f.write(track_chunk(ev))


# ------------------------------------------------------------ audio preview
def midi_to_hz(n):
    return 440.0 * 2.0 ** ((n - 69) / 12.0)


def synth_kick(vel):
    n = int(SR * 0.30)
    t = np.arange(n) / SR
    f = 48 + 130 * np.exp(-t * 32)
    s = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 18)
    s[:120] += (noise_rng.random(120) * 2 - 1) * np.linspace(1, 0, 120) * 0.5
    return s * (vel / 127) * 1.05


def synth_snare(vel):
    rate = 0.85 + 0.50 * vel / 127          # louder hits crack brighter
    n = int(SR * 0.20 / rate)
    t = np.arange(n) / SR * rate
    nz = noise_rng.random(n) * 2 - 1
    hp = np.diff(nz, prepend=0.0)
    s = hp * np.exp(-t * 30) * 0.9 + np.sin(2 * np.pi * 185 * rate * t) * np.exp(-t * 60) * 0.8
    return s * (vel / 127) * 0.95


def synth_hat(vel, open_=False):
    n = int(SR * (0.35 if open_ else 0.06))
    t = np.arange(n) / SR
    nz = noise_rng.random(n) * 2 - 1
    return np.diff(nz, prepend=0.0) * np.exp(-t * (18 if open_ else 80)) * (vel / 127) * 0.55


def synth_tom(vel, base):
    n = int(SR * 0.18)
    t = np.arange(n) / SR
    f = base * (1 + 0.6 * np.exp(-t * 25))
    s = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 16)
    nz = noise_rng.random(n) * 2 - 1
    return (s + np.diff(nz, prepend=0.0) * np.exp(-t * 50) * 0.25) * (vel / 127) * 0.9


def synth_crash(vel):
    n = int(SR * 2.2)
    t = np.arange(n) / SR
    nz = noise_rng.random(n) * 2 - 1
    return np.diff(nz, prepend=0.0) * np.exp(-t * 3.0) * (vel / 127) * 0.6


def synth_bell(freq, dur, vel):
    n = int(SR * min(dur + 1.2, 2.2))
    t = np.arange(n) / SR
    idx = 2.8 * np.exp(-t * 3.5)
    s = np.sin(2 * np.pi * freq * t + idx * np.sin(2 * np.pi * freq * 3.47 * t))
    s += 0.35 * np.sin(2 * np.pi * freq * 2.006 * t)
    return s * np.exp(-t * 2.6) * (vel / 127) * 0.40


def render_acid(n_total):
    """Whole acid line as one continuous mono synth: saw -> resonant SVF."""
    freq = np.zeros(n_total)
    gate = np.zeros(n_total)
    env = np.zeros(n_total)
    events = sorted(acid)
    prev_hz = midi_to_hz(events[0][2])
    prev_slide = False
    for tick, dur, note, vel, slide in events:
        i0 = int(tick * SEC_PER_TICK * SR)
        i1 = min(int((tick + dur) * SEC_PER_TICK * SR), n_total)
        if i1 <= i0:
            continue
        hz = midi_to_hz(note)
        freq[i0:i1] = hz
        if prev_slide:                          # glide in from previous pitch
            g = min(int(0.04 * SR), i1 - i0)
            freq[i0:i0 + g] = np.geomspace(max(prev_hz, 1.0), hz, g)
        gate[i0:i1] = 0.75 + 0.25 * (vel / 127)
        edge = min(140, i1 - i0)
        if not prev_slide:
            gate[i0:i0 + edge] *= np.linspace(0, 1, edge)
        if not slide:                           # release fade at the gate close
            gate[i1 - edge:i1] *= np.linspace(1, 0, edge)
        t = np.arange(i1 - i0) / SR
        env[i0:i1] += (0.55 + 0.45 * (vel > 100)) * np.exp(-t * (14 if vel > 100 else 9))
        prev_hz, prev_slide = hz, slide
    saw = 2.0 * (np.cumsum(freq) / SR % 1.0) - 1.0
    cutoff = np.clip(90 + 2400 * env, 60, 5500)
    fcoef = 2.0 * np.sin(np.pi * cutoff / SR)
    out = np.empty(n_total)
    low = band = 0.0
    q = 0.22
    tanh = math.tanh
    for i in range(n_total):
        hi = saw[i] - low - q * band
        band = tanh(band + fcoef[i] * hi)
        low += fcoef[i] * band
        out[i] = low
    return np.tanh(out * 2.4) * gate * 0.5


def render_wav(path):
    end_tick = BARS * STEPS * T16
    n_total = int(end_tick * SEC_PER_TICK * SR + 3.0 * SR)
    L = np.zeros(n_total)
    R = np.zeros(n_total)
    DL = np.zeros(n_total)
    DR = np.zeros(n_total)

    def place(sig, tick, pan, send):
        i0 = int(tick * SEC_PER_TICK * SR)
        i1 = min(i0 + len(sig), n_total)
        seg = sig[: i1 - i0]
        L[i0:i1] += seg * (1 - pan)
        R[i0:i1] += seg * pan
        if send:
            DL[i0:i1] += seg * send * (1 - pan)
            DR[i0:i1] += seg * send * pan

    for tick, note, vel, pan, send in drums:
        if note == KICK:
            sig = synth_kick(vel)
        elif note == SNARE:
            sig = synth_snare(vel)
        elif note == HC:
            sig = synth_hat(vel)
        elif note == HO:
            sig = synth_hat(vel, open_=True)
        elif note in (TOM_L, TOM_M, TOM_H):
            sig = synth_tom(vel, {TOM_L: 150, TOM_M: 200, TOM_H: 260}[note])
        else:
            sig = synth_crash(vel)
        place(sig, tick, pan, send)

    for tick, dur, note, vel in lead:
        pan = 0.30 if (tick // (2 * T16)) % 2 else 0.70
        place(synth_bell(midi_to_hz(note), dur * SEC_PER_TICK, vel), tick, pan, 0.5)

    ac = render_acid(n_total)
    L += ac * 0.55
    R += ac * 0.45
    DL += ac * 0.05

    d = int(3 * T16 * SEC_PER_TICK * SR)        # dotted-8th ping-pong delay
    for i in range(d, n_total):
        DL[i] += DR[i - d] * 0.42
        DR[i] += DL[i - d] * 0.42
    L += DL * 0.8
    R += DR * 0.8

    mix = np.tanh(np.stack((L, R), axis=1) * 1.25)
    mix *= 0.89 / max(np.max(np.abs(mix)), 1e-9)
    pcm = (mix * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    return n_total / SR


def main():
    program_drums()
    program_acid()
    program_lead()
    write_midi("aphex_beat.mid")
    secs = render_wav("aphex_beat.wav")
    print(f"{BARS} bars @ {BPM:.0f} BPM, {secs:.1f} s")
    print(f"drum hits: {len(drums)}  acid notes: {len(acid)}  lead notes: {len(lead)}")
    print("wrote aphex_beat.mid + aphex_beat.wav")


if __name__ == "__main__":
    main()
