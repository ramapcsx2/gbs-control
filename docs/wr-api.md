# /wr — Live Register Tuning API

Write TV5725 output timing and scaling registers over HTTP without reflashing firmware. Connect to the GBS-Control web UI (default: `http://192.168.4.1/`) and use this endpoint to adjust the video output in real time.

## How to Use

### Named Register Write

```
http://192.168.4.1/wr?reg=<name>&val=<value>
```

Replace `<name>` with a register name from the tables below, and `<value>` with a number between 0 and 4095.

```javascript
fetch('/wr?reg=hscale&val=620');
```

### Named Register Read

Omit `val` to read the current value:

```
http://192.168.4.1/wr?reg=hscale
```

Returns: `hscale=620`

### Raw Byte Write (any register)

Write any register by segment and address:

```
http://192.168.4.1/wr?seg=<0-5>&addr=<0-255>&val=<0-255>
```

```javascript
fetch('/wr?seg=3&addr=0x10&val=108');  // write 108 to S3_0x10
```

This gives access to ALL 477+ registers on the TV5725, including ones not listed in the named tables below. Refer to `tv5725.h` or the programming guide for segment/address mappings.

### Raw Byte Read (any register)

Omit `val` to read:

```
http://192.168.4.1/wr?seg=3&addr=0x10
```

Returns: `s3_0x10=108`

### Reading Timing Summary

Send the timing readback command for a formatted overview:

```
http://192.168.4.1/sc?,
```

The output appears in the web UI text area and looks like:

```
HT / scale   : 2225 512
HS ST/SP     : 7 133
HB ST/SP(d)  : 1938 302
HB ST/SP     : 1870 192
------
VT / scale   : 806 640
VS ST/SP     : 5 1
VB ST/SP(d)  : 806 38
VB ST/SP     : 12 14
```

- `HT / scale` = HTotal and HSCALE
- `HB ST/SP(d)` = Display horizontal blanking (start and stop)
- `HB ST/SP` = Memory horizontal blanking (start and stop)
- `VT / scale` = VTotal and VSCALE
- `VB ST/SP(d)` = Display vertical blanking (start and stop)

## Named Registers

### Scaling — Controls image size

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `hscale` | Horizontal scale factor | **Lower = wider image, higher = narrower image.** At 512, input is doubled. At 620, input is ~1.65x (fits more content in less space). |
| `vscale` | Vertical scale factor | **Lower = taller image, higher = shorter image.** For 768p NTSC: 640 maps 480 input lines to 768 output lines. |

These write the registers directly with **no side effects**. The serial commands `h`/`z` (horizontal) and `4`/`5` (vertical) also adjust scale but additionally recalculate blanking, which is usually not what you want when tuning.

### Horizontal Blanking — Controls what portion of the line is visible

Think of each output line as a row of pixels numbered 0 to HTotal. The blanking registers define where the visible (active) portion starts and ends.

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `hbstd` | Display HBlank Start | **Right edge of visible image.** Increasing this shows more on the right. |
| `hbspd` | Display HBlank Stop | **Left edge of visible image.** Decreasing this shows more on the left. |
| `hbst` | Memory HBlank Start | Right edge of what gets read from the line buffer. Must track `hbstd` with offset per programming guide. |
| `hbsp` | Memory HBlank Stop | Left edge of what gets read from the line buffer. Must track `hbspd` with offset per programming guide. |

**Active width** = hbstd - hbspd (display) or hbst - hbsp (memory).

To shift the image left/right without changing its width, increase or decrease both start and stop by the same amount. To make the image wider/narrower, change only one.

**Important:** Memory blanking (hbst/hbsp) must maintain a specific offset from display blanking (hbstd/hbspd) based on the current HSCALE value — see the TV5725 programming guide section 9.1.6 for the offset table. Changing display blanking without updating memory blanking causes artifacts.

### Horizontal Sync — Controls where the monitor locks onto the signal

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `hsst` | HSync Start | Beginning of the horizontal sync pulse. |
| `hssp` | HSync Stop | End of the horizontal sync pulse. Sync width = hssp - hsst. |

Adjusting these shifts where the monitor positions the image horizontally. Normally you don't need to change these.

### Vertical Blanking — Controls how many lines are visible

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `vbstd` | Display VBlank Start | **Bottom edge of visible image.** Usually set to VTotal (e.g., 806). |
| `vbspd` | Display VBlank Stop | **Top edge of visible image.** Usually set to a small value (e.g., 38). Active lines = vbstd - vbspd. |
| `vbst` | Memory VBlank Start | Bottom edge of what gets read from the frame buffer. |
| `vbsp` | Memory VBlank Stop | Top edge of what gets read from the frame buffer. |

