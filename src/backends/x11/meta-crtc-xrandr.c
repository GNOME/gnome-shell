/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013-2017 Red Hat Inc.
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

#include "config.h"

#include "backends/x11/meta-crtc-xrandr.h"

#include <stdlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>

#include "backends/x11/meta-monitor-manager-xrandr.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

gboolean
meta_crtc_xrandr_set_config (MetaCrtc            *crtc,
                             xcb_randr_crtc_t     xrandr_crtc,
                             xcb_timestamp_t      timestamp,
                             int                  x,
                             int                  y,
                             xcb_randr_mode_t     mode,
                             xcb_randr_rotation_t rotation,
                             xcb_randr_output_t  *outputs,
                             int                  n_outputs,
                             xcb_timestamp_t     *out_timestamp)
{
  MetaMonitorManager *monitor_manager = meta_crtc_get_monitor_manager (crtc);
  MetaMonitorManagerXrandr *monitor_manager_xrandr =
    META_MONITOR_MANAGER_XRANDR (monitor_manager);
  Display *xdisplay;
  XRRScreenResources *resources;
  xcb_connection_t *xcb_conn;
  xcb_timestamp_t config_timestamp;
  xcb_randr_set_crtc_config_cookie_t cookie;
  xcb_randr_set_crtc_config_reply_t *reply;
  xcb_generic_error_t *xcb_error = NULL;

  xdisplay = meta_monitor_manager_xrandr_get_xdisplay (monitor_manager_xrandr);
  xcb_conn = XGetXCBConnection (xdisplay);
  resources =
    meta_monitor_manager_xrandr_get_resources (monitor_manager_xrandr);
  config_timestamp = resources->configTimestamp;
  cookie = xcb_randr_set_crtc_config (xcb_conn,
                                      xrandr_crtc,
                                      timestamp,
                                      config_timestamp,
                                      x, y,
                                      mode,
                                      rotation,
                                      n_outputs,
                                      outputs);
  reply = xcb_randr_set_crtc_config_reply (xcb_conn,
                                           cookie,
                                           &xcb_error);
  if (xcb_error || !reply)
    {
      free (xcb_error);
      free (reply);
      return FALSE;
    }

  *out_timestamp = reply->timestamp;
  free (reply);

  return TRUE;
}

static MetaMonitorTransform
meta_monitor_transform_from_xrandr (Rotation rotation)
{
  static const MetaMonitorTransform y_reflected_map[4] = {
    META_MONITOR_TRANSFORM_FLIPPED_180,
    META_MONITOR_TRANSFORM_FLIPPED_90,
    META_MONITOR_TRANSFORM_FLIPPED,
    META_MONITOR_TRANSFORM_FLIPPED_270
  };
  MetaMonitorTransform ret;

  switch (rotation & 0x7F)
    {
    default:
    case RR_Rotate_0:
      ret = META_MONITOR_TRANSFORM_NORMAL;
      break;
    case RR_Rotate_90:
      ret = META_MONITOR_TRANSFORM_90;
      break;
    case RR_Rotate_180:
      ret = META_MONITOR_TRANSFORM_180;
      break;
    case RR_Rotate_270:
      ret = META_MONITOR_TRANSFORM_270;
      break;
    }

  if (rotation & RR_Reflect_X)
    return ret + 4;
  else if (rotation & RR_Reflect_Y)
    return y_reflected_map[ret];
  else
    return ret;
}

#define ALL_ROTATIONS (RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_180 | RR_Rotate_270)

static MetaMonitorTransform
meta_monitor_transform_from_xrandr_all (Rotation rotation)
{
  unsigned ret;

  /* Handle the common cases first (none or all) */
  if (rotation == 0 || rotation == RR_Rotate_0)
    return (1 << META_MONITOR_TRANSFORM_NORMAL);

  /* All rotations and one reflection -> all of them by composition */
  if ((rotation & ALL_ROTATIONS) &&
      ((rotation & RR_Reflect_X) || (rotation & RR_Reflect_Y)))
    return ALL_TRANSFORMS;

  ret = 1 << META_MONITOR_TRANSFORM_NORMAL;
  if (rotation & RR_Rotate_90)
    ret |= 1 << META_MONITOR_TRANSFORM_90;
  if (rotation & RR_Rotate_180)
    ret |= 1 << META_MONITOR_TRANSFORM_180;
  if (rotation & RR_Rotate_270)
    ret |= 1 << META_MONITOR_TRANSFORM_270;
  if (rotation & (RR_Rotate_0 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED;
  if (rotation & (RR_Rotate_90 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED_90;
  if (rotation & (RR_Rotate_180 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED_180;
  if (rotation & (RR_Rotate_270 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED_270;

  return ret;
}

MetaCrtc *
meta_create_xrandr_crtc (MetaMonitorManager *monitor_manager,
                         XRRCrtcInfo        *xrandr_crtc,
                         RRCrtc              crtc_id,
                         XRRScreenResources *resources)
{
  MetaCrtc *crtc;
  unsigned int i;

  crtc = g_object_new (META_TYPE_CRTC, NULL);

  crtc->monitor_manager = monitor_manager;
  crtc->crtc_id = crtc_id;
  crtc->rect.x = xrandr_crtc->x;
  crtc->rect.y = xrandr_crtc->y;
  crtc->rect.width = xrandr_crtc->width;
  crtc->rect.height = xrandr_crtc->height;
  crtc->is_dirty = FALSE;
  crtc->transform =
    meta_monitor_transform_from_xrandr (xrandr_crtc->rotation);
  crtc->all_transforms =
    meta_monitor_transform_from_xrandr_all (xrandr_crtc->rotations);

  for (i = 0; i < (unsigned int) resources->nmode; i++)
    {
      if (resources->modes[i].id == xrandr_crtc->mode)
        {
          crtc->current_mode = g_list_nth_data (monitor_manager->modes, i);
          break;
        }
    }

  return crtc;
}
