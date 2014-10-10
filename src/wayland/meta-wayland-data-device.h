/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef META_WAYLAND_DATA_DEVICE_H
#define META_WAYLAND_DATA_DEVICE_H

#include <wayland-server.h>

#include "meta-wayland-types.h"

typedef struct _MetaWaylandDragGrab MetaWaylandDragGrab;
typedef struct _MetaWaylandDataSourceFuncs MetaWaylandDataSourceFuncs;

struct _MetaWaylandDataDevice
{
  uint32_t selection_serial;
  MetaWaylandDataSource *selection_data_source;
  MetaWaylandDataSource *dnd_data_source;
  struct wl_listener selection_data_source_listener;
  struct wl_list resource_list;
  MetaWaylandDragGrab *current_grab;

  struct wl_signal selection_ownership_signal;
  struct wl_signal dnd_ownership_signal;
};

struct _MetaWaylandDataSourceFuncs
{
  void (* send)    (MetaWaylandDataSource *source,
                    const gchar           *mime_type,
                    gint                   fd);
  void (* target)  (MetaWaylandDataSource *source,
                    const gchar           *mime_type);
  void (* cancel)  (MetaWaylandDataSource *source);
};

struct _MetaWaylandDataSource
{
  MetaWaylandDataSourceFuncs funcs;
  struct wl_resource *resource;
  struct wl_array mime_types;
  gpointer user_data;
  gboolean has_target;
};

void meta_wayland_data_device_manager_init (MetaWaylandCompositor *compositor);

void meta_wayland_data_device_init (MetaWaylandDataDevice *data_device);

void meta_wayland_data_device_set_keyboard_focus (MetaWaylandDataDevice *data_device);

gboolean meta_wayland_data_device_is_dnd_surface (MetaWaylandDataDevice *data_device,
                                                  MetaWaylandSurface    *surface);
void meta_wayland_data_device_update_dnd_surface (MetaWaylandDataDevice *data_device);

void meta_wayland_data_device_set_dnd_source     (MetaWaylandDataDevice *data_device,
                                                  MetaWaylandDataSource *source);
void meta_wayland_data_device_set_selection      (MetaWaylandDataDevice *data_device,
                                                  MetaWaylandDataSource *source,
                                                  guint32 serial);
MetaWaylandDataSource *
         meta_wayland_data_source_new            (const MetaWaylandDataSourceFuncs *funcs,
                                                  struct wl_resource               *resource,
                                                  gpointer                          user_data);
void     meta_wayland_data_source_free           (MetaWaylandDataSource *source);

gboolean meta_wayland_data_source_add_mime_type  (MetaWaylandDataSource *source,
                                                  const gchar           *mime_type);

gboolean meta_wayland_data_source_has_mime_type  (const MetaWaylandDataSource *source,
                                                  const gchar                 *mime_type);

void     meta_wayland_data_source_send           (MetaWaylandDataSource *source,
                                                  const gchar           *mime_type,
                                                  gint                   fd);

#endif /* META_WAYLAND_DATA_DEVICE_H */
