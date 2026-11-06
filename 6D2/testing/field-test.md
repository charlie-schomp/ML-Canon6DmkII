# 6D Mark II ML: field test

For testing a new build on a real camera. About an hour. Nothing here touches Canon settings or factory data.

**Firmware 1.1.1 only.** If the camera ever locks up, take the battery out, and keep any `CRASH*.LOG` or `ASSERT*.LOG` from the card root.

## What you need

- A 6D Mark II on firmware 1.1.1, and a fast SD card.
- A lens with an **AF/MF switch**, set to **MF**. With the lens on AF, Canon refuses the 5x zoom that crop mode and bit-depth changes use.
- A computer with Python 3, for `cardcheck.py` (in the zip, under `extras/tools`).

## 1. Install

- **ML already installed:** copy `autoexec.bin` and the `ML` folder to the card root, replacing the old ones.
- **First time on this camera:** format the card in the camera, copy `autoexec.bin`, `ML` and `ML-SETUP.FIR` to the card root, then Wrench menu > Firmware > update from the card, and restart.

The **Trash** button opens ML. In **Modules**, turn on `mlv_lite` and `bench`, then restart.

**Camera:** Movie mode, **FHD 23.98p** (NTSC), M exposure, shutter 1/48, lens on MF.

## 2. Card speed

Switch to a photo mode, then ML > Debug > Benchmarks > Card Benchmarks > **Quick R/W benchmark (1 min)**. With RAW video on, the benchmark can't get its memory. Note write and read in MB/s, and how full the card is.

Write speed falls as a card fills: one 64 GB card managed about 74 MB/s empty and 62 MB/s at 75% full.

## 3. Menus

Check each is where it should be:

| Menu | Should show |
|---|---|
| Movie | RAW video, Scratch audio, Shutter angle lock, Clip info, Top Button C-FN, MOV/MP4 time limit |
| RAW video | Resolution, Aspect ratio, Data format (default 12-bit lossless), Crop mode, H.264 proxy, Gyro log (experimental) |
| Overlay | RAW preview, default Real-time |
| Prefs | Temperature warning |

## 4. Exposure tools

With Waveform and Histogram on, Source RAW:

- Switch Source between Preview and RAW. The two graphs should differ.
- Check for the **yellow line** at about 73% on both.
- Put the middle of the scene just left of the line, shoot 10 s, and check it in post. It should sit around the middle of Resolve's waveform.
- Turn on False color with Source RAW. Clipped highlights show magenta.

## 5. Frame guide

1080p (Crop mode OFF), Overlay > Cropmarks **Recorded area**, RAW preview **Real-time**, aspect ratio 2.39:1. You should see solid black bars above and below the recorded area. They should change when you change the aspect ratio.

## 6. Formats

Record each row for **up to 60 s**. Write **cont** if it ran the full 60 s, or the second it stopped by itself. Data rates depend on the scene: noise and high ISO can add half as much again.

| Row | Readout | Resolution, aspect | Format | Frame rate | Typical MB/s | Result |
|---|---|---|---|---|---|---|
| A | 1080p | 2048, 2.39:1 | 12-bit lossless | 23.976 | 25 | |
| B | 1080p | 2048, 16:9 | 12-bit lossless | 23.976 | 32 | |
| C | 1080p | 1920, 2.67:1 | 12-bit lossless | 59.94 | 49 | |
| D | 1080p | 2048, 2.39:1 | 12-bit lossless | 59.94 | 58 | |
| E | Crop | 3072, 2.39:1 | 12-bit lossless | 29.97 | 34 | |
| F | Crop | 3072, 5:1 | 12-bit lossless | 29.97 | 34 | |

Rows A and B are estimates from clips at similar settings. 59.94p needs Canon's movie size set to **FHD 59.94p**. Above 1920×960 at 59.94p, the encoder can run out of time, so expect some of those to fail.

## 7. Takes in a row

At row A: **five 30 s takes back to back**, each started within 3 s of the previous stop. All five should record. A 1 KB clip means a take stopped before its first frame.

## 8. Clip info

Movie > Clip info ON, then **Q**. Set a scene, shot and take, and record two RAW takes. Take should count up. Then turn RAW video off, record a short MP4, and check `SLATE.CSV` beside the clips: its file name should match the MP4.

## 9. Scratch audio

Movie > Scratch audio > Mode **Auto with RAW**. Record about 20 s, clapping near the start and the end. A `.WAV` named after the clip should appear in `DCIM/AUDIO`.

## 10. Run the checker

Card in the computer. In a terminal, go to the zip's `extras/tools` folder and run:

```
python cardcheck.py E:\
```

On macOS: `python3 cardcheck.py /Volumes/EOS_DIGITAL`. It reads the card without changing it and saves `cardcheck-report-<date>-<time>.txt` in the current folder. On a Mac, avoid browsing the card in Finder: its `._` files corrupted a card's directory once.

## Send back

Your results, the `cardcheck-report-*.txt` file, and any `CRASH*` or `ASSERT*` logs. Clips are only needed if the report lists a **PROBLEM** for them.

```
build: ______   card: brand/model ____________  size ____ GB
benchmark: write ____ MB/s   read ____ MB/s   card ____% full
menus as listed: ____
RAW and Preview graphs differ: ____   yellow line on both: ____
clip placed on the line, in post: ____________
solid black bars: ____
A ____  B ____  C ____  D ____  E ____  F ____
five takes in a row: ______
clip info take counted up: ____   SLATE.CSV name matches MP4: ____
WAV in DCIM/AUDIO: ____
anything odd:
```
