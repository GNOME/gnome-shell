/*
 * Wayland Support
 *
 * Copyright (C) 2015 Red Hat
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#define _GNU_SOURCE

#include "config.h"

#include <glib.h>
#include "meta-wayland-pointer-gestures.h"
#include "pointer-gestures-server-protocol.h"
#include "meta-wayland-versions.h"
#include "meta-wayland-private.h"

static void
gestures_get_swipe (struct wl_client   *client,
                    struct wl_resource *resource,
                    uint32_t            id,
                    struct wl_resource *pointer_resource)
{
  MetaWaylandPointer *pointer = wl_resource_get_user_data (pointer_resource);

  meta_wayland_pointer_gesture_swipe_create_new_resource (pointer, client, resource, id);
}

static void
gestures_get_pinch (struct wl_client   *client,
                    struct wl_resource *resource,
                    uint32_t            id,
                    struct wl_resource *pointer_resource)
{
  MetaWaylandPointer *pointer = wl_resource_get_user_data (pointer_resource);

  meta_wayland_pointer_gesture_pinch_create_new_resource (pointer, client, resource, id);
}

static const struct _wl_pointer_gestures_interface pointer_gestures_interface = {
  gestures_get_swipe,
  gestures_get_pinch
};

static void
bind_pointer_gestures (struct wl_client *client,
                       void             *data,
                       guint32           version,
                       guint32           id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &_wl_pointer_gestures_interface, version, id);

  if (version != META__WL_POINTER_GESTURES_VERSION)
    {
      wl_resource_post_error (resource,
                              _WL_POINTER_GESTURES_ERROR_VERSION_MISMATCH,
                              "The client bound a non-supported version");
      return;
    }

  wl_resource_set_implementation (resource, &pointer_gestures_interface,
                                  NULL, NULL);
}

void
meta_wayland_pointer_gestures_init (MetaWaylandCompositor *compositor)
{
  wl_global_create (compositor->wayland_display,
                    &_wl_pointer_gestures_interface,
                    META__WL_POINTER_GESTURES_VERSION,
                    NULL, bind_pointer_gestures);
}
