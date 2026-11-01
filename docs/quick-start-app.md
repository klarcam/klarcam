# Quick start: the KlarCam app

The app reads your camera and sends the processed picture to the virtual camera
**KlarCam**, which Zoom, Microsoft Teams, Google Meet in Chrome or Edge and other
apps can use. Every control is explained in the [parameter reference](parameters.md).

## 1. Install and start

1. Run `KlarCam-Setup-0.2.0.exe`. It installs for your Windows user, no admin rights
   needed. If you use OBS Studio, tick **Install the OBS Studio filter**: the app setup
   includes the filter. Had you installed the filter-only setup before, uninstall
   **KlarCam OBS Filter** first.
2. Start **KlarCam** from the Start menu.
3. KlarCam opens your camera (the one you used last, or the first one it finds) and
   registers the virtual camera **KlarCam** for your Windows user. The **Virtual
   camera** section then shows "Registered for this user".

## 2. Choose your camera

In the **Camera** section, choose the **Device** and a **Format**. 1080p or more works
best. **Refresh** lists cameras you plug in later.

## 3. Use it in your call app

In Zoom, Teams, Meet or any other app, choose the camera **KlarCam**. Once the app
reads it, the **Virtual camera** section shows "An app is receiving the camera".

- KlarCam has to run. Closing its window hides it to the tray, and the camera keeps
  working. To quit, right-click the tray icon and choose **Quit KlarCam**.
- In Teams, open Settings > Devices and switch off **Adjust brightness** and **Soft
  focus**; otherwise Teams adjusts the picture again after KlarCam. Your self-view in
  Teams is mirrored; the picture the others see is not.
- **Output size** (1920 x 1080 by default) reaches an app when it opens the camera
  again.

## 4. Pick a look

The bar at the top of the panel:

- **Output: Processed / Original** switches between the processed and the untouched
  camera picture, in the preview and on the virtual camera. The **B** key does the
  same.
- **Look: Natural / Stream** applies a built-in look. KlarCam starts with **Stream**.
- **Save...** stores the current settings in slot 1, 2 or 3; the numbered buttons
  bring them back.

A look also sets Exposure and Background, so choose the look first.

## 5. Fine-tune

- **Skin > Smoothing:** until the skin looks calm while the face keeps its shape.
  With a webcam, **Sharpen (edges)** at 0.2 to 0.3 is plenty: webcams already sharpen
  in the camera.
- **Exposure:** use **Exposure (EV)** like the exposure compensation on a camera.
  Switch on **Automatic: hold the face level** only if your camera's own automatic
  exposure is off. Never run both.
- **Background (EV):** stay within about half a stop (-0.5 to +0.5) and light the wall
  for the rest.
- **Far from the camera?** Tick **Auto frame (fill the frame with the face)** in the
  Framing section.

## 6. License

The **Status** section at the bottom of the panel shows the license under
**License**: "Trial: 14 days left, everything works." Paste your key into the field
and click **Activate**; **Buy a key (klarcam.com)** opens the shop. The key also
unlocks the OBS filter.

## Next

- [Every control explained](parameters.md)
- [Troubleshooting](troubleshooting.md), for example when the virtual camera does not
  show up
