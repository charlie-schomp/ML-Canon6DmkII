# Gyro log: the camera tests

What `Movie > RAW video > Gyro log` still has to prove. Everything runs from ordinary takes; no
prompts to follow and no timing to hit. Mount the card afterwards and the analysis comes from the
clips.

**For every take:** RAW video on, lossless, Gyro log ON. Note the clip order; digital IS is not
recorded in the clip.

## Set A: readout time and pitch/roll scale

The picture measures both, but only if it stays sharp, so go slow and use a fast shutter.

1. Super Takumar 55 mm (a known focal length). Canon Movie digital IS **off**.
2. Shutter **1/250 or faster**, raising ISO as needed. Aim at something with plenty of detail a few
   metres away, like a bookshelf. Note the focus distance if it's close.
3. **A1, 23.976p, about 40 s:** hold still for 3 s. Then slow, smooth moves, each taking about 2 s to
   sweep the frame, for about 12 s each: **pan** back and forth, **tilt** up and down, **roll** side
   to side, keeping the subject in frame.
4. **A2, 59.94p 12-bit, about 15 s:** the same, pan and tilt only. The line time differs from 23.976p,
   and this checks the log doesn't cost frames at the highest rate.

Results: yaw, pitch and roll scale, the delay between log and frames, readout at each frame rate, and
whether samples were lost (`gyro log:` line in `ML/LOGS/RECDBG.LOG`).

## Set B: readout time from light flicker (a cross-check, skip if there's no suitable light)

**B1, about 5 s:** camera on a table, any lens. A plain wall under a light that bands on camera: try a
dimmable LED or a fluorescent tube, with the shutter at **1/1000 or faster**. If you can see horizontal
bands in LiveView, it works. Tell me the mains frequency if it isn't 60 Hz.

## Set C: does Canon's digital IS change the data?

EF 24-105 f/4L IS II at one focal length, lens **IS switch off**. A warning flashing on screen is
expected.

1. **C1, about 20 s:** digital IS **off**. Hold still 3 s, then slow pan and tilt.
2. **C2, about 20 s:** digital IS **on**, the same moves.

Results: scale and resting signal compared between the two. With lens IS off, the gyro should be all
that's moving. Tell me if Canon's menu won't let you turn digital IS on.

## Set D: other software

1. **D1:** open one of these clips in **MLV App**. It should play normally; `mlv_dump` and MLV Drive
   already step over the gyro blocks, but MLV App hasn't been tried.
2. **D2:** Gyroflow. `python gyro_export.py <card>` writes `<clip>.gcsv`. Export the clip from MLV App
   to a movie with the same name, put it beside the log and open it. Run **Auto sync** (rough offset 0).
   If it stabilises the wrong way, the axes need changing: try Gyroflow's IMU orientation options and
   tell me which one worked. The sign convention was measured against image motion, but it hasn't been
   tried in Gyroflow.
