# Third-party notices

KlarCam contains or uses the components below. The installers put this overview and
the full license texts next to the program: in `%LOCALAPPDATA%\Programs\KlarCam` with
a `licenses` folder for the app, and in `%ProgramData%\obs-studio\plugins\klarcam`
with a `licenses` folder for the OBS filter. The same license texts are in the
[licenses](licenses) folder of this repository.

## GPUPixel (Apache License 2.0)

Copyright (c) PixPark. https://github.com/pixpark/gpupixel

KlarCam's skin-smoothing math is a port to HLSL of GPUPixel's `BeautyFaceUnitFilter`,
`BoxMonoBlurFilter` and `BoxDifferenceFilter` (mean and variance adaptive blend,
box blur, high-pass sharpening). No GPUPixel source files are compiled into the app
or the OBS filter. License text: [licenses/Apache-2.0.txt](licenses/Apache-2.0.txt).

## MediaPipe models (Apache License 2.0)

Copyright (c) Google LLC. https://github.com/google-ai-edge/mediapipe

KlarCam ships the MediaPipe BlazeFace (short range), Face Mesh (468 landmarks) and
Selfie Segmentation models, converted from TFLite to ONNX without changing the
weights, and uses MediaPipe's face mesh topology for the face regions. The license
text also ships with the models as `data\models\LICENSE.mediapipe.txt`. License text:
[licenses/Apache-2.0.txt](licenses/Apache-2.0.txt).

## ONNX Runtime (MIT License)

Copyright (c) Microsoft Corporation. https://github.com/microsoft/onnxruntime

Redistributed as `onnxruntime.dll` (DirectML build). License text:
[licenses/onnxruntime-MIT.txt](licenses/onnxruntime-MIT.txt).

## DirectML (Microsoft Software License Terms)

Copyright (c) Microsoft Corporation.

Redistributed as `DirectML.dll` from the `Microsoft.AI.DirectML` package under its
redistribution terms. License text: [licenses/directml.txt](licenses/directml.txt).

## Microsoft Visual C++ Runtime

Copyright (c) Microsoft Corporation.

`msvcp140*.dll`, `vcruntime140*.dll` and `concrt140.dll` are the Visual C++
Redistributable, shipped next to the program under the Visual Studio redistributable
license. No separate notice is required; they are listed for completeness.

## Dear ImGui (MIT License)

Copyright (c) 2014-2026 Omar Cornut. https://github.com/ocornut/imgui

The KlarCam app's control window. License text:
[licenses/imgui-MIT.txt](licenses/imgui-MIT.txt).

## DirectShow BaseClasses (MIT License)

Copyright (c) Microsoft Corporation.
https://github.com/microsoft/Windows-classic-samples

Compiled into the virtual camera `klarcam-vcam.dll`. License text:
[licenses/dshow-baseclasses-MIT.txt](licenses/dshow-baseclasses-MIT.txt).

## Bundled color LUTs

The LUTs "Reduce red skin" and "Reduce yellow skin" were created by Lars von Thienen
in DaVinci Resolve and are part of KlarCam. The "Contrast look" LUT is generated from
KlarCam's own look model. No third-party LUT data is included.

## libobs (GNU GPL v2)

Copyright (c) Lain Bailey and the OBS Project. https://github.com/obsproject/obs-studio

The OBS filter is loaded by OBS Studio at run time and is compiled against the public
libobs headers. No libobs source is redistributed with it.

## w32-pthreads (GNU LGPL v2.1)

Headers used at compile time only, as required by the libobs public headers. No
w32-pthreads code is linked into or redistributed with the OBS filter.
