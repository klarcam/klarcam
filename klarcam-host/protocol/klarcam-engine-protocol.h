/*
 * KlarCam engine protocol: what the KlarCam filter host inside OBS Studio and
 * the KlarCam engine process (klarcam-engine.exe) exchange. See PROTOCOL.md.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Lars von Thienen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include <cstdint>

namespace kce {

// A host and an engine talk only when their protocol versions are equal.
constexpr uint32_t kProtocolVersion = 1;

// Filter instances one engine serves (frame slots in the shared block).
constexpr uint32_t kMaxInstances = 32;

// The largest payload a message may carry.
constexpr uint32_t kMaxPayload = 1u << 20;

// ---------------------------------------------------------------------------
// Messages. Two byte pipes per engine, "<name>-h2e" (host to engine) and
// "<name>-e2h" (engine to host); every message is a MsgHeader followed by
// `size` bytes of payload. Text payloads are UTF-8 (see klarcam-engine-text.h).
// ---------------------------------------------------------------------------
enum class Msg : uint16_t {
	// host -> engine
	Hello = 1,           // Hello + the data directory (UTF-8)
	InstanceCreate = 2,  // InstanceCreate + the instance's settings text
	InstanceDestroy = 3, // no payload
	Settings = 4,        // the instance's settings text, complete
	FrameSetup = 5,      // FrameSetup
	Request = 6,         // request text ("verb" plus arguments); answered by Reply
	Quit = 7,            // no payload: the engine finishes and exits
	Command = 8,         // command text ("verb" plus arguments) for an instance; no answer
	// engine -> host
	HelloReply = 101, // HelloReply + text (the adapter in use, or why the engine cannot run)
	Log = 102,        // uint8_t LogLevel + text
	Status = 103,     // the instance's status text (whole, replaces the previous one)
	Writeback = 104,  // settings text for the host to store in the instance's settings
	Reply = 105,      // reply text to the Request with the same `request` id
	SetupDone = 106,  // SetupDone + text (empty, or why the textures cannot be used)
};

enum class LogLevel : uint8_t { Debug = 0, Info = 1, Warning = 2, Error = 3 };

#pragma pack(push, 8)

struct MsgHeader {
	uint32_t size;     // payload bytes after the header
	uint16_t type;     // Msg
	uint16_t flags;    // 0
	uint32_t instance; // the filter instance; 0 = the engine as a whole
	uint32_t request;  // a Request's id, echoed by its Reply; 0 otherwise
};

struct Hello {
	uint32_t protocol;       // kProtocolVersion
	uint32_t host_pid;       // the OBS process
	uint32_t luid_low;       // the adapter OBS renders on: the engine's device must sit on it
	int32_t luid_high;
	uint64_t block_handle;   // the SharedBlock's file mapping, a handle valid in the engine
	uint32_t priority_class; // OBS's process priority class; the engine takes the same
	uint32_t reserved;
	char host_version[32];   // the host's version, for the log
};

struct HelloReply {
	uint32_t protocol;     // kProtocolVersion
	uint32_t engine_pid;
	uint32_t ok;           // 1 = ready for instances; 0 = the text says why not
	uint32_t permanent;    // with ok = 0: 1 = starting again cannot help (another protocol),
	                       // 0 = it may (a graphics adapter that is resetting)
	char engine_version[32];
};

struct InstanceCreate {
	uint32_t slot;        // the instance's FrameSlot in the SharedBlock
	uint32_t reserved;
	uint64_t ready_event; // auto-reset events, handles valid in the engine:
	uint64_t done_event;  // the host sets ready for a frame, the engine sets done
};

// Two RGBA8 textures the host created with keyed mutexes (D3D11 legacy shared
// handles, the kind OBS creates with GS_SHARED_KM_TEX), same size.
struct FrameSetup {
	uint32_t generation; // counts the host's setups; the frame slot names the one in use
	uint32_t width;
	uint32_t height;
	uint32_t format;     // DXGI_FORMAT of the textures
	uint64_t input_handle;
	uint64_t output_handle;
};

struct SetupDone {
	uint32_t generation;
	uint32_t ok; // 1 = both textures open; frames may follow
};

// ---------------------------------------------------------------------------
// Frames. The SharedBlock lives in a file mapping the host creates and hands to
// the engine in Hello; one FrameSlot per instance. Per frame:
//
//   host:   input  AcquireSync(0) -> copy the frame in -> ReleaseSync(1)
//           frame_sent = n, SetEvent(ready), wait for done (with a timeout)
//   engine: input  AcquireSync(1), output AcquireSync(0) -> process ->
//           output ReleaseSync(1), input ReleaseSync(0)
//           result, frame_done = n, SetEvent(done)
//   host:   result Processed: output AcquireSync(1) -> copy out -> ReleaseSync(0)
//
// A Passthrough or Error result leaves the output alone: the engine never
// acquired it, the host shows the frame unprocessed. The input always goes back
// to key 0. The host signals a frame only while ready_generation equals the
// generation of its last FrameSetup, and writes that generation into
// frame_generation with frame_sent; the engine answers a frame of another
// generation with kResultError without touching any texture, and ignores a
// ready signal whose frame_sent it has answered already.
// ---------------------------------------------------------------------------
constexpr uint32_t kBlockMagic = 0x3145434bu; // "KCE1"

enum FrameResult : int32_t {
	kResultNone = 0,
	kResultProcessed = 1,   // the output holds the processed frame
	kResultPassthrough = 2, // nothing to do (no license, or settings that change nothing)
	kResultError = 3,       // the frame could not be processed
};

struct FrameSlot {
	volatile int64_t frame_sent;        // host: the frame number it signaled
	volatile int64_t frame_done;        // engine: the frame number it finished
	volatile int32_t result;            // engine: FrameResult of frame_done
	volatile uint32_t ready_generation; // engine: the setup it holds open with the instance ready; 0 = none
	volatile uint32_t frame_generation; // host: the setup the signaled frame uses
	volatile float engine_ms;           // engine: ready event to done, last frame
	volatile float core_cpu_ms;         // engine: the core's averages over its perf window
	volatile float core_gpu_ms;
	volatile uint64_t busy_until;       // engine: GetTickCount64() up to which it holds frames back on
	                                    // purpose (applying settings, creating a processor: about a
	                                    // second); no hang until then. 0 = not busy
	volatile float auto_ev;             // engine: the EV the automatic exposure applies now
	uint32_t reserved[3];
};

struct SharedBlock {
	uint32_t magic;   // kBlockMagic
	uint32_t version; // kProtocolVersion
	uint32_t slot_count;
	uint32_t reserved;
	FrameSlot slots[kMaxInstances];
};

#pragma pack(pop)

static_assert(sizeof(MsgHeader) == 16, "MsgHeader layout");
static_assert(sizeof(Hello) == 64, "Hello layout");
static_assert(sizeof(HelloReply) == 48, "HelloReply layout");
static_assert(sizeof(InstanceCreate) == 24, "InstanceCreate layout");
static_assert(sizeof(FrameSetup) == 32, "FrameSetup layout");
static_assert(sizeof(SetupDone) == 8, "SetupDone layout");
static_assert(sizeof(FrameSlot) == 64, "FrameSlot layout");

} // namespace kce
