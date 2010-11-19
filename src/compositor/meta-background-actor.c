/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * meta-background-actor.c: Actor for painting the root window background
 *
 * Copyright 2009 Sander Dijkhuis
 * Copyright 2010 Red Hat, Inc.
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
 * Portions adapted from gnome-shell/src/shell-global.c
 */

#include <config.h>

#define COGL_ENABLE_EXPERIMENTAL_API
#include <cogl/cogl-texture-pixmap-x11.h>

#include <X11/Xatom.h>

#include "cogl-utils.h"
#include "compositor-private.h"
#include "errors.h"
#include "meta-background-actor.h"

struct _MetaBackgroundActorClass
{
  ClutterActorClass parent_class;
};

struct _MetaBackgroundActor
{
  ClutterActor parent;

  CoglHandle material;
  MetaScreen *screen;
  cairo_region_t *visible_region;
  float texture_width;
  float texture_height;

  guint have_pixmap : 1;
};

G_DEFINE_TYPE (MetaBackgroundActor, meta_background_actor, CLUTTER_TYPE_ACTOR);

static void
update_wrap_mode (MetaBackgroundActor *self)
{
  int width, height;
  CoglMaterialWrapMode wrap_mode;

  meta_screen_get_size (self->screen, &width, &height);

  /* We turn off repeating when we have a full-screen pixmap to keep from
   * getting artifacts from one side of the image sneaking into the other
   * side of the image via bilinear filtering.
   */
  if (width == self->texture_width && height == self->texture_height)
    wrap_mode = COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE;
  else
    wrap_mode = COGL_MATERIAL_WRAP_MODE_REPEAT;

  cogl_material_set_layer_wrap_mode (self->material, 0, wrap_mode);
}

static void
set_texture (MetaBackgroundActor *self,
             CoglHandle           texture)
{
  MetaDisplay *display;

  display = meta_screen_get_display (self->screen);

  /* This may trigger destruction of an old texture pixmap, which, if
   * the underlying X pixmap is already gone has the tendency to trigger
   * X errors inside DRI. For safety, trap errors */
  meta_error_trap_push (display);
  cogl_material_set_layer (self->material, 0, texture);
  meta_error_trap_pop (display);

  self->texture_width = cogl_texture_get_width (texture);
  self->texture_height = cogl_texture_get_height (texture);

  update_wrap_mode (self);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (self));
}

/* Sets our material to paint with a 1x1 texture of the stage's background
 * color; doing this when we have no pixmap allows the application to turn
 * off painting the stage. There might be a performance benefit to
 * painting in this case with a solid color, but the normal solid color
 * case is a 1x1 root pixmap, so we'd have to reverse-engineer that to
 * actually pick up the (small?) performance win. This is just a fallback.
 */
static void
set_texture_to_stage_color (MetaBackgroundActor *self)
{
  ClutterActor *stage = meta_get_stage_for_screen (self->screen);
  ClutterColor color;
  CoglHandle texture;

  clutter_stage_get_color (CLUTTER_STAGE (stage), &color);
  texture = meta_create_color_texture_4ub (color.red, color.green,
                                           color.blue, 0xff);
  set_texture (self, texture);
  cogl_handle_unref (texture);
}

static void
on_notify_stage_color (GObject             *stage,
                       GParamSpec          *pspec,
                       MetaBackgroundActor *self)
{
  if (!self->have_pixmap)
    set_texture_to_stage_color (self);
}

static void
meta_background_actor_dispose (GObject *object)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (object);

  meta_background_actor_set_visible_region (self, NULL);

  if (self->material != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (self->material);
      self->material = COGL_INVALID_HANDLE;
    }

  if (self->screen != NULL)
    {
      ClutterActor *stage = meta_get_stage_for_screen (self->screen);
      g_signal_handlers_disconnect_by_func (stage,
                                            (gpointer) on_notify_stage_color,
                                            self);
      self->screen = NULL;
    }
}

static void
meta_background_actor_get_preferred_width (ClutterActor *actor,
                                           gfloat        for_height,
                                           gfloat       *min_width_p,
                                           gfloat       *natural_width_p)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (actor);
  int width, height;

  meta_screen_get_size (self->screen, &width, &height);

  if (min_width_p)
    *min_width_p = width;
  if (natural_width_p)
    *natural_width_p = height;
}

static void
meta_background_actor_get_preferred_height (ClutterActor *actor,
                                            gfloat        for_width,
                                            gfloat       *min_height_p,
                                            gfloat       *natural_height_p)

{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (actor);
  int width, height;

  meta_screen_get_size (self->screen, &width, &height);

  if (min_height_p)
    *min_height_p = height;
  if (natural_height_p)
    *natural_height_p = height;
}

static void
meta_background_actor_paint (ClutterActor *actor)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (actor);
  int width, height;

  meta_screen_get_size (self->screen, &width, &height);

  cogl_set_source (self->material);

  if (self->visible_region)
    {
      int n_rectangles = cairo_region_num_rectangles (self->visible_region);
      int i;

      for (i = 0; i < n_rectangles; i++)
        {
          cairo_rectangle_int_t rect;
          cairo_region_get_rectangle (self->visible_region, i, &rect);

          cogl_rectangle_with_texture_coords (rect.x, rect.y,
                                              rect.x + rect.width, rect.y + rect.height,
                                              rect.x / self->texture_width,
                                              rect.y / self->texture_height,
                                              (rect.x + rect.width) / self->texture_width,
                                              (rect.y + rect.height) / self->texture_height);
        }
    }
  else
    {
      cogl_rectangle_with_texture_coords (0.0f, 0.0f,
                                          width, height,
                                          0.0f, 0.0f,
                                          width / self->texture_width,
                                          height / self->texture_height);
    }
}

