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

#ifndef META_CRTC_XRANDR_H
#define META_CRTC_XRANDR_H

#include <X11/extensions/Xrandr.h>
#include <xcb/randr.h>

#include "backends/meta-crtc.h"

gboolean meta_crtc_xrandr_set_config (MetaCrtc            *crtc,
                                      xcb_randr_crtc_t     xrandr_crtc,
                                      xcb_timestamp_t      timestamp,
                                      int                  x,
                                      int                  y,
                                      xcb_randr_mode_t     mode,
                                      xcb_randr_rotation_t rotation,
                                      xcb_randr_output_t  *outputs,
                                      int                  n_outputs,
                                      xcb_timestamp_t     *out_timestamp);

MetaCrtc * meta_create_xrandr_crtc (MetaMonitorManager *monitor_manager,
                                    XRRCrtcInfo        *xrandr_crtc,
                                    RRCrtc              crtc_id,
                                    XRRScreenResources *resources);

#endif /* META_CRTC_XRANDR_H */