### Vertical Sync

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `vsst` | VSync Start | Beginning of the vertical sync pulse. |
| `vssp` | VSync Stop | End of the vertical sync pulse. |

### Total Line/Frame Length — Controls refresh rate and line frequency

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `ht` | HTotal (pixels per line) | Determines line rate: `line_rate = pixel_clock / ht`. **Note:** The firmware may auto-adjust this value slightly via `applyBestHTotal()`. |
| `vt` | VTotal (lines per frame) | Determines frame rate: `frame_rate = line_rate / vt`. **Note:** `set_vtotal()` also adjusts vertical blanking proportionally. |

### Input Front-End — Controls what part of the analog input signal gets captured

These registers operate on the input side, before the scaler. They determine what portion of each incoming video line gets digitized into the line buffer. This is the first stage in the signal path: **input capture -> line buffer -> scaler -> display output**.

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `ifhbinsp` | Input horizontal active position | **The most useful input register.** Controls horizontal canvas position. **Lower = image shifts right** (shows more right-side content), **higher = image shifts left.** Serial commands `6`/`7` adjust this but also change other registers as side effects. |
| `ifhbinst` | Input horizontal active start | Controls where input capture begins in each line. |
| `ifhbst` | Input HBlank Start | End of input active area. |
| `ifhbsp` | Input HBlank Stop | Start of input active area. |
| `ifhbst2` | Input HBlank Start (alt) | Alternate input HBlank start — used by firmware for EDTV (480p/576p) modes. |
| `ifhbsp2` | Input HBlank Stop (alt) | Alternate input HBlank stop — used by firmware for EDTV modes. |
| `ifvbst` | Input VBlank Start | End of input active lines. |
| `ifvbsp` | Input VBlank Stop | Start of input active lines. |
| `ifhsyncrst` | Input HSync Reset (HTotal) | Input-side horizontal total. Controls the input line timing. |
| `iflinest` | Input line buffer start | Where the line buffer read window starts. |
| `iflinesp` | Input line buffer stop | Where the line buffer read window ends. |
| `ifinist` | Input initialization start | Where the input formatter initialization begins. |
| `ifldst` | Input load start | Where data loading begins in each line. |

The input registers are 11-bit (0-2047), except `ifhbinst` and `ifhbinsp` which are 12-bit (0-4095).

### Output Blanking (EXT) — Controls the actual blanking signal sent to the monitor

These are the third set of blanking registers. They control the blanking signal that the monitor/CRT physically receives. The firmware sets these equal to the display blanking (DIS_HB/DIS_VB) after preset load. **If you change display blanking, you must also update these to match**, otherwise the monitor sees stale blanking timing.

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `exthbst` | Output HBlank Start | Must match or track `hbstd` for correct monitor blanking. |
| `exthbsp` | Output HBlank Stop | Must match or track `hbspd` for correct monitor blanking. |
| `extvbst` | Output VBlank Start | Must match or track `vbstd` for correct monitor blanking. |
| `extvbsp` | Output VBlank Stop | Must match or track `vbspd` for correct monitor blanking. |

### VDS Output Processing

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `vdsdsp` | VDS line buffer start position | Controls where scaled output starts reading from the internal line buffer. |
| `ygain` | Luminance gain | Brightness/contrast adjustment for Y channel. |
| `yofst` | Luminance offset | Black level offset for Y channel. |
| `uofst` | Chrominance U offset | Color offset for U (Cb) channel. |
| `vofst` | Chrominance V offset | Color offset for V (Cr) channel. |
| `synclev` | Sync level | Output sync signal level. |
| `ucgain` | Chrominance U gain | Color saturation for Pb/U (blue difference) channel. |
| `vcgain` | Chrominance V gain | Color saturation for Pr/V (red difference) channel. |
| `pklb` | Peaking low-band gain | Edge enhancement for low-frequency horizontal detail (0-63). |
| `pklh` | Peaking low-high-band gain | Edge enhancement for mid-frequency horizontal detail (0-63). |

### ADC Per-Channel Gain/Offset

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `rgain` | Red ADC gain | White level for red channel. **Disables auto-gain when written.** |
| `ggain` | Green ADC gain | White level for green channel. **Disables auto-gain when written.** |
| `bgain` | Blue ADC gain | White level for blue channel. **Disables auto-gain when written.** |
| `roff` | Red ADC offset | Black level for red channel. |
| `goff` | Green ADC offset | Black level for green channel. |
| `boff` | Blue ADC offset | Black level for blue channel. |

