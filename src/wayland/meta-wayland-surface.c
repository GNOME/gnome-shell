/*
 * Wayland Support
 *
 * Copyright (C) 2012,2013 Intel Corporation
 *               2013 Red Hat, Inc.
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

#include "meta-wayland-surface.h"

#include <clutter/clutter.h>
#include <clutter/wayland/clutter-wayland-compositor.h>
#include <clutter/wayland/clutter-wayland-surface.h>
#include <cogl/cogl-wayland-server.h>

#include <gobject/gvaluecollector.h>
#include <wayland-server.h>

#include "meta-wayland-private.h"
#include "meta-xwayland-private.h"
#include "meta-wayland-buffer.h"
#include "meta-wayland-region.h"
#include "meta-wayland-seat.h"
#include "meta-wayland-keyboard.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-data-device.h"
#include "meta-wayland-outputs.h"
#include "meta-wayland-xdg-shell.h"
#include "meta-wayland-wl-shell.h"
#include "meta-wayland-gtk-shell.h"

#include "meta-cursor-tracker-private.h"
#include "display-private.h"
#include "window-private.h"
#include "meta-window-wayland.h"

#include "compositor/region-utils.h"
#include "compositor/meta-shaped-texture-private.h"

#include "meta-surface-actor.h"
#include "meta-surface-actor-wayland.h"
#include "meta-xwayland-private.h"

enum {
  PENDING_STATE_SIGNAL_APPLIED,

  PENDING_STATE_SIGNAL_LAST_SIGNAL
};

enum
{
  SURFACE_ROLE_PROP_0,

  SURFACE_ROLE_PROP_SURFACE,
};

static guint pending_state_signals[PENDING_STATE_SIGNAL_LAST_SIGNAL];

typedef struct _MetaWaylandSurfaceRolePrivate
{
  MetaWaylandSurface *surface;
} MetaWaylandSurfaceRolePrivate;

typedef enum
{
  META_WAYLAND_SUBSURFACE_PLACEMENT_ABOVE,
  META_WAYLAND_SUBSURFACE_PLACEMENT_BELOW
} MetaWaylandSubsurfacePlacement;

typedef struct
{
  MetaWaylandSubsurfacePlacement placement;
  MetaWaylandSurface *sibling;
  struct wl_listener sibling_destroy_listener;
} MetaWaylandSubsurfacePlacementOp;

G_DEFINE_TYPE (MetaWaylandSurface, meta_wayland_surface, G_TYPE_OBJECT);

G_DEFINE_TYPE_WITH_PRIVATE (MetaWaylandSurfaceRole,
                            meta_wayland_surface_role,
                            G_TYPE_OBJECT);

G_DEFINE_TYPE (MetaWaylandSurfaceRoleActorSurface,
               meta_wayland_surface_role_actor_surface,
               META_TYPE_WAYLAND_SURFACE_ROLE);

G_DEFINE_TYPE (MetaWaylandShellSurface,
               meta_wayland_shell_surface,
               META_TYPE_WAYLAND_SURFACE_ROLE_ACTOR_SURFACE);

G_DEFINE_TYPE (MetaWaylandPendingState,
               meta_wayland_pending_state,
               G_TYPE_OBJECT);

struct _MetaWaylandSurfaceRoleSubsurface
{
  MetaWaylandSurfaceRoleActorSurface parent;
};

G_DEFINE_TYPE (MetaWaylandSurfaceRoleSubsurface,
               meta_wayland_surface_role_subsurface,
               META_TYPE_WAYLAND_SURFACE_ROLE_ACTOR_SURFACE);

struct _MetaWaylandSurfaceRoleDND
{
  MetaWaylandSurfaceRole parent;
};

G_DEFINE_TYPE (MetaWaylandSurfaceRoleDND,
               meta_wayland_surface_role_dnd,
               META_TYPE_WAYLAND_SURFACE_ROLE);

enum {
  SURFACE_DESTROY,
  SURFACE_UNMAPPED,
  SURFACE_CONFIGURE,
  SURFACE_SHORTCUTS_INHIBITED,
  SURFACE_SHORTCUTS_RESTORED,
  N_SURFACE_SIGNALS
};

guint surface_signals[N_SURFACE_SIGNALS] = { 0 };

static void
meta_wayland_surface_role_assigned (MetaWaylandSurfaceRole *surface_role);

static void
meta_wayland_surface_role_pre_commit (MetaWaylandSurfaceRole  *surface_role,
                                      MetaWaylandPendingState *pending);

static void
meta_wayland_surface_role_commit (MetaWaylandSurfaceRole  *surface_role,
                                  MetaWaylandPendingState *pending);

static gboolean
meta_wayland_surface_role_is_on_logical_monitor (MetaWaylandSurfaceRole *surface_role,
                                                 MetaLogicalMonitor     *logical_monitor);

static MetaWaylandSurface *
meta_wayland_surface_role_get_toplevel (MetaWaylandSurfaceRole *surface_role);

static void
meta_wayland_shell_surface_configure (MetaWaylandShellSurface *shell_surface,
                                      int                      new_x,
                                      int                      new_y,
                                      int                      new_width,
                                      int                      new_height,
                                      MetaWaylandSerial       *sent_serial);

static void
meta_wayland_shell_surface_ping (MetaWaylandShellSurface *shell_surface,
                                 uint32_t                 serial);

static void
meta_wayland_shell_surface_close (MetaWaylandShellSurface *shell_surface);

static void
meta_wayland_shell_surface_managed (MetaWaylandShellSurface *shell_surface,
                                    MetaWindow              *window);

static void
surface_actor_mapped_notify (MetaSurfaceActorWayland *surface_actor,
                             GParamSpec              *pspec,
                             MetaWaylandSurface      *surface);
static void
surface_actor_allocation_notify (MetaSurfaceActorWayland *surface_actor,
                                 GParamSpec              *pspec,
                                 MetaWaylandSurface      *surface);
static void
surface_actor_position_notify (MetaSurfaceActorWayland *surface_actor,
                               GParamSpec              *pspec,
                               MetaWaylandSurface      *surface);
static void
window_position_changed (MetaWindow         *window,
                         MetaWaylandSurface *surface);

static void
unset_param_value (GParameter *param)
{
  g_value_unset (&param->value);
}

static GArray *
role_assignment_valist_to_params (GType       role_type,
                                  const char *first_property_name,
                                  va_list     var_args)
{
  GObjectClass *object_class;
  const char *property_name = first_property_name;
  GArray *params;

  object_class = g_type_class_ref (role_type);

  params = g_array_new (FALSE, FALSE, sizeof (GParameter));
  g_array_set_clear_func (params, (GDestroyNotify) unset_param_value);

  while (property_name)
    {
      GParameter param = {
        .name = property_name,
        .value = G_VALUE_INIT
      };
      GParamSpec *pspec;
      GType ptype;
      gchar *error = NULL;

      pspec = g_object_class_find_property (object_class,
                                            property_name);
      g_assert (pspec);

      ptype = G_PARAM_SPEC_VALUE_TYPE (pspec);
      G_VALUE_COLLECT_INIT (&param.value, ptype, var_args, 0, &error);
      g_assert (!error);

      g_array_append_val (params, param);

      property_name = va_arg (var_args, const char *);
    }

  g_type_class_unref (object_class);

  return params;
}

gboolean
meta_wayland_surface_assign_role (MetaWaylandSurface *surface,
                                  GType               role_type,
                                  const char         *first_property_name,
                                  ...)
{
  va_list var_args;

  if (!surface->role)
    {
      if (first_property_name)
        {
          GArray *params;
          GParameter param;

          va_start (var_args, first_property_name);
          params = role_assignment_valist_to_params (role_type,
                                                     first_property_name,
                                                     var_args);
          va_end (var_args);

          param = (GParameter) {
            .name = "surface",
            .value = G_VALUE_INIT
          };
          g_value_init (&param.value, META_TYPE_WAYLAND_SURFACE);
          g_value_set_object (&param.value, surface);
          g_array_append_val (params, param);

          surface->role = g_object_newv (role_type, params->len,
                                         (GParameter *) params->data);

          g_array_unref (params);
        }
      else
        {
          surface->role = g_object_new (role_type, "surface", surface, NULL);
        }

      meta_wayland_surface_role_assigned (surface->role);

      /* Release the use count held on behalf of the just assigned role. */
      if (surface->unassigned.buffer)
        {
          meta_wayland_surface_unref_buffer_use_count (surface);
          g_clear_object (&surface->unassigned.buffer);
        }

      return TRUE;
    }
  else if (G_OBJECT_TYPE (surface->role) != role_type)
    {
      return FALSE;
    }
  else
    {
      va_start (var_args, first_property_name);
      g_object_set_valist (G_OBJECT (surface->role),
                           first_property_name, var_args);
      va_end (var_args);

      meta_wayland_surface_role_assigned (surface->role);

      return TRUE;
    }
}

