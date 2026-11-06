/*
 * KlarCam filter host for OBS Studio: the filter's setting keys, defaults and
 * list values. The defaults are the KlarCam engine's own, listed here so the
 * host builds on its own; the KlarCam build compares the two so they stay
 * equal.
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

namespace klarcam_host {

// Setting keys (stable: scene collections keep their values).
constexpr const char *S_SMOOTHING = "smoothing";
constexpr const char *S_SHARPEN = "sharpen";
constexpr const char *S_EYE_SHARPEN = "eye_sharpen";
constexpr const char *S_EYE_ZONE = "eye_zone";
constexpr const char *S_EYE_DEREFLECT = "eye_dereflect";
constexpr const char *S_TONE_SMOOTH = "tone_smooth";
constexpr const char *S_TONE_BAND = "tone_band";
constexpr const char *S_TONE_CONTRAST = "tone_contrast";
constexpr const char *S_SKIN_PULL = "skin_pull";
constexpr const char *S_SKIN_LO = "skin_band_lo";
constexpr const char *S_SKIN_HI = "skin_band_hi";
constexpr const char *S_RELIGHT = "relight";
constexpr const char *S_RELIGHT_DIR = "relight_dir";
constexpr const char *S_RELIGHT_AUTO = "relight_auto";
constexpr const char *S_RELIGHT_SIDE_LOCK = "relight_side_lock"; // written back once measured
constexpr const char *S_RELIGHT_HEIGHT = "relight_height";
constexpr const char *S_RELIGHT_SPREAD = "relight_spread";
constexpr const char *S_RELIGHT_EDGE = "relight_edge";
constexpr const char *S_TEETH = "teeth_whiten";
constexpr const char *S_EYE_BRIGHTEN = "eye_brighten";
constexpr const char *S_FILM = "film";
constexpr const char *S_RADIUS = "radius";
constexpr const char *S_SKIN_GATE = "skin_gate";
constexpr const char *S_DEBUG_VIEW = "debug_view";
constexpr const char *S_SCOPE_MODE = "scope_mode";
constexpr const char *S_SCOPE_FACE = "scope_face_only";
constexpr const char *S_SCOPE_ZOOM = "scope_zoom";
constexpr const char *S_SCOPE_QUAD = "scope_quadrant";
constexpr const char *S_LUT_CUSTOM_PATH = "lut_custom_path";
constexpr const char *S_EXPOSURE = "exposure_ev";
constexpr const char *S_AUTO_EXPOSURE = "auto_exposure";
constexpr const char *S_AUTO_TARGET = "auto_target_ire";
constexpr const char *S_BACKGROUND = "background_ev";
constexpr const char *S_BACKGROUND_RADIAL = "background_radial";
constexpr const char *S_FACE_LIGHT = "face_light";
constexpr const char *S_FACE_LIGHT_REACH = "face_light_reach";
constexpr const char *S_AUTO_FRAME = "auto_frame";
constexpr const char *S_FRAME_TARGET = "frame_target";
constexpr const char *S_FRAME_LOCK_X = "frame_lock_x"; // the measured crop, written back
constexpr const char *S_FRAME_LOCK_Y = "frame_lock_y";
constexpr const char *S_FRAME_LOCK_Z = "frame_lock_z";
constexpr const char *S_MEASURE = "measure";
constexpr const char *S_MEASURE_SET = "measure_set";
constexpr const char *S_MEASURE_SOURCE = "measure_source";
constexpr const char *S_WALL_SIDE = "wall_side";
constexpr const char *S_WALL_OFFSET = "wall_offset";
constexpr const char *S_WALL_WIDTH = "wall_width";
constexpr const char *S_BYPASS = "bypass";
constexpr const char *S_BEARD = "beard";
constexpr const char *S_BEARD_AUTO = "beard_auto";
constexpr const char *S_PRESET = "preset";           // 0 = keep the values, 1 Natural, 2 Stream (applied once)
constexpr const char *S_LICENSE_KEY = "license_key"; // pasted once, stored by the engine, then cleared

constexpr int kLutSlots = 4;
constexpr int kLutCustomSlot = 3;
constexpr const char *kLutKeys[kLutSlots] = {"lut_red", "lut_yellow", "lut_contrast", "lut_custom"};
constexpr const char *kLutLabels[kLutSlots] = {"LutRed", "LutYellow", "LutContrast", "LutCustom"};

// List values the engine understands.
enum LutRegion : int { kLutSkin = 0, kLutPerson = 1, kLutFrame = 2 };
enum ScopeMode : int { kScopeOff = 0, kScopeOverlay = 1, kScopeFull = 2 };
enum DebugView : int {
	kDebugOff = 0,
	kDebugMask = 1,
	kDebugMean = 2,
	kDebugLandmarks = 3,
	kDebugTone = 5,
	kDebugFaceOrig = 7,
	kDebugFaceProc = 8,
	kDebugNormals = 9,
	kDebugZones = 10
};
enum MeasureSet : int { kSetN = 0, kSetS = 1, kSetT = 2, kSetH = 3, kSetK = 4 };

struct DefaultValue {
	const char *key;
	char type; // 'd' double, 'i' integer, 'b' bool, 's' string
	double number;
	const char *text;
};

// Float literals on purpose: the engine's values are floats, and the defaults
// are those floats widened, exactly.
inline constexpr DefaultValue kDefaults[] = {
	{S_SMOOTHING, 'd', 0.6f, nullptr},
	{S_SHARPEN, 'd', 0.25f, nullptr},
	{S_EYE_SHARPEN, 'd', 0.0f, nullptr},
	{S_EYE_ZONE, 'd', 1.2f, nullptr},
	{S_EYE_DEREFLECT, 'd', 0.0f, nullptr},
	{S_TONE_SMOOTH, 'd', 0.0f, nullptr},
	{S_TONE_BAND, 'd', 0.5f, nullptr},
	{S_TONE_CONTRAST, 'd', 0.0f, nullptr},
	{S_SKIN_PULL, 'd', 0.0f, nullptr},
	{S_SKIN_LO, 'd', 123.0f, nullptr},
	{S_SKIN_HI, 'd', 126.5f, nullptr},
	{S_RELIGHT, 'd', 0.0f, nullptr},
	{S_RELIGHT_DIR, 'd', 0.6f, nullptr},
	{S_RELIGHT_AUTO, 'b', 1, nullptr},
	{S_RELIGHT_SIDE_LOCK, 'i', 0, nullptr},
	{S_RELIGHT_HEIGHT, 'd', 0.4f, nullptr},
	{S_RELIGHT_SPREAD, 'd', 1.5f, nullptr},
	{S_RELIGHT_EDGE, 'd', 0.5f, nullptr},
	{S_TEETH, 'd', 0.0f, nullptr},
	{S_EYE_BRIGHTEN, 'd', 0.0f, nullptr},
	{S_FILM, 'd', 0.0f, nullptr},
	{S_RADIUS, 'i', 4, nullptr},
	{S_SKIN_GATE, 'd', 0.8f, nullptr},
	{S_DEBUG_VIEW, 'i', kDebugOff, nullptr},
	{S_SCOPE_MODE, 'i', kScopeOff, nullptr},
	{S_SCOPE_FACE, 'b', 1, nullptr},
	{S_SCOPE_ZOOM, 'i', 2, nullptr},
	{S_SCOPE_QUAD, 'b', 1, nullptr},
	{"lut_red_amount", 'd', 0.0f, nullptr},
	{"lut_red_region", 'i', kLutPerson, nullptr},
	{"lut_yellow_amount", 'd', 0.0f, nullptr},
	{"lut_yellow_region", 'i', kLutPerson, nullptr},
	{"lut_contrast_amount", 'd', 0.0f, nullptr},
	{"lut_contrast_region", 'i', kLutFrame, nullptr},
	{"lut_custom_amount", 'd', 0.0f, nullptr},
	{"lut_custom_region", 'i', kLutFrame, nullptr},
	{S_LUT_CUSTOM_PATH, 's', 0, ""},
	{S_EXPOSURE, 'd', 0.0f, nullptr},
	{S_AUTO_EXPOSURE, 'b', 0, nullptr},
	{S_AUTO_TARGET, 'd', 67.0f, nullptr},
	{S_BACKGROUND, 'd', 0.0f, nullptr},
	{S_BACKGROUND_RADIAL, 'd', 0.0f, nullptr},
	{S_FACE_LIGHT, 'd', 0.0f, nullptr},
	{S_FACE_LIGHT_REACH, 'd', 2.0f, nullptr},
	{S_AUTO_FRAME, 'b', 0, nullptr},
	{S_FRAME_TARGET, 'd', 0.41f, nullptr},
	{S_FRAME_LOCK_X, 'd', 0.5f, nullptr},
	{S_FRAME_LOCK_Y, 'd', 0.5f, nullptr},
	{S_FRAME_LOCK_Z, 'd', 0.0f, nullptr},
	{S_MEASURE, 'b', 0, nullptr},
	{S_MEASURE_SET, 'i', kSetN, nullptr},
	{S_MEASURE_SOURCE, 'i', 0, nullptr},
	{S_WALL_SIDE, 'i', 0, nullptr},
	{S_WALL_OFFSET, 'd', 0.25f, nullptr},
	{S_WALL_WIDTH, 'd', 0.5f, nullptr},
	{S_BYPASS, 'b', 0, nullptr},
	{S_BEARD, 'd', 1.0f, nullptr},
	{S_BEARD_AUTO, 'b', 1, nullptr},
	{S_PRESET, 'i', 0, nullptr},
	{S_LICENSE_KEY, 's', 0, ""},
};

} // namespace klarcam_host
