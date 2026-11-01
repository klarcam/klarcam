# KlarCam controls

Every control of the OBS filter and of the KlarCam app: the name exactly as shown,
what it changes in the picture, its range and default, when to touch it, and which
controls work together.

The OBS filter and the app run the same processing, and most controls exist in both
with the same name and range. This page follows the OBS filter's properties from top
to bottom; [The KlarCam app](#the-klarcam-app) then lists what is different in the
app and what only the app has.

**Default** in the tables is the value of a freshly added OBS filter. The app starts
with the **Stream** look instead; the [Presets](#presets-natural-and-stream) table
shows both looks.

Contents:
[Before you start](#before-you-start) ·
[Presets](#presets-natural-and-stream) ·
[Top of the filter](#top-of-the-filter) ·
[Framing](#framing) ·
[Exposure](#exposure-first-in-the-chain) ·
[Skin](#skin) ·
[Skin tone](#skin-tone-vectorscope) ·
[Eyes and teeth](#eyes-and-teeth) ·
[Relight](#relight-virtual-key-light) ·
[Color LUTs](#color-luts) ·
[Vectorscope](#vectorscope-rec709-skin-band-sector) ·
[Measurement](#measurement-broadcast-protocol-v1) ·
[Diagnostics](#diagnostics) ·
[The KlarCam app](#the-klarcam-app) ·
[How the controls work together](#how-the-controls-work-together)

## Before you start

**Order of processing.** KlarCam works on each frame in this order: auto frame, face
tracking, noise reduction, exposure and background, smoothing and sharpening, key
light, skin tone, color LUTs, eyes and teeth, film look. The vectorscope and the
measurement read the result. A control further down the list works on what the
earlier ones left.

**What runs by itself, without a control:**

- **Noise reduction over time.** On a noisy camera, KlarCam averages what does not
  change from frame to frame and lets motion through. It measures the noise on the
  static background: below about 2/255 (a clean camera) it does nothing, from about
  4.5/255 (a webcam in dim light) it works at full strength. The app shows the state
  in its Exposure section: "Denoise: automatic, noise .../255, strength ... %".
- **Fine texture.** After the smoothing, a fixed share of the finest texture (pores,
  stubble) always goes back in, so the skin keeps its grain.
- **Color over brightness.** The smoothing evens out color more than brightness, so
  redness and blotches calm down while the skin keeps its relief.

**Status lines that say "measuring".** OBS draws the properties once when you open
them. Lines such as "Room light: measuring" or "Frame: measuring" update when you
close and reopen the properties.

## Presets (Natural and Stream)

A preset writes a complete look into the sliders once. **Natural**: light smoothing,
crisp edges, a touch of tone correction. **Stream**: the tuned look with key light,
tone correction and LUTs.

| Control | Default (new filter) | Natural | Stream |
|---|---|---|---|
| Exposure (EV) | 0 | 0 | 0 |
| Automatic: hold the face level | off | off | off |
| Face target p90 (IRE) | 67 | 67 | 67 |
| Background (EV) | 0 | 0 | 0 |
| Background falloff | 0 | 0 | 0 |
| Smoothing | 0.60 | 0.20 | 0.27 |
| Sharpen (edges) | 0.25 | 0.30 | 0.55 |
| Smoothing radius (px) | 4 | 4 | 4 |
| Skin tone detection | 0.80 | 0.80 | 0.80 |
| Correction to skin line | 0 | 0.20 | 0.38 |
| Tolerance (band width) | 0.50 | 0.50 | 0.28 |
| Saturation contrast | 0 | 0 | -0.45 |
| Pull to skin line | 0 | 0 | 0 |
| Skin band from / to (deg) | 123.0 / 126.5 | 123.0 / 126.5 | 123.0 / 126.5 |
| Eye sharpen | 0 | 0.30 | 0.62 |
| Eye zone | 1.20 | 1.20 | 1.20 |
| Remove eye reflections | 0 | 0 | 0.33 |
| Eye brightening (whites) | 0 | 0 | 0.21 |
| Teeth whitening | 0 | 0 | 0.53 |
| Key light strength | 0 | 0 | 0.42 |
| Light direction | 0.60 | 0.60 | -0.66 |
| Follow the room's light (side) | on | on | on |
| Light height | 0.40 | 0.40 | 0.65 |
| Spread (beyond the face) | 1.50 | 1.50 | 1.74 |
| Edge tightness | 0.50 | 0.50 | 0.10 |
| Reduce red skin, applies to | 0, Person | 0, Person | 0.79, Person |
| Reduce yellow skin, applies to | 0, Person | 0, Person | 0.35, Person |
| Contrast look, applies to | 0, Whole frame | 0, Whole frame | 0.11, Whole frame |
| Custom LUT, applies to | 0, Whole frame | 0, Whole frame | 0, Whole frame |
| Film look (grade) | 0 | 0 | 0.29 |

A preset leaves alone: Framing (auto frame, face height and the locked frame), the
beard settings, the measured room-light side, A/B, the vectorscope, the measurement
and the debug view. Your custom LUT file stays loaded, but its amount is set to the
preset's 0. A preset does set Exposure and Background, so choose the preset first and
set the exposure after.

## Top of the filter

| Control | Range | Default | What it changes |
|---|---|---|---|
| **A/B: show the original** | on / off | off | Sends the untouched camera frame to the output. The vectorscope and the measurement keep working on the processed frame, so you can compare. |
| **Preset** | (keep the current values), Natural, Stream | (keep the current values) | Applies a look once, see [Presets](#presets-natural-and-stream). The list then returns to "(keep the current values)". |
| **License key** | text | empty | Paste your key once. It is checked at once, stored for the OBS filter and the app, and the field is cleared. The field shows only while no key is stored. |

The line above **License key** shows the license state:

- `Trial: N days left, then the filter passes the video through. One-time purchase at klarcam.com`
- `Trial over: the filter passes the video through until a key is entered (klarcam.com).`
- `Licensed to <e-mail> (KlarCam 1.x)`

A rejected key shows its reason on that line; the reasons are listed in
[Troubleshooting](troubleshooting.md#a-key-is-rejected).

## Framing

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Auto frame** | on / off | off | Crops the camera frame around your face and scales the crop up to the full size, so a face far from the camera fills the frame like a close one. Up to 2x. |
| **Face height (share of frame)** | 0.25 to 0.60 | 0.41 | The height of your face as a share of the frame height that Auto frame aims for. 0.41 is a well-framed talking head. |
| **Reframe** | button | | Measures the frame anew, for example after you moved the camera or the chair. The new frame is eased in. Shown while Auto frame is on. |

While Auto frame is on, the status line reads "Frame: measuring" or, for example,
"Frame: 1.29x, locked". After ticking Auto frame, close and reopen the properties to
see this line and the **Reframe** button.

**When to use it.** When you sit far from the camera and your face is small in the
picture. KlarCam measures the face over about a second, locks the crop and eases it
in over about a second. It never follows your head. It measures again by itself only
after you have sat outside the middle of the frame, or much closer or farther, for
about two seconds. After changing **Face height**, click **Reframe**.

**Good to know.** The crop happens first, so the face tracking, the person mask and
everything after them work on the framed picture; the person mask even gets finer.
At 1080p a 2x crop is 960 x 540 pixels scaled up, so the picture gets softer; a 4K
camera keeps its sharpness at 2x.

## Exposure (first in the chain)

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Exposure (EV)** | -2 to +2 EV | 0 | Brightness of the whole frame, like ISO or exposure compensation on a camera. Applied as gain in linear light with a soft shoulder: highlights roll into white instead of clipping hard. With the automatic on, this slider is its compensation and shifts the target. |
| **Automatic: hold the face level** | on / off | off | Holds the brightness of your face skin at the target. Slow near the target (about two seconds), faster while far off (a lamp switched on), frozen without a face and while your head moves fast. Measured on the processed output, so LUTs and the film look are included. Switched off, it keeps the level it reached: its EV moves into the Exposure slider. |
| **Face target p90 (IRE)** | 50 to 80 | 67 | The face level the automatic holds: the level 90 % of your face skin stays below, in IRE (0 = black, 100 = white). |
| **Background (EV)** | -1.5 to +1.5 EV | 0 | Exposure of everything outside your silhouette, relative to you. Negative separates you from the wall; positive lifts the wall, and its noise with it. |
| **Background falloff** | 0 to 1 | 0 | 0: the same Background EV everywhere outside you. 1: none right next to your head, growing to the full value about two face widths away, like a vignette. |

**When to touch it.**

- **One automatic only.** If your camera adjusts its exposure by itself (auto
  exposure, auto ISO), keep KlarCam's automatic off and use **Exposure (EV)** as a
  fixed trim. KlarCam's automatic is meant for a camera with a fixed exposure. Two
  automatics work against each other. In Teams, also switch off "Adjust brightness".
- **Face target:** the default of 67 IRE sits in the band of the measurement's set N
  (65 to 68).
- **Background:** stay within about half a stop (-0.5 to +0.5 EV) and light the wall
  for the rest. The further you push it, the more the edge of your silhouette shows.
  Against a strongly darkened wall, a moving arm or a white shirt can flicker at the
  edge: that is the limit of the person detection, not of the setting.
- **Background falloff:** raise it when a light or dark halo shows next to your head
  at stronger Background values. Its best value depends on the room.

## Skin

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Smoothing** | 0 to 1 | 0.60 | Evens out the skin: flat areas blend toward their surroundings while edges stay, because the strength follows the local detail. Acts on the face skin only; eyes, brows and lips are left out. |
| **Sharpen (edges)** | 0 to 1 | 0.25 | Sharpens the face's edges (eyes, brows, lips, contours) and leaves flat skin and pores alone. |
| **Smoothing radius (px)** | 1 to 16 | 4 | The radius of the blur the smoothing evens out toward, in pixels of the processed frame. |
| **Skin tone detection** | 0 to 1 | 0.80 | How strictly the smoothing keeps to skin-colored pixels inside the face. 0 ignores the color and smooths the whole face area; 1 smooths only what looks like skin. |
| **Detect the beard automatically** | on / off | on | Compares the texture of the lower face with that of the cheeks: stubble is dense texture, bare skin is not. The line below shows "Beard detected: N %". The Beard slider is then the strength for what is detected. |
| **Beard (keep stubble)** | 0 to 1 | 1.00 | Keeps the lower face (upper lip, chin, jaw, the cheeks below the cheek line) out of the smoothing and out of the skin statistics, so a beard or stubble keeps its texture. Tone correction, key light and LUTs still cover the whole face. |

**When to touch it.**

- **Smoothing:** compare with **A/B: show the original**. If the face loses its shape
  (nose, cheekbones and jaw look flat), go lower. Natural uses 0.20, Stream 0.27; a
  freshly added filter starts at 0.60.
- **Sharpen (edges):** a webcam already sharpens in the camera, so 0.2 to 0.3 is
  plenty there. With a beard, high values make the stubble look crisper; the
  smoothing itself leaves stubble alone.
- **Smoothing radius (px):** both presets keep 4. The radius counts pixels, so the
  same value covers less of the face at 4K than at 1080p, and less when the face is
  larger in the frame (Auto frame zooms the face up).
- **Skin tone detection:** the smoothing stays inside the face either way; this
  setting decides how much the skin color matters within it.
- **Beard:** check what is kept out with **Debug view > Show smoothing mask**. Without
  a beard, the automatic detects nothing and the lower face is smoothed like the rest.
  Presets do not change the beard settings.

## Skin tone (vectorscope)

These controls work in the geometry of a Rec.709 vectorscope. Skin sits in a narrow
band around the skin tone line in the upper left quadrant. **Skin band from** and
**to** set that band; its center is the line the corrections aim at.

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Correction to skin line** | 0 to 1 | 0 | Turns skin colors that lie outside the tolerance (redder or yellower patches) toward the skin line. Their saturation stays, so the face cannot turn gray. |
| **Tolerance (band width)** | 0 to 1 | 0.50 | How much natural variation stays untouched. 0 pulls every skin color onto the line, which looks painted; 1 corrects only strong outliers. |
| **Saturation contrast** | -1 to +1 | 0 | Along the skin line: negative evens out the face's saturation, so patches of stronger color calm down; positive spreads it. |
| **Pull to skin line** | 0 to 1 | 0 | Turns all skin colors together, so the face's overall hue moves toward the band's center. 0 = the camera's hue, 1 = on the line. The natural spread stays. |
| **Skin band from (deg)** | 110 to 140 | 123.0 | Lower edge of the skin band, as an angle in the Rec.709 Cb/Cr plane. |
| **Skin band to (deg)** | 110 to 140 | 126.5 | Upper edge of the skin band. It always stays at least 0.5 degrees above "from". |

**When to touch it.**

- **Correction and Tolerance work as a pair:** Correction sets how hard outliers are
  turned, Tolerance how wide the untouched band is. Stream uses 0.38 and 0.28.
- **Pull to skin line** is for a face that is too red or too yellow as a whole. In
  the app, **Match** sets it from the measurement.
- **Skin band:** the band is used in three places: the vectorscope draws it as a
  sector, the corrections aim at its center (the dashed line), and the measurement
  report judges the skin hue against it. The default 123 to 126.5 degrees is the band
  of the broadcast measurement protocol (126 to 129 degrees in BT.601 terms),
  expressed in Rec.709. Leave it unless you work to your own scope standard.
- The corrections read the colors of the camera picture, so neither the key light nor
  the LUTs throw them off. The LUTs come after them.

## Eyes and teeth

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Eye sharpen** | 0 to 1 | 0 | Adds sharpness inside the eye area: lids, lashes, iris. |
| **Eye zone** | 1.0 to 1.6 | 1.20 | How far Eye sharpen and Eye brightening reach beyond the lid line. 1.05 = lids and lashes only, the skin around the eyes stays calm; 1.4 = a crisp ring around the eyes ("soft face, sharp eyes"). |
| **Remove eye reflections** | 0 to 1 | 0 | Removes catchlights, the reflections of a lamp in the eyes: near-white spots are pulled to the brightness around them. |
| **Eye brightening (whites)** | 0 to 1 | 0 | Brightens the whites of the eyes (bright, low-color pixels inside the eye area; skin is excluded). |
| **Teeth whitening** | 0 to 1 | 0 | Takes the yellow out of the bright teeth pixels inside the lips and lifts them a little. It acts gently at low values and grows faster toward 1. |

**When to touch it.** Eye zone 1.2 is the default, chosen by eye; go toward 1.4 for
crisper eyes or toward 1.05 when the skin around the eyes looks too sharp. Stream
uses Eye sharpen 0.62, Remove eye reflections 0.33, Eye brightening 0.21 and Teeth
whitening 0.53.

## Relight (virtual key light)

A virtual key light computed from the 3D shape of your face. It models the face, one
side brighter and the other darker, without making it brighter overall. It stays on
your head; your body gets neither light nor shadow from it.

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Key light strength** | 0 to 1 | 0 | Strength of the key light. 0 = off. |
| **Light direction** | -1 to +1 | 0.60 | Left (-) to right (+) in the camera's own, unmirrored picture. With **Follow the room's light (side)** on, only how far to the side the light comes from; the side itself is measured. |
| **Follow the room's light (side)** | on / off | on | Puts the key light on the side your room's light comes from. Measured on your face once (a few seconds), then locked, so it never changes sides during a stream. The locked side is saved with the scene collection. Off: the sign of Light direction is the side. |
| **Measure again** | button | | Measures the room's light anew over a few seconds, for example after you moved a lamp or the desk. The light swings over to the result once and stays there. |
| **Swap side** | button | | Puts the light on the other side if you like that better. Stays until you measure again or switch the automatic off. |
| **Light height** | 0 to 1 | 0.40 | From the side (0) to above (1). |
| **Spread (beyond the face)** | 1.0 to 2.5 | 1.50 | How far the light reaches beyond the face, over the rest of the head. |
| **Edge tightness** | 0 to 1 | 0.50 | How tightly the light is cut to your silhouette. Higher values cut it more sharply at the edge; lower values let it fade out into the background. |

While **Follow the room's light (side)** is on, a status line with the two buttons
below it shows "Room light: measuring", "from the left, locked", "from the right,
locked" or "frontal, locked (the slider's sign is the side)".

**When to touch it.**

- Keep **Follow the room's light (side)** on. A key light opposite to the real light
  flattens the face and darkens its highlights.
- After moving a lamp or your desk, click **Measure again**. If you prefer the other
  side, click **Swap side**.
- When the room's light is measured as frontal, the sign of **Light direction**
  decides the side.
- Left and right refer to the camera's own picture, which is what the filter
  receives. A horizontal flip on the source in OBS, or a mirrored self-view in a call
  app, shows them swapped on your screen; the picture others receive is not mirrored.

## Color LUTs

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Reduce red skin** | 0 to 1 | 0 | A bundled color table that takes redness out of skin. |
| **Reduce yellow skin** | 0 to 1 | 0 | A bundled color table that takes yellow out of skin. |
| **Contrast look** | 0 to 1 | 0 | A bundled contrast look. |
| **Custom LUT file (.cube)** | file | none | Your own 3D LUT in `.cube` format. |
| **Custom LUT** | 0 to 1 | 0 | Strength of your own LUT. |
| **Applies to** (one per LUT) | Face skin, Person (face, neck, hair), Whole frame | Person for the red and yellow LUTs, Whole frame for Contrast look and Custom LUT | **Face skin:** the face mask, eyes, brows and lips excluded. **Person:** your silhouette from the person detection, the face always included. **Whole frame:** like an OBS LUT filter. |
| **Film look (grade)** | 0 to 1 | 0 | A filmic grade: lifted blacks, a gentle S-curve, teal shadows and warm highlights. |

**Good to know.**

- KlarCam applies LUTs exactly as OBS's **Apply LUT** filter does (in linear light),
  in the order contrast look, red, yellow, custom. With **Whole frame** and the same
  amount, a LUT gives the same picture as OBS's filter; **Person** keeps the
  background out of it. You can therefore move an Apply LUT filter into KlarCam's
  custom slot.
- The LUTs come after the skin-tone correction, so the correction does not undo them.
- If a `.cube` file cannot be used, a warning with the reason appears in the LUT
  group.

## Vectorscope (Rec.709, skin band sector)

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Vectorscope** | Off; Overlay in the corner (processed); Full screen: original \| processed | Off | Draws a Rec.709 vectorscope into the filter's output: small in the lower right corner, or full screen with the original and the processed picture side by side. The skin band shows as a sector with a dashed center line, the skin tone line. |
| **Face skin only** | on / off | on | Only the face skin feeds the scope (eyes, brows and lips excluded). |
| **Skin quadrant only (larger)** | on / off | on | Shows only the upper left quadrant, where skin and the skin line sit, twice as large. |
| **Zoom** | 1x, 2x, 3x, 4x | 2x | Magnification of the scope. |

The vectorscope is part of the filter's output: switch it off before you go live.

## Measurement (broadcast protocol v1)

For those who light and grade by numbers. KlarCam measures the live frame in zones
taken from the face: the face, the forehead, the two cheeks (key and shadow side), a
wall sample next to the head, and the whole frame. Luma is Rec.709, IRE = Y/255 x 100,
levels are p50 and p90, clipping counts Y >= 250, stops use gamma 2.2.

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Measure every frame** | on / off | off | Switches the measurement on. |
| **Set** | N, S, T, H, K | N | The target bands the report judges against: **N** Dunkel (Abend/Winter), dark for evening and winter; **S** Sommerabend, summer evening; **T** Tag, daylight; **H** Hell/High-Key, bright; **K** Kino/warm, cinematic and warm. |
| **Source** | Processed output, Camera input | Processed output | Which picture is measured. |
| **Wall sample** | Right of the head, Left of the head | Right of the head | Where the wall box sits, at head height and outside your silhouette. |
| **Wall gap (face widths)** | 0 to 3 | 0.25 | Gap between your face and the wall box. |
| **Wall width (face widths)** | 0.1 to 3 | 0.50 | Width of the wall box. |
| **Refresh report** | button | | Updates the report below with the current frame. |

The report lists each value with the set's band and an OK mark, then the one control
to change next. For a valid reading: face the camera, mouth closed, monitor dark, and
the wall box on bare wall (no shelf, panel or plant). **Debug view > Show measurement
zones** shows where the zones are.

## Diagnostics

| Control | Range | Default | What it changes |
|---|---|---|---|
| **Debug view** | see below | Off | Replaces the output with a diagnostic picture. |

The debug views:

- **Show smoothing mask:** where the smoothing acts, and where the beard is kept out.
- **Show blurred layer:** what the smoothing evens out toward.
- **Show face landmarks:** the 468 tracked points; use it to check the face tracking.
- **Show tone change:** the skin-tone correction, amplified.
- **Face only: original (for scopes)** and **Face only: processed (for scopes):** only
  the face skin, for an external scope.
- **Show face normals (relight geometry):** the 3D shape the key light uses.
- **Show measurement zones:** the measurement's zones over the dimmed picture.

Below the list, two status lines:

- **Face tracker:** for example `ONNX Runtime 1.24.4 (DirectML on NVIDIA GeForce RTX
  5060 Ti) ready, pipelined`, or the reason it is not running.
- **Pipeline:** `CPU ... ms, GPU ... ms per frame`, followed by one line with the GPU
  time of each processing step and the face tracker's timing: `GPU passes (ms): frame
  0.02, mask 0.02, ... Tracker: models ... ms, wait ... ms, older results ... %`. They
  appear after about 120 frames. OBS's log carries the same lines.

Debug views are part of the output: set **Debug view** back to **Off** before you go
live.

## The KlarCam app

The app runs the same processing with the same controls, in collapsible sections in
the right-hand panel: Camera, Virtual camera, Framing, Exposure, Skin, Skin tone
(vectorscope), Relight, Eyes and teeth, Look, Vectorscope, Measurement (protocol v1),
Debug and Status. Its defaults are the **Stream** look. This part covers what is
different and what only the app has.

### Top bar

| Control | What it does |
|---|---|
| **Output: Processed / Original** | A/B. **Original** sends the untouched camera frame to the preview and to the virtual camera; the vectorscope and the measurement keep running on the processed frame. The **B** key switches as well. |
| **Snapshot** | Saves the camera frame and the output as PNG files in `Pictures\KlarCam`. |
| **Record 2 s** | Saves 60 raw camera frames (two seconds) to `Pictures\KlarCam` as a sequence file, for motion tests. Move your head while it records. Only needed when support asks for it. |
| **Look: Natural / Stream** | Applies a built-in look, see [Presets](#presets-natural-and-stream). |
| **1 / 2 / 3** | Your own looks. Grayed out while a slot is empty. |
| **Save...** | Saves the current settings as Look 1, 2 or 3, replacing what is there. |
| **Match set N** (named after the chosen set) / **Stop** | For the measurement protocol: a few seconds of small steps on Exposure (face p90), Background (wall p50, keeping the distance between face and wall), Key light strength (key to shadow cheek ratio) and Pull to skin line (skin hue), from the live measurement of the output, until everything sits in the set's bands. Then it says what it could not reach. Choose the set in the Measurement section and put the wall box on bare wall first. |

Like a preset in OBS, a look leaves the beard settings, Framing, the measured
room-light side, A/B, the vectorscope and the measurement as they are.

### Camera

| Control | What it does |
|---|---|
| **Device** | The camera KlarCam reads: webcams and capture cards, and software cameras such as NVIDIA Broadcast or OBS Virtual Camera. **Refresh** lists cameras plugged in later. |
| **Format** | Resolution, frame rate and pixel format of the camera. 1080p or more works best. |

The line below shows the camera's state or an error.

### Virtual camera

| Control | Default | What it does |
|---|---|---|
| **Output to "KlarCam"** | on | Sends the processed picture to the virtual camera **KlarCam**. |
| **Output size** | 1920 x 1080 | Camera size, 1920 x 1080 or 1280 x 720: the size apps receive. Apps pick up a change when they open the camera again. |
| **Register** / **Unregister** | | The virtual camera is registered per Windows user, without admin rights; KlarCam does that when it starts. The line says "Registered for this user" or "Not registered". |

While it streams, the section shows "Streaming 1920x1080, N frames" and either "An app
is receiving the camera" or "No app is reading the camera right now".

### Framing, Exposure, Skin, Skin tone, Relight, Eyes and teeth

The same controls as in the OBS filter, with these differences:

| OBS filter | App |
|---|---|
| Auto frame | **Auto frame (fill the frame with the face)** |
| Face height (share of frame) | **Framing (face height)**. Changing it measures the frame anew. |
| Frame status | "Frame: 1.29x, locked (now 1.24x)" also shows the zoom in use while it eases. |
| Face target p90 (IRE) | Shown only while the automatic is on, with the automatic's current EV below it. |
| (none) | Exposure section: "Denoise: automatic, noise .../255, strength ... %". |
| Beard detected: N % | "detected: N %" next to the checkbox. |
| Room light status | "Room light: measuring (...)" also shows the measured value. **Swap side** appears once the side is locked. |
| Spread (beyond the face) | **Spread beyond the face** |
| Eye brightening (whites) | **Eye brightening** |

The Relight and Eyes and teeth sections start collapsed.

### Look

The app's **Look** section holds what the OBS filter calls Color LUTs: **Film look
(grade)** first, then **Reduce red skin**, **Reduce yellow skin** and **Contrast look**,
each with its own **applies to** list, then your own LUT: the file path, **Browse...**,
**Clear**, and the **Custom** slider with its **applies to** list.

### Vectorscope

In the app, the vectorscope is shown in the app only and never goes to the virtual
camera.

| Control | Default | What it does |
|---|---|---|
| **Show vectorscope** | on | Shows the scope. |
| **Face skin only** | on | Only the face skin feeds the scope. |
| **Skin quadrant only (larger)** | on | Only the upper left quadrant, twice as large. |
| **Overlay on the video** | on | Draws the scope of the processed picture in the lower right corner of the preview. Off: a panel under the preview. |
| **Compare with original** | on | In the panel: original and processed side by side. Not available while the overlay is on. |
| **Zoom** | 2.0x | 1.0x to 4.0x. |

### Measurement (protocol v1)

The app measures while this section is open. It has the same **Set**, **Source**,
**Wall sample**, **Wall gap (face widths)** and **Wall width (face widths)** as the
OBS filter, plus **Show the zones in the preview** and **Copy report**. The report
updates live.

### Debug

| Control | What it does |
|---|---|
| **Debug view** | Off, Smoothing mask, Blurred layer, Landmarks, Tone change, Face only: original, Face only: processed, Face normals, Measurement zones. The same views as in OBS. Set it back to Off before a call. |
| **Show log** | Shows the app's log under the panel. |

### Status

| Control | Default | What it does |
|---|---|---|
| **GPU** | Automatic | Which graphics card runs KlarCam. "Automatic (most memory: ...)" takes the one with the most dedicated memory. A change restarts the processing. |
| **Save diagnostics** | | Writes a text report (computer, GPU, camera and format, timings, settings) to `Pictures\KlarCam` and to the log. Useful for a bug report; it contains your computer name and the path of your settings, which you may remove before posting. |
| **Activate** | | Stores the key pasted into the field (KLARCAM-...). |
| **Buy a key (klarcam.com)** | | Opens the shop. |
| **Remove key** | | Deletes the stored key. The trial does not start again. |
| **Reset all sliders to the defaults** | | Back to the app's defaults: the Stream look, Beard 1.00 with Detect the beard automatically on, the vectorscope on as an overlay (face skin only, skin quadrant, 2x), Auto frame off with Face height 0.41, A/B and the debug view off. Unlike the Look buttons, it also releases the room-light lock, so the side is measured anew. |
| **Hide to tray on close** | on | Closing the window hides KlarCam to the tray and the camera keeps running. Quit from the tray icon's menu with **Quit KlarCam**. |
| **Start hidden** | off | KlarCam starts in the tray, without its window. |
| **Start with Windows** | off | Starts KlarCam hidden in the tray when you sign in to Windows. |

The Status section also shows what KlarCam runs on and how fast: "Running on",
"Tracker", "Pipeline: CPU ... ms, GPU ... ms per frame", the GPU time per step, "Face:
tracked" or "not found", "Loop: N frames/s", and where the settings file is. Under
**License**: "Trial: N days left, everything works.", "Trial over: the camera frame
goes out until a key is entered." or "Licensed to <e-mail> (KlarCam 1.x)". A problem
with a key shows in red above that line.

## How the controls work together

- **Presets and looks** overwrite the look controls, Exposure and Background
  included. They keep Framing, the beard settings, the room-light side, A/B, the
  vectorscope and the measurement. Choose the look first, then set the exposure.
- **Exposure (EV) and the automatic:** with the automatic on, the slider is its
  compensation. Switching the automatic off moves the level it reached into the
  slider.
- **Camera exposure and KlarCam's automatic:** use one of them, never both.
- **Light direction and Follow the room's light:** with the automatic on, the
  slider sets only how far to the side; off, its sign also sets the side.
- **Skin band:** moves the vectorscope's sector, the target of Correction to skin
  line and Pull to skin line, and the measurement report's judgement together.
- **Correction to skin line and Tolerance:** how hard, and how wide the untouched
  band is.
- **Eye zone** sets the reach of Eye sharpen and Eye brightening.
- **Beard** takes the lower face out of the smoothing and the skin statistics only;
  tone correction, key light and LUTs still cover it. A high **Sharpen (edges)**
  makes stubble crisper.
- **Background (EV) and Background falloff:** the falloff keeps the background
  change away from your head, where the silhouette's edge would show.
- **LUTs** come after the skin-tone correction, in the order contrast look, red,
  yellow, custom, each on its own region.
- **Auto frame** comes first, so everything else works on the framed picture.
- **A/B** changes only the output; the vectorscope and the measurement keep running
  on the processed frame.
- **Vectorscope and debug views** are part of the OBS filter's output. In the app the
  vectorscope stays in the app, a debug view does not.
- **One license key** unlocks the app and the OBS filter; it is stored once in
  `%APPDATA%\KlarCam`.