static void
surface_process_damage (MetaWaylandSurface *surface,
                        cairo_region_t     *surface_region,
                        cairo_region_t     *buffer_region)
{
  MetaWaylandBuffer *buffer = surface->buffer_ref.buffer;
  unsigned int buffer_width;
  unsigned int buffer_height;
  cairo_rectangle_int_t surface_rect;
  cairo_region_t *scaled_region;
  int i, n_rectangles;

  /* If the client destroyed the buffer it attached before committing, but
   * still posted damage, or posted damage without any buffer, don't try to
   * process it on the non-existing buffer.
   */
  if (!buffer)
    return;

  /* Intersect the damage region with the surface region before scaling in
   * order to avoid integer overflow when scaling a damage region is too large
   * (for example INT32_MAX which mesa passes). */
  buffer_width = cogl_texture_get_width (buffer->texture);
  buffer_height = cogl_texture_get_height (buffer->texture);
  surface_rect = (cairo_rectangle_int_t) {
    .width = buffer_width / surface->scale,
    .height = buffer_height / surface->scale,
  };
  cairo_region_intersect_rectangle (surface_region, &surface_rect);

  /* The damage region must be in the same coordinate space as the buffer,
   * i.e. scaled with surface->scale. */
  scaled_region = meta_region_scale (surface_region, surface->scale);

  /* Now add the buffer damage on top of the scaled damage region, as buffer
   * damage is already in that scale. */
  cairo_region_union (scaled_region, buffer_region);

  /* First update the buffer. */
  meta_wayland_buffer_process_damage (buffer, scaled_region);

  /* Now damage the actor. The actor expects damage in the unscaled texture
   * coordinate space, i.e. same as the buffer. */
  /* XXX: Should this be a signal / callback on MetaWaylandBuffer instead? */
  n_rectangles = cairo_region_num_rectangles (scaled_region);
  for (i = 0; i < n_rectangles; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (scaled_region, i, &rect);

      meta_surface_actor_process_damage (surface->surface_actor,
                                         rect.x, rect.y,
                                         rect.width, rect.height);
    }

  cairo_region_destroy (scaled_region);
}

void
meta_wayland_surface_queue_pending_state_frame_callbacks (MetaWaylandSurface      *surface,
                                                          MetaWaylandPendingState *pending)
{
  wl_list_insert_list (&surface->compositor->frame_callbacks,
                       &pending->frame_callback_list);
  wl_list_init (&pending->frame_callback_list);
}

static void
dnd_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                    MetaWaylandPendingState *pending)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  meta_wayland_surface_queue_pending_state_frame_callbacks (surface, pending);
}

void
meta_wayland_surface_calculate_window_geometry (MetaWaylandSurface *surface,
                                                MetaRectangle      *total_geometry,
                                                float               parent_x,
                                                float               parent_y)
{
  MetaSurfaceActorWayland *surface_actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);
  MetaRectangle subsurface_rect;
  MetaRectangle geom;
  GList *l;

  /* Unmapped surfaces don't count. */
  if (!CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (surface_actor)))
    return;

  if (!surface->buffer_ref.buffer)
    return;

  meta_surface_actor_wayland_get_subsurface_rect (surface_actor,
                                                  &subsurface_rect);

  geom.x = parent_x + subsurface_rect.x;
  geom.y = parent_x + subsurface_rect.y;
  geom.width = subsurface_rect.width;
  geom.height = subsurface_rect.height;

  meta_rectangle_union (total_geometry, &geom, total_geometry);

  for (l = surface->subsurfaces; l != NULL; l = l->next)
    {
      MetaWaylandSurface *subsurface = l->data;
      meta_wayland_surface_calculate_window_geometry (subsurface,
                                                      total_geometry,
                                                      subsurface_rect.x,
                                                      subsurface_rect.y);
    }
}

void
meta_wayland_surface_destroy_window (MetaWaylandSurface *surface)
{
  if (surface->window)
    {
      MetaDisplay *display = meta_get_display ();
      guint32 timestamp = meta_display_get_current_time_roundtrip (display);

      meta_window_unmanage (surface->window, timestamp);
    }

  g_assert (surface->window == NULL);
}

MetaWaylandBuffer *
meta_wayland_surface_get_buffer (MetaWaylandSurface *surface)
{
  return surface->buffer_ref.buffer;
}

void
meta_wayland_surface_ref_buffer_use_count (MetaWaylandSurface *surface)
{
  g_return_if_fail (surface->buffer_ref.buffer);
  g_warn_if_fail (surface->buffer_ref.buffer->resource);

  surface->buffer_ref.use_count++;
}

void
meta_wayland_surface_unref_buffer_use_count (MetaWaylandSurface *surface)
{
  MetaWaylandBuffer *buffer = surface->buffer_ref.buffer;

  g_return_if_fail (surface->buffer_ref.use_count != 0);

  surface->buffer_ref.use_count--;

  g_return_if_fail (buffer);

  if (surface->buffer_ref.use_count == 0 && buffer->resource)
    wl_buffer_send_release (buffer->resource);
}

static void
queue_surface_actor_frame_callbacks (MetaWaylandSurface      *surface,
                                     MetaWaylandPendingState *pending)
{
  MetaSurfaceActorWayland *surface_actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  meta_surface_actor_wayland_add_frame_callbacks (surface_actor,
                                                  &pending->frame_callback_list);
  wl_list_init (&pending->frame_callback_list);
}

static void
pending_buffer_resource_destroyed (MetaWaylandBuffer       *buffer,
                                   MetaWaylandPendingState *pending)
{
  g_signal_handler_disconnect (buffer, pending->buffer_destroy_handler_id);
  pending->buffer = NULL;
}

static void
pending_state_init (MetaWaylandPendingState *state)
{
  state->newly_attached = FALSE;
  state->buffer = NULL;
  state->dx = 0;
  state->dy = 0;
  state->scale = 0;

  state->input_region = NULL;
  state->input_region_set = FALSE;
  state->opaque_region = NULL;
  state->opaque_region_set = FALSE;

  state->surface_damage = cairo_region_create ();
  state->buffer_damage = cairo_region_create ();
  wl_list_init (&state->frame_callback_list);

  state->has_new_geometry = FALSE;
  state->has_new_min_size = FALSE;
  state->has_new_max_size = FALSE;
}

static void
pending_state_destroy (MetaWaylandPendingState *state)
{
  MetaWaylandFrameCallback *cb, *next;

  g_clear_pointer (&state->surface_damage, cairo_region_destroy);
  g_clear_pointer (&state->buffer_damage, cairo_region_destroy);
  g_clear_pointer (&state->input_region, cairo_region_destroy);
  g_clear_pointer (&state->opaque_region, cairo_region_destroy);

  if (state->buffer)
    g_signal_handler_disconnect (state->buffer,
                                 state->buffer_destroy_handler_id);
  wl_list_for_each_safe (cb, next, &state->frame_callback_list, link)
    wl_resource_destroy (cb->resource);
}

static void
pending_state_reset (MetaWaylandPendingState *state)
{
  pending_state_destroy (state);
  pending_state_init (state);
}

