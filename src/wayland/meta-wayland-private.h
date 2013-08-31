/*
 * Copyright (C) 2012 Intel Corporation
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

#ifndef META_WAYLAND_PRIVATE_H
#define META_WAYLAND_PRIVATE_H

#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>
#include <clutter/clutter.h>

#include <glib.h>
#include <cairo.h>

#include "window-private.h"
#include "meta-weston-launch.h"
#include <meta/meta-cursor-tracker.h>

typedef struct _MetaWaylandCompositor MetaWaylandCompositor;

typedef struct _MetaWaylandSeat MetaWaylandSeat;
typedef struct _MetaWaylandPointer MetaWaylandPointer;
typedef struct _MetaWaylandPointerGrab MetaWaylandPointerGrab;
typedef struct _MetaWaylandPointerGrabInterface MetaWaylandPointerGrabInterface;
typedef struct _MetaWaylandKeyboard MetaWaylandKeyboard;
typedef struct _MetaWaylandKeyboardGrab MetaWaylandKeyboardGrab;
typedef struct _MetaWaylandKeyboardGrabInterface MetaWaylandKeyboardGrabInterface;
typedef struct _MetaWaylandDataOffer MetaWaylandDataOffer;
typedef struct _MetaWaylandDataSource MetaWaylandDataSource;

typedef struct
{
  struct wl_resource *resource;
  struct wl_signal destroy_signal;
  struct wl_listener destroy_listener;

  int32_t width, height;
  uint32_t busy_count;
} MetaWaylandBuffer;

typedef struct
{
  MetaWaylandBuffer *buffer;
  struct wl_listener destroy_listener;
} MetaWaylandBufferReference;

typedef struct
{
  struct wl_resource *resource;
  cairo_region_t *region;
} MetaWaylandRegion;

struct _MetaWaylandSurface
{
  struct wl_resource *resource;
  MetaWaylandCompositor *compositor;
  guint32 xid;
  int x;
  int y;
  MetaWaylandBufferReference buffer_ref;
  MetaWindow *window;
  gboolean has_shell_surface;

  /* All the pending state, that wl_surface.commit will apply. */
  struct
  {
    /* wl_surface.attach */
    gboolean newly_attached;
    MetaWaylandBuffer *buffer;
    struct wl_listener buffer_destroy_listener;
    int32_t sx;
    int32_t sy;

    /* wl_surface.damage */
    cairo_region_t *damage;

    cairo_region_t *input_region;
    cairo_region_t *opaque_region;

    /* wl_surface.frame */
    struct wl_list frame_callback_list;
  } pending;
};

#ifndef HAVE_META_WAYLAND_SURFACE_TYPE
typedef struct _MetaWaylandSurface MetaWaylandSurface;
#endif

typedef struct
{
  MetaWaylandSurface *surface;
  struct wl_resource *resource;
  struct wl_listener surface_destroy_listener;
} MetaWaylandShellSurface;

typedef struct
{
  GSource source;
  GPollFD pfd;
  struct wl_display *display;
} WaylandEventSource;

typedef struct
{
  struct wl_list link;

  /* Pointer back to the compositor */
  MetaWaylandCompositor *compositor;

  struct wl_resource *resource;
} MetaWaylandFrameCallback;

struct _MetaWaylandCompositor
{
  struct wl_display *wayland_display;
  struct wl_event_loop *wayland_loop;
  GMainLoop *init_loop;
  ClutterActor *stage;
  GHashTable *outputs;
  GSource *wayland_event_source;
  GList *surfaces;
  struct wl_list frame_callbacks;

  int xwayland_display_index;
  char *xwayland_lockfile;
  int xwayland_abstract_fd;
  int xwayland_unix_fd;
  pid_t xwayland_pid;
  struct wl_client *xwayland_client;
  struct wl_resource *xserver_resource;

  MetaLauncher *launcher;
  int drm_fd;

  MetaWaylandSeat *seat;

  /* This surface is only used to keep drag of the implicit grab when
     synthesizing XEvents for Mutter */
  MetaWaylandSurface *implicit_grab_surface;
  /* Button that was pressed to initiate an implicit grab. The
     implicit grab will only be released when this button is
     released */
  guint32 implicit_grab_button;
};

struct _MetaWaylandPointerGrabInterface
{
  void (*focus) (MetaWaylandPointerGrab * grab,
                 MetaWaylandSurface * surface, wl_fixed_t x, wl_fixed_t y);
  void (*motion) (MetaWaylandPointerGrab * grab,
                  uint32_t time, wl_fixed_t x, wl_fixed_t y);
  void (*button) (MetaWaylandPointerGrab * grab,
                  uint32_t time, uint32_t button, uint32_t state);
};

struct _MetaWaylandPointerGrab
{
  const MetaWaylandPointerGrabInterface *interface;
  MetaWaylandPointer *pointer;
  MetaWaylandSurface *focus;
  wl_fixed_t x, y;
};

struct _MetaWaylandPointer
{
  struct wl_list resource_list;
  MetaWaylandSurface *focus;
  struct wl_resource *focus_resource;
  struct wl_listener focus_listener;
  guint32 focus_serial;
  struct wl_signal focus_signal;

