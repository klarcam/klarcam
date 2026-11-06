<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Lars von Thienen -->
# KlarCam engine protocol, version 1

How the KlarCam filter host inside OBS Studio (`klarcam.dll`, this folder, GPL-2.0-or-later)
and the KlarCam engine (`engine\klarcam-engine.exe` beside it, a separate program) work
together. The definitions are in `protocol/klarcam-engine-protocol.h` and
`protocol/klarcam-engine-text.h`, both under the MIT license, so either side may include them.

## Processes

- The host starts one engine per OBS process, with the first KlarCam filter, as a child of
  OBS in a job object that ends the engine together with OBS (a crash of OBS included).
- The engine renders on the adapter OBS renders on (the host passes its LUID); shared
  textures do not cross adapters.
- When the engine ends or stops answering, the host passes the video through and starts a
  new engine: at once, then after 1, 2, 4 and 8 s. After more than five failures within ten
  minutes, or an engine that cannot run at all, it waits until OBS rebuilds its graphics
  device or restarts.
- An engine that loses its graphics device ends itself with code 5, and the host starts a
  fresh one. When OBS loses its own device, the host releases its shared textures before OBS
  rebuilds its objects (OBS would try to reopen them by their old handles) and starts a fresh
  engine on the adapter OBS rebuilt on.
- Nothing on OBS's threads waits for the engine: messages go out through the host's writer
  thread, and only requests from OBS's UI thread (a preset, checking a typed key) wait for
  their reply, two seconds at most. A message the engine does not take within 2 s means it
  hangs; the host ends it.

## Connection

1. The host creates two named pipes, `\\.\pipe\<name>-h2e` (host to engine) and
   `\\.\pipe\<name>-e2h` (engine to host). The name is random, both pipes admit only one
   client and only the current user (DACL), and remote clients are rejected.
2. It starts `klarcam-engine.exe --pipe <name>` and checks that the process that connected to
   both pipes is the one it started.
3. It duplicates the handle of a file mapping holding the `SharedBlock` into the engine and
   sends `Hello`: protocol version, OBS's process id, the adapter LUID, the mapping's handle,
   OBS's priority class, the host's version, then the data directory (UTF-8).
4. The engine answers `HelloReply`: `ok` = 1 and the adapter's name, or `ok` = 0 and the
   reason. With `permanent` = 1 (another protocol version, a malformed hello) starting again
   cannot help; with 0 (an adapter that is resetting, say) the host tries again.

## Messages

Every message is a 16-byte `MsgHeader` (`size`, `type`, `flags`, `instance`, `request`) and
`size` bytes of payload. `instance` names a filter instance (0 = the engine as a whole).

| From | Message | Payload |
|---|---|---|
| host | `Hello` | `Hello` + data directory |
| host | `InstanceCreate` | `InstanceCreate` (slot, `ready` and `done` events duplicated into the engine) + settings text |
| host | `InstanceDestroy` | none |
| host | `Settings` | the instance's settings text, complete (only the newest one counts) |
| host | `FrameSetup` | `FrameSetup`: generation, size, format, the two shared textures' handles |
| host | `Request` | request text (`verb:s=...` and its arguments); answered by `Reply` with the same `request` id |
| host | `Command` | command text for an instance (`verb:s=...`); no answer |
| host | `Quit` | none |
| engine | `HelloReply` | `HelloReply` + text |
| engine | `Log` | one byte `LogLevel` + text; the host writes it into OBS's log |
| engine | `Status` | the instance's status text, whole; replaces the last one (about twice a second) |
| engine | `Writeback` | settings text the host stores in the instance's settings (a measured value) |
| engine | `Reply` | reply text |
| engine | `SetupDone` | `SetupDone` (generation, ok) + error text |

Closed pipes end the engine the same way `Quit` does.

## Text payloads

One value per line, `key:type=value`, with type `i` (integer), `d` (double), `b` (bool, 0 or
1) or `s` (string; backslash, CR and LF escaped as `\\`, `\r`, `\n`). Numbers are written in
the C locale's form. A reader skips lines it does not understand.