static void
move_pending_state (MetaWaylandPendingState *from,
                    MetaWaylandPendingState *to)
{
  if (from->buffer)
    g_signal_handler_disconnect (from->buffer, from->buffer_destroy_handler_id);

  to->newly_attached = from->newly_attached;
  to->buffer = from->buffer;
  to->dx = from->dx;
  to->dy = from->dy;
  to->scale = from->scale;
  to->surface_damage = from->surface_damage;
  to->buffer_damage = from->buffer_damage;
  to->input_region = from->input_region;
  to->input_region_set = from->input_region_set;
  to->opaque_region = from->opaque_region;
  to->opaque_region_set = from->opaque_region_set;
  to->new_geometry = from->new_geometry;
  to->has_new_geometry = from->has_new_geometry;
  to->has_new_min_size = from->has_new_min_size;
  to->new_min_width = from->new_min_width;
  to->new_min_height = from->new_min_height;
  to->has_new_max_size = from->has_new_max_size;
  to->new_max_width = from->new_max_width;
  to->new_max_height = from->new_max_height;

  wl_list_init (&to->frame_callback_list);
  wl_list_insert_list (&to->frame_callback_list, &from->frame_callback_list);

  if (to->buffer)
    {
      to->buffer_destroy_handler_id =
        g_signal_connect (to->buffer, "resource-destroyed",
                          G_CALLBACK (pending_buffer_resource_destroyed),
                          to);
    }

  pending_state_init (from);
}

static void
meta_wayland_pending_state_finalize (GObject *object)
{
  MetaWaylandPendingState *state = META_WAYLAND_PENDING_STATE (object);

  pending_state_destroy (state);

  G_OBJECT_CLASS (meta_wayland_pending_state_parent_class)->finalize (object);
}

static void
meta_wayland_pending_state_init (MetaWaylandPendingState *state)
{
  pending_state_init (state);
}

static void
meta_wayland_pending_state_class_init (MetaWaylandPendingStateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_wayland_pending_state_finalize;

  pending_state_signals[PENDING_STATE_SIGNAL_APPLIED] =
    g_signal_new ("applied",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
subsurface_role_commit (MetaWaylandSurfaceRole  *surface_role,
                        MetaWaylandPendingState *pending)
{
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaSurfaceActorWayland *surface_actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_surface_role_subsurface_parent_class);
  surface_role_class->commit (surface_role, pending);

  if (surface->buffer_ref.buffer != NULL)
    clutter_actor_show (CLUTTER_ACTOR (surface_actor));
  else
    clutter_actor_hide (CLUTTER_ACTOR (surface_actor));
}

static MetaWaylandSurface *
subsurface_role_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurface *parent = surface->sub.parent;

  if (parent && parent->role)
    return meta_wayland_surface_role_get_toplevel (parent->role);
  else
    return NULL;
}

/* A non-subsurface is always desynchronized.
 *
 * A subsurface is effectively synchronized if either its parent is
 * synchronized or itself is in synchronized mode. */
static gboolean
is_surface_effectively_synchronized (MetaWaylandSurface *surface)
{
  if (surface->wl_subsurface == NULL)
    {
      return FALSE;
    }
  else
    {
      if (surface->sub.synchronous)
        return TRUE;
      else
        return is_surface_effectively_synchronized (surface->sub.parent);
    }
}

static void
apply_pending_state (MetaWaylandSurface      *surface,
                     MetaWaylandPendingState *pending);

static void
parent_surface_state_applied (gpointer data, gpointer user_data)
{
  MetaWaylandSurface *surface = data;

  if (surface->sub.pending_pos)
    {
      surface->sub.x = surface->sub.pending_x;
      surface->sub.y = surface->sub.pending_y;
      surface->sub.pending_pos = FALSE;
    }

  if (surface->sub.pending_placement_ops)
    {
      GSList *it;
      MetaWaylandSurface *parent = surface->sub.parent;
      ClutterActor *parent_actor =
        clutter_actor_get_parent (CLUTTER_ACTOR (parent->surface_actor));
      ClutterActor *surface_actor = CLUTTER_ACTOR (surface->surface_actor);

      for (it = surface->sub.pending_placement_ops; it; it = it->next)
        {
          MetaWaylandSubsurfacePlacementOp *op = it->data;
          ClutterActor *sibling_actor;

          if (!op->sibling)
            {
              g_slice_free (MetaWaylandSubsurfacePlacementOp, op);
              continue;
            }

          sibling_actor = CLUTTER_ACTOR (op->sibling->surface_actor);

          switch (op->placement)
            {
            case META_WAYLAND_SUBSURFACE_PLACEMENT_ABOVE:
              clutter_actor_set_child_above_sibling (parent_actor,
                                                     surface_actor,
                                                     sibling_actor);
              break;
            case META_WAYLAND_SUBSURFACE_PLACEMENT_BELOW:
              clutter_actor_set_child_below_sibling (parent_actor,
                                                     surface_actor,
                                                     sibling_actor);
              break;
            }

          wl_list_remove (&op->sibling_destroy_listener.link);
          g_slice_free (MetaWaylandSubsurfacePlacementOp, op);
        }

      g_slist_free (surface->sub.pending_placement_ops);
      surface->sub.pending_placement_ops = NULL;
    }

  if (is_surface_effectively_synchronized (surface))
    apply_pending_state (surface, surface->sub.pending);

  meta_surface_actor_wayland_sync_subsurface_state (
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor));
}

static void
apply_pending_state (MetaWaylandSurface      *surface,
                     MetaWaylandPendingState *pending)
{
  if (surface->role)
    {
      meta_wayland_surface_role_pre_commit (surface->role, pending);
    }
  else
    {
      if (pending->newly_attached && surface->unassigned.buffer)
        {
          meta_wayland_surface_unref_buffer_use_count (surface);
          g_clear_object (&surface->unassigned.buffer);
        }
    }

  if (pending->newly_attached)
    {
      gboolean switched_buffer;

      if (!surface->buffer_ref.buffer && surface->window)
        meta_window_queue (surface->window, META_QUEUE_CALC_SHOWING);

      /* Always release any previously held buffer. If the buffer held is same
       * as the newly attached buffer, we still need to release it here, because
       * wl_surface.attach+commit and wl_buffer.release on the attached buffer
       * is symmetric.
       */
      if (surface->buffer_held)
        meta_wayland_surface_unref_buffer_use_count (surface);

      switched_buffer = g_set_object (&surface->buffer_ref.buffer,
                                      pending->buffer);

      if (pending->buffer)
        meta_wayland_surface_ref_buffer_use_count (surface);

      if (pending->buffer)
        {
          GError *error = NULL;

          if (!meta_wayland_buffer_attach (pending->buffer, &error))
            {
              g_warning ("Could not import pending buffer: %s", error->message);
              wl_resource_post_error (surface->resource, WL_DISPLAY_ERROR_NO_MEMORY,
                                      "Failed to create a texture for surface %i: %s",
                                      wl_resource_get_id (surface->resource),
                                      error->message);
              g_error_free (error);
              goto cleanup;
            }

          if (switched_buffer)
            {
              MetaShapedTexture *stex;
              CoglTexture *texture;
              CoglSnippet *snippet;
              gboolean is_y_inverted;

              stex = meta_surface_actor_get_texture (surface->surface_actor);
              texture = meta_wayland_buffer_get_texture (pending->buffer);
              snippet = meta_wayland_buffer_create_snippet (pending->buffer);
              is_y_inverted = meta_wayland_buffer_is_y_inverted (pending->buffer);

              meta_shaped_texture_set_texture (stex, texture);
              meta_shaped_texture_set_snippet (stex, snippet);
              meta_shaped_texture_set_is_y_inverted (stex, is_y_inverted);
              g_clear_pointer (&snippet, cogl_object_unref);
            }
        }

      /* If the newly attached buffer is going to be accessed directly without
       * making a copy, such as an EGL buffer, mark it as in-use don't release
       * it until is replaced by a subsequent wl_surface.commit or when the
       * wl_surface is destroyed.
       */
      surface->buffer_held = (pending->buffer &&
                              !wl_shm_buffer_get (pending->buffer->resource));
    }

  if (pending->scale > 0)
    surface->scale = pending->scale;

  if (!cairo_region_is_empty (pending->surface_damage) ||
      !cairo_region_is_empty (pending->buffer_damage))
    surface_process_damage (surface,
                            pending->surface_damage,
                            pending->buffer_damage);

  surface->offset_x += pending->dx;
  surface->offset_y += pending->dy;

  if (pending->opaque_region_set)
    {
      if (surface->opaque_region)
        cairo_region_destroy (surface->opaque_region);
      if (pending->opaque_region)
        surface->opaque_region = cairo_region_reference (pending->opaque_region);
      else
        surface->opaque_region = NULL;
    }

  if (pending->input_region_set)
    {
      if (surface->input_region)
        cairo_region_destroy (surface->input_region);
      if (pending->input_region)
        surface->input_region = cairo_region_reference (pending->input_region);
      else
        surface->input_region = NULL;
    }

  if (surface->role)
    {
      meta_wayland_surface_role_commit (surface->role, pending);
      g_assert (wl_list_empty (&pending->frame_callback_list));
    }
  else
    {
      /* Since there is no role assigned to the surface yet, keep frame
       * callbacks queued until a role is assigned and we know how
       * the surface will be drawn.
       */
      wl_list_insert_list (&surface->pending_frame_callback_list,
                           &pending->frame_callback_list);
      wl_list_init (&pending->frame_callback_list);

      if (pending->newly_attached)
        {
          /* The need to keep the wl_buffer from being released depends on what
           * role the surface is given. That means we need to also keep a use
           * count for wl_buffer's that are used by unassigned wl_surface's.
           */
          g_set_object (&surface->unassigned.buffer, surface->buffer_ref.buffer);
          if (surface->unassigned.buffer)
            meta_wayland_surface_ref_buffer_use_count (surface);
        }
    }

cleanup:
  /* If we have a buffer that we are not using, decrease the use count so it may
   * be released if no-one else has a use-reference to it.
   */
  if (pending->newly_attached &&
      !surface->buffer_held && surface->buffer_ref.buffer)
    meta_wayland_surface_unref_buffer_use_count (surface);

  g_signal_emit (pending,
                 pending_state_signals[PENDING_STATE_SIGNAL_APPLIED],
                 0);

  pending_state_reset (pending);

  g_list_foreach (surface->subsurfaces, parent_surface_state_applied, NULL);
}