#if CLUTTER_CHECK_VERSION(1, 5, 2)
static gboolean
meta_background_actor_get_paint_volume (ClutterActor       *actor,
                                        ClutterPaintVolume *volume)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (actor);
  int width, height;

  meta_screen_get_size (self->screen, &width, &height);

  clutter_paint_volume_set_width (volume, width);
  clutter_paint_volume_set_height (volume, height);

  return TRUE;
}
#endif

static void
meta_background_actor_class_init (MetaBackgroundActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  object_class->dispose = meta_background_actor_dispose;

  actor_class->get_preferred_width = meta_background_actor_get_preferred_width;
  actor_class->get_preferred_height = meta_background_actor_get_preferred_height;
  actor_class->paint = meta_background_actor_paint;
#if CLUTTER_CHECK_VERSION(1, 5, 2)
  actor_class->get_paint_volume = meta_background_actor_get_paint_volume;
#endif
}

static void
meta_background_actor_init (MetaBackgroundActor *background_actor)
{
}

/**
 * @screen: the #MetaScreen
 * meta_background_actor_new:
 *
 * Creates a new actor to draw the background for the given screen.
 *
 * Return value: (transfer none): the newly created background actor
 */
ClutterActor *
meta_background_actor_new (MetaScreen *screen)
{
  MetaBackgroundActor *self;
  ClutterActor *stage;

  g_return_val_if_fail (META_IS_SCREEN (screen), NULL);

  self = g_object_new (META_TYPE_BACKGROUND_ACTOR, NULL);

  self->screen = screen;

  self->material = meta_create_texture_material (NULL);
  cogl_material_set_layer_wrap_mode (self->material, 0,
                                     COGL_MATERIAL_WRAP_MODE_REPEAT);

  stage = meta_get_stage_for_screen (self->screen);
  g_signal_connect (stage, "notify::color",
                    G_CALLBACK (on_notify_stage_color), self);

  meta_background_actor_update (self);

  return CLUTTER_ACTOR (self);
}

/**
 * meta_background_actor_update:
 * @self: a #MetaBackgroundActor
 *
 * Refetches the _XROOTPMAP_ID property for the root window and updates
 * the contents of the background actor based on that. There's no attempt
 * to optimize out pixmap values that don't change (since a root pixmap
 * could be replaced by with another pixmap with the same ID under some
 * circumstances), so this should only be called when we actually receive
 * a PropertyNotify event for the property.
 */
void
meta_background_actor_update (MetaBackgroundActor *self)
{
  MetaDisplay *display;
  MetaCompositor *compositor;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  Pixmap root_pixmap_id;

  g_return_if_fail (META_IS_BACKGROUND_ACTOR (self));

  display = meta_screen_get_display (self->screen);
  compositor = meta_display_get_compositor (display);

  root_pixmap_id = None;
  if (!XGetWindowProperty (meta_display_get_xdisplay (display),
                           meta_screen_get_xroot  (self->screen),
                           compositor->atom_x_root_pixmap,
                           0, LONG_MAX,
                           False,
                           AnyPropertyType,
                           &type, &format, &nitems, &bytes_after, &data) &&
      type != None)
  {
     /* Got a property. */
     if (type == XA_PIXMAP && format == 32 && nitems == 1)
       {
         /* Was what we expected. */
         root_pixmap_id = *(Pixmap *)data;
       }

     XFree(data);
  }

  if (root_pixmap_id != None)
    {
      CoglHandle texture;

      meta_error_trap_push (display);
      texture = cogl_texture_pixmap_x11_new (root_pixmap_id, FALSE);
      meta_error_trap_pop (display);

      if (texture != COGL_INVALID_HANDLE)
        {
          set_texture (self, texture);
          cogl_handle_unref (texture);

          self->have_pixmap = True;
          return;
        }
    }

  self->have_pixmap = False;
  set_texture_to_stage_color (self);
}

/**
 * meta_background_actor_set_visible_region:
 * @self: a #MetaBackgroundActor
 * @visible_region: (allow-none): the area of the actor (in allocate-relative
 *   coordinates) that is visible.
 *
 * Sets the area of the background that is unobscured by overlapping windows.
 * This is used to optimize and only paint the visible portions.
 */
void
meta_background_actor_set_visible_region (MetaBackgroundActor *self,
                                          cairo_region_t      *visible_region)
{
  g_return_if_fail (META_IS_BACKGROUND_ACTOR (self));

  if (self->visible_region)
    {
      cairo_region_destroy (self->visible_region);
      self->visible_region = NULL;
    }

  if (visible_region)
    {
      cairo_rectangle_int_t screen_rect = { 0 };
      meta_screen_get_size (self->screen, &screen_rect.width, &screen_rect.height);

      /* Doing the intersection here is probably unnecessary - MetaWindowGroup
       * should never compute a visible area that's larger than the root screen!
       * but it's not that expensive and adds some extra robustness.
       */
      self->visible_region = cairo_region_create_rectangle (&screen_rect);
      cairo_region_intersect (self->visible_region, visible_region);
    }
}

/**
 * meta_background_actor_screen_size_changed:
 * @self: a #MetaBackgroundActor
 *
 * Called by the compositor when the size of the #MetaScreen changes
 */
void
meta_background_actor_screen_size_changed (MetaBackgroundActor *self)
{
  update_wrap_mode (self);
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}
