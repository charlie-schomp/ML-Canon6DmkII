# Sync calibration and clock-drift test (v91)

Two sets of takes. Set A measures the one constant the scratch-WAV sync still needs (how late the
WAV's first sample comes after the moment the camera stamps). Set B checks whether picture and
scratch run off one clock and how far the camera's clock moves as it heats up. No Tentacle needed;
a phone recording stands in as the steady reference clock.

## Bring

- A clapper: a slate's sticks are best. Two flat pieces of wood or two hardback books work. Hands
  are the fallback (fingers straight, palms flat, edge-on to the camera).
- A phone that records audio, as WAV if the app can (an .m4a also works, but needs ffmpeg
  installed on the PC first).
- A tape measure, a tripod, and space on the card. A 10-minute take at 2048x856 10-bit is about
  31 GB. Set B needs two of them; use a smaller resolution if the card is short, since picture
  quality doesn't matter here.

## Camera settings (both sets)

- Movie mode, RAW video on, at the size and frame rate you shoot (2048x856 10-bit, 23.976). If you
  also shoot 29.97 crop, add one Set A take there.
- Scratch audio: Record WAV **Auto**, **Gain: Manual**, set so the clap peaks well below CLIP on
  the meter.
- Exposure Manual. **Shutter angle lock OFF**, shutter **1/500 or faster**, so the sticks are
  sharp in the frame where they meet. Plenty of light; daylight is easiest.
- Tripod, focus on the clapper.

## Framing

- The clapper in the **vertical middle of the frame**, filling about a third of the frame height.
  The sensor reads out top to bottom, so a clap at mid-frame is timed at the frame's middle.
- Sticks horizontal, closing **vertically**, against a plain background that contrasts with them.
- Clapper **1 m from the camera**. Measure it, and tell me the distance: sound takes 2.9 ms per
  metre.

## How to clap

Start recording, wait 2 s, then **5 claps 2-4 s apart, unevenly** (so each lands at a different
point between frames). For each clap, open the sticks wide, close them in one firm motion over
about half a second (I fit the motion across several frames to time the contact between frames),
and hold them shut for a second.

## Set A - latency (any temperature)

1. **3 takes of about 25 s** on the internal mic, each one 5 claps as above. Separate takes,
   because the WAV's start moves every take, and that is what the clip's stamp corrects.
2. If you have any 3.5 mm mic: **3 more takes** with Input **External** and the mic right at the
   clapper. The Tentacle will come in through this input, so its latency is the one that counts.
3. If you shoot 29.97 crop: 1 take there.

## Set B - drift and heat

1. Camera **off for 30+ minutes** beforehand. Note the room temperature.
2. Start the **phone recording** about 30 cm from the clapper, and leave it running through all of
   Set B. Don't move the phone, the camera or the clapper between takes.
3. **Cold take:** power on, go straight to movie LiveView, record: 5 claps, let it roll **10
   minutes** (5 at the least), then 5 claps, stop. Note **Debug → Internal Temp** before and
   after.
4. **Warm up:** keep LiveView running (or keep recording) until the top bar shows the temperature
   (45 °C or more), about 30 min.
5. **Hot take:** the same as the cold take. Note the temperature before and after.
6. Stop the phone recording.

## Send back

The card (the MLVs and DCIM/AUDIO), the phone file, the clapper distance(s), which input each set
used, the room temperature, and the temperatures you noted.

## What comes out of it

- **Set A:** for every clap, the sound's onset in the aligned scratch WAV against the moment the
  sticks meet (between frames, from their motion). The median over the claps is the
  `--latency-ms` constant for MLV Drive and `scratch_sync.py`, and the spread shows how tightly it
  holds take to take and on each input.
- **Set B:**
  - Clap offsets at the start of each take against the end show whether picture and scratch share
    a clock (they should agree within about 2 ms over 10 minutes). If they do, the Tentacle's LTC
    on the scratch measures the picture's clock too.
  - The camera's claps against the phone's, start to end, give the camera's audio clock against
    the phone's to a fraction of a ppm. The phone stays at room temperature, so the cold-to-hot
    change is the camera crystal's heat drift.

Why this matters with a Tentacle: every take takes its start timecode from the LTC at frame 1, so
clock drift cannot build up from take to take, as it does on a camera whose own timecode
free-runs between jams. It can only build up within a take: camera ppm x take length (20 ppm over
a 12-minute take is 14 ms, a third of a frame at 24p). Once the Tentacle is in the jack, MLV Drive
and `scratch_sync.py` measure this on every take, from the LTC at the first and last frame.