static void
meta_wayland_surface_commit (MetaWaylandSurface *surface)
{
  /*
   * If this is a sub-surface and it is in effective synchronous mode, only
   * cache the pending surface state until either one of the following two
   * scenarios happens:
   *  1) Its parent surface gets its state applied.
   *  2) Its mode changes from synchronized to desynchronized and its parent
   *     surface is in effective desynchronized mode.
   */
  if (is_surface_effectively_synchronized (surface))
    move_pending_state (surface->pending, surface->sub.pending);
  else
    apply_pending_state (surface, surface->pending);
}

static void
wl_surface_destroy (struct wl_client *client,
                    struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
wl_surface_attach (struct wl_client *client,
                   struct wl_resource *surface_resource,
                   struct wl_resource *buffer_resource,
                   gint32 dx, gint32 dy)
{
  MetaWaylandSurface *surface =
    wl_resource_get_user_data (surface_resource);
  MetaWaylandBuffer *buffer;

  /* X11 unmanaged window */
  if (!surface)
    return;

  if (buffer_resource)
    buffer = meta_wayland_buffer_from_resource (buffer_resource);
  else
    buffer = NULL;

  if (surface->pending->buffer)
    {
      g_signal_handler_disconnect (surface->pending->buffer,
                                   surface->pending->buffer_destroy_handler_id);
    }

  surface->pending->newly_attached = TRUE;
  surface->pending->buffer = buffer;
  surface->pending->dx = dx;
  surface->pending->dy = dy;

  if (buffer)
    {
      surface->pending->buffer_destroy_handler_id =
        g_signal_connect (buffer, "resource-destroyed",
                          G_CALLBACK (pending_buffer_resource_destroyed),
                          surface->pending);
    }
}

static void
wl_surface_damage (struct wl_client   *client,
                   struct wl_resource *surface_resource,
                   int32_t             x,
                   int32_t             y,
                   int32_t             width,
                   int32_t             height)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  cairo_rectangle_int_t rectangle;

  /* X11 unmanaged window */
  if (!surface)
    return;

  rectangle = (cairo_rectangle_int_t) {
    .x = x,
    .y = y,
    .width = width,
    .height = height
  };
  cairo_region_union_rectangle (surface->pending->surface_damage, &rectangle);
}

static void
destroy_frame_callback (struct wl_resource *callback_resource)
{
  MetaWaylandFrameCallback *callback =
    wl_resource_get_user_data (callback_resource);

  wl_list_remove (&callback->link);
  g_slice_free (MetaWaylandFrameCallback, callback);
}

static void
wl_surface_frame (struct wl_client *client,
                  struct wl_resource *surface_resource,
                  guint32 callback_id)
{
  MetaWaylandFrameCallback *callback;
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  callback = g_slice_new0 (MetaWaylandFrameCallback);
  callback->surface = surface;
  callback->resource = wl_resource_create (client, &wl_callback_interface, META_WL_CALLBACK_VERSION, callback_id);
  wl_resource_set_implementation (callback->resource, NULL, callback, destroy_frame_callback);

  wl_list_insert (surface->pending->frame_callback_list.prev, &callback->link);
}

static void
wl_surface_set_opaque_region (struct wl_client *client,
                              struct wl_resource *surface_resource,
                              struct wl_resource *region_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  g_clear_pointer (&surface->pending->opaque_region, cairo_region_destroy);
  if (region_resource)
    {
      MetaWaylandRegion *region = wl_resource_get_user_data (region_resource);
      cairo_region_t *cr_region = meta_wayland_region_peek_cairo_region (region);
      surface->pending->opaque_region = cairo_region_copy (cr_region);
    }
  surface->pending->opaque_region_set = TRUE;
}

static void
wl_surface_set_input_region (struct wl_client *client,
                             struct wl_resource *surface_resource,
                             struct wl_resource *region_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  g_clear_pointer (&surface->pending->input_region, cairo_region_destroy);
  if (region_resource)
    {
      MetaWaylandRegion *region = wl_resource_get_user_data (region_resource);
      cairo_region_t *cr_region = meta_wayland_region_peek_cairo_region (region);
      surface->pending->input_region = cairo_region_copy (cr_region);
    }
  surface->pending->input_region_set = TRUE;
}

static void
wl_surface_commit (struct wl_client *client,
                   struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  /* X11 unmanaged window */
  if (!surface)
    return;

  meta_wayland_surface_commit (surface);
}

static void
wl_surface_set_buffer_transform (struct wl_client *client,
                                 struct wl_resource *resource,
                                 int32_t transform)
{
  g_warning ("TODO: support set_buffer_transform request");
}

static void
wl_surface_set_buffer_scale (struct wl_client *client,
                             struct wl_resource *resource,
                             int scale)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  if (scale > 0)
    surface->pending->scale = scale;
  else
    g_warning ("Trying to set invalid buffer_scale of %d\n", scale);
}

static void
wl_surface_damage_buffer (struct wl_client   *client,
                          struct wl_resource *surface_resource,
                          int32_t             x,
                          int32_t             y,
                          int32_t             width,
                          int32_t             height)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  cairo_rectangle_int_t rectangle;

  /* X11 unmanaged window */
  if (!surface)
    return;

  rectangle = (cairo_rectangle_int_t) {
    .x = x,
    .y = y,
    .width = width,
    .height = height
  };
  cairo_region_union_rectangle (surface->pending->buffer_damage, &rectangle);
}

static const struct wl_surface_interface meta_wayland_wl_surface_interface = {
  wl_surface_destroy,
  wl_surface_attach,
  wl_surface_damage,
  wl_surface_frame,
  wl_surface_set_opaque_region,
  wl_surface_set_input_region,
  wl_surface_commit,
  wl_surface_set_buffer_transform,
  wl_surface_set_buffer_scale,
  wl_surface_damage_buffer,
};

static gboolean
surface_should_be_reactive (MetaWaylandSurface *surface)
{
  /* If we have a toplevel window, we should be reactive */
  if (surface->window)
    return TRUE;

  /* If we're a subsurface, we should be reactive */
  if (surface->wl_subsurface)
    return TRUE;

  return FALSE;
}

static void
sync_reactive (MetaWaylandSurface *surface)
{
  clutter_actor_set_reactive (CLUTTER_ACTOR (surface->surface_actor),
                              surface_should_be_reactive (surface));
}

static void
sync_drag_dest_funcs (MetaWaylandSurface *surface)
{
  if (surface->window &&
      surface->window->client_type == META_WINDOW_CLIENT_TYPE_X11)
    surface->dnd.funcs = meta_xwayland_selection_get_drag_dest_funcs ();
  else
    surface->dnd.funcs = meta_wayland_data_device_get_drag_dest_funcs ();
}

