/*
 * KlarCam filter host for OBS Studio: the filter.
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

void register_filter();
// Ends the engine and releases what the filter module holds (module unload).
void shutdown_filter();

} // namespace klarcam_host