  MetaWaylandPointerGrab *grab;
  MetaWaylandPointerGrab default_grab;
  wl_fixed_t grab_x, grab_y;
  guint32 grab_button;
  guint32 grab_serial;
  guint32 grab_time;

  wl_fixed_t x, y;
  MetaWaylandSurface *current;
  struct wl_listener current_listener;
  wl_fixed_t current_x, current_y;

  guint32 button_count;
};

struct _MetaWaylandKeyboardGrabInterface
{
  void (*key) (MetaWaylandKeyboardGrab * grab, uint32_t time,
               uint32_t key, uint32_t state);
  void (*modifiers) (MetaWaylandKeyboardGrab * grab, uint32_t serial,
                     uint32_t mods_depressed, uint32_t mods_latched,
                     uint32_t mods_locked, uint32_t group);
};

struct _MetaWaylandKeyboardGrab
{
  const MetaWaylandKeyboardGrabInterface *interface;
  MetaWaylandKeyboard *keyboard;
  MetaWaylandSurface *focus;
  uint32_t key;
};

typedef struct
{
  struct xkb_keymap *keymap;
  int keymap_fd;
  size_t keymap_size;
  char *keymap_area;
  xkb_mod_index_t shift_mod;
  xkb_mod_index_t caps_mod;
  xkb_mod_index_t ctrl_mod;
  xkb_mod_index_t alt_mod;
  xkb_mod_index_t mod2_mod;
  xkb_mod_index_t mod3_mod;
  xkb_mod_index_t super_mod;
  xkb_mod_index_t mod5_mod;
} MetaWaylandXkbInfo;

typedef struct
{
  uint32_t mods_depressed;
  uint32_t mods_latched;
  uint32_t mods_locked;
  uint32_t group;
} MetaWaylandXkbState;

struct _MetaWaylandKeyboard
{
  struct wl_list resource_list;
  MetaWaylandSurface *focus;
  struct wl_resource *focus_resource;
  struct wl_listener focus_listener;
  uint32_t focus_serial;
  struct wl_signal focus_signal;

  MetaWaylandKeyboardGrab *grab;
  MetaWaylandKeyboardGrab default_grab;
  uint32_t grab_key;
  uint32_t grab_serial;
  uint32_t grab_time;

  struct wl_array keys;

  MetaWaylandXkbState modifier_state;

  struct wl_display *display;

  struct xkb_context *xkb_context;
  struct xkb_state *xkb_state;
  gboolean is_evdev;

  MetaWaylandXkbInfo xkb_info;
  struct xkb_rule_names xkb_names;

  MetaWaylandKeyboardGrab input_method_grab;
  struct wl_resource *input_method_resource;
};

struct _MetaWaylandDataOffer
{
  struct wl_resource *resource;
  MetaWaylandDataSource *source;
  struct wl_listener source_destroy_listener;
};

struct _MetaWaylandDataSource
{
  struct wl_resource *resource;
  struct wl_array mime_types;

  void (*accept) (MetaWaylandDataSource * source,
                  uint32_t serial, const char *mime_type);
  void (*send) (MetaWaylandDataSource * source,
                const char *mime_type, int32_t fd);
  void (*cancel) (MetaWaylandDataSource * source);
};

struct _MetaWaylandSeat
{
  struct wl_list base_resource_list;
  struct wl_signal destroy_signal;

  uint32_t selection_serial;
  MetaWaylandDataSource *selection_data_source;
  struct wl_listener selection_data_source_listener;
  struct wl_signal selection_signal;

  struct wl_list drag_resource_list;
  struct wl_client *drag_client;
  MetaWaylandDataSource *drag_data_source;
  struct wl_listener drag_data_source_listener;
  MetaWaylandSurface *drag_focus;
  struct wl_resource *drag_focus_resource;
  struct wl_listener drag_focus_listener;
  MetaWaylandPointerGrab drag_grab;
  MetaWaylandSurface *drag_surface;
  struct wl_listener drag_icon_listener;
  struct wl_signal drag_icon_signal;

  MetaWaylandPointer pointer;
  MetaWaylandKeyboard keyboard;

  struct wl_display *display;

  MetaCursorTracker *cursor_tracker;
  MetaWaylandSurface *sprite;
  int hotspot_x, hotspot_y;
  struct wl_listener sprite_destroy_listener;

  ClutterActor *current_stage;
};

void                    meta_wayland_init                       (void);
void                    meta_wayland_finalize                   (void);

/* We maintain a singleton MetaWaylandCompositor which can be got at via this
 * API after meta_wayland_init() has been called. */
MetaWaylandCompositor  *meta_wayland_compositor_get_default     (void);

void                    meta_wayland_compositor_repick          (MetaWaylandCompositor *compositor);

void                    meta_wayland_compositor_set_input_focus (MetaWaylandCompositor *compositor,
                                                                 MetaWindow            *window);

MetaLauncher           *meta_wayland_compositor_get_launcher    (MetaWaylandCompositor *compositor);

void                    meta_wayland_surface_free               (MetaWaylandSurface    *surface);

#endif /* META_WAYLAND_PRIVATE_H */