static void
surface_entered_output (MetaWaylandSurface *surface,
                        MetaWaylandOutput *wayland_output)
{
  GList *iter;
  struct wl_resource *resource;

  for (iter = wayland_output->resources; iter != NULL; iter = iter->next)
    {
      resource = iter->data;

      if (wl_resource_get_client (resource) !=
          wl_resource_get_client (surface->resource))
        continue;

      wl_surface_send_enter (surface->resource, resource);
    }
}

static void
surface_left_output (MetaWaylandSurface *surface,
                     MetaWaylandOutput *wayland_output)
{
  GList *iter;
  struct wl_resource *resource;

  for (iter = wayland_output->resources; iter != NULL; iter = iter->next)
    {
      resource = iter->data;

      if (wl_resource_get_client (resource) !=
          wl_resource_get_client (surface->resource))
        continue;

      wl_surface_send_leave (surface->resource, resource);
    }
}

static void
set_surface_is_on_output (MetaWaylandSurface *surface,
                          MetaWaylandOutput *wayland_output,
                          gboolean is_on_output);

static void
surface_handle_output_destroy (MetaWaylandOutput *wayland_output,
                               MetaWaylandSurface *surface)
{
  set_surface_is_on_output (surface, wayland_output, FALSE);
}

static void
set_surface_is_on_output (MetaWaylandSurface *surface,
                          MetaWaylandOutput *wayland_output,
                          gboolean is_on_output)
{
  gpointer orig_id;
  gboolean was_on_output = g_hash_table_lookup_extended (surface->outputs_to_destroy_notify_id,
                                                         wayland_output,
                                                         NULL, &orig_id);

  if (!was_on_output && is_on_output)
    {
      gulong id;

      id = g_signal_connect (wayland_output, "output-destroyed",
                             G_CALLBACK (surface_handle_output_destroy),
                             surface);
      g_hash_table_insert (surface->outputs_to_destroy_notify_id, wayland_output,
                           GSIZE_TO_POINTER ((gsize)id));
      surface_entered_output (surface, wayland_output);
    }
  else if (was_on_output && !is_on_output)
    {
      g_hash_table_remove (surface->outputs_to_destroy_notify_id, wayland_output);
      g_signal_handler_disconnect (wayland_output, (gulong) GPOINTER_TO_SIZE (orig_id));
      surface_left_output (surface, wayland_output);
    }
}

static gboolean
actor_surface_is_on_logical_monitor (MetaWaylandSurfaceRole *surface_role,
                                     MetaLogicalMonitor     *logical_monitor)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaSurfaceActorWayland *actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  return meta_surface_actor_wayland_is_on_monitor (actor, logical_monitor);
}

static void
update_surface_output_state (gpointer key, gpointer value, gpointer user_data)
{
  MetaWaylandOutput *wayland_output = value;
  MetaWaylandSurface *surface = user_data;
  MetaLogicalMonitor *logical_monitor;
  gboolean is_on_logical_monitor;

  g_assert (surface->role);

  logical_monitor = wayland_output->logical_monitor;
  if (!logical_monitor)
    {
      set_surface_is_on_output (surface, wayland_output, FALSE);
      return;
    }

  is_on_logical_monitor =
    meta_wayland_surface_role_is_on_logical_monitor (surface->role,
                                                     logical_monitor);
  set_surface_is_on_output (surface, wayland_output, is_on_logical_monitor);
}

static void
surface_output_disconnect_signal (gpointer key, gpointer value, gpointer user_data)
{
  g_signal_handler_disconnect (key, (gulong) GPOINTER_TO_SIZE (value));
}

void
meta_wayland_surface_update_outputs (MetaWaylandSurface *surface)
{
  if (!surface->compositor)
    return;

  g_hash_table_foreach (surface->compositor->outputs,
                        update_surface_output_state,
                        surface);
}

static void
meta_wayland_surface_update_outputs_recursively (MetaWaylandSurface *surface)
{
  GList *l;

  meta_wayland_surface_update_outputs (surface);

  for (l = surface->subsurfaces; l != NULL; l = l->next)
    meta_wayland_surface_update_outputs_recursively (l->data);
}

void
meta_wayland_surface_set_window (MetaWaylandSurface *surface,
                                 MetaWindow         *window)
{
  gboolean was_unmapped = surface->window && !window;

  if (surface->window == window)
    return;

  if (surface->window)
    {
      g_signal_handlers_disconnect_by_func (surface->window,
                                            window_position_changed,
                                            surface);
    }

  surface->window = window;
  sync_reactive (surface);
  sync_drag_dest_funcs (surface);

  if (was_unmapped)
    g_signal_emit (surface, surface_signals[SURFACE_UNMAPPED], 0);

  if (window)
    {
      g_signal_connect_object (window,
                               "position-changed",
                               G_CALLBACK (window_position_changed),
                               surface, 0);
    }
}

static void
wl_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandCompositor *compositor = surface->compositor;
  MetaWaylandFrameCallback *cb, *next;

  g_signal_emit (surface, surface_signals[SURFACE_DESTROY], 0);

  g_signal_handlers_disconnect_by_func (surface->surface_actor,
                                        surface_actor_mapped_notify,
                                        surface);
  g_signal_handlers_disconnect_by_func (surface->surface_actor,
                                        surface_actor_allocation_notify,
                                        surface);
  g_signal_handlers_disconnect_by_func (surface->surface_actor,
                                        surface_actor_position_notify,
                                        surface);

  g_clear_object (&surface->role);

  /* If we still have a window at the time of destruction, that means that
   * the client is disconnecting, as the resources are destroyed in a random
   * order. Simply destroy the window in this case. */
  if (surface->window)
    meta_wayland_surface_destroy_window (surface);

  if (surface->unassigned.buffer)
    {
      meta_wayland_surface_unref_buffer_use_count (surface);
      g_clear_object (&surface->unassigned.buffer);
    }

  if (surface->buffer_held)
    meta_wayland_surface_unref_buffer_use_count (surface);
  g_clear_object (&surface->buffer_ref.buffer);

  g_clear_object (&surface->pending);

  if (surface->opaque_region)
    cairo_region_destroy (surface->opaque_region);
  if (surface->input_region)
    cairo_region_destroy (surface->input_region);

  g_object_unref (surface->surface_actor);

  meta_wayland_compositor_destroy_frame_callbacks (compositor, surface);

  g_hash_table_foreach (surface->outputs_to_destroy_notify_id, surface_output_disconnect_signal, surface);
  g_hash_table_unref (surface->outputs_to_destroy_notify_id);

  wl_list_for_each_safe (cb, next, &surface->pending_frame_callback_list, link)
    wl_resource_destroy (cb->resource);

  if (surface->resource)
    wl_resource_set_user_data (surface->resource, NULL);

  if (surface->wl_subsurface)
    wl_resource_destroy (surface->wl_subsurface);

  g_hash_table_destroy (surface->shortcut_inhibited_seats);

  g_object_unref (surface);

  meta_wayland_compositor_repick (compositor);
}

static void
surface_actor_mapped_notify (MetaSurfaceActorWayland *surface_actor,
                             GParamSpec              *pspec,
                             MetaWaylandSurface      *surface)
{
  meta_wayland_surface_update_outputs_recursively (surface);
}

static void
surface_actor_allocation_notify (MetaSurfaceActorWayland *surface_actor,
                                 GParamSpec              *pspec,
                                 MetaWaylandSurface      *surface)
{
  meta_wayland_surface_update_outputs_recursively (surface);
}

static void
surface_actor_position_notify (MetaSurfaceActorWayland *surface_actor,
                               GParamSpec              *pspec,
                               MetaWaylandSurface      *surface)
{
  meta_wayland_surface_update_outputs_recursively (surface);
}

static void
window_position_changed (MetaWindow         *window,
                         MetaWaylandSurface *surface)
{
  meta_wayland_surface_update_outputs_recursively (surface);
}

