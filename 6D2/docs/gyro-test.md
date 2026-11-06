# Gyro log: what's left to test

Current as of v136. `Movie > RAW video > Gyro log (experimental)` records the body's gyro inside each clip.

## Done

| Test | Result |
|---|---|
| Log during lossless takes | Works at 59.94p: 4,146 frames and 69,748 samples with none lost |
| Readout, binned 1080p | 10.59 µs per row at 23.976 and 59.94p, from mains flicker |
| Canon's digital IS | No measurable effect on the gyro or the raw picture |

Details are in the [findings](findings.md). The readout in crop mode has not been measured.

## Still to do

Everything runs from ordinary takes. For every take: RAW video on, lossless, Gyro log ON, lens IS off. Note the clip order.

### Set A: scale on each axis

The takes so far were too dark and too fast to pin down the scale: pitch is 83 to 89 LSB per deg/s, yaw a looser 62 to 86, and roll is unresolved. Spins need no light.

1. Camera on a rotating surface, or turned by hand as evenly as you can.
2. **Each axis in turn** (pan, tilt, roll): 4 full turns at about 5 s per turn.
3. **One slow turn** per axis, about 20 s.

Note which axis each clip is.

### Set B: roll direction and delay

One bright take, with plenty of detail, shutter **1/250 or faster**. Hold still for 3 s, then roll side to side slowly for about 15 s, keeping the subject in frame.

### Set C: other software

1. Open one of these clips in **MLV App**. It should play normally. MLV Drive and `mlv_dump` already step over the gyro blocks, but MLV App hasn't been tried.
2. **Gyroflow:** `python gyro_export.py <card>` writes `<clip>.gcsv`. Export the clip to a movie with the same name, put it beside the log and open it. Run **Auto sync** (rough offset 0). If it stabilises the wrong way, try Gyroflow's IMU orientation options and note which one worked.
