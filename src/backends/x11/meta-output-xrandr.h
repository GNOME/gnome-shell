/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_OUTPUT_XRANDR
#define META_OUTPUT_XRANDR

#include <X11/extensions/Xrandr.h>

#include "backends/meta-output.h"
#include "backends/x11/meta-monitor-manager-xrandr.h"

void meta_output_xrandr_apply_mode (MetaOutput *output);

void meta_output_xrandr_change_backlight (MetaOutput *output,
                                          int         value);

GBytes * meta_output_xrandr_read_edid (MetaOutput *output);

MetaOutput * meta_create_xrandr_output (MetaMonitorManager *monitor_manager,
                                        XRROutputInfo      *xrandr_output,
                                        RROutput            output_id,
                                        RROutput            primary_output);

#endif /* META_OUTPUT_XRANDR */