MetaWaylandSurface *
meta_wayland_surface_create (MetaWaylandCompositor *compositor,
                             struct wl_client      *client,
                             struct wl_resource    *compositor_resource,
                             guint32                id)
{
  MetaWaylandSurface *surface = g_object_new (META_TYPE_WAYLAND_SURFACE, NULL);

  surface->compositor = compositor;
  surface->scale = 1;

  surface->resource = wl_resource_create (client, &wl_surface_interface, wl_resource_get_version (compositor_resource), id);
  wl_resource_set_implementation (surface->resource, &meta_wayland_wl_surface_interface, surface, wl_surface_destructor);

  surface->surface_actor = g_object_ref_sink (meta_surface_actor_wayland_new (surface));

  wl_list_init (&surface->pending_frame_callback_list);

  g_signal_connect_object (surface->surface_actor,
                           "notify::allocation",
                           G_CALLBACK (surface_actor_allocation_notify),
                           surface, 0);
  g_signal_connect_object (surface->surface_actor,
                           "notify::position",
                           G_CALLBACK (surface_actor_position_notify),
                           surface, 0);
  g_signal_connect_object (surface->surface_actor,
                           "notify::mapped",
                           G_CALLBACK (surface_actor_mapped_notify),
                           surface, 0);

  sync_drag_dest_funcs (surface);

  surface->outputs_to_destroy_notify_id = g_hash_table_new (NULL, NULL);
  surface->shortcut_inhibited_seats = g_hash_table_new (NULL, NULL);

  return surface;
}

gboolean
meta_wayland_surface_begin_grab_op (MetaWaylandSurface *surface,
                                    MetaWaylandSeat    *seat,
                                    MetaGrabOp          grab_op,
                                    gfloat              x,
                                    gfloat              y)
{
  MetaWindow *window = surface->window;

  if (grab_op == META_GRAB_OP_NONE)
    return FALSE;

  /* This is an input driven operation so we set frame_action to
     constrain it in the same way as it would be if the window was
     being moved/resized via a SSD event. */
  return meta_display_begin_grab_op (window->display,
                                     window->screen,
                                     window,
                                     grab_op,
                                     TRUE, /* pointer_already_grabbed */
                                     TRUE, /* frame_action */
                                     1, /* button. XXX? */
                                     0, /* modmask */
                                     meta_display_get_current_time_roundtrip (window->display),
                                     x, y);
}

static void
unparent_actor (MetaWaylandSurface *surface)
{
  ClutterActor *parent_actor;
  parent_actor = clutter_actor_get_parent (CLUTTER_ACTOR (surface->surface_actor));
  clutter_actor_remove_child (parent_actor, CLUTTER_ACTOR (surface->surface_actor));
}

static void
wl_subsurface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);
  if (surface->sub.parent)
    {
      wl_list_remove (&surface->sub.parent_destroy_listener.link);
      surface->sub.parent->subsurfaces =
        g_list_remove (surface->sub.parent->subsurfaces, surface);
      unparent_actor (surface);
      surface->sub.parent = NULL;
    }

  g_clear_object (&surface->sub.pending);
  surface->wl_subsurface = NULL;
}

static void
wl_subsurface_destroy (struct wl_client *client,
                       struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
wl_subsurface_set_position (struct wl_client *client,
                            struct wl_resource *resource,
                            int32_t x,
                            int32_t y)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->sub.pending_x = x;
  surface->sub.pending_y = y;
  surface->sub.pending_pos = TRUE;
}

static gboolean
is_valid_sibling (MetaWaylandSurface *surface, MetaWaylandSurface *sibling)
{
  if (surface->sub.parent == sibling)
    return TRUE;
  if (surface->sub.parent == sibling->sub.parent)
    return TRUE;
  return FALSE;
}

static void
subsurface_handle_pending_sibling_destroyed (struct wl_listener *listener, void *data)
{
  MetaWaylandSubsurfacePlacementOp *op =
    wl_container_of (listener, op, sibling_destroy_listener);

  op->sibling = NULL;
}

static void
queue_subsurface_placement (MetaWaylandSurface *surface,
                            MetaWaylandSurface *sibling,
                            MetaWaylandSubsurfacePlacement placement)
{
  MetaWaylandSubsurfacePlacementOp *op =
    g_slice_new (MetaWaylandSubsurfacePlacementOp);

  op->placement = placement;
  op->sibling = sibling;
  op->sibling_destroy_listener.notify =
    subsurface_handle_pending_sibling_destroyed;
  wl_resource_add_destroy_listener (sibling->resource,
                                    &op->sibling_destroy_listener);

  surface->sub.pending_placement_ops =
    g_slist_append (surface->sub.pending_placement_ops, op);
}

static void
wl_subsurface_place_above (struct wl_client *client,
                           struct wl_resource *resource,
                           struct wl_resource *sibling_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *sibling = wl_resource_get_user_data (sibling_resource);

  if (!is_valid_sibling (surface, sibling))
    {
      wl_resource_post_error (resource, WL_SUBSURFACE_ERROR_BAD_SURFACE,
                              "wl_subsurface::place_above: wl_surface@%d is "
                              "not a valid parent or sibling",
                              wl_resource_get_id (sibling->resource));
      return;
    }

  queue_subsurface_placement (surface,
                              sibling,
                              META_WAYLAND_SUBSURFACE_PLACEMENT_ABOVE);
}

static void
wl_subsurface_place_below (struct wl_client *client,
                           struct wl_resource *resource,
                           struct wl_resource *sibling_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *sibling = wl_resource_get_user_data (sibling_resource);

  if (!is_valid_sibling (surface, sibling))
    {
      wl_resource_post_error (resource, WL_SUBSURFACE_ERROR_BAD_SURFACE,
                              "wl_subsurface::place_below: wl_surface@%d is "
                              "not a valid parent or sibling",
                              wl_resource_get_id (sibling->resource));
      return;
    }

  queue_subsurface_placement (surface,
                              sibling,
                              META_WAYLAND_SUBSURFACE_PLACEMENT_BELOW);
}

static void
wl_subsurface_set_sync (struct wl_client *client,
                        struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->sub.synchronous = TRUE;
}

static void
wl_subsurface_set_desync (struct wl_client *client,
                          struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  gboolean was_effectively_synchronized;

  was_effectively_synchronized = is_surface_effectively_synchronized (surface);
  surface->sub.synchronous = FALSE;
  if (was_effectively_synchronized &&
      !is_surface_effectively_synchronized (surface))
    apply_pending_state (surface, surface->sub.pending);
}

static const struct wl_subsurface_interface meta_wayland_wl_subsurface_interface = {
  wl_subsurface_destroy,
  wl_subsurface_set_position,
  wl_subsurface_place_above,
  wl_subsurface_place_below,
  wl_subsurface_set_sync,
  wl_subsurface_set_desync,
};