- **Settings**: the OBS settings of the filter instance as they are, every key with its
  value (defaults included), except `license_key` and `preset`, which the host handles. The
  measured locks (`relight_side_lock`, `frame_lock_x`, `frame_lock_y`, `frame_lock_z`) count
  only in `InstanceCreate`, as the scene saved them; after that they are the engine's, so a
  settings message can never undo a lock it has just measured.
- **Commands**: `relight_measure` (measure the room's light anew), `relight_swap` (a locked
  light side flips; with none, the host flips the direction slider's sign itself),
  `frame_reframe` (measure the auto frame anew). The engine writes the changed lock back.
- **Requests**: `look` (`preset:i=1` Natural or `2` Stream; the reply is the look's settings),
  `license_verify` and `license_activate` (`key:s=...`; the reply carries `ok` or `licensed`,
  `license_email`, `license_major`, `trial_days_left`, `trial_over`, `license_error`),
  `license_state`.
- **Status**: the license state (as above), `core` (the processor runs), `error`, `tracker`,
  `perf_valid`, `perf_cpu`, `perf_gpu`, `perf_passes`, `tracker_model`, `tracker_wait`,
  `tracker_older`, `relight_side_lock`, `light_side`, `frame_lock_z`, `beard`, `auto_ev`,
  `noise`, `denoise`, `lut_error_0` to `lut_error_3`, and `report` while measuring.
- **Writebacks**: `relight_side_lock`, `frame_lock_x`, `frame_lock_y`, `frame_lock_z` once
  measured or changed by a command. The automatic exposure starts from `exposure_ev` and
  ignores it from then on; the host itself writes the automatic's EV (`FrameSlot.auto_ev`,
  the status' `auto_ev`) into `exposure_ev` when the automatic is switched off (before it
  sends the settings) and when OBS saves the scene collection, so a restart continues at
  the level reached.

## Frames

Per instance the host creates two RGBA8 textures of the frame's size with keyed mutexes
(D3D11 legacy shared handles, what OBS creates with `GS_SHARED_KM_TEX`) and sends their handles
in `FrameSetup` with a new generation number. The engine opens them, answers `SetupDone`, and
writes the generation into the instance's `FrameSlot.ready_generation` once it can process.
The host signals a frame only while `ready_generation` equals its last setup's generation, and
resets both events whenever it makes new textures.

Per frame, within OBS's frame:

1. Host: input `AcquireSync(0)`, copy the frame in, `ReleaseSync(1)`, flush; write
   `frame_generation`, then `frame_sent` = n; set `ready`; wait for `done`. All KlarCam filters
   of an OBS frame share one wait budget of 0.75 frame intervals (at least 6 ms).
2. Engine: a signal whose `frame_sent` it has answered already is stale and ignored. Else:
   input `AcquireSync(1)`. To process: output `AcquireSync(0)`, process, output
   `ReleaseSync(1)`. Then input `ReleaseSync(0)`; write `result`, the timings, `auto_ev` and
   `frame_done` = n; set `done`.
3. Host: `result` Processed: output `AcquireSync(1)`, copy it out, `ReleaseSync(0)`, draw.
   Passthrough or Error: draw the frame unprocessed (the engine never acquired the output).

A frame of another generation is answered with Error and no texture touched. A frame that
misses the budget stays outstanding; the host sends no new one until it is answered and gives
its output back (`AcquireSync(1)`, `ReleaseSync(0)`) when it was processed. Meanwhile the last
processed frame holds for up to 0.5 s, then the video passes through.

- **Busy**: while the engine applies settings, a command or a new instance (a processor's
  models load for about a second), it sets `busy_until` in every slot (`GetTickCount64`, 30 s
  ahead) and clears it afterwards. The host waits on then.
- **Hang**: an engine that answers nothing for 2 s, outside `busy_until`, is ended and
  restarted.
- **Stuck**: textures left at the wrong key, or Error answers for half a second, make the host
  set up new textures; three such renewals within 10 s restart the engine.

## Not in the protocol

The image processing, the face tracking, the models, the license verification, the trial
and the looks' values live in the engine. The host knows the settings' names and how to show
them in OBS, and nothing of what they do.
