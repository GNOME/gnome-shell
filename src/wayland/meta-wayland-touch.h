/*
 * Wayland Support
 *
 * Copyright (C) 2014 Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef META_WAYLAND_TOUCH_H
#define META_WAYLAND_TOUCH_H

#include <wayland-server.h>

#include <glib.h>

#include "meta-wayland-types.h"

#define META_TYPE_WAYLAND_TOUCH (meta_wayland_touch_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandTouch, meta_wayland_touch,
                      META, WAYLAND_TOUCH,
                      MetaWaylandInputDevice)

typedef struct _MetaWaylandTouchSurface MetaWaylandTouchSurface;
typedef struct _MetaWaylandTouchInfo MetaWaylandTouchInfo;

struct _MetaWaylandTouch
{
  MetaWaylandInputDevice parent;

  struct wl_list resource_list;

  GHashTable *touch_surfaces; /* HT of MetaWaylandSurface->MetaWaylandTouchSurface */
  GHashTable *touches; /* HT of sequence->MetaWaylandTouchInfo */

  ClutterInputDevice *device;
  guint64 frame_slots;
};

void meta_wayland_touch_enable (MetaWaylandTouch *touch);

void meta_wayland_touch_disable (MetaWaylandTouch *touch);

void meta_wayland_touch_update (MetaWaylandTouch   *touch,
                                const ClutterEvent *event);

gboolean meta_wayland_touch_handle_event (MetaWaylandTouch   *touch,
                                          const ClutterEvent *event);

void meta_wayland_touch_create_new_resource (MetaWaylandTouch   *touch,
                                             struct wl_client   *client,
                                             struct wl_resource *seat_resource,
                                             uint32_t            id);
void meta_wayland_touch_cancel (MetaWaylandTouch *touch);


ClutterEventSequence * meta_wayland_touch_find_grab_sequence (MetaWaylandTouch   *touch,
                                                              MetaWaylandSurface *surface,
                                                              uint32_t            serial);

gboolean meta_wayland_touch_get_press_coords (MetaWaylandTouch     *touch,
                                              ClutterEventSequence *sequence,
                                              gfloat               *x,
                                              gfloat               *y);

gboolean meta_wayland_touch_can_popup        (MetaWaylandTouch *touch,
                                              uint32_t          serial);

#endif /* META_WAYLAND_TOUCH_H */