static void
wl_subcompositor_destroy (struct wl_client *client,
                          struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
surface_handle_parent_surface_destroyed (struct wl_listener *listener,
                                         void *data)
{
  MetaWaylandSurface *surface = wl_container_of (listener,
                                                 surface,
                                                 sub.parent_destroy_listener);

  surface->sub.parent = NULL;
  unparent_actor (surface);
}

static void
wl_subcompositor_get_subsurface (struct wl_client *client,
                                 struct wl_resource *resource,
                                 guint32 id,
                                 struct wl_resource *surface_resource,
                                 struct wl_resource *parent_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandSurface *parent = wl_resource_get_user_data (parent_resource);

  if (surface->wl_subsurface != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_subcompositor::get_subsurface already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_SURFACE_ROLE_SUBSURFACE,
                                         NULL))
    {
      /* FIXME: There is no subcompositor "role" error yet, so lets just use something
       * similar until there is.
       */
      wl_resource_post_error (resource, WL_SHELL_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  surface->wl_subsurface = wl_resource_create (client, &wl_subsurface_interface, wl_resource_get_version (resource), id);
  wl_resource_set_implementation (surface->wl_subsurface, &meta_wayland_wl_subsurface_interface, surface, wl_subsurface_destructor);

  surface->sub.pending = g_object_new (META_TYPE_WAYLAND_PENDING_STATE, NULL);
  surface->sub.synchronous = TRUE;
  surface->sub.parent = parent;
  surface->sub.parent_destroy_listener.notify = surface_handle_parent_surface_destroyed;
  wl_resource_add_destroy_listener (parent->resource, &surface->sub.parent_destroy_listener);
  parent->subsurfaces = g_list_append (parent->subsurfaces, surface);

  clutter_actor_add_child (CLUTTER_ACTOR (parent->surface_actor),
                           CLUTTER_ACTOR (surface->surface_actor));

  sync_reactive (surface);
}

static const struct wl_subcompositor_interface meta_wayland_subcompositor_interface = {
  wl_subcompositor_destroy,
  wl_subcompositor_get_subsurface,
};

static void
bind_subcompositor (struct wl_client *client,
                    void             *data,
                    guint32           version,
                    guint32           id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_subcompositor_interface, version, id);
  wl_resource_set_implementation (resource, &meta_wayland_subcompositor_interface, data, NULL);
}

void
meta_wayland_shell_init (MetaWaylandCompositor *compositor)
{
  meta_wayland_xdg_shell_init (compositor);
  meta_wayland_wl_shell_init (compositor);
  meta_wayland_gtk_shell_init (compositor);

  if (wl_global_create (compositor->wayland_display,
                        &wl_subcompositor_interface,
                        META_WL_SUBCOMPOSITOR_VERSION,
                        compositor, bind_subcompositor) == NULL)
    g_error ("Failed to register a global wl-subcompositor object");
}

void
meta_wayland_surface_configure_notify (MetaWaylandSurface *surface,
                                       int                 new_x,
                                       int                 new_y,
                                       int                 new_width,
                                       int                 new_height,
                                       MetaWaylandSerial  *sent_serial)
{
  MetaWaylandShellSurface *shell_surface =
    META_WAYLAND_SHELL_SURFACE (surface->role);

  g_signal_emit (surface, surface_signals[SURFACE_CONFIGURE], 0);

  meta_wayland_shell_surface_configure (shell_surface,
                                        new_x, new_y,
                                        new_width, new_height,
                                        sent_serial);
}

void
meta_wayland_surface_ping (MetaWaylandSurface *surface,
                           guint32             serial)
{
  MetaWaylandShellSurface *shell_surface =
    META_WAYLAND_SHELL_SURFACE (surface->role);

  meta_wayland_shell_surface_ping (shell_surface, serial);
}

void
meta_wayland_surface_delete (MetaWaylandSurface *surface)
{
  MetaWaylandShellSurface *shell_surface =
    META_WAYLAND_SHELL_SURFACE (surface->role);

  meta_wayland_shell_surface_close (shell_surface);
}

void
meta_wayland_surface_window_managed (MetaWaylandSurface *surface,
                                     MetaWindow         *window)
{
  MetaWaylandShellSurface *shell_surface =
    META_WAYLAND_SHELL_SURFACE (surface->role);

  meta_wayland_shell_surface_managed (shell_surface, window);
}

void
meta_wayland_surface_drag_dest_focus_in (MetaWaylandSurface   *surface,
                                         MetaWaylandDataOffer *offer)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandDataDevice *data_device = &compositor->seat->data_device;

  surface->dnd.funcs->focus_in (data_device, surface, offer);
}

void
meta_wayland_surface_drag_dest_motion (MetaWaylandSurface *surface,
                                       const ClutterEvent *event)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandDataDevice *data_device = &compositor->seat->data_device;

  surface->dnd.funcs->motion (data_device, surface, event);
}

void
meta_wayland_surface_drag_dest_focus_out (MetaWaylandSurface *surface)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandDataDevice *data_device = &compositor->seat->data_device;

  surface->dnd.funcs->focus_out (data_device, surface);
}

void
meta_wayland_surface_drag_dest_drop (MetaWaylandSurface *surface)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandDataDevice *data_device = &compositor->seat->data_device;

  surface->dnd.funcs->drop (data_device, surface);
}

void
meta_wayland_surface_drag_dest_update (MetaWaylandSurface *surface)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWaylandDataDevice *data_device = &compositor->seat->data_device;

  surface->dnd.funcs->update (data_device, surface);
}

MetaWaylandSurface *
meta_wayland_surface_get_toplevel (MetaWaylandSurface *surface)
{
  if (surface->role)
    return meta_wayland_surface_role_get_toplevel (surface->role);
  else
    return NULL;
}

MetaWindow *
meta_wayland_surface_get_toplevel_window (MetaWaylandSurface *surface)
{
  MetaWaylandSurface *toplevel;

  toplevel = meta_wayland_surface_get_toplevel (surface);
  if (toplevel)
    return toplevel->window;
  else
    return NULL;
}

void
meta_wayland_surface_get_relative_coordinates (MetaWaylandSurface *surface,
                                               float               abs_x,
                                               float               abs_y,
                                               float               *sx,
                                               float               *sy)
{
  /* Using clutter API to transform coordinates is only accurate right
   * after a clutter layout pass but this function is used e.g. to
   * deliver pointer motion events which can happen at any time. This
   * isn't a problem for wayland clients since they don't control
   * their position, but X clients do and we'd be sending outdated
   * coordinates if a client is moving a window in response to motion
   * events.
   */
  if (surface->window &&
      surface->window->client_type == META_WINDOW_CLIENT_TYPE_X11)
    {
      MetaRectangle window_rect;

      meta_window_get_buffer_rect (surface->window, &window_rect);
      *sx = abs_x - window_rect.x;
      *sy = abs_y - window_rect.y;
    }
  else
    {
      ClutterActor *actor =
        CLUTTER_ACTOR (meta_surface_actor_get_texture (surface->surface_actor));

      clutter_actor_transform_stage_point (actor, abs_x, abs_y, sx, sy);
      *sx /= surface->scale;
      *sy /= surface->scale;
    }
}

void
meta_wayland_surface_get_absolute_coordinates (MetaWaylandSurface *surface,
                                               float               sx,
                                               float               sy,
                                               float               *x,
                                               float               *y)
{
  ClutterActor *actor =
    CLUTTER_ACTOR (meta_surface_actor_get_texture (surface->surface_actor));
  ClutterVertex sv = {
    .x = sx * surface->scale,
    .y = sy * surface->scale,
  };
  ClutterVertex v = { 0 };

  clutter_actor_apply_relative_transform_to_point (actor, NULL, &sv, &v);

  *x = v.x;
  *y = v.y;
}

static void
meta_wayland_surface_init (MetaWaylandSurface *surface)
{
  surface->pending = g_object_new (META_TYPE_WAYLAND_PENDING_STATE, NULL);
}

