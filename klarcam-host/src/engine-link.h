/*
 * KlarCam filter host for OBS Studio: starts and supervises the KlarCam engine
 * process (klarcam-engine.exe) and speaks the engine protocol with it. No
 * libobs in here, so a test host can drive the engine the same way.
 *
 * Nothing here blocks the caller on the engine except request(), which waits
 * for an answer and is for OBS's UI thread only: messages go out through a
 * writer thread, answers come in on the link's own thread.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2026 Lars von Thienen
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. See the file COPYING.
 */
#pragma once

#include "klarcam-engine-protocol.h"

#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace klarcam_host {

struct EngineConfig {
	std::wstring exe;         // klarcam-engine.exe
	std::string data_dir;     // the engine's data directory (UTF-8)
	LUID luid = {};           // the adapter the host renders on
	std::string host_version; // for the engine's log
};

// Called on the link's own thread.
struct EngineEvents {
	std::function<void(kce::LogLevel, const std::string &)> log;
	std::function<void(uint32_t id, const std::string &text)> status;
	std::function<void(uint32_t id, const std::string &text)> writeback;
	std::function<void(uint32_t id, uint32_t generation, bool ok, const std::string &error)> setup_done;
	std::function<void()> disconnected; // the engine is gone: its statuses no longer hold
};

// An instance's frame slot and events; valid until remove_instance.
struct InstanceHandles {
	kce::FrameSlot *slot = nullptr;
	HANDLE ready = nullptr; // the host sets it for a frame
	HANDLE done = nullptr;  // the engine sets it when the frame is done
};

class EngineLink {
public:
	enum class State { Starting, Running, Failed };
	// ok = false: the engine went away before it answered.
	using ReplyFn = std::function<void(bool ok, const std::string &reply)>;

	EngineLink(EngineConfig config, EngineEvents events);
	~EngineLink(); // asks the engine to quit, ends it after 2 s
	EngineLink(const EngineLink &) = delete;
	EngineLink &operator=(const EngineLink &) = delete;

	// Instances live in the link and are created in every engine it starts.
	// add_instance returns 0 when all kce::kMaxInstances slots are taken.
	uint32_t add_instance(const std::string &settings, InstanceHandles &handles);
	void remove_instance(uint32_t id);
	void update_settings(uint32_t id, const std::string &settings);
	void frame_setup(uint32_t id, const kce::FrameSetup &setup);
	void command(uint32_t id, const std::string &text);
	// Waits up to `timeout_ms` for the answer: OBS's UI thread only. False when
	// the engine is not running or did not answer in time.
	bool request(uint32_t id, const std::string &text, std::string &reply, DWORD timeout_ms);
	// The answer comes later, on the link's thread.
	void request_async(uint32_t id, const std::string &text, ReplyFn done);

	// Connected and past the engine's hello.
	bool ready() const { return ready_; }
	// Counts the engines started; a new value means: set the textures up again.
	uint32_t epoch() const { return epoch_; }
	// Ends a hanging engine; the link starts a new one.
	void restart(const std::string &why);
	// OBS rebuilt its graphics device: a fresh engine on its adapter, also after
	// the link had given up.
	void device_rebuilt(LUID luid);
	State state() const;
	// The adapter while running, else why not (empty while simply starting).
	std::string state_text() const;

private:
	struct Instance {
		uint32_t slot = 0;
		HANDLE ready = nullptr, done = nullptr;
		std::string settings;
		bool created = false; // in the engine now running
	};
	struct Pending {
		bool answered = false;
		std::string reply;
		ReplyFn done; // async requests
	};
	struct Out {
		std::string bytes;
		uint16_t type = 0;
		uint32_t instance = 0;
	};

	void run();
	bool launch(std::string &error, bool &permanent);
	void serve();
	void disconnect();
	bool send(kce::Msg type, uint32_t instance, uint32_t request, const void *a, size_t na, const void *b = nullptr,
		  size_t nb = 0);
	bool send_create(uint32_t id, Instance &inst); // with m_ held
	void write_loop();
	bool read_message(kce::MsgHeader &h, std::string &payload, DWORD timeout_ms);
	void dispatch(const kce::MsgHeader &h, std::string &payload);
	void kill(const std::string &why);
	void log(kce::LogLevel level, const std::string &text);
	void reset_slots();

	EngineConfig cfg_;
	EngineEvents ev_;

	mutable std::mutex m_; // instances_, slots, state texts, cfg_.luid
	std::map<uint32_t, Instance> instances_;
	uint32_t next_id_ = 1;
	bool slot_used_[kce::kMaxInstances] = {};
	State state_ = State::Starting;
	std::string adapter_, error_;
	std::vector<ULONGLONG> failures_; // recent failures, for the restart backoff

	HANDLE map_ = nullptr; // the SharedBlock
	kce::SharedBlock *block_ = nullptr;
	HANDLE job_ = nullptr;    // ends the engine with OBS, crash included
	HANDLE stop_ = nullptr;   // manual reset: the link is ending
	HANDLE revive_ = nullptr; // auto reset: try again after giving up
	std::thread thread_;

	// The connection. The pipes are set and closed by the link's thread, around
	// the writer thread's life; the process handle is guarded by proc_m_.
	HANDLE h2e_ = INVALID_HANDLE_VALUE, e2h_ = INVALID_HANDLE_VALUE;
	HANDLE read_ev_ = nullptr, write_ev_ = nullptr;
	std::mutex proc_m_;
	HANDLE proc_ = nullptr;
	std::atomic<bool> ready_{false};
	std::atomic<uint32_t> epoch_{0};
	std::atomic<bool> killed_{false};   // ended on purpose (restart, hang)
	std::atomic<bool> stopping_{false}; // the link is being destroyed

	// Messages out: queued by any thread, written by the writer thread.
	std::mutex out_m_;
	std::condition_variable out_cv_;
	std::deque<Out> outq_;
	bool out_open_ = false;
	bool out_stop_ = false;
	bool writing_ = false; // a message is on its way into the pipe
	std::thread writer_;

	std::mutex req_m_;
	std::condition_variable req_cv_;
	uint32_t next_request_ = 1;
	std::map<uint32_t, Pending> pending_;
};

} // namespace klarcam_host
