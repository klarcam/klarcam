# Quick start: OBS Studio

From install to a finished look in a few minutes. Every control is explained in
detail in the [parameter reference](parameters.md).

## 1. Install

1. Close OBS Studio.
2. Run `KlarCam-Filter-Setup-0.4.0.exe` (installs only the filter), or
   `KlarCam-Setup-0.4.0.exe` with **Install the filter for OBS Studio** ticked if you
   also want the app with its virtual camera.
3. KlarCam needs OBS Studio 33 or newer. **Help > About** in OBS shows your version.

Use one of the two setups: the app setup already includes the filter, and the filter
setup does not install while the KlarCam app is installed. The filter goes to
`%ProgramData%\obs-studio\plugins\klarcam`.

## 2. Add the filter to your camera

1. Start OBS.
2. In the **Sources** dock, right-click your camera source (for example a **Video
   Capture Device**) and choose **Filters**.
3. Under **Effect Filters**, click **+**, choose **KlarCam** and confirm the name.

For a second or two the video passes through unfiltered while the KlarCam engine
starts, then the look fades in. The same happens each time OBS starts. Once the engine
runs, the first line of the filter's **Diagnostics** group reads "KlarCam engine:
running on" and the name of your graphics card.

The filter's settings belong to this source and are saved with your scene
collection. To use the same settings on another source, right-click the source and
use **Copy Filters**, then **Paste Filters** on the other source.

## 3. Trial or key

At the top of the filter's properties, a line shows the license state:

- `Trial: 14 days left, then the filter passes the video through. One-time purchase at klarcam.com`
- `Licensed to <your e-mail> (KlarCam 1.x)` once a key is stored

While the engine is still starting, the line shows the engine's state instead; close
and reopen the properties a moment later.

To enter a key, paste it into **License key**. The line changes to "Licensed to ..."
and the field disappears. A key entered in the KlarCam app works in OBS as well
(OBS notices it within about ten seconds), and the other way round.

Buy a key at [klarcam.com](https://klarcam.com). After the trial, the filter passes
the camera picture through unchanged until a key is entered.

## 4. Starting values

1. **Preset:** choose **Natural** (light smoothing, crisp edges, a touch of tone
   correction) or **Stream** (the tuned look with key light, tone correction and
   LUTs). The values land in the sliders, and the list returns to "(keep the current
   values)". A freshly added filter starts with Smoothing 0.60 and Sharpen 0.25, and
   without key light, tone correction or LUTs; Natural sets Smoothing to 0.20.
   A preset also sets Exposure, Face light and Background vs. you back to 0, so choose
   the preset first.
2. **Compare:** tick **A/B: show the original** to see the untouched camera picture;
   untick it to go back.
3. **Skin:** move **Smoothing** until the skin looks calm while the face keeps its
   shape. With a webcam, **Sharpen (edges)** at 0.2 to 0.3 is plenty: webcams already
   sharpen in the camera.
4. **Light:** with **Stream**, the key light is on and **Follow the room's light
   (side)** is ticked. After a few seconds the side locks; reopen the properties and
   the Lighting group shows, for example, "Room light: from the left, locked".
5. **Exposure:** use **Exposure (EV)** like the exposure compensation on a camera.
   Switch on **Automatic: hold the face level** only if your camera's own automatic
   exposure is off. Never run both. While the automatic runs, the Exposure slider is
   hidden and your face goes to **Face target p90 (IRE)**.
6. **Face light and background:** **Face light (EV)** lights your face like a lamp
   close to it and fades out over **Face light reach**. **Background vs. you (EV)**
   darkens or lifts everything outside you. Stay within about half a stop (-0.5 to
   +0.5) and light the wall for the rest.
7. **Far from the camera?** Tick **Auto frame** in the Framing group. KlarCam crops
   around your face once, locks that frame and eases it in.

If you already use OBS's **Apply LUT** filters, you can load the same `.cube` file
into KlarCam's **Custom LUT file (.cube)**. KlarCam applies it the way OBS does, so
with **Applies to: Whole frame** and the same amount you get the same picture; with
**Person** the background stays out of it.

## 5. Before you go live

- **Vectorscope:** Off. The scope is drawn into the filter's output.
- **Debug view:** Off.
- **A/B: show the original:** unticked.

## Next

- [Every control explained](parameters.md)
- [Troubleshooting](troubleshooting.md), for example when KlarCam is missing from the
  filter list
