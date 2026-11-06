# Troubleshooting

If nothing here helps, [open a bug report](https://github.com/klarcam/klarcam/issues/new/choose) with the
status lines described in [Collecting the details for a report](#collecting-the-details-for-a-report).
Never post your license key or your e-mail address in an issue.

Contents:
[The picture passes through unchanged](#the-picture-passes-through-unchanged) ·
[GPU not supported](#the-graphics-card-is-not-supported-or-not-found) ·
[KlarCam is missing in OBS](#klarcam-is-missing-from-the-filter-list-in-obs) ·
[The KlarCam engine](#the-klarcam-engine-filter-in-obs) ·
[Virtual camera not listed](#the-virtual-camera-klarcam-is-not-listed) ·
[Trial over](#trial-over) ·
[Key rejected](#a-key-is-rejected) ·
[Picture problems](#picture-problems) ·
[Where KlarCam keeps things](#where-klarcam-keeps-things) ·
[Collecting the details](#collecting-the-details-for-a-report)

## The picture passes through unchanged

Check these in order:

1. **The trial is over and no key is stored.** The app's Status > License reads
   "Trial over: the camera frame goes out until a key is entered."; in OBS, the filter's
   license line reads "Trial over: the filter passes the video through until a key is
   entered (klarcam.com)." See [Trial over](#trial-over).
2. **A/B is on.** App: **Output** is set to **Original** (the **B** key switches). OBS:
   **A/B: show the original** is ticked.
3. **No face is found.** Most of KlarCam acts on the face. The app's Status shows
   "Face: tracked" or "Face: not found". Face the camera with your face lit; check
   the tracking with **Debug view > Show face landmarks**.
4. **The KlarCam engine is not running (filter in OBS).** For a second or two after
   OBS starts, and while a second KlarCam filter is added, the video passes through
   while the engine loads. If it stays unfiltered, see
   [The KlarCam engine](#the-klarcam-engine-filter-in-obs).
5. **The processing or the face tracking could not start.** See the next section.

## The graphics card is not supported or not found

KlarCam needs two things from the graphics card: Direct3D 11 to process the picture,
and Direct3D 12 for the face tracking, which runs on DirectML.

**Direct3D 11: the processing does not start.** The picture passes through
unchanged.

- **App:** the Status section shows "Core unavailable: <reason>" in red.
- **OBS:** the first line of the filter's Diagnostics group reads "KlarCam engine: not
  available (OBS does not render with Direct3D 11)", or a red line below it gives the
  reason the processing did not start. OBS's log (**Help > Log Files > View Current
  Log**) has a line starting with `[KlarCam]`, for example
  `[KlarCam] no Direct3D 11 device (OBS renderer is not D3D11), filter will pass through`.
- **What to do:** in OBS, **Settings > Advanced > Video > Renderer** must be
  **Direct3D 11** (the default on Windows); restart OBS after changing it. Install the
  current graphics driver from NVIDIA, AMD or Intel.

**Direct3D 12: the face tracking does not start.** The tracker line (OBS: Diagnostics
> **Face tracker**; app: Status > **Tracker**) does not end in "ready", and the log
has a line such as `tracker: no Direct3D 12 device on <your GPU>`. There is no
fallback without it, so nothing that needs the face works: smoothing, key light, skin
tone, the person regions of the LUTs, the background exposure, auto frame.

- **What to do:** install the current graphics driver from NVIDIA, AMD or Intel. To
  check the card: press Win+R, type `dxdiag`, open the **Display** tab and look at
  **DDI Version**. It reads 12 when the card and its driver support Direct3D 12; if
  it stays below 12 after a driver update, the card is too old for KlarCam.
- **Where the log is:** OBS: **Help > Log Files > View Current Log** (the lines start
  with `[KlarCam]`). App: **Debug > Show log**, or `app.log` in `%APPDATA%\KlarCam`.

**Two GPUs (laptops).** In the app, **Status > GPU** chooses the graphics card;
**Automatic** takes the one with the most dedicated memory. The filter in OBS runs on
the graphics card OBS itself uses.

If the tracker line reports another error, copy it into a bug report.

## KlarCam is missing from the filter list in OBS

1. **OBS version:** KlarCam is built for and tested with OBS Studio 33; older
   versions are not supported. **Help > About** shows your version. Update OBS, then
   run the KlarCam setup again.
2. **The right list:** KlarCam is a video filter. Right-click the camera source >
   **Filters**, then click **+** under **Effect Filters** (not under Audio/Video
   Filters).
3. **Installed while OBS was running?** Close OBS and run the setup again.
4. **Files in place?** The folder `%ProgramData%\obs-studio\plugins\klarcam` must
   exist and contain `klarcam.dll`. With the app setup, the filter is installed only
   if **Install the filter for OBS Studio** was ticked; the setup offers it only when
   OBS is installed. The filter setup does not install while the KlarCam app is
   installed: the app setup includes the filter, so run it again and tick the filter.
5. **OBS's log:** **Help > Log Files > View Current Log**, search for `[KlarCam]`. The
   line `[KlarCam] plugin loaded, version 0.4.0 (filter host; the KlarCam engine starts
   with the first filter)` means OBS found the filter.

## The KlarCam engine (filter in OBS)

The filter in OBS has two parts. OBS loads `klarcam.dll`, which hands each
frame to the KlarCam engine, `klarcam-engine.exe` in the plugin folder's `engine`
folder. The engine processes the frame and gives it back within the same OBS frame.
It starts with the first KlarCam filter and ends with OBS, also when OBS crashes;
Task Manager shows it while OBS has a KlarCam filter.

The first line of the filter's **Diagnostics** group shows the engine's state. OBS
draws the properties once when you open them, so close and reopen them to see a
change.

| Line | Meaning |
|---|---|
| `KlarCam engine: running on <graphics card>` | The engine runs. |
| `KlarCam engine: starting` | The engine loads, usually for 1.5 to 2 seconds, longer on a slow PC. The video passes through meanwhile, then the look fades in. After a failure, the reason follows in brackets. |
| `KlarCam engine: stopped: <reason>` | The engine does not run, and the video passes through. |
| `KlarCam engine: not available (OBS does not render with Direct3D 11)` | See [The graphics card is not supported or not found](#the-graphics-card-is-not-supported-or-not-found). |

- **The engine crashed.** OBS keeps running. The engine starts again at once and the
  look returns after about 1.5 seconds; after further failures it waits 1, 2, 4 and
  8 seconds before the next start. An engine that does not answer for 2 seconds is
  restarted.
- **The engine stopped after more than five failures within ten minutes,** or it
  cannot run at all. The filter passes the video through until OBS restarts. Restart
  OBS; if the line comes back, open a bug report with the lines starting with
  `[KlarCam]` from OBS's log.
- **The reason reads `klarcam-engine.exe is missing next to the plugin (reinstall
  KlarCam)`.** Run the setup again. Security software can block or remove the engine,
  because it is not code-signed yet; allow `klarcam-engine.exe` there, then run the
  setup again.
- **After a graphics driver update or reset,** the filter passes the video through for
  a moment and starts a fresh engine.

## The virtual camera "KlarCam" is not listed

1. **KlarCam must have started once** on this Windows user: it registers the camera
   when it starts. In the app's **Virtual camera** section, "Registered for this
   user" should show. If it says "Not registered", click **Register**.
2. **Output to "KlarCam"** must be ticked.
3. **Restart the call app.** Most apps look for cameras only when they start.
4. **The app has to support it.** Apps that use DirectShow cameras list KlarCam: Zoom,
   Microsoft Teams, Chrome, Edge (and Google Meet in them), OBS. Apps that only use
   Windows' Media Foundation cameras, such as the Windows Camera app, do not. The
   virtual camera is 64-bit, so 32-bit apps do not see it either.
5. **Each Windows user** registers their own camera: start KlarCam once in every
   Windows account that needs it.

**KlarCam is listed, but the picture is dark or frozen:** KlarCam is not running, or
**Output to "KlarCam"** is off. Without the app, the virtual camera delivers a dark
placeholder picture so the call app keeps a working camera. Start KlarCam (it may be
in the tray).

**A new output size does not arrive:** apps take a new **Output size** only when they
open the camera again. Switch the camera off and on in the call app, or restart it.

## Trial over

- Everything works for 14 days from the first start. After that, the camera picture
  passes through unchanged until a key is entered.
- The trial runs once. Reinstalling, updating, a new major version or removing a key
  do not restart it.
- Buy a key at [klarcam.com](https://klarcam.com). It appears on the page after the
  payment and arrives by e-mail.
- Enter it in the app under **Status > License** (paste, **Activate**) or in the
  filter's **License key** field in OBS. One key unlocks both.
- Lost the key? [klarcam.com/key](https://klarcam.com/key) sends it again to the
  purchase e-mail.

## A key is rejected

The app shows the reason in red under **Status > License**; the filter in OBS shows it
on the license line above the **License key** field.

| Message | What it means | What to do |
|---|---|---|
| `not a KlarCam key (it starts with KLARCAM-)` | The text does not start with `KLARCAM-`. | Copy the whole key, including `KLARCAM-`. |
| `incomplete key` | The key is cut off. | Copy it again, completely. |
| `damaged key (copy it completely)` | Characters are missing or changed. | Copy it again from the e-mail or the page, without spaces or line breaks in between. |
| `the key's signature is not valid` | The key was changed or does not come from klarcam.com. | Copy it again. If the message stays, write to hello@klarcam.com. |
| `unknown key version` | The key is newer than this KlarCam. | Install the current KlarCam. |
| `this key covers KlarCam 1.x; KlarCam 2 is a separate purchase (klarcam.com)` | A key bought for KlarCam 1 in a later major version. | KlarCam 2 is a separate purchase at the regular price. Your 1.x key keeps working with KlarCam 1.x releases. |
| `the key names no owner` | The key carries no owner. | Write to hello@klarcam.com. |
| `stored key rejected: <reason>` | The key stored earlier no longer passes, for the reason given (for example after installing a new major version). | As for the reason given. |
| `cannot write <folder>\license.key` | The key is valid, but KlarCam cannot save it. | Check that `%APPDATA%\KlarCam` is writable (security software can block it), then try again. |

Questions about a key or a purchase go to hello@klarcam.com, never into a public
issue.

## Picture problems

- **The key light jumps or sits on the wrong side.** **Follow the room's light (side)**
  measures once and then locks. After moving a lamp or the desk, click **Measure
  again**; to choose the other side yourself, click **Swap side**.
- **A halo around the head when the background is darker or brighter.** Stay within
  about half a stop of **Background vs. you (EV)** (-0.5 to +0.5), raise **Background
  falloff**, and light the wall for the rest. To separate yourself from the wall
  without an edge, lift your face with **Face light (EV)** instead of darkening the
  room.
- **Arms or shoulders flicker at the edge with a dark background.** That is the limit
  of the person detection, most visible with a white shirt against a strongly
  darkened wall. Use a smaller **Background vs. you (EV)** value.
- **The face looks flat or waxy.** Lower **Smoothing** and compare with A/B.
- **Stubble looks too crisp.** Lower **Sharpen (edges)**. The beard detection already
  keeps stubble out of the smoothing; check it with **Debug view > Show smoothing
  mask**.
- **The brightness pumps up and down.** Two automatics work against each other: turn
  off either the camera's automatic exposure or KlarCam's **Automatic: hold the face
  level**. In Teams, also switch off **Adjust brightness**.
- **Teams shows the picture differently from KlarCam's preview.** In Teams, Settings >
  Devices, switch off **Adjust brightness** and **Soft focus**. Your self-view in Teams
  is mirrored; the picture the others see is not.
- **A vectorscope or a colored overlay is visible in the stream (OBS).** Set
  **Vectorscope** and **Debug view** to **Off**: both are drawn into the filter's
  output.
- **The picture holds for a moment or shows the camera unfiltered (OBS).** When a
  frame comes back late from the engine, the filter shows the last processed frame
  for up to half a second, then passes the video through until the engine catches up.
  That happens under heavy GPU load. About once a minute, OBS's log has a line
  `[KlarCam] engine hand-off` with the time per frame and the number of late frames.
- **Low frame rate.** The app's Status shows the GPU time per frame ("Pipeline") and
  the frame rate ("Loop"). The vectorscope costs GPU time: switch it off on a slow
  machine. Then send a report with **Save diagnostics**.

## Where KlarCam keeps things

| What | Where |
|---|---|
| The app | `%LOCALAPPDATA%\Programs\KlarCam` |
| The filter for OBS Studio | `%ProgramData%\obs-studio\plugins\klarcam`, the KlarCam engine in its `engine` folder |
| The filter setup's uninstaller | `%LOCALAPPDATA%\Programs\KlarCam Filter` |
| App settings, the app's log, your license key | `%APPDATA%\KlarCam` |
| Filter settings in OBS | In your OBS scene collection, with the camera source |
| Snapshots, recordings, diagnostics reports | `Pictures\KlarCam` |
| Third-party license texts | `licenses` next to the app and next to the filter for OBS Studio |
| The filter host's license and source | In the filter's `licenses` folder: `klarcam-host-GPL-2.0.txt`, `klarcam-host-SOURCE.txt` and the source as a zip |

Uninstalling (Windows Settings > Apps > Installed apps: **KlarCam** for the app setup,
**KlarCam Filter** for the filter setup) removes the program and the filter but keeps
`%APPDATA%\KlarCam`, so your settings and your key survive a reinstall.

## Collecting the details for a report

The bug report form asks for:

- **Windows version:** press Win+R, type `winver`.
- **GPU and driver version:** Device Manager > Display adapters > your GPU >
  Properties > Driver, or the vendor's app.
- **OBS version** (for the filter): **Help > About**.
- **KlarCam version:** Windows Settings > Apps > Installed apps, or the OBS log line
  `[KlarCam] plugin loaded, version ...`.
- **Status lines.** App: the lines of the **Status** section (Running on, Tracker,
  Pipeline, Face, Loop). Better still: **Save diagnostics** and paste the text from
  the file in `Pictures\KlarCam`. OBS: the filter's **Diagnostics** group (KlarCam
  engine, Face tracker, Pipeline) and the lines starting with `[KlarCam]` from **Help >
  Log Files > View Current Log**.

Before posting, remove your e-mail address (the license line shows it after "Licensed
to") and, if you like, your computer name and Windows user name.