static void
meta_wayland_surface_class_init (MetaWaylandSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  surface_signals[SURFACE_DESTROY] =
    g_signal_new ("destroy",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  surface_signals[SURFACE_UNMAPPED] =
    g_signal_new ("unmapped",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  surface_signals[SURFACE_CONFIGURE] =
    g_signal_new ("configure",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  surface_signals[SURFACE_SHORTCUTS_INHIBITED] =
    g_signal_new ("shortcuts-inhibited",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  surface_signals[SURFACE_SHORTCUTS_RESTORED] =
    g_signal_new ("shortcuts-restored",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
meta_wayland_surface_role_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  MetaWaylandSurfaceRole *surface_role = META_WAYLAND_SURFACE_ROLE (object);
  MetaWaylandSurfaceRolePrivate *priv =
    meta_wayland_surface_role_get_instance_private (surface_role);

  switch (prop_id)
    {
    case SURFACE_ROLE_PROP_SURFACE:
      priv->surface = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_wayland_surface_role_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  MetaWaylandSurfaceRole *surface_role = META_WAYLAND_SURFACE_ROLE (object);
  MetaWaylandSurfaceRolePrivate *priv =
    meta_wayland_surface_role_get_instance_private (surface_role);

  switch (prop_id)
    {
    case SURFACE_ROLE_PROP_SURFACE:
      g_value_set_object (value, priv->surface);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_wayland_surface_role_init (MetaWaylandSurfaceRole *role)
{
}

static void
meta_wayland_surface_role_class_init (MetaWaylandSurfaceRoleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = meta_wayland_surface_role_set_property;
  object_class->get_property = meta_wayland_surface_role_get_property;

  g_object_class_install_property (object_class,
                                   SURFACE_ROLE_PROP_SURFACE,
                                   g_param_spec_object ("surface",
                                                        "MetaWaylandSurface",
                                                        "The MetaWaylandSurface instance",
                                                        META_TYPE_WAYLAND_SURFACE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
meta_wayland_surface_role_assigned (MetaWaylandSurfaceRole *surface_role)
{
  META_WAYLAND_SURFACE_ROLE_GET_CLASS (surface_role)->assigned (surface_role);
}

static void
meta_wayland_surface_role_pre_commit (MetaWaylandSurfaceRole  *surface_role,
                                      MetaWaylandPendingState *pending)
{
  MetaWaylandSurfaceRoleClass *klass;

  klass = META_WAYLAND_SURFACE_ROLE_GET_CLASS (surface_role);
  if (klass->pre_commit)
    klass->pre_commit (surface_role, pending);
}

static void
meta_wayland_surface_role_commit (MetaWaylandSurfaceRole  *surface_role,
                                  MetaWaylandPendingState *pending)
{
  META_WAYLAND_SURFACE_ROLE_GET_CLASS (surface_role)->commit (surface_role,
                                                              pending);
}

static gboolean
meta_wayland_surface_role_is_on_logical_monitor (MetaWaylandSurfaceRole *surface_role,
                                                 MetaLogicalMonitor     *logical_monitor)
{
  MetaWaylandSurfaceRoleClass *klass;

  klass = META_WAYLAND_SURFACE_ROLE_GET_CLASS (surface_role);
  if (klass->is_on_logical_monitor)
    return klass->is_on_logical_monitor (surface_role, logical_monitor);
  else
    return FALSE;
}

static MetaWaylandSurface *
meta_wayland_surface_role_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurfaceRoleClass *klass;

  klass = META_WAYLAND_SURFACE_ROLE_GET_CLASS (surface_role);
  if (klass->get_toplevel)
    return klass->get_toplevel (surface_role);
  else
    return NULL;
}

MetaWaylandSurface *
meta_wayland_surface_role_get_surface (MetaWaylandSurfaceRole *role)
{
  MetaWaylandSurfaceRolePrivate *priv =
    meta_wayland_surface_role_get_instance_private (role);

  return priv->surface;
}

static void
meta_wayland_shell_surface_configure (MetaWaylandShellSurface *shell_surface,
                                      int                      new_x,
                                      int                      new_y,
                                      int                      new_width,
                                      int                      new_height,
                                      MetaWaylandSerial       *sent_serial)
{
  MetaWaylandShellSurfaceClass *shell_surface_class =
    META_WAYLAND_SHELL_SURFACE_GET_CLASS (shell_surface);

  shell_surface_class->configure (shell_surface,
                                  new_x,
                                  new_y,
                                  new_width,
                                  new_height,
                                  sent_serial);
}

static void
meta_wayland_shell_surface_ping (MetaWaylandShellSurface *shell_surface,
                                 uint32_t                 serial)
{
  MetaWaylandShellSurfaceClass *shell_surface_class =
    META_WAYLAND_SHELL_SURFACE_GET_CLASS (shell_surface);

  shell_surface_class->ping (shell_surface, serial);
}

static void
meta_wayland_shell_surface_close (MetaWaylandShellSurface *shell_surface)
{
  MetaWaylandShellSurfaceClass *shell_surface_class =
    META_WAYLAND_SHELL_SURFACE_GET_CLASS (shell_surface);

  shell_surface_class->close (shell_surface);
}

static void
meta_wayland_shell_surface_managed (MetaWaylandShellSurface *shell_surface,
                                    MetaWindow              *window)
{
  MetaWaylandShellSurfaceClass *shell_surface_class =
    META_WAYLAND_SHELL_SURFACE_GET_CLASS (shell_surface);

  shell_surface_class->managed (shell_surface, window);
}

void
meta_wayland_surface_queue_pending_frame_callbacks (MetaWaylandSurface *surface)
{
  wl_list_insert_list (&surface->compositor->frame_callbacks,
                       &surface->pending_frame_callback_list);
  wl_list_init (&surface->pending_frame_callback_list);
}

static void
default_role_assigned (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  meta_wayland_surface_queue_pending_frame_callbacks (surface);
}

static void
actor_surface_assigned (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaSurfaceActorWayland *surface_actor =
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

  meta_surface_actor_wayland_add_frame_callbacks (surface_actor,
                                                  &surface->pending_frame_callback_list);
  wl_list_init (&surface->pending_frame_callback_list);
}

static void
actor_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                      MetaWaylandPendingState *pending)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurface *toplevel_surface;

  queue_surface_actor_frame_callbacks (surface, pending);

  toplevel_surface = meta_wayland_surface_get_toplevel (surface);
  if (!toplevel_surface || !toplevel_surface->window)
    return;

  meta_surface_actor_wayland_sync_state (
    META_SURFACE_ACTOR_WAYLAND (surface->surface_actor));
}

static void
meta_wayland_surface_role_actor_surface_init (MetaWaylandSurfaceRoleActorSurface *role)
{
}

static void
meta_wayland_surface_role_actor_surface_class_init (MetaWaylandSurfaceRoleActorSurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->assigned = actor_surface_assigned;
  surface_role_class->commit = actor_surface_commit;
  surface_role_class->is_on_logical_monitor = actor_surface_is_on_logical_monitor;
}

static void
meta_wayland_shell_surface_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                                           MetaWaylandPendingState *pending)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWindow *window;
  MetaWaylandBuffer *buffer;
  CoglTexture *texture;
  MetaSurfaceActorWayland *actor;
  double scale;

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_shell_surface_parent_class);
  surface_role_class->commit (surface_role, pending);

  buffer = surface->buffer_ref.buffer;
  if (!buffer)
    return;

  window = surface->window;
  if (!window)
    return;

  actor = META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);
  scale = meta_surface_actor_wayland_get_scale (actor);
  texture = buffer->texture;

  window->buffer_rect.width = cogl_texture_get_width (texture) * scale;
  window->buffer_rect.height = cogl_texture_get_height (texture) * scale;
}

static void
meta_wayland_shell_surface_init (MetaWaylandShellSurface *role)
{
}

static void
meta_wayland_shell_surface_class_init (MetaWaylandShellSurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->commit = meta_wayland_shell_surface_surface_commit;
}

static void
meta_wayland_surface_role_dnd_init (MetaWaylandSurfaceRoleDND *role)
{
}

static void
meta_wayland_surface_role_dnd_class_init (MetaWaylandSurfaceRoleDNDClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->assigned = default_role_assigned;
  surface_role_class->commit = dnd_surface_commit;
}

static void
meta_wayland_surface_role_subsurface_init (MetaWaylandSurfaceRoleSubsurface *role)
{
}

static void
meta_wayland_surface_role_subsurface_class_init (MetaWaylandSurfaceRoleSubsurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->commit = subsurface_role_commit;
  surface_role_class->get_toplevel = subsurface_role_get_toplevel;
}

cairo_region_t *
meta_wayland_surface_calculate_input_region (MetaWaylandSurface *surface)
{
  cairo_region_t *region;
  cairo_rectangle_int_t buffer_rect;
  CoglTexture *texture;

  if (!surface->buffer_ref.buffer)
    return NULL;

  texture = surface->buffer_ref.buffer->texture;
  buffer_rect = (cairo_rectangle_int_t) {
    .width = cogl_texture_get_width (texture) / surface->scale,
    .height = cogl_texture_get_height (texture) / surface->scale,
  };
  region = cairo_region_create_rectangle (&buffer_rect);

  if (surface->input_region)
    cairo_region_intersect (region, surface->input_region);

  return region;
}

void
meta_wayland_surface_inhibit_shortcuts (MetaWaylandSurface *surface,
                                        MetaWaylandSeat    *seat)
{
  g_hash_table_add (surface->shortcut_inhibited_seats, seat);
  g_signal_emit (surface, surface_signals[SURFACE_SHORTCUTS_INHIBITED], 0);
}

void
meta_wayland_surface_restore_shortcuts (MetaWaylandSurface *surface,
                                        MetaWaylandSeat    *seat)
{
  g_signal_emit (surface, surface_signals[SURFACE_SHORTCUTS_RESTORED], 0);
  g_hash_table_remove (surface->shortcut_inhibited_seats, seat);
}

gboolean
meta_wayland_surface_is_shortcuts_inhibited (MetaWaylandSurface *surface,
                                             MetaWaylandSeat    *seat)
{
  if (surface->shortcut_inhibited_seats == NULL)
    return FALSE;

  return g_hash_table_contains (surface->shortcut_inhibited_seats, seat);
}