### Scanline Tuning

These registers control the scanline emulation effect. They only have visible effect when scanlines are enabled via the web UI toggle.

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `slstr` | Scanline strength | How dark the scanline gaps are (0 = invisible, 25 = maximum darkness). UI inverts the register value so higher = darker. Writes both Y and UV mixing offsets. Default: 2. |
| `slsoft` | Scanline softness | Vertical IIR filter that blurs scanline edges (0 = sharp binary lines, 25 = very soft). Default: 16. |
| `slbrt` | Scanline brightness boost | White level expansion gain to compensate for scanline darkening (0-25). Default: 8. |

### HD Bypass Blanking — Controls blanking in HD bypass/passthrough mode

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `hdhbst` | HD HBlank Start | Horizontal blanking start for bypass mode. |
| `hdhbsp` | HD HBlank Stop | Horizontal blanking stop for bypass mode. |
| `hdvbst` | HD VBlank Start | Vertical blanking start for bypass mode. |
| `hdvbsp` | HD VBlank Stop | Vertical blanking stop for bypass mode. |

### FIFO / Buffer — Controls memory buffer read/write geometry

These registers control how the chip reads and writes video data to its external memory buffer. The fetch number and offset must match the horizontal active pixel count. Formula: `fetch = H_active_pixels / 8`, `offset = fetch * 2`.

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `pbfetch` | Playback fetch number | Number of 64-bit words to read per line from capture/playback buffer. |
| `pboffset` | Playback/capture offset | Line-to-line address stride in capture/playback buffer (= pbfetch * 2). |
| `rfffetch` | Read FIFO fetch number | Number of 64-bit words to read per line from deinterlace buffer. |
| `rffoffset` | Read/Write FIFO offset | Line-to-line address stride in deinterlace buffer (= rfffetch * 2). |

### Other

| reg | What it does | Practical effect |
|-----|-------------|-----------------|
| `sog` | Sync-on-Green threshold | Adjusts the detection level for sync signals embedded in the green channel. |

## Responses

- **200 "ok"** — Register was written successfully.
- **200 "reg=value"** — Register read (named mode).
- **200 "sN_0xAA=value"** — Register read (raw mode).
- **400 "val out of range"** — Named register value must be 0-4095.
- **400 "raw val 0-255"** — Raw byte value must be 0-255.
- **400 "seg 0-5"** — Segment must be 0-5.
- **400 "unknown reg"** — Named register not recognized.
- **400 "need reg+val, reg (read), or seg+addr params"** — URL is missing required parameters.

## Register Write Order (CRITICAL)

When changing blanking registers, you **must** write them in the correct order to avoid artifacts. The TV5725 programming guide and firmware (`set_htotal()` at line 2537) establish this sequence:

### Horizontal Blanking Change Sequence

1. **`hsst`, `hssp`** — Sync position (depends on `hbstd`, see formula below)
2. **`hbstd`, `hbspd`** — Display blanking (defines visible window)
3. **`hbst`, `hbsp`** — Memory blanking (must offset from display, see offset table)
4. **`exthbst`, `exthbsp`** — Output blanking (must match display blanking)

### Vertical Blanking Change Sequence

1. **`vsst`, `vssp`** — Sync position
2. **`vbstd`, `vbspd`** — Display blanking
3. **`vbst`, `vbsp`** — Memory blanking
4. **`extvbst`, `extvbsp`** — Output blanking (must match display blanking)

### Sync Position Formula

When changing `hbstd` (DIS_HB_ST), you must also recalculate `hsst`:

```
For separate sync with V2CLK = VCLK:
  hs_st = dis_hb_st + a/vclk + 6 (in v2clk units)

For separate sync with V2CLK = 2*VCLK:
  hs_st = dis_hb_st + a/vclk + 5.5 (in v2clk units)
```

In practice, the firmware calculates sync from the blanking stop position:
```
center_blank = ((hbspd / 2) * 3) / 4
hsst = center_blank - (center_blank / 2)
hssp = center_blank + (center_blank / 2)
```

### Memory Blanking Offset Table (from programming guide section 9.1.6)

Memory blanking must maintain a specific offset from display blanking based on HSCALE. For V2CLK=VCLK:

| HSCALE range | HB_ST offset | HB_SP offset |
|-------------|-------------|-------------|
| No scale | DIS_HB_ST - 60 | DIS_HB_SP - 79 |
| (642, 658] | DIS_HB_ST - 77 | DIS_HB_SP - 97 |
| (628, 642] | DIS_HB_ST - 78 | DIS_HB_SP - 98 |
| (614, 628] | DIS_HB_ST - 79 | DIS_HB_SP - 99 |
| (601, 614] | DIS_HB_ST - 80 | DIS_HB_SP - 100 |
| (588, 601] | DIS_HB_ST - 81 | DIS_HB_SP - 101 |
| (575, 588] | DIS_HB_ST - 82 | DIS_HB_SP - 102 |
| (564, 575] | DIS_HB_ST - 83 | DIS_HB_SP - 103 |
| (552, 564] | DIS_HB_ST - 84 | DIS_HB_SP - 104 |
| (542, 552] | DIS_HB_ST - 85 | DIS_HB_SP - 105 |
| (531, 542] | DIS_HB_ST - 86 | DIS_HB_SP - 106 |
| (521, 531] | DIS_HB_ST - 87 | DIS_HB_SP - 107 |
| (511, 521] | DIS_HB_ST - 88 | DIS_HB_SP - 108 |

**Failure to maintain these offsets causes red lines or garbage pixels on screen edges.**

### Additional Registers (set alongside blanking per programming guide)

When programming blanking from scratch, also set:
```
vds_uv_flip = 0
vds_u_delay = 1
vds_v_delay = 0
vds_y_delay = 2
```

### Example: Correctly Changing HSCALE + Blanking

```javascript
// Target: HSCALE=580, wider display window
const HT = 2225;
const HSCALE = 580;
const DIS_HB_ST = HT - 1;  // 2224 (programming guide recommends H.total - 1)
const DIS_HB_SP = 476;      // adjusted for centering

// Offset table for HSCALE 575-588: -82 / -102
const HB_ST = DIS_HB_ST - 82;   // 2142
const HB_SP = DIS_HB_SP - 102;  // 374

// Sync from blanking stop (firmware formula)
const center = ((DIS_HB_SP / 2) * 3) / 4;
const HSST = Math.round(center - center/2);
const HSSP = Math.round(center + center/2);

// Write in correct order
const writes = [
  ['hscale', HSCALE],
  ['hsst', HSST], ['hssp', HSSP],           // 1. sync
  ['hbstd', DIS_HB_ST], ['hbspd', DIS_HB_SP], // 2. display blanking
  ['hbst', HB_ST], ['hbsp', HB_SP],           // 3. memory blanking
  ['exthbst', DIS_HB_ST], ['exthbsp', DIS_HB_SP] // 4. output blanking
];
for (const [reg, val] of writes) {
  await fetch('/wr?reg=' + reg + '&val=' + val);
}
```

## Tips

- Changes are **live but not saved**. Switching presets or power cycling resets everything to the preset values. Once you find good values, update the preset header file and reflash.
- To quickly A/B test a change, note the current value from the timing readback, apply the new value, then toggle back by re-sending the original value.
- The `hscale` and `vscale` registers are 10-bit (0-1023). All other timing registers are 12-bit (0-4095).
- Raw mode writes single bytes (0-255). Named mode handles multi-byte register fields automatically.
- If you lose the picture (monitor shows "no signal"), switching presets via the web UI (e.g., click the 960p button) will recover. The web UI stays responsive even when the video output is bad.
- Use raw read mode to inspect any register: `fetch('/wr?seg=3&addr=0x10')` returns the byte value at S3_0x10.

## Common Tuning Scenarios

**Image is cut off on one side:** First try adjusting `ifhbinsp` to shift the input canvas (lower = shows more right, higher = shows more left). If that's not enough, increase `hscale` to compress the image so more content fits in the display window.

**Image is shifted left/right on the output:** Adjust `hbstd` and `hbspd` by the same amount (both +N to shift right, both -N to shift left). If that doesn't help, try `ifhbinsp` to shift the input capture position instead.

**Image is too tall/short:** Adjust `vscale`. Lower values = taller, higher values = shorter.

**Vertical position is off:** Adjust `vbstd` and `vbspd` to shift the image up or down.

**Refresh rate is wrong:** Adjust `vt`. Frame rate = pixel_clock / (ht * vt). For example, at 108MHz with HT=2225: VT=806 gives 60Hz, VT=2680 would be needed for 50Hz PAL.

**Blanking artifacts (red lines, garbage on edges):** Memory blanking (hbst/hbsp) is out of sync with display blanking (hbstd/hbspd). Consult the programming guide's HB/DIS_HB offset table (section 9.1.6) for the correct offset based on your HSCALE value. Also ensure EXT blanking matches display blanking.
