/*
 * KlarCam filter host for OBS Studio: starts and supervises the KlarCam engine
 * process and speaks the engine protocol with it.
 *
 * The engine runs as a child of OBS in a job object that ends it with OBS (a
 * crash of OBS included). The link creates two named pipes with random names
 * that only this user may open, starts the engine with their name, checks that
 * the engine is the process that connected, and hands it the shared block and
 * the instances' events as duplicated handles. Messages go out through a writer
 * thread, so no caller ever waits on the engine; a message the engine does not
 * take within 2 s means it hangs, and it is ended. When the engine ends, the
 * link starts a new one: at once, then after 1, 2, 4 and 8 s. After more than
 * five failures within ten minutes, or an engine that cannot run at all, it
 * waits until OBS rebuilds its graphics device or restarts.
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
#include "engine-link.h"

#include <bcrypt.h>
#include <sddl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace klarcam_host {
namespace {

constexpr DWORD kConnectMs = 5000; // the engine opens its pipes
constexpr DWORD kHelloMs = 15000;  // it creates its device and answers
constexpr DWORD kWriteMs = 2000;   // a message the engine does not take in 2 s: it hangs
constexpr ULONGLONG kFailureWindowMs = 10ull * 60 * 1000;
constexpr size_t kMaxFailures = 5;

std::wstring hex(const unsigned char *p, size_t n)
{
	static const wchar_t digits[] = L"0123456789abcdef";
	std::wstring s;
	for (size_t i = 0; i < n; ++i) {
		s.push_back(digits[p[i] >> 4]);
		s.push_back(digits[p[i] & 15]);
	}
	return s;
}

std::string code_text(DWORD code)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "0x%lx", static_cast<unsigned long>(code));
	return buf;
}

// Only this user may open the pipes.
struct UserOnly {
	SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE};
	PSECURITY_DESCRIPTOR sd = nullptr;

	UserOnly()
	{
		HANDLE token = nullptr;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
			return;
		DWORD n = 0;
		GetTokenInformation(token, TokenUser, nullptr, 0, &n);
		std::vector<BYTE> buf(n);
		LPWSTR sid = nullptr;
		if (n && GetTokenInformation(token, TokenUser, buf.data(), n, &n) &&
		    ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(buf.data())->User.Sid, &sid)) {
			const std::wstring sddl = std::wstring(L"D:P(A;;GA;;;") + sid + L")";
			if (ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr))
				sa.lpSecurityDescriptor = sd;
			LocalFree(sid);
		}
		CloseHandle(token);
	}
	~UserOnly()
	{
		if (sd)
			LocalFree(sd);
	}
};

void close_pipe(HANDLE &h)
{
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
	h = INVALID_HANDLE_VALUE;
}

void close_handle(HANDLE &h)
{
	if (h)
		CloseHandle(h);
	h = nullptr;
}

} // namespace

EngineLink::EngineLink(EngineConfig config, EngineEvents events) : cfg_(std::move(config)), ev_(std::move(events))
{
	map_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(kce::SharedBlock), nullptr);
	if (map_)
		block_ = static_cast<kce::SharedBlock *>(
			MapViewOfFile(map_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(kce::SharedBlock)));
	if (block_) {
		memset(block_, 0, sizeof(*block_));
		block_->magic = kce::kBlockMagic;
		block_->version = kce::kProtocolVersion;
		block_->slot_count = kce::kMaxInstances;
	}
	job_ = CreateJobObjectW(nullptr, nullptr);
	if (job_) {
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION li = {};
		li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &li, sizeof(li));
	}
	stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	revive_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	write_ev_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	read_ev_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!block_ || !stop_ || !revive_ || !write_ev_ || !read_ev_) {
		state_ = State::Failed;
		error_ = "cannot create the shared block";
		return;
	}
	thread_ = std::thread([this] { run(); });
}

EngineLink::~EngineLink()
{
	// A clean end: Quit goes out first, then the pipes close; the link's thread
	// waits for the engine (disconnect) and ends it after 2 s.
	stopping_ = true;
	if (ready_)
		send(kce::Msg::Quit, 0, 0, nullptr, 0);
	if (stop_)
		SetEvent(stop_);
	if (thread_.joinable())
		thread_.join();
	for (auto &it : instances_) {
		CloseHandle(it.second.ready);
		CloseHandle(it.second.done);
	}
	if (block_)
		UnmapViewOfFile(block_);
	close_handle(map_);
	close_handle(job_); // ends whatever is left in it
	close_handle(stop_);
	close_handle(revive_);
	close_handle(write_ev_);
	close_handle(read_ev_);
}

uint32_t EngineLink::add_instance(const std::string &settings, InstanceHandles &handles)
{
	std::lock_guard<std::mutex> lock(m_);
	if (!block_)
		return 0;
	uint32_t slot = 0;
	while (slot < kce::kMaxInstances && slot_used_[slot])
		++slot;
	if (slot == kce::kMaxInstances)
		return 0;
	Instance inst;
	inst.slot = slot;
	inst.ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	inst.done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!inst.ready || !inst.done) {
		close_handle(inst.ready);
		close_handle(inst.done);
		return 0;
	}
	inst.settings = settings;
	// The frame numbers go on from the slot's last user: a late answer to a
	// removed filter's frame never counts as an answer to this one's.
	kce::FrameSlot &s = block_->slots[slot];
	s.result = kce::kResultNone;
	s.ready_generation = 0;
	s.frame_generation = 0;
	s.busy_until = 0;
	s.auto_ev = 0.0f;
	slot_used_[slot] = true;
	const uint32_t id = next_id_++;
	if (ready_)
		send_create(id, inst);
	handles.slot = &s;
	handles.ready = inst.ready;
	handles.done = inst.done;
	instances_.emplace(id, std::move(inst));
	return id;
}

void EngineLink::remove_instance(uint32_t id)
{
	std::lock_guard<std::mutex> lock(m_);
	const auto it = instances_.find(id);
	if (it == instances_.end())
		return;
	if (ready_ && it->second.created)
		send(kce::Msg::InstanceDestroy, id, 0, nullptr, 0);
	InterlockedExchange(reinterpret_cast<volatile LONG *>(&block_->slots[it->second.slot].ready_generation), 0);
	CloseHandle(it->second.ready);
	CloseHandle(it->second.done);
	slot_used_[it->second.slot] = false;
	instances_.erase(it);
}

void EngineLink::update_settings(uint32_t id, const std::string &settings)
{
	std::lock_guard<std::mutex> lock(m_);
	const auto it = instances_.find(id);
	if (it == instances_.end())
		return;
	it->second.settings = settings;
	if (ready_ && it->second.created)
		send(kce::Msg::Settings, id, 0, settings.data(), settings.size());
}

void EngineLink::frame_setup(uint32_t id, const kce::FrameSetup &setup)
{
	std::lock_guard<std::mutex> lock(m_);
	const auto it = instances_.find(id);
	if (it != instances_.end() && ready_ && it->second.created)
		send(kce::Msg::FrameSetup, id, 0, &setup, sizeof(setup));
}

void EngineLink::command(uint32_t id, const std::string &text)
{
	std::lock_guard<std::mutex> lock(m_);
	const auto it = instances_.find(id);
	if (it != instances_.end() && ready_ && it->second.created)
		send(kce::Msg::Command, id, 0, text.data(), text.size());
}

bool EngineLink::request(uint32_t id, const std::string &text, std::string &reply, DWORD timeout_ms)
{
	if (!ready_)
		return false;
	uint32_t rid;
	{
		std::lock_guard<std::mutex> lock(req_m_);
		rid = next_request_++;
		pending_[rid] = Pending{};
	}
	const bool sent = send(kce::Msg::Request, id, rid, text.data(), text.size());
	std::unique_lock<std::mutex> lock(req_m_);
	if (sent)
		req_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
			const auto it = pending_.find(rid);
			return it == pending_.end() || it->second.answered || !ready_;
		});
	bool got = false;
	const auto it = pending_.find(rid);
	if (it != pending_.end()) {
		if (it->second.answered) {
			reply = std::move(it->second.reply);
			got = true;
		}
		pending_.erase(it);
	}
	return got;
}

void EngineLink::request_async(uint32_t id, const std::string &text, ReplyFn done)
{
	uint32_t rid;
	{
		std::lock_guard<std::mutex> lock(req_m_);
		rid = next_request_++;
		Pending p;
		p.done = std::move(done);
		pending_[rid] = std::move(p);
	}
	if (ready_ && send(kce::Msg::Request, id, rid, text.data(), text.size()))
		return;
	ReplyFn fn;
	{
		std::lock_guard<std::mutex> lock(req_m_);
		const auto it = pending_.find(rid);
		if (it != pending_.end()) {
			fn = std::move(it->second.done);
			pending_.erase(it);
		}
	}
	if (fn)
		fn(false, {});
}

void EngineLink::restart(const std::string &why)
{
	kill(why);
}

void EngineLink::device_rebuilt(LUID luid)
{
	State st;
	{
		std::lock_guard<std::mutex> lock(m_);
		cfg_.luid = luid;
		st = state_;
	}
	// The engine's textures belonged to OBS's old device, and the engine's own
	// device may be gone as well: a fresh engine, even after the link gave up.
	if (st == State::Failed) {
		log(kce::LogLevel::Info, "OBS rebuilt its graphics device; starting the KlarCam engine again");
		SetEvent(revive_);
	} else {
		kill("OBS rebuilt its graphics device");
	}
}

EngineLink::State EngineLink::state() const
{
	std::lock_guard<std::mutex> lock(m_);
	return state_;
}

std::string EngineLink::state_text() const
{
	std::lock_guard<std::mutex> lock(m_);
	return state_ == State::Running ? adapter_ : error_;
}

// ---------------------------------------------------------------------------
// The link's thread: start, serve, notice the end, start again.
// ---------------------------------------------------------------------------
void EngineLink::run()
{
	DWORD backoff = 0;
	for (;;) {
		if (WaitForSingleObject(stop_, backoff) == WAIT_OBJECT_0)
			return;
		std::string error;
		bool permanent = false;
		if (launch(error, permanent)) {
			serve();
			error = killed_ ? "restarted" : "the engine ended";
		} else if (!stopping_ && !permanent) {
			log(kce::LogLevel::Warning, "the KlarCam engine did not start: " + error);
		}
		disconnect();
		if (stopping_ || WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0)
			return;
		bool give_up = false;
		{
			std::lock_guard<std::mutex> lock(m_);
			error_ = error;
			const ULONGLONG now = GetTickCount64();
			failures_.push_back(now);
			failures_.erase(std::remove_if(failures_.begin(), failures_.end(),
						       [now](ULONGLONG t) { return now - t > kFailureWindowMs; }),
					failures_.end());
			if (permanent) {
				state_ = State::Failed;
				log(kce::LogLevel::Error,
				    "the KlarCam engine cannot run: " + error + "; the filter passes the video through");
				give_up = true;
			} else if (failures_.size() > kMaxFailures) {
				state_ = State::Failed;
				error_ = std::to_string(failures_.size()) + " failures in ten minutes";
				log(kce::LogLevel::Error, "the KlarCam engine stopped after " + error_ +
								  "; the filter passes the video through until OBS restarts");
				give_up = true;
			} else {
				// At once the first time (a crash mid-stream shows the raw
				// camera only while the engine loads), then after 1, 2, 4 and 8 s.
				state_ = State::Starting;
				backoff = failures_.size() == 1 ? 0 : 1000u << std::min<size_t>(failures_.size() - 2, 3);
				log(kce::LogLevel::Info,
				    backoff ? "starting the KlarCam engine again in " + std::to_string(backoff / 1000) + " s"
					    : std::string("starting the KlarCam engine again"));
			}
		}
		if (give_up) {
			HANDLE w[2] = {stop_, revive_};
			if (WaitForMultipleObjects(2, w, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
				return;
			std::lock_guard<std::mutex> lock(m_);
			failures_.clear();
			error_.clear();
			state_ = State::Starting;
			backoff = 0;
		}
	}
}

bool EngineLink::launch(std::string &error, bool &permanent)
{
	const ULONGLONG t0 = GetTickCount64();
	if (GetFileAttributesW(cfg_.exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
		error = "klarcam-engine.exe is missing in the plugin's engine folder (reinstall KlarCam)";
		permanent = true;
		return false;
	}
	unsigned char rnd[12] = {};
	BCryptGenRandom(nullptr, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	const std::wstring name = L"klarcam-engine-" + std::to_wstring(GetCurrentProcessId()) + L"-" + hex(rnd, sizeof(rnd));
	const std::wstring base = L"\\\\.\\pipe\\" + name;
	UserOnly sec;
	const DWORD mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
	HANDLE h2e = CreateNamedPipeW((base + L"-h2e").c_str(),
				      PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED, mode, 1,
				      1 << 16, 1 << 16, 0, &sec.sa);
	HANDLE e2h = CreateNamedPipeW((base + L"-e2h").c_str(),
				      PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED, mode, 1,
				      1 << 16, 1 << 16, 0, &sec.sa);
	HANDLE c1 = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	HANDLE c2 = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	OVERLAPPED o1 = {}, o2 = {};
	o1.hEvent = c1;
	o2.hEvent = c2;
	bool pending1 = false, pending2 = false;
	PROCESS_INFORMATION pi = {};
	auto fail = [&](const std::string &why) {
		error = why;
		if (pi.hProcess) {
			TerminateProcess(pi.hProcess, 1);
			CloseHandle(pi.hProcess);
		}
		// A connect still pending writes into its OVERLAPPED: finish it first.
		DWORD n = 0;
		if (pending1) {
			CancelIoEx(h2e, &o1);
			GetOverlappedResult(h2e, &o1, &n, TRUE);
		}
		if (pending2) {
			CancelIoEx(e2h, &o2);
			GetOverlappedResult(e2h, &o2, &n, TRUE);
		}
		close_pipe(h2e);
		close_pipe(e2h);
		close_handle(c1);
		close_handle(c2);
		return false;
	};
	auto connect = [](HANDLE pipe, OVERLAPPED &ov, bool &pending) {
		if (ConnectNamedPipe(pipe, &ov))
			return true;
		const DWORD e = GetLastError();
		if (e == ERROR_PIPE_CONNECTED)
			SetEvent(ov.hEvent);
		pending = e == ERROR_IO_PENDING;
		return e == ERROR_PIPE_CONNECTED || e == ERROR_IO_PENDING;
	};
	if (h2e == INVALID_HANDLE_VALUE || e2h == INVALID_HANDLE_VALUE || !c1 || !c2 || !connect(h2e, o1, pending1) ||
	    !connect(e2h, o2, pending2))
		return fail("cannot create the engine's pipes (" + code_text(GetLastError()) + ")");

	const std::wstring dir = cfg_.exe.substr(0, cfg_.exe.find_last_of(L"\\/"));
	std::wstring cmd = L"\"" + cfg_.exe + L"\" --pipe " + name;
	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	if (!CreateProcessW(cfg_.exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
			    nullptr, dir.c_str(), &si, &pi)) {
		pi.hProcess = nullptr;
		return fail("cannot start klarcam-engine.exe (" + code_text(GetLastError()) + ")");
	}
	if (job_ && !AssignProcessToJobObject(job_, pi.hProcess))
		log(kce::LogLevel::Warning, "the engine runs outside OBS's job object (" + code_text(GetLastError()) + ")");
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);

	// Both pipes connected, by the process we started.
	const ULONGLONG deadline = GetTickCount64() + kConnectMs;
	auto wait_for = [&](HANDLE ev) {
		const ULONGLONG now = GetTickCount64();
		if (now >= deadline)
			return false;
		HANDLE w[3] = {ev, stop_, pi.hProcess};
		return WaitForMultipleObjects(3, w, FALSE, static_cast<DWORD>(deadline - now)) == WAIT_OBJECT_0;
	};
	if (!wait_for(c1) || !wait_for(c2)) {
		DWORD code = STILL_ACTIVE;
		GetExitCodeProcess(pi.hProcess, &code);
		return fail(code == STILL_ACTIVE ? "the engine did not connect"
						 : "the engine ended at its start (exit code " + code_text(code) + ")");
	}
	pending1 = pending2 = false; // both connects completed
	ULONG pid1 = 0, pid2 = 0;
	if (!GetNamedPipeClientProcessId(h2e, &pid1) || !GetNamedPipeClientProcessId(e2h, &pid2) ||
	    pid1 != pi.dwProcessId || pid2 != pi.dwProcessId)
		return fail("another process connected to the engine's pipes");
	close_handle(c1);
	close_handle(c2);

	// The connection belongs to the link from here; disconnect() ends it.
	h2e_ = h2e;
	e2h_ = e2h;
	{
		std::lock_guard<std::mutex> lock(proc_m_);
		proc_ = pi.hProcess;
	}
	reset_slots();
	{
		std::lock_guard<std::mutex> lock(out_m_);
		outq_.clear();
		out_open_ = true;
		out_stop_ = false;
	}
	writer_ = std::thread([this] { write_loop(); });

	HANDLE remote_map = nullptr;
	if (!DuplicateHandle(GetCurrentProcess(), map_, pi.hProcess, &remote_map, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
		error = "cannot hand the shared block to the engine (" + code_text(GetLastError()) + ")";
		return false;
	}
	kce::Hello hello = {};
	hello.protocol = kce::kProtocolVersion;
	hello.host_pid = GetCurrentProcessId();
	{
		std::lock_guard<std::mutex> lock(m_);
		hello.luid_low = cfg_.luid.LowPart;
		hello.luid_high = cfg_.luid.HighPart;
	}
	hello.block_handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(remote_map));
	hello.priority_class = GetPriorityClass(GetCurrentProcess());
	strncpy_s(hello.host_version, cfg_.host_version.c_str(), _TRUNCATE);
	if (!send(kce::Msg::Hello, 0, 0, &hello, sizeof(hello), cfg_.data_dir.data(), cfg_.data_dir.size())) {
		error = "cannot reach the engine";
		return false;
	}

	// Its answer; log lines before it pass through.
	const ULONGLONG hello_deadline = GetTickCount64() + kHelloMs;
	for (;;) {
		const ULONGLONG now = GetTickCount64();
		kce::MsgHeader h;
		std::string payload;
		if (now >= hello_deadline || !read_message(h, payload, static_cast<DWORD>(hello_deadline - now))) {
			error = "the engine did not answer";
			return false;
		}
		if (h.type != static_cast<uint16_t>(kce::Msg::HelloReply)) {
			dispatch(h, payload);
			continue;
		}
		if (payload.size() < sizeof(kce::HelloReply)) {
			error = "the engine's answer is malformed";
			return false;
		}
		kce::HelloReply r;
		memcpy(&r, payload.data(), sizeof(r));
		const std::string text = payload.substr(sizeof(r));
		if (!r.ok) {
			error = text;
			permanent = r.permanent != 0;
			return false;
		}
		std::lock_guard<std::mutex> lock(m_);
		adapter_ = text;
		error_.clear();
		state_ = State::Running;
		// The instances first, then the new epoch: a filter that sees it sets
		// up its textures, and the setup must find the instance.
		for (auto &it : instances_)
			send_create(it.first, it.second);
		++epoch_;
		ready_ = true;
		char eng_version[sizeof(r.engine_version) + 1] = {};
		memcpy(eng_version, r.engine_version, sizeof(r.engine_version));
		log(kce::LogLevel::Info, std::string("engine ") + eng_version + " started (pid " +
						 std::to_string(r.engine_pid) + ") on " + text + ", ready after " +
						 std::to_string(GetTickCount64() - t0) + " ms");
		return true;
	}
}

void EngineLink::serve()
{
	for (;;) {
		kce::MsgHeader h;
		std::string payload;
		if (!read_message(h, payload, INFINITE))
			return;
		dispatch(h, payload);
	}
}

void EngineLink::disconnect()
{
	const bool was_ready = ready_.exchange(false);
	{
		std::lock_guard<std::mutex> lock(m_);
		for (auto &it : instances_)
			it.second.created = false;
	}
	reset_slots();

	// Waiting requests give up (ready_ is false before req_m_ is taken, so no
	// waiter misses the wakeup); asynchronous ones hear that the engine went.
	std::vector<ReplyFn> gone;
	{
		std::lock_guard<std::mutex> lock(req_m_);
		for (auto it = pending_.begin(); it != pending_.end();) {
			if (it->second.done) {
				gone.push_back(std::move(it->second.done));
				it = pending_.erase(it);
			} else {
				++it;
			}
		}
	}
	req_cv_.notify_all();
	for (auto &fn : gone)
		fn(false, {});
	if (was_ready && ev_.disconnected)
		ev_.disconnected();

	// The writer first (a Quit still goes out when OBS closes), then the pipes:
	// closed pipes end the engine as Quit does.
	if (writer_.joinable()) {
		std::unique_lock<std::mutex> lock(out_m_);
		if (stopping_)
			out_cv_.wait_for(lock, std::chrono::milliseconds(500),
					 [&] { return outq_.empty() && !writing_; });
		out_open_ = false;
		out_stop_ = true;
		outq_.clear();
		lock.unlock();
		out_cv_.notify_all();
		if (h2e_ != INVALID_HANDLE_VALUE)
			CancelIoEx(h2e_, nullptr); // a write blocked on a full pipe
		writer_.join();
	}
	HANDLE proc;
	{
		std::lock_guard<std::mutex> lock(proc_m_);
		proc = proc_;
		proc_ = nullptr;
	}
	close_pipe(h2e_);
	close_pipe(e2h_);
	if (proc) {
		// Give the engine the time to let go of its device and models.
		if (WaitForSingleObject(proc, stopping_ ? 2000 : 1000) != WAIT_OBJECT_0) {
			TerminateProcess(proc, 1);
			WaitForSingleObject(proc, 1000);
		}
		DWORD code = 0;
		GetExitCodeProcess(proc, &code);
		CloseHandle(proc);
		if (was_ready && !stopping_ && !killed_)
			log(kce::LogLevel::Error, "the KlarCam engine ended unexpectedly (exit code " + code_text(code) +
							  "); the filter passes the video through until it is back");
	}
	killed_ = false;
}

bool EngineLink::send(kce::Msg type, uint32_t instance, uint32_t request, const void *a, size_t na, const void *b,
		      size_t nb)
{
	kce::MsgHeader h = {};
	h.size = static_cast<uint32_t>(na + nb);
	h.type = static_cast<uint16_t>(type);
	h.instance = instance;
	h.request = request;
	Out o;
	o.type = h.type;
	o.instance = instance;
	o.bytes.assign(sizeof(h) + na + nb, '\0');
	memcpy(o.bytes.data(), &h, sizeof(h));
	if (na)
		memcpy(o.bytes.data() + sizeof(h), a, na);
	if (nb)
		memcpy(o.bytes.data() + sizeof(h) + na, b, nb);

	std::lock_guard<std::mutex> lock(out_m_);
	if (!out_open_)
		return false;
	if (type == kce::Msg::Settings) {
		// Only an instance's newest settings matter: an older set still waiting goes.
		outq_.erase(std::remove_if(outq_.begin(), outq_.end(),
					   [&](const Out &x) {
						   return x.type == o.type && x.instance == instance;
					   }),
			    outq_.end());
	}
	outq_.push_back(std::move(o));
	out_cv_.notify_all();
	return true;
}

void EngineLink::write_loop()
{
	for (;;) {
		Out o;
		{
			std::unique_lock<std::mutex> lock(out_m_);
			out_cv_.wait(lock, [&] { return out_stop_ || !outq_.empty(); });
			if (out_stop_)
				return;
			o = std::move(outq_.front());
			outq_.pop_front();
			writing_ = true;
		}
		OVERLAPPED ov = {};
		ov.hEvent = write_ev_;
		ResetEvent(write_ev_);
		DWORD put = 0;
		bool ok = WriteFile(h2e_, o.bytes.data(), static_cast<DWORD>(o.bytes.size()), nullptr, &ov) ||
			  GetLastError() == ERROR_IO_PENDING;
		if (ok && WaitForSingleObject(write_ev_, kWriteMs) != WAIT_OBJECT_0) {
			CancelIoEx(h2e_, &ov);
			GetOverlappedResult(h2e_, &ov, &put, TRUE);
			bool stopping;
			{
				std::lock_guard<std::mutex> lock(out_m_);
				writing_ = false;
				stopping = out_stop_;
			}
			if (!stopping)
				kill("the engine took no message for 2 s");
			return;
		}
		if (ok)
			ok = GetOverlappedResult(h2e_, &ov, &put, FALSE) && put == o.bytes.size();
		{
			std::lock_guard<std::mutex> lock(out_m_);
			writing_ = false;
		}
		out_cv_.notify_all(); // a drain waits for the empty queue
		if (!ok)
			return; // the pipe broke: the reader sees it too
	}
}

bool EngineLink::send_create(uint32_t id, Instance &inst)
{
	std::lock_guard<std::mutex> lock(proc_m_);
	HANDLE r = nullptr, d = nullptr;
	if (!proc_ || !DuplicateHandle(GetCurrentProcess(), inst.ready, proc_, &r, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
	    !DuplicateHandle(GetCurrentProcess(), inst.done, proc_, &d, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
		if (r)
			DuplicateHandle(proc_, r, nullptr, nullptr, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
		return false;
	}
	kce::InstanceCreate ic = {};
	ic.slot = inst.slot;
	ic.ready_event = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
	ic.done_event = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(d));
	inst.created = send(kce::Msg::InstanceCreate, id, 0, &ic, sizeof(ic), inst.settings.data(), inst.settings.size());
	if (!inst.created) {
		// Not sent: the handles would stay open in the engine until it ends.
		DuplicateHandle(proc_, r, nullptr, nullptr, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
		DuplicateHandle(proc_, d, nullptr, nullptr, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
	}
	return inst.created;
}

bool EngineLink::read_message(kce::MsgHeader &h, std::string &payload, DWORD timeout_ms)
{
	const ULONGLONG deadline = timeout_ms == INFINITE ? ~0ull : GetTickCount64() + timeout_ms;
	auto read = [&](void *dst, size_t n) {
		auto *p = static_cast<char *>(dst);
		while (n > 0) {
			OVERLAPPED ov = {};
			ov.hEvent = read_ev_;
			ResetEvent(read_ev_);
			if (!ReadFile(e2h_, p, static_cast<DWORD>(std::min<size_t>(n, 1u << 16)), nullptr, &ov) &&
			    GetLastError() != ERROR_IO_PENDING)
				return false;
			DWORD wait = INFINITE;
			if (deadline != ~0ull) {
				const ULONGLONG now = GetTickCount64();
				wait = now >= deadline ? 0 : static_cast<DWORD>(deadline - now);
			}
			HANDLE w[2] = {read_ev_, stop_};
			DWORD got = 0;
			if (WaitForMultipleObjects(2, w, FALSE, wait) != WAIT_OBJECT_0) {
				CancelIoEx(e2h_, &ov);
				GetOverlappedResult(e2h_, &ov, &got, TRUE);
				return false;
			}
			if (!GetOverlappedResult(e2h_, &ov, &got, FALSE) || got == 0)
				return false;
			p += got;
			n -= got;
		}
		return true;
	};
	if (!read(&h, sizeof(h)) || h.size > kce::kMaxPayload)
		return false;
	payload.assign(h.size, '\0');
	return h.size == 0 || read(payload.data(), h.size);
}

void EngineLink::dispatch(const kce::MsgHeader &h, std::string &payload)
{
	switch (static_cast<kce::Msg>(h.type)) {
	case kce::Msg::Log:
		if (!payload.empty())
			log(static_cast<kce::LogLevel>(static_cast<uint8_t>(payload[0])), payload.substr(1));
		break;
	case kce::Msg::Status:
		if (ev_.status)
			ev_.status(h.instance, payload);
		break;
	case kce::Msg::Writeback:
		if (ev_.writeback)
			ev_.writeback(h.instance, payload);
		break;
	case kce::Msg::SetupDone:
		if (payload.size() >= sizeof(kce::SetupDone) && ev_.setup_done) {
			kce::SetupDone sd;
			memcpy(&sd, payload.data(), sizeof(sd));
			ev_.setup_done(h.instance, sd.generation, sd.ok != 0, payload.substr(sizeof(sd)));
		}
		break;
	case kce::Msg::Reply: {
		ReplyFn fn;
		{
			std::lock_guard<std::mutex> lock(req_m_);
			const auto it = pending_.find(h.request);
			if (it != pending_.end()) {
				if (it->second.done) {
					fn = std::move(it->second.done);
					pending_.erase(it);
				} else {
					it->second.answered = true;
					it->second.reply = payload;
				}
			}
		}
		req_cv_.notify_all();
		if (fn)
			fn(true, payload);
		break;
	}
	default:
		break;
	}
}

void EngineLink::kill(const std::string &why)
{
	std::lock_guard<std::mutex> lock(proc_m_);
	if (!proc_)
		return;
	log(kce::LogLevel::Warning, why + "; restarting the KlarCam engine");
	killed_ = true;
	TerminateProcess(proc_, 1);
}

void EngineLink::log(kce::LogLevel level, const std::string &text)
{
	if (ev_.log)
		ev_.log(level, text);
}

void EngineLink::reset_slots()
{
	if (!block_)
		return;
	for (auto &s : block_->slots) {
		InterlockedExchange(reinterpret_cast<volatile LONG *>(&s.ready_generation), 0);
		InterlockedExchange64(reinterpret_cast<volatile LONG64 *>(&s.busy_until), 0);
	}
}

} // namespace klarcam_host
