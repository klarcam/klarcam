/*
 * KlarCam filter host for OBS Studio: the filter.
 *
 * Registers the "KlarCam" filter (id aiskinsmooth_filter as before, so scene
 * collections keep their filters and values), shows its properties and hands
 * each frame to the KlarCam engine (engine-link.h): the filter target renders
 * into a texture, the texture goes to the engine as a keyed-mutex shared
 * texture and comes back processed within the same OBS frame. The engine does
 * the image processing, the license and the looks; this host knows the
 * settings' names and how to show them, nothing of what they do.
 *
 * While the engine starts or restarts the video passes through, and the
 * processed picture fades in once it is there. A frame the engine does not
 * answer in time shows the last processed one (for half a second at most);
 * an engine that does not answer for two seconds is restarted.
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
#include "klarcam-filter.h"

#include "engine-link.h"
#include "filter-defaults.h"
#include "klarcam-engine-text.h"

#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <util/platform.h>

#include <d3d11.h>
#include <dxgi.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.0.0"
#endif

namespace klarcam_host {
namespace {

constexpr const char *kFilterId = "aiskinsmooth_filter";
constexpr const char *kLogPrefix = "[KlarCam]";
constexpr uint64_t kFadeNs = 750000000ull;  // the processed picture fades in after a (re)start
constexpr uint64_t kHoldNs = 500000000ull;  // a late frame shows the last processed one this long at most
constexpr uint64_t kHangNs = 2000000000ull; // an engine silent this long is restarted
constexpr uint64_t kStuckNs = 500000000ull; // stuck textures this long are set up anew
constexpr uint64_t kRenewWindowNs = 10000000000ull;
constexpr size_t kStatsFrames = 1800;       // a hand-off line in the log per 1800 processed frames

#define T_(key) obs_module_text(key)

struct FilterData {
	obs_source_t *source = nullptr;
	uint32_t id = 0; // the engine link's instance, 0 = none
	InstanceHandles h;

	gs_texrender_t *tr_source = nullptr;              // the filter target, captured
	gs_texture_t *sh_in = nullptr, *sh_out = nullptr; // shared with the engine
	gs_texture_t *last_out = nullptr;                 // the last processed frame
	uint32_t width = 0, height = 0;
	uint32_t generation = 0;                 // of the last FrameSetup sent
	uint32_t epoch = 0;                      // the engine those textures were set up for
	uint32_t in_handle = 0, out_handle = 0;  // their shared handles (new ones: OBS rebuilt its device)
	int64_t frame = 0;                       // the last frame number signaled
	bool outstanding = false;                // that frame missed its timeout and is not back yet
	uint64_t outstanding_since = 0;

	uint64_t frame_time = 0;   // obs_get_video_frame_time() of the OBS frame last rendered
	bool frame_ok = false;     // that frame drew last_out ...
	float frame_fade = 1.0f;   // ... over the source by this much
	uint64_t last_ok_ns = 0;   // when last_out was last filled
	bool fade_pending = true;  // the next processed frame starts a fade-in
	uint64_t fade_start = 0;

	// Textures stuck at the wrong key, or an engine answering errors: new
	// textures after 0.5 s, a new engine after three renewals within 10 s.
	uint64_t stuck_since = 0;
	uint64_t renewals_since = 0;
	uint32_t renewals = 0;

	// Graphics thread only (update and render both run there).
	bool auto_exposure_was = false; // the automatic exposure's last state, to fold its EV in when it goes off
	std::string pending_key;        // a license key in the settings, for the engine to activate
	std::string key_sent;           // ... the one last sent,
	uint32_t key_epoch = 0;         // ... to the engine of this epoch

	// Filled on the engine link's thread.
	std::mutex m;
	kce::Values status;
	bool has_status = false;
	std::string writeback;

	// Hand-off statistics for the log.
	std::vector<float> ms;
	uint32_t timeouts = 0, held = 0, raw = 0;
};

// The filters by instance id, for the engine link's callbacks.
std::mutex g_reg_m;
std::map<uint32_t, FilterData *> g_reg;

// One engine per OBS process, started with the first filter.
std::mutex g_link_m;
std::unique_ptr<EngineLink> g_link;
bool g_loss_registered = false;

gs_effect_t *g_fade = nullptr; // effects/klarcam-fade.effect

// The wait budget of the OBS frame being rendered, shared by all filters
// (graphics thread only).
struct FrameBudget {
	uint64_t frame_time = 0;
	uint64_t deadline = 0;
};
FrameBudget g_budget;

EngineLink *link()
{
	std::lock_guard<std::mutex> lock(g_link_m);
	return g_link.get();
}

int obs_level(kce::LogLevel level)
{
	switch (level) {
	case kce::LogLevel::Debug:
		return LOG_DEBUG;
	case kce::LogLevel::Warning:
		return LOG_WARNING;
	case kce::LogLevel::Error:
		return LOG_ERROR;
	default:
		return LOG_INFO;
	}
}

LUID adapter_luid(ID3D11Device *device)
{
	LUID luid = {};
	IDXGIDevice *dxgi = nullptr;
	if (device && SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgi)))) {
		IDXGIAdapter *adapter = nullptr;
		if (SUCCEEDED(dxgi->GetAdapter(&adapter))) {
			DXGI_ADAPTER_DESC desc = {};
			if (SUCCEEDED(adapter->GetDesc(&desc)))
				luid = desc.AdapterLuid;
			adapter->Release();
		}
		dxgi->Release();
	}
	return luid;
}

std::wstring widen(const char *s)
{
	if (!s || !*s)
		return {};
	const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
	std::wstring w(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
	if (n > 1)
		MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
	return w;
}

void release_textures(FilterData *f);

// OBS lost its graphics device (a driver reset or update) and is about to
// rebuild every texture. Shared ones it would reopen by their old handle on the
// new device: when that fails OBS crashes, when it works it waits forever for a
// key the engine may hold. So the shared textures go now; the filters set up new
// ones with a fresh engine (on_device_rebuild), the video passing through until.
void on_device_release(void *)
{
	std::lock_guard<std::mutex> lock(g_reg_m);
	for (auto &it : g_reg) {
		FilterData *f = it.second;
		release_textures(f);
		f->epoch = 0;
		f->outstanding = false;
		f->last_ok_ns = 0; // nothing held over the reset
		f->frame_ok = false;
		f->stuck_since = 0;
		if (f->h.ready)
			ResetEvent(f->h.ready);
		if (f->h.done)
			ResetEvent(f->h.done);
	}
}

void on_device_rebuild(void *device, void *)
{
	if (EngineLink *l = link())
		l->device_rebuilt(adapter_luid(static_cast<ID3D11Device *>(device)));
}

// The engine link, started on first use (in the graphics context, for the
// device's adapter).
EngineLink *start_link(ID3D11Device *device)
{
	std::lock_guard<std::mutex> lock(g_link_m);
	if (g_link)
		return g_link.get();
	EngineConfig cfg;
	const char *dll = obs_get_module_binary_path(obs_current_module());
	std::wstring exe = widen(dll);
	// The engine and its runtime live in engine\ next to the module.
	exe = exe.substr(0, exe.find_last_of(L"\\/") + 1) + L"engine\\klarcam-engine.exe";
	cfg.exe = exe;
	if (const char *data = obs_get_module_data_path(obs_current_module()))
		cfg.data_dir = data;
	cfg.luid = adapter_luid(device);
	cfg.host_version = PLUGIN_VERSION;
	EngineEvents ev;
	ev.log = [](kce::LogLevel level, const std::string &text) {
		blog(obs_level(level), "%s %s", kLogPrefix, text.c_str());
	};
	ev.status = [](uint32_t id, const std::string &text) {
		kce::Values v = kce::parse(text);
		std::lock_guard<std::mutex> lock(g_reg_m);
		const auto it = g_reg.find(id);
		if (it == g_reg.end())
			return;
		std::lock_guard<std::mutex> lock2(it->second->m);
		it->second->status = std::move(v);
		it->second->has_status = true;
	};
	ev.writeback = [](uint32_t id, const std::string &text) {
		std::lock_guard<std::mutex> lock(g_reg_m);
		const auto it = g_reg.find(id);
		if (it == g_reg.end())
			return;
		std::lock_guard<std::mutex> lock2(it->second->m);
		it->second->writeback += text;
	};
	ev.setup_done = [](uint32_t, uint32_t, bool ok, const std::string &error) {
		if (!ok)
			blog(LOG_WARNING, "%s the engine cannot use the frame textures: %s", kLogPrefix, error.c_str());
	};
	ev.disconnected = [] {
		// The properties show the engine's state, not a gone engine's last word.
		std::lock_guard<std::mutex> lock(g_reg_m);
		for (auto &it : g_reg) {
			std::lock_guard<std::mutex> lock2(it.second->m);
			it.second->status.clear();
			it.second->has_status = false;
		}
	};
	g_link = std::make_unique<EngineLink>(std::move(cfg), std::move(ev));
	if (!g_loss_registered) {
		gs_device_loss callbacks = {};
		callbacks.device_loss_release = on_device_release;
		callbacks.device_loss_rebuild = on_device_rebuild;
		callbacks.data = &g_link; // any unique pointer: the key to unregister
		gs_register_loss_callbacks(&callbacks);
		g_loss_registered = true;
	}
	return g_link.get();
}

// Every setting except the ones only the host handles, as engine protocol text.
std::string settings_text(obs_data_t *settings)
{
	std::string t;
	for (obs_data_item_t *item = obs_data_first(settings); item; obs_data_item_next(&item)) {
		const char *name = obs_data_item_get_name(item);
		if (!name || !strcmp(name, S_LICENSE_KEY) || !strcmp(name, S_PRESET))
			continue;
		switch (obs_data_item_gettype(item)) {
		case OBS_DATA_NUMBER:
			if (obs_data_item_numtype(item) == OBS_DATA_NUM_INT)
				kce::put_int(t, name, obs_data_item_get_int(item));
			else
				kce::put_double(t, name, obs_data_item_get_double(item));
			break;
		case OBS_DATA_BOOLEAN:
			kce::put_bool(t, name, obs_data_item_get_bool(item));
			break;
		case OBS_DATA_STRING: {
			const char *s = obs_data_item_get_string(item);
			kce::put_string(t, name, s ? s : "");
			break;
		}
		default:
			break;
		}
	}
	return t;
}

// Values from the engine (a look, a measured lock) into the settings.
void apply_values(obs_data_t *settings, const kce::Values &values)
{
	for (const auto &it : values) {
		const char *key = it.first.c_str();
		switch (it.second.type) {
		case 'd':
			obs_data_set_double(settings, key, it.second.d);
			break;
		case 'i':
			obs_data_set_int(settings, key, it.second.i);
			break;
		case 'b':
			obs_data_set_bool(settings, key, it.second.i != 0);
			break;
		default:
			obs_data_set_string(settings, key, it.second.s.c_str());
			break;
		}
	}
}

bool status_of(FilterData *f, kce::Values &out)
{
	if (!f)
		return false;
	std::lock_guard<std::mutex> lock(f->m);
	out = f->status;
	return f->has_status;
}

// The engine's state as a line for the properties.
std::string engine_text()
{
	EngineLink *l = link();
	if (!l)
		return std::string(T_("Engine")) + ": " + T_("Engine.NoDevice");
	const std::string detail = l->state_text();
	switch (l->state()) {
	case EngineLink::State::Running:
		return std::string(T_("Engine")) + ": " + T_("Engine.Running") + " " + detail;
	case EngineLink::State::Starting:
		return std::string(T_("Engine")) + ": " + T_("Engine.Starting") + (detail.empty() ? "" : " (" + detail + ")");
	default:
		return std::string(T_("Engine")) + ": " + T_("Engine.Failed") + " " + detail;
	}
}

// ---------------------------------------------------------------------------
// obs_source_info callbacks
// ---------------------------------------------------------------------------

const char *filter_get_name(void *)
{
	return T_("KlarCam");
}

// OBS runs update on its graphics thread (deferred to the source's tick), so
// nothing here may wait for the engine: the settings go out through the link's
// writer, a pasted key is activated by video_render once the engine runs.
void filter_update(void *data, obs_data_t *settings)
{
	auto *f = static_cast<FilterData *>(data);
	if (!f)
		return;
	// Switching the automatic exposure off keeps the level it had reached: its
	// EV, which the engine reports with every frame, goes into the slider (the
	// engine starts from the slider when the automatic is switched on and
	// ignores it while it runs).
	const bool auto_exposure = obs_data_get_bool(settings, S_AUTO_EXPOSURE);
	if (f->auto_exposure_was && !auto_exposure && f->h.slot) {
		const double ev = std::clamp(static_cast<double>(f->h.slot->auto_ev), -2.0, 2.0);
		obs_data_set_double(settings, S_EXPOSURE, ev);
	}
	f->auto_exposure_was = auto_exposure;
	const char *key = obs_data_get_string(settings, S_LICENSE_KEY);
	f->pending_key = key ? key : "";
	EngineLink *l = link();
	if (l && f->id)
		l->update_settings(f->id, settings_text(settings));
}

// OBS saves the scene collection (its UI thread): while the automatic exposure
// runs, its EV goes into the slider's setting, so the next start continues at
// the level reached instead of the value the slider had when the automatic
// was switched on. The engine's status carries the EV (the frame slot belongs
// to the graphics thread).
void filter_save(void *data, obs_data_t *settings)
{
	auto *f = static_cast<FilterData *>(data);
	if (!f || !obs_data_get_bool(settings, S_AUTO_EXPOSURE))
		return;
	std::lock_guard<std::mutex> lock(f->m);
	if (!f->has_status || f->status.find("auto_ev") == f->status.end())
		return;
	obs_data_set_double(settings, S_EXPOSURE, std::clamp(kce::get_number(f->status, "auto_ev", 0.0), -2.0, 2.0));
}

// The exposure slider shows while the automatic is off, the target while it is on.
bool auto_exposure_modified(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const bool on = obs_data_get_bool(settings, S_AUTO_EXPOSURE);
	obs_property_set_visible(obs_properties_get(props, S_EXPOSURE), !on);
	obs_property_set_visible(obs_properties_get(props, S_AUTO_TARGET), on);
	return true;
}

void *filter_create(obs_data_t *settings, obs_source_t *source)
{
	auto *f = new FilterData();
	f->source = source;

	obs_enter_graphics();
	auto *device = static_cast<ID3D11Device *>(gs_get_device_obj());
	f->tr_source = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	if (!g_fade) {
		if (char *path = obs_module_file("effects/klarcam-fade.effect")) {
			g_fade = gs_effect_create_from_file(path, nullptr);
			bfree(path);
		}
	}
	EngineLink *l = device ? start_link(device) : nullptr;
	obs_leave_graphics();

	if (!l) {
		blog(LOG_ERROR, "%s no Direct3D 11 device (OBS renderer is not D3D11), filter will pass through", kLogPrefix);
		return f;
	}
	f->auto_exposure_was = obs_data_get_bool(settings, S_AUTO_EXPOSURE);
	const char *key = obs_data_get_string(settings, S_LICENSE_KEY);
	f->pending_key = key ? key : ""; // activated once the engine runs
	f->id = l->add_instance(settings_text(settings), f->h);
	if (!f->id) {
		blog(LOG_ERROR, "%s more than %u KlarCam filters: this one passes the video through", kLogPrefix,
		     kce::kMaxInstances);
		return f;
	}
	f->frame = f->h.slot->frame_sent; // the slot's numbers go on
	{
		std::lock_guard<std::mutex> lock(g_reg_m);
		g_reg[f->id] = f;
	}
	return f;
}

// OBS calls destroy on its destruction thread. The textures go first, in the
// graphics context, so a device reset meanwhile finds none of them.
void filter_destroy(void *data)
{
	auto *f = static_cast<FilterData *>(data);
	obs_enter_graphics();
	release_textures(f);
	gs_texture_destroy(f->last_out);
	f->last_out = nullptr;
	gs_texrender_destroy(f->tr_source);
	f->tr_source = nullptr;
	obs_leave_graphics();
	if (f->id) {
		{
			std::lock_guard<std::mutex> lock(g_reg_m);
			g_reg.erase(f->id);
		}
		if (EngineLink *l = link())
			l->remove_instance(f->id);
	}
	delete f;
}

void filter_get_defaults(obs_data_t *settings)
{
	for (const DefaultValue &d : kDefaults) {
		switch (d.type) {
		case 'd':
			obs_data_set_default_double(settings, d.key, d.number);
			break;
		case 'i':
			obs_data_set_default_int(settings, d.key, static_cast<long long>(d.number));
			break;
		case 'b':
			obs_data_set_default_bool(settings, d.key, d.number != 0.0);
			break;
		default:
			obs_data_set_default_string(settings, d.key, d.text ? d.text : "");
			break;
		}
	}
}

// The Messprotokoll report of the last measured frame, for the properties text.
std::string current_report(FilterData *f)
{
	if (!f)
		return "";
	obs_data_t *s = obs_source_get_settings(f->source);
	const bool measure = obs_data_get_bool(s, S_MEASURE);
	obs_data_release(s);
	if (!measure)
		return T_("MeasureOff");
	kce::Values st;
	if (!status_of(f, st))
		return engine_text();
	const std::string report = kce::get_string(st, "report");
	return report.empty() ? T_("Measure.Waiting") : report;
}

bool measure_refresh_clicked(obs_properties_t *props, obs_property_t *, void *data)
{
	obs_property_t *text = obs_properties_get(props, "measure_report");
	if (text)
		obs_property_set_description(text, current_report(static_cast<FilterData *>(data)).c_str());
	return true; // redraw the properties
}

// Choosing a preset writes its look into the settings (host state, the beard
// weight and the measurement stay as they are) and puts the list back to "keep".
bool preset_modified(obs_properties_t *, obs_property_t *, obs_data_t *settings)
{
	const int which = static_cast<int>(obs_data_get_int(settings, S_PRESET));
	if (which == 0)
		return false;
	std::string req, reply;
	kce::put_string(req, "verb", "look");
	kce::put_int(req, "preset", which);
	EngineLink *l = link();
	if (l && l->request(0, req, reply, 2000))
		apply_values(settings, kce::parse(reply));
	else
		blog(LOG_WARNING, "%s the engine is not running: the preset was not applied", kLogPrefix);
	obs_data_set_int(settings, S_PRESET, 0);
	return true; // redraw the properties with the new values
}

// The measured locks (light side, auto frame) belong to the engine: it measures,
// locks and writes them back, and only these buttons release or flip them, as
// commands. A settings message cannot reset a lock the engine has just measured.
void lock_command(FilterData *f, const char *verb)
{
	EngineLink *l = link();
	if (!l || !f->id)
		return;
	std::string cmd;
	kce::put_string(cmd, "verb", verb);
	l->command(f->id, cmd);
}

// "Reframe": releases the auto frame's lock; the engine measures anew and the
// crop eases over to the result (written back into the scene's settings).
bool frame_reframe_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	auto *f = static_cast<FilterData *>(data);
	if (!f)
		return false;
	lock_command(f, "frame_reframe");
	return true;
}

// "Measure again": releases the light side's lock; the engine measures the
// room's light anew (a few seconds) and the result is written back.
bool relight_measure_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	auto *f = static_cast<FilterData *>(data);
	if (!f)
		return false;
	lock_command(f, "relight_measure");
	return true; // redraw: the status line
}

// "Swap side": the user's choice over the measurement. A locked side flips (the
// engine's lock); measured frontal (or still measuring), the slider's sign is
// the side, so that flips instead. The key swings over within a second (the
// engine eases it).
bool relight_swap_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	auto *f = static_cast<FilterData *>(data);
	if (!f || !f->source)
		return false;
	kce::Values st;
	status_of(f, st);
	const int64_t lock = kce::get_int(st, "relight_side_lock", 0);
	if (lock == 1 || lock == -1) {
		lock_command(f, "relight_swap");
		return true;
	}
	obs_data_t *s = obs_source_get_settings(f->source);
	obs_data_set_double(s, S_RELIGHT_DIR, -obs_data_get_double(s, S_RELIGHT_DIR));
	obs_data_release(s);
	obs_source_update(f->source, nullptr);
	return true;
}

// The license line of the properties: the owner, or the trial's state.
std::string license_text(FilterData *f)
{
	kce::Values st;
	if (!status_of(f, st))
		return engine_text();
	char buf[256];
	if (kce::get_bool(st, "licensed", false))
		snprintf(buf, sizeof(buf), "%s %s (KlarCam %d.x)", T_("Licensed"), kce::get_string(st, "license_email").c_str(),
			 static_cast<int>(kce::get_int(st, "license_major", 1)));
	else if (kce::get_bool(st, "trial_over", false))
		snprintf(buf, sizeof(buf), "%s", T_("TrialOver"));
	else
		snprintf(buf, sizeof(buf), T_("TrialDays"), static_cast<int>(kce::get_int(st, "trial_days_left", 0)));
	std::string s = buf;
	const std::string error = kce::get_string(st, "license_error");
	if (!error.empty())
		s += "  (" + error + ")";
	return s;
}

// A key typed into the properties: show the verdict at once (storing it is
// filter_update's job, which runs right after).
bool license_modified(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const char *key = obs_data_get_string(settings, S_LICENSE_KEY);
	if (!key || !*key)
		return false;
	obs_property_t *info = obs_properties_get(props, "license_info");
	if (!info)
		return true;
	std::string req, reply;
	kce::put_string(req, "verb", "license_verify");
	kce::put_string(req, "key", key);
	EngineLink *l = link();
	if (!l || !l->request(0, req, reply, 2000)) {
		obs_property_set_description(info, engine_text().c_str());
		return true;
	}
	const kce::Values v = kce::parse(reply);
	if (kce::get_bool(v, "ok", false))
		obs_property_set_description(info, (std::string(T_("Licensed")) + " " + kce::get_string(v, "license_email") +
						    " (KlarCam " + std::to_string(kce::get_int(v, "license_major", 1)) +
						    ".x)")
							   .c_str());
	else
		obs_property_set_description(info, kce::get_string(v, "license_error").c_str());
	return true;
}

// Short labels keep OBS's label column narrow (the controls get the width); the
// explanation of a control is its long description, a tooltip, read from the
// locale as "<key>.Desc" where there is one.
void describe(obs_property_t *p, const char *key)
{
	if (!p || !key)
		return;
	const std::string k = std::string(key) + ".Desc";
	const char *text = nullptr;
	if (obs_module_get_string(k.c_str(), &text) && text && *text)
		obs_property_set_long_description(p, text);
}

obs_property_t *add_slider(obs_properties_t *props, const char *setting, const char *key, double lo, double hi,
			   double step)
{
	obs_property_t *p = obs_properties_add_float_slider(props, setting, T_(key), lo, hi, step);
	describe(p, key);
	return p;
}

obs_property_t *add_bool(obs_properties_t *props, const char *setting, const char *key)
{
	obs_property_t *p = obs_properties_add_bool(props, setting, T_(key));
	describe(p, key);
	return p;
}

std::string lut_key(int slot, const char *suffix)
{
	return std::string(kLutKeys[slot]) + suffix;
}

obs_properties_t *filter_get_properties(void *data)
{
	auto *f = static_cast<FilterData *>(data);
	obs_properties_t *props = obs_properties_create();
	kce::Values st;
	const bool has_status = status_of(f, st);
	obs_data_t *settings = f ? obs_source_get_settings(f->source) : nullptr;
	auto setting_bool = [&](const char *key) { return settings && obs_data_get_bool(settings, key); };

	// A/B, presets and the license first.
	add_bool(props, S_BYPASS, "Bypass");
	obs_property_t *preset = obs_properties_add_list(props, S_PRESET, T_("Preset"), OBS_COMBO_TYPE_LIST,
							 OBS_COMBO_FORMAT_INT);
	describe(preset, "Preset");
	obs_property_list_add_int(preset, T_("Preset.Keep"), 0);
	obs_property_list_add_int(preset, T_("Preset.Natural"), 1);
	obs_property_list_add_int(preset, T_("Preset.Stream"), 2);
	obs_property_set_modified_callback(preset, preset_modified);
	obs_properties_add_text(props, "license_info", license_text(f).c_str(), OBS_TEXT_INFO);
	if (!f || !kce::get_bool(st, "licensed", false)) {
		obs_property_t *lic = obs_properties_add_text(props, S_LICENSE_KEY, T_("LicenseKey"), OBS_TEXT_DEFAULT);
		describe(lic, "LicenseKey");
		obs_property_set_modified_callback(lic, license_modified);
	}

	// Framing: the auto frame, measured once and locked (the lock lives in the
	// scene's settings, written back once measured).
	obs_properties_t *frame_props = obs_properties_create();
	add_bool(frame_props, S_AUTO_FRAME, "AutoFrame");
	add_slider(frame_props, S_FRAME_TARGET, "FrameTarget", 0.25, 0.60, 0.01);
	if (has_status && kce::get_bool(st, "core", false) && setting_bool(S_AUTO_FRAME)) {
		char status[160];
		const double zoom = kce::get_number(st, "frame_lock_z", 0.0);
		if (zoom <= 0.0)
			snprintf(status, sizeof(status), "%s: %s", T_("Frame"), T_("Frame.Measuring"));
		else
			snprintf(status, sizeof(status), "%s: %.2fx, %s", T_("Frame"), zoom, T_("Frame.Locked"));
		obs_properties_add_text(frame_props, "frame_info", status, OBS_TEXT_INFO);
		describe(obs_properties_add_button2(frame_props, "frame_reframe", T_("FrameReframe"), frame_reframe_clicked, f),
			 "FrameReframe");
	}
	obs_properties_add_group(props, "frame_group", T_("FrameGroup"), OBS_GROUP_NORMAL, frame_props);

	// Lighting: everything that sets levels, in the chain's order. The exposure
	// (the whole frame; with the automatic on, the target is the level control
	// and the slider hides), the face light, the background relative to the
	// person, and the virtual key light.
	obs_properties_t *light_props = obs_properties_create();
	const bool auto_exposure = setting_bool(S_AUTO_EXPOSURE);
	obs_property_set_visible(add_slider(light_props, S_EXPOSURE, "Exposure", -2.0, 2.0, 0.05), !auto_exposure);
	obs_property_set_modified_callback(add_bool(light_props, S_AUTO_EXPOSURE, "AutoExposure"), auto_exposure_modified);
	obs_property_set_visible(add_slider(light_props, S_AUTO_TARGET, "AutoTarget", 50.0, 80.0, 0.5), auto_exposure);
	add_slider(light_props, S_FACE_LIGHT, "FaceLight", 0.0, 1.5, 0.01);
	add_slider(light_props, S_FACE_LIGHT_REACH, "FaceLightReach", 1.2, 3.0, 0.05);
	add_slider(light_props, S_BACKGROUND, "Background", -1.5, 1.5, 0.05);
	add_slider(light_props, S_BACKGROUND_RADIAL, "BackgroundRadial", 0.0, 1.0, 0.01);
	obs_properties_add_text(light_props, "keylight_info", T_("KeyLight"), OBS_TEXT_INFO);
	add_slider(light_props, S_RELIGHT, "Relight", 0.0, 1.0, 0.01);
	add_slider(light_props, S_RELIGHT_DIR, "RelightDir", -1.0, 1.0, 0.01);
	add_bool(light_props, S_RELIGHT_AUTO, "RelightAuto");
	if (has_status && kce::get_bool(st, "core", false) && setting_bool(S_RELIGHT_AUTO)) {
		// The room's light as measured (the lock), the two buttons next to it.
		const char *state = T_("RoomLight.Measuring");
		if (kce::get_int(st, "relight_side_lock", 0) != 0) {
			const double side = kce::get_number(st, "light_side", 0.0);
			state = side > 0.0 ? T_("RoomLight.Right") : side < 0.0 ? T_("RoomLight.Left") : T_("RoomLight.Frontal");
		}
		char status[256];
		snprintf(status, sizeof(status), "%s: %s", T_("RoomLight"), state);
		obs_properties_add_text(light_props, "relight_side_info", status, OBS_TEXT_INFO);
		describe(obs_properties_add_button2(light_props, "relight_measure", T_("RelightMeasure"), relight_measure_clicked,
						    f),
			 "RelightMeasure");
		describe(obs_properties_add_button2(light_props, "relight_swap", T_("RelightSwap"), relight_swap_clicked, f),
			 "RelightSwap");
	}
	add_slider(light_props, S_RELIGHT_HEIGHT, "RelightHeight", 0.0, 1.0, 0.01);
	add_slider(light_props, S_RELIGHT_SPREAD, "RelightSpread", 1.0, 2.5, 0.01);
	add_slider(light_props, S_RELIGHT_EDGE, "RelightEdge", 0.0, 1.0, 0.01);
	obs_properties_add_group(props, "lighting_group", T_("LightingGroup"), OBS_GROUP_NORMAL, light_props);

	// Skin.
	obs_properties_t *skin_props = obs_properties_create();
	add_slider(skin_props, S_SMOOTHING, "Smoothing", 0.0, 1.0, 0.01);
	add_slider(skin_props, S_SHARPEN, "Sharpen", 0.0, 1.0, 0.01);
	describe(obs_properties_add_int_slider(skin_props, S_RADIUS, T_("Radius"), 1, 16, 1), "Radius");
	add_slider(skin_props, S_SKIN_GATE, "SkinGate", 0.0, 1.0, 0.01);
	add_bool(skin_props, S_BEARD_AUTO, "BeardAuto");
	add_slider(skin_props, S_BEARD, "Beard", 0.0, 1.0, 0.01);
	if (has_status && kce::get_bool(st, "core", false) && setting_bool(S_BEARD_AUTO)) {
		char status[128];
		snprintf(status, sizeof(status), "%s: %.0f %%", T_("BeardDetected"), kce::get_number(st, "beard", 1.0) * 100.0);
		obs_properties_add_text(skin_props, "beard_info", status, OBS_TEXT_INFO);
	}
	obs_properties_add_group(props, "skin_group", T_("SkinGroup"), OBS_GROUP_NORMAL, skin_props);

	// Skin tone (vectorscope).
	obs_properties_t *tone_props = obs_properties_create();
	add_slider(tone_props, S_TONE_SMOOTH, "ToneSmooth", 0.0, 1.0, 0.01);
	add_slider(tone_props, S_TONE_BAND, "ToneBand", 0.0, 1.0, 0.01);
	add_slider(tone_props, S_TONE_CONTRAST, "ToneContrast", -1.0, 1.0, 0.01);
	add_slider(tone_props, S_SKIN_PULL, "SkinPull", 0.0, 1.0, 0.01);
	add_slider(tone_props, S_SKIN_LO, "SkinBandLo", 110.0, 140.0, 0.5);
	add_slider(tone_props, S_SKIN_HI, "SkinBandHi", 110.0, 140.0, 0.5);
	obs_properties_add_group(props, "tone_group", T_("ToneGroup"), OBS_GROUP_NORMAL, tone_props);

	// Eyes and teeth.
	obs_properties_t *eye_props = obs_properties_create();
	add_slider(eye_props, S_EYE_SHARPEN, "EyeSharpen", 0.0, 1.0, 0.01);
	add_slider(eye_props, S_EYE_ZONE, "EyeZone", 1.0, 1.6, 0.01);
	add_slider(eye_props, S_EYE_DEREFLECT, "EyeDereflect", 0.0, 1.0, 0.01);
	add_slider(eye_props, S_EYE_BRIGHTEN, "EyeBrighten", 0.0, 1.0, 0.01);
	add_slider(eye_props, S_TEETH, "Teeth", 0.0, 1.0, 0.01);
	obs_properties_add_group(props, "eye_group", T_("EyeGroup"), OBS_GROUP_NORMAL, eye_props);

	// Color LUTs: strength + region per slot, the custom slot also has a file
	// picker; the film look belongs to the color section too.
	obs_properties_t *lut_props = obs_properties_create();
	for (int i = 0; i < kLutSlots; ++i) {
		if (i == kLutCustomSlot)
			describe(obs_properties_add_path(lut_props, S_LUT_CUSTOM_PATH, T_("LutCustomFile"), OBS_PATH_FILE,
							 "Cube LUT (*.cube);;All files (*.*)", nullptr),
				 "LutCustomFile");
		add_slider(lut_props, lut_key(i, "_amount").c_str(), kLutLabels[i], 0.0, 1.0, 0.01);
		obs_property_t *region = obs_properties_add_list(lut_props, lut_key(i, "_region").c_str(), T_("LutRegion"),
								 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
		describe(region, "LutRegion");
		obs_property_list_add_int(region, T_("LutRegion.Skin"), kLutSkin);
		obs_property_list_add_int(region, T_("LutRegion.Person"), kLutPerson);
		obs_property_list_add_int(region, T_("LutRegion.Frame"), kLutFrame);
		const std::string error = kce::get_string(st, "lut_error_" + std::to_string(i));
		if (!error.empty()) {
			obs_property_t *info =
				obs_properties_add_text(lut_props, lut_key(i, "_error").c_str(), error.c_str(), OBS_TEXT_INFO);
			obs_property_text_set_info_type(info, OBS_TEXT_INFO_WARNING);
		}
	}
	add_slider(lut_props, S_FILM, "Film", 0.0, 1.0, 0.01);
	obs_properties_add_text(lut_props, "lut_info", T_("LutInfo"), OBS_TEXT_INFO);
	obs_properties_add_group(props, "lut_group", T_("LutGroup"), OBS_GROUP_NORMAL, lut_props);

	// Vectorscope: drawn into the filter output, so it is a setup tool.
	obs_properties_t *scope_props = obs_properties_create();
	obs_property_t *mode = obs_properties_add_list(scope_props, S_SCOPE_MODE, T_("ScopeMode"), OBS_COMBO_TYPE_LIST,
						      OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode, T_("ScopeMode.Off"), kScopeOff);
	obs_property_list_add_int(mode, T_("ScopeMode.Overlay"), kScopeOverlay);
	obs_property_list_add_int(mode, T_("ScopeMode.Full"), kScopeFull);
	add_bool(scope_props, S_SCOPE_FACE, "ScopeFaceOnly");
	add_bool(scope_props, S_SCOPE_QUAD, "ScopeQuadrant");
	obs_property_t *zoom = obs_properties_add_list(scope_props, S_SCOPE_ZOOM, T_("ScopeZoom"), OBS_COMBO_TYPE_LIST,
						      OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(zoom, "1x", 1);
	obs_property_list_add_int(zoom, "2x", 2);
	obs_property_list_add_int(zoom, "3x", 3);
	obs_property_list_add_int(zoom, "4x", 4);
	obs_properties_add_text(scope_props, "scope_info", T_("ScopeInfo"), OBS_TEXT_INFO);
	obs_properties_add_group(props, "scope_group", T_("ScopeGroup"), OBS_GROUP_NORMAL, scope_props);

	// Measurement: the Messprotokoll report of the live frame (refreshed on click).
	obs_properties_t *meas_props = obs_properties_create();
	add_bool(meas_props, S_MEASURE, "Measure");
	obs_property_t *mset = obs_properties_add_list(meas_props, S_MEASURE_SET, T_("MeasureSet"), OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mset, "N  Dunkel (Abend/Winter)", kSetN);
	obs_property_list_add_int(mset, "S  Sommerabend", kSetS);
	obs_property_list_add_int(mset, "T  Tag", kSetT);
	obs_property_list_add_int(mset, "H  Hell/High-Key", kSetH);
	obs_property_list_add_int(mset, "K  Kino/warm", kSetK);
	obs_property_t *msrc = obs_properties_add_list(meas_props, S_MEASURE_SOURCE, T_("MeasureSource"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(msrc, T_("MeasureSource.Output"), 0);
	obs_property_list_add_int(msrc, T_("MeasureSource.Input"), 1);
	obs_property_t *wside = obs_properties_add_list(meas_props, S_WALL_SIDE, T_("WallSide"), OBS_COMBO_TYPE_LIST,
							OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(wside, T_("WallSide.Right"), 0);
	obs_property_list_add_int(wside, T_("WallSide.Left"), 1);
	add_slider(meas_props, S_WALL_OFFSET, "WallOffset", 0.0, 3.0, 0.05);
	add_slider(meas_props, S_WALL_WIDTH, "WallWidth", 0.1, 3.0, 0.05);
	obs_properties_add_button2(meas_props, "measure_refresh", T_("MeasureRefresh"), measure_refresh_clicked, f);
	obs_properties_add_text(meas_props, "measure_report", current_report(f).c_str(), OBS_TEXT_INFO);
	obs_properties_add_text(meas_props, "measure_info", T_("MeasureInfo"), OBS_TEXT_INFO);
	obs_properties_add_group(props, "measure_group", T_("MeasureGroup"), OBS_GROUP_NORMAL, meas_props);

	// Diagnostics: the debug views, the engine, the tracker and the pipeline's
	// timings (the OBS log carries the same every 120 frames).
	obs_properties_t *diag_props = obs_properties_create();
	obs_property_t *dbg = obs_properties_add_list(diag_props, S_DEBUG_VIEW, T_("DebugView"), OBS_COMBO_TYPE_LIST,
						     OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(dbg, T_("DebugView.Off"), kDebugOff);
	obs_property_list_add_int(dbg, T_("DebugView.Mask"), kDebugMask);
	obs_property_list_add_int(dbg, T_("DebugView.Mean"), kDebugMean);
	obs_property_list_add_int(dbg, T_("DebugView.Landmarks"), kDebugLandmarks);
	obs_property_list_add_int(dbg, T_("DebugView.Tone"), kDebugTone);
	obs_property_list_add_int(dbg, T_("DebugView.FaceOrig"), kDebugFaceOrig);
	obs_property_list_add_int(dbg, T_("DebugView.FaceProc"), kDebugFaceProc);
	obs_property_list_add_int(dbg, T_("DebugView.Normals"), kDebugNormals);
	obs_property_list_add_int(dbg, T_("DebugView.Zones"), kDebugZones);
	if (f) {
		obs_properties_add_text(diag_props, "engine_status", engine_text().c_str(), OBS_TEXT_INFO);
		const std::string error = kce::get_string(st, "error");
		if (!error.empty()) {
			obs_property_t *info = obs_properties_add_text(diag_props, "engine_error", error.c_str(), OBS_TEXT_INFO);
			obs_property_text_set_info_type(info, OBS_TEXT_INFO_ERROR);
		}
		if (has_status && kce::get_bool(st, "core", false)) {
			char status[256];
			snprintf(status, sizeof(status), "%s: %s", T_("TrackerStatus"), kce::get_string(st, "tracker").c_str());
			obs_properties_add_text(diag_props, "tracker_status", status, OBS_TEXT_INFO);
			if (kce::get_bool(st, "perf_valid", false)) {
				char text[160];
				snprintf(text, sizeof(text), "%s: CPU %.2f ms, GPU %.2f ms per frame", T_("Perf"),
					 kce::get_number(st, "perf_cpu", 0.0), kce::get_number(st, "perf_gpu", 0.0));
				obs_properties_add_text(diag_props, "perf_info", text, OBS_TEXT_INFO);
				char text2[640];
				snprintf(text2, sizeof(text2),
					 "GPU passes (ms): %s. Tracker: models %.1f ms, wait %.1f ms, older results %.0f %%",
					 kce::get_string(st, "perf_passes").c_str(), kce::get_number(st, "tracker_model", 0.0),
					 kce::get_number(st, "tracker_wait", 0.0), kce::get_number(st, "tracker_older", 0.0));
				obs_properties_add_text(diag_props, "perf_passes", text2, OBS_TEXT_INFO);
			}
		}
	}
	obs_properties_add_group(props, "diag_group", T_("DiagGroup"), OBS_GROUP_NORMAL, diag_props);
	obs_data_release(settings);
	return props;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

// Renders the filter target (the source + preceding filters) into the texrender.
gs_texture_t *capture_target(FilterData *f, obs_source_t *target, uint32_t width, uint32_t height)
{
	gs_texrender_reset(f->tr_source);
	if (!gs_texrender_begin(f->tr_source, width, height))
		return nullptr;

	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	obs_source_video_render(target);
	gs_blend_state_pop();

	gs_texrender_end(f->tr_source);
	return gs_texrender_get_texture(f->tr_source);
}

void draw_texture(gs_texture_t *tex, uint32_t width, uint32_t height)
{
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, tex);
	while (gs_effect_loop(effect, "Draw"))
		gs_draw_sprite(tex, 0, width, height);
}

// The processed frame over the source by `fade` (1 = processed only).
void draw_result(FilterData *f, gs_texture_t *src, float fade)
{
	if (fade >= 1.0f || !g_fade || !src) {
		draw_texture(f->last_out, f->width, f->height);
		return;
	}
	gs_effect_set_texture(gs_effect_get_param_by_name(g_fade, "image"), src);
	gs_effect_set_texture(gs_effect_get_param_by_name(g_fade, "image2"), f->last_out);
	gs_effect_set_float(gs_effect_get_param_by_name(g_fade, "fade"), fade);
	while (gs_effect_loop(g_fade, "Draw"))
		gs_draw_sprite(src, 0, f->width, f->height);
}

void release_textures(FilterData *f)
{
	gs_texture_destroy(f->sh_in);
	gs_texture_destroy(f->sh_out);
	f->sh_in = f->sh_out = nullptr;
	f->in_handle = f->out_handle = 0;
}

// Shared textures for the engine now running and this size; sets them up with
// the engine when new. False when they cannot be made.
bool ensure_textures(FilterData *f, EngineLink *l, uint32_t width, uint32_t height)
{
	const uint32_t epoch = l->epoch();
	bool renew = !f->sh_in || !f->sh_out || f->width != width || f->height != height || f->epoch != epoch;
	if (!renew && (gs_texture_get_shared_handle(f->sh_in) != f->in_handle ||
		       gs_texture_get_shared_handle(f->sh_out) != f->out_handle))
		renew = true; // OBS rebuilt its device: the textures are new ones
	if (!renew)
		return true;
	release_textures(f);
	if (!f->last_out || f->width != width || f->height != height) {
		gs_texture_destroy(f->last_out);
		f->last_out = gs_texture_create(width, height, GS_RGBA, 1, nullptr, 0);
		f->last_ok_ns = 0;
	}
	f->width = width;
	f->height = height;
	f->epoch = epoch;
	f->outstanding = false;
	f->fade_pending = true;
	// A signal left from the old textures must not count for the new ones.
	ResetEvent(f->h.ready);
	ResetEvent(f->h.done);
	f->sh_in = gs_texture_create(width, height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET | GS_SHARED_KM_TEX);
	f->sh_out = gs_texture_create(width, height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET | GS_SHARED_KM_TEX);
	if (!f->sh_in || !f->sh_out || !f->last_out) {
		release_textures(f);
		return false;
	}
	f->in_handle = gs_texture_get_shared_handle(f->sh_in);
	f->out_handle = gs_texture_get_shared_handle(f->sh_out);
	if (f->in_handle == GS_INVALID_HANDLE || f->out_handle == GS_INVALID_HANDLE) {
		release_textures(f);
		return false;
	}
	// libobs holds key 0 of a new shared texture; the output goes to the engine.
	gs_texture_release_sync(f->sh_out, 0);
	if (++f->generation == 0)
		++f->generation;
	kce::FrameSetup fs = {};
	fs.generation = f->generation;
	fs.width = width;
	fs.height = height;
	fs.format = DXGI_FORMAT_R8G8B8A8_UNORM;
	fs.input_handle = f->in_handle;
	fs.output_handle = f->out_handle;
	l->frame_setup(f->id, fs);
	return true;
}

// The settings the engine measured (a light side, a crop) or folded in (the
// exposure): into the scene's settings, so the next start has them.
void apply_writebacks(FilterData *f)
{
	std::string wb;
	{
		std::lock_guard<std::mutex> lock(f->m);
		wb.swap(f->writeback);
	}
	if (wb.empty())
		return;
	obs_data_t *settings = obs_source_get_settings(f->source);
	apply_values(settings, kce::parse(wb));
	const char *key = obs_data_get_string(settings, S_LICENSE_KEY); // cleared once activated
	f->pending_key = key ? key : "";
	obs_data_release(settings);
}

// A license key pasted into the properties: the engine verifies and stores it
// (for the app as well), and the field is cleared once it holds (the file is the
// license, not the scene). Once per key and engine, never waiting for it.
void activate_key(FilterData *f, EngineLink *l)
{
	if (f->pending_key.empty() || (f->pending_key == f->key_sent && f->key_epoch == l->epoch()))
		return;
	f->key_sent = f->pending_key;
	f->key_epoch = l->epoch();
	std::string req;
	kce::put_string(req, "verb", "license_activate");
	kce::put_string(req, "key", f->pending_key);
	const uint32_t id = f->id;
	l->request_async(id, req, [id](bool ok, const std::string &reply) {
		if (!ok || !kce::get_bool(kce::parse(reply), "licensed", false))
			return;
		std::string clear;
		kce::put_string(clear, S_LICENSE_KEY, "");
		std::lock_guard<std::mutex> lock(g_reg_m);
		const auto it = g_reg.find(id);
		if (it == g_reg.end())
			return;
		std::lock_guard<std::mutex> lock2(it->second->m);
		it->second->writeback += clear;
	});
}

// Frames per hand-off line; KLARCAM_HOST_STATS_FRAMES sets it for test runs.
size_t stats_frames()
{
	static const size_t n = [] {
		char buf[16] = {};
		size_t len = 0;
		if (getenv_s(&len, buf, sizeof(buf), "KLARCAM_HOST_STATS_FRAMES") == 0 && len > 1)
			return static_cast<size_t>(std::max(30, atoi(buf)));
		return kStatsFrames;
	}();
	return n;
}

void log_stats(FilterData *f)
{
	if (f->ms.size() < stats_frames())
		return;
	std::vector<float> s = f->ms;
	std::sort(s.begin(), s.end());
	double sum = 0.0;
	for (float x : s)
		sum += x;
	const kce::FrameSlot *slot = f->h.slot;
	blog(LOG_INFO,
	     "%s engine hand-off @%ux%u: render %.2f ms avg, p99 %.2f, max %.2f over %zu frames; late %u (held %u, raw %u); "
	     "engine %.2f ms, core CPU %.2f GPU %.2f ms",
	     kLogPrefix, f->width, f->height, sum / static_cast<double>(s.size()), s[s.size() * 99 / 100], s.back(),
	     s.size(), f->timeouts, f->held, f->raw, slot->engine_ms, slot->core_cpu_ms, slot->core_gpu_ms);
	f->ms.clear();
	f->timeouts = f->held = f->raw = 0;
}

// A second view within the same OBS frame (the filter window's preview, a
// projector) draws what the first one drew.
void draw_again(FilterData *f)
{
	if (!f->frame_ok || !f->last_out) {
		obs_source_skip_video_filter(f->source);
		return;
	}
	const bool previous_srgb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(false);
	draw_result(f, f->frame_fade < 1.0f ? gs_texrender_get_texture(f->tr_source) : nullptr, f->frame_fade);
	gs_enable_framebuffer_srgb(previous_srgb);
}

void filter_video_render(void *data, gs_effect_t *)
{
	auto *f = static_cast<FilterData *>(data);
	const uint64_t t0 = os_gettime_ns();
	// Once per OBS frame, however many views render the source.
	const uint64_t frame_time = obs_get_video_frame_time();
	if (frame_time == f->frame_time) {
		draw_again(f);
		return;
	}
	f->frame_time = frame_time;
	f->frame_ok = false;
	apply_writebacks(f);

	obs_source_t *target = obs_filter_get_target(f->source);
	obs_source_t *parent = obs_filter_get_parent(f->source);
	EngineLink *l = link();
	if (!l || !f->id || !target || !parent || !l->ready()) {
		obs_source_skip_video_filter(f->source);
		return;
	}
	const uint32_t width = obs_source_get_base_width(target);
	const uint32_t height = obs_source_get_base_height(target);
	if (width == 0 || height == 0) {
		obs_source_skip_video_filter(f->source);
		return;
	}

	// Everything works on gamma-encoded 8-bit RGBA.
	const bool previous_srgb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(false);
	gs_texture_t *src = capture_target(f, target, width, height);
	if (!src || !ensure_textures(f, l, width, height)) {
		if (src)
			draw_texture(src, width, height);
		gs_enable_framebuffer_srgb(previous_srgb);
		if (!src)
			obs_source_skip_video_filter(f->source);
		return;
	}

	kce::FrameSlot *slot = f->h.slot;
	// One wait budget per OBS frame for all KlarCam filters: however many there
	// are, a slow engine costs OBS no more than 0.75 of a frame interval.
	if (g_budget.frame_time != frame_time) {
		g_budget.frame_time = frame_time;
		g_budget.deadline = t0 + static_cast<uint64_t>(
						 std::max(6.0e6, 0.75 * static_cast<double>(obs_get_frame_interval_ns())));
	}
	auto left_ms = [&]() {
		const uint64_t n = os_gettime_ns();
		return n >= g_budget.deadline ? 0.0 : static_cast<double>(g_budget.deadline - n) / 1.0e6;
	};
	activate_key(f, l);
	bool processed = false, late = false, handed = false, trouble = false;

	// A frame that missed its timeout: back by now, or the engine hangs.
	if (f->outstanding) {
		if (slot->frame_done >= f->frame) {
			// Its output goes back to the engine.
			if (slot->result == kce::kResultProcessed && gs_texture_acquire_sync(f->sh_out, 1, 0) == 0)
				gs_texture_release_sync(f->sh_out, 0);
			f->outstanding = false;
		} else if (slot->busy_until > GetTickCount64()) {
			f->outstanding_since = t0; // applying settings or creating a processor: no hang yet
			late = true;
		} else if (t0 - f->outstanding_since > kHangNs) {
			l->restart("the engine did not answer a frame for 2 s");
			f->outstanding = false; // the next engine gets new textures
		} else {
			late = true;
		}
	}

	if (!f->outstanding && slot->ready_generation == f->generation && left_ms() < 1.0) {
		late = true; // the frame's budget went to the other filters
	} else if (!f->outstanding && slot->ready_generation == f->generation) {
		const int acquired = gs_texture_acquire_sync(f->sh_in, 0, 0);
		if (acquired == 0) {
			gs_copy_texture(f->sh_in, src);
			gs_texture_release_sync(f->sh_in, 1);
			gs_flush();
			slot->frame_generation = f->generation;
			InterlockedExchange64(&slot->frame_sent, ++f->frame);
			SetEvent(f->h.ready);
			handed = true;
			bool done = false;
			for (;;) {
				const double left = left_ms();
				if (left <= 0.0 ||
				    WaitForSingleObject(f->h.done, static_cast<DWORD>(std::ceil(left))) != WAIT_OBJECT_0)
					break;
				if (slot->frame_done >= f->frame) { // else a late signal of an earlier frame
					done = true;
					break;
				}
			}
			if (!done) {
				f->outstanding = true;
				f->outstanding_since = t0;
				late = true;
				++f->timeouts;
			} else if (slot->result == kce::kResultProcessed) {
				const double left = left_ms();
				const uint32_t wait = left > 1.0 ? static_cast<uint32_t>(left) : 1;
				if (gs_texture_acquire_sync(f->sh_out, 1, wait) == 0) {
					gs_copy_texture(f->last_out, f->sh_out);
					gs_texture_release_sync(f->sh_out, 0);
					processed = true;
				} else {
					f->epoch = 0; // the output is stuck: new textures next frame
				}
			} else if (slot->result == kce::kResultError) {
				trouble = true;
			}
		} else if (acquired == ETIMEDOUT) {
			trouble = true; // the input is not back at key 0
		} else {
			f->epoch = 0; // abandoned by an engine that ended: new textures next frame
		}
	}

	// Stuck: textures left at the wrong key, or an engine that answers errors.
	// New textures after half a second; three renewals within 10 s: a new engine.
	if (trouble) {
		if (!f->stuck_since) {
			f->stuck_since = t0;
		} else if (t0 - f->stuck_since > kStuckNs) {
			f->stuck_since = 0;
			f->epoch = 0;
			if (t0 - f->renewals_since > kRenewWindowNs) {
				f->renewals_since = t0;
				f->renewals = 0;
			}
			if (++f->renewals >= 3) {
				f->renewals = 0;
				l->restart("the frame textures stayed stuck");
			} else {
				blog(LOG_WARNING, "%s the frame textures were stuck; setting them up anew", kLogPrefix);
			}
		}
	} else if (handed && !f->outstanding) {
		f->stuck_since = 0;
	}

	const uint64_t now = os_gettime_ns();
	if (processed) {
		f->last_ok_ns = now;
		if (f->fade_pending) {
			f->fade_pending = false;
			f->fade_start = now;
		}
		f->frame_fade = std::min(1.0f, static_cast<float>(now - f->fade_start) / static_cast<float>(kFadeNs));
		draw_result(f, src, f->frame_fade);
		f->frame_ok = true;
	} else if (late && f->last_ok_ns && now - f->last_ok_ns < kHoldNs) {
		// Late: the last processed frame holds; no flash of the raw camera.
		draw_result(f, src, f->frame_fade);
		f->frame_ok = true;
		++f->held;
	} else {
		draw_texture(src, width, height);
		if (late)
			++f->raw;
	}
	gs_enable_framebuffer_srgb(previous_srgb);
	if (handed) {
		f->ms.push_back(static_cast<float>(static_cast<double>(os_gettime_ns() - t0) / 1.0e6));
		log_stats(f);
	}
}

} // namespace

void register_filter()
{
	obs_source_info info = {};
	info.id = kFilterId;
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = filter_get_name;
	info.create = filter_create;
	info.destroy = filter_destroy;
	info.update = filter_update;
	info.save = filter_save;
	info.get_defaults = filter_get_defaults;
	info.get_properties = filter_get_properties;
	info.video_render = filter_video_render;
	obs_register_source(&info);
}

void shutdown_filter()
{
	std::unique_ptr<EngineLink> l;
	{
		std::lock_guard<std::mutex> lock(g_link_m);
		l = std::move(g_link);
	}
	l.reset(); // the engine quits
	obs_enter_graphics();
	if (g_loss_registered) {
		gs_unregister_loss_callbacks(&g_link);
		g_loss_registered = false;
	}
	gs_effect_destroy(g_fade);
	g_fade = nullptr;
	obs_leave_graphics();
}

} // namespace klarcam_host
