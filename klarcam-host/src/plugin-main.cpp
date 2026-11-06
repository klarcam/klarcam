/*
 * KlarCam filter host for OBS Studio: the module's entry points.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2026 Lars von Thienen
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <https://www.gnu.org/licenses/>.
 */
#include <obs-module.h>

#include "klarcam-filter.h"

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.0.0"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("klarcam", "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return "KlarCam";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "KlarCam face and skin enhancement: the filter host, which hands the frames to the KlarCam engine";
}

bool obs_module_load(void)
{
	klarcam_host::register_filter();
	blog(LOG_INFO, "[KlarCam] plugin loaded, version %s (filter host; the KlarCam engine starts with the first filter)",
	     PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	klarcam_host::shutdown_filter();
	blog(LOG_INFO, "[KlarCam] plugin unloaded");
}
