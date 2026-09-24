"""Merge a 6D2 scratch WAV recorded with "Right channel = Safety -N dB" into one
32-bit float mono WAV - the same idea as a 32-bit float field recorder, done in
post: the left channel (normal gain) is used everywhere it is clean, and where
it approaches clipping the right channel (same source, N dB lower) takes over,
scaled back up.  Peaks that clipped on the left come back intact.

Only meaningful when both channels carry the SAME source - an external mono
mic (the 6D2 feeds a mono plug to both channels; the v63 external-mic test
takes show L and R identical, correlation 0.998-1.0).  The internal mic is two
capsules, so L and R differ and the merge would smear the stereo image.

    python wav_dualgain.py DCIM/AUDIO/M24-1720.WAV            -> M24-1720_float.wav
    python wav_dualgain.py in.wav out.wav --db 12              # force the offset

The gain offset between the channels is measured from the take (least squares
on samples that are loud enough to trust and below the clipping zone on both
channels) rather than assumed, so it also absorbs any small difference
between the two channel paths.  Needs numpy.
"""
import sys, struct, argparse
import numpy as np


def read_wav(path):
    d = open(path, 'rb').read()
    if d[:4] != b'RIFF' or d[8:12] != b'WAVE':
        raise SystemExit('%s: not a WAV file' % path)
    i, fmt, data = 12, None, None
    while i + 8 <= len(d):
        cid, cs = d[i:i + 4], struct.unpack('<I', d[i + 4:i + 8])[0]
        if cid == b'fmt ':
            fmt = struct.unpack('<HHIIHH', d[i + 8:i + 24])
        elif cid == b'data':
            data = d[i + 8:i + 8 + cs]
            break
        i += 8 + cs + (cs & 1)
    if not fmt or data is None:
        raise SystemExit('%s: no fmt/data chunk' % path)
    tag, ch, sr, _, _, bits = fmt
    if tag != 1 or bits != 16 or ch != 2:
        raise SystemExit('%s: expected 16-bit PCM stereo, got tag %d, %d-bit, %d ch' % (path, tag, bits, ch))
    a = np.frombuffer(data[:len(data) // 4 * 4], dtype='<i2').reshape(-1, 2).astype(np.float64)
    return sr, a


def write_float_wav(path, sr, y):
    y = y.astype('<f4')
    body = y.tobytes()
    fmt = struct.pack('<HHIIHH', 3, 1, sr, sr * 4, 4, 32)          # WAVE_FORMAT_IEEE_FLOAT, mono
    fact = struct.pack('<I', len(y))
    riff = b'WAVE' + b'fmt ' + struct.pack('<I', 16) + fmt + b'fact' + struct.pack('<I', 4) + fact \
           + b'data' + struct.pack('<I', len(body)) + body
    open(path, 'wb').write(b'RIFF' + struct.pack('<I', len(riff)) + riff)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('input')
    ap.add_argument('output', nargs='?')
    ap.add_argument('--db', type=float, help='use this L-R offset in dB instead of measuring it')
    ap.add_argument('--knee', type=float, default=-6.0, help='start crossfading to R at this L level, dBFS (default -6)')
    a = ap.parse_args()
    out = a.output or a.input.rsplit('.', 1)[0] + '_float.wav'

    sr, x = read_wav(a.input)
    x = x - x.mean(axis=0)                  # the external input carries a DC offset (seen: -53 / +80)
    L, R = x[:, 0] / 32768.0, x[:, 1] / 32768.0

    clip = 32000 / 32768.0
    if a.db is not None:
        g = 10 ** (a.db / 20.0)
        how = 'given'
    else:
        m = (np.abs(L) > 0.02) & (np.abs(L) < 0.5) & (np.abs(R) > 0.002) & (np.abs(R) < 0.5)
        if m.sum() < 1000:
            raise SystemExit('not enough clean, loud-enough samples to measure the offset - pass --db')
        g = float(np.dot(L[m], R[m]) / np.dot(R[m], R[m]))
        how = 'measured on %d samples' % m.sum()
        corr = float(np.corrcoef(L[m], R[m])[0, 1])
        if corr < 0.95:
            print('warning: L/R correlation %.3f - the channels do not look like the same source '
                  '(internal mic?). The merge assumes a mono source on both.' % corr)

    # weight of R: 0 below the knee, 1 at the clip level, smooth in between.
    # Worked out per 1 ms block and spread +-3 ms, so the hand-over happens
    # before a peak arrives rather than in the middle of a waveform cycle.
    k0, k1 = 10 ** (a.knee / 20.0), clip
    env = np.maximum(np.abs(L), np.abs(R) * g)
    blk = max(1, sr // 1000)
    nb = (len(env) + blk - 1) // blk
    bmax = np.pad(env, (0, nb * blk - len(env))).reshape(nb, blk).max(axis=1)
    spread = bmax.copy()
    for s in range(1, 4):
        spread[:-s] = np.maximum(spread[:-s], bmax[s:])
        spread[s:] = np.maximum(spread[s:], bmax[:-s])
    wb = np.clip((spread - k0) / (k1 - k0), 0.0, 1.0)
    wb = wb * wb * (3 - 2 * wb)
    w = np.interp(np.arange(len(env)), np.arange(nb) * blk + blk / 2.0, wb)
    y = (1 - w) * L + w * (R * g)

    lc = float(np.mean(np.abs(x[:, 0]) >= 32700)) * 100
    rc = float(np.mean(np.abs(x[:, 1]) >= 32700)) * 100
    write_float_wav(out, sr, y)
    print('%s: offset %.2f dB (%s); L clipped %.3f%% of samples, R %.3f%%; peak of result %.1f dBFS'
          % (out, 20 * np.log10(g), how, lc, rc, 20 * np.log10(np.max(np.abs(y)) + 1e-12)))
    if rc > 0:
        print('warning: the safety channel clipped too - use a larger safety offset or lower gain')


if __name__ == '__main__':
    main()
