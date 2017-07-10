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

#ifndef META_CRTC_H
#define META_CRTC_H

#include <glib-object.h>

#include "backends/meta-gpu.h"

/* Same as KMS mode flags and X11 randr flags */
typedef enum _MetaCrtcModeFlag
{
  META_CRTC_MODE_FLAG_NONE = 0,
  META_CRTC_MODE_FLAG_PHSYNC = (1 << 0),
  META_CRTC_MODE_FLAG_NHSYNC = (1 << 1),
  META_CRTC_MODE_FLAG_PVSYNC = (1 << 2),
  META_CRTC_MODE_FLAG_NVSYNC = (1 << 3),
  META_CRTC_MODE_FLAG_INTERLACE = (1 << 4),
  META_CRTC_MODE_FLAG_DBLSCAN = (1 << 5),
  META_CRTC_MODE_FLAG_CSYNC = (1 << 6),
  META_CRTC_MODE_FLAG_PCSYNC = (1 << 7),
  META_CRTC_MODE_FLAG_NCSYNC = (1 << 8),
  META_CRTC_MODE_FLAG_HSKEW = (1 << 9),
  META_CRTC_MODE_FLAG_BCAST = (1 << 10),
  META_CRTC_MODE_FLAG_PIXMUX = (1 << 11),
  META_CRTC_MODE_FLAG_DBLCLK = (1 << 12),
  META_CRTC_MODE_FLAG_CLKDIV2 = (1 << 13),

  META_CRTC_MODE_FLAG_MASK = 0x3fff
} MetaCrtcModeFlag;

struct _MetaCrtc
{
  GObject parent;

  MetaGpu *gpu;

  glong crtc_id;
  MetaRectangle rect;
  MetaCrtcMode *current_mode;
  MetaMonitorTransform transform;
  unsigned int all_transforms;

  MetaLogicalMonitor *logical_monitor;

  /* Used when changing configuration */
  gboolean is_dirty;

  /* Used by cursor renderer backend */
  void *cursor_renderer_private;

  gpointer driver_private;
  GDestroyNotify driver_notify;
};

struct _MetaCrtcMode
{
  GObject parent;

  /* The low-level ID of this mode, used to apply back configuration */
  glong mode_id;
  char *name;

  int width;
  int height;
  float refresh_rate;
  MetaCrtcModeFlag flags;

  gpointer driver_private;
  GDestroyNotify driver_notify;
};

#define META_TYPE_CRTC (meta_crtc_get_type ())
G_DECLARE_FINAL_TYPE (MetaCrtc, meta_crtc, META, CRTC, GObject)

#define META_TYPE_CRTC_MODE (meta_crtc_mode_get_type ())
G_DECLARE_FINAL_TYPE (MetaCrtcMode, meta_crtc_mode, META, CRTC_MODE, GObject)

MetaGpu * meta_crtc_get_gpu (MetaCrtc *crtc);

#endif /* META_CRTC_H */
