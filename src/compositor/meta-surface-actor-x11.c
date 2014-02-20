/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2013 Red Hat
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
 * Written by:
 *     Owen Taylor <otaylor@redhat.com>
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "meta-surface-actor-x11.h"

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <cogl/cogl-texture-pixmap-x11.h>

#include <meta/errors.h>
#include "window-private.h"
#include "meta-shaped-texture-private.h"
#include "meta-cullable.h"

struct _MetaSurfaceActorX11Private
{
  MetaWindow *window;

  MetaDisplay *display;

  CoglTexture *texture;
  Pixmap pixmap;
  Damage damage;

  int last_width;
  int last_height;

  /* This is used to detect fullscreen windows that need to be unredirected */
  guint full_damage_frames_count;
  guint does_full_damage  : 1;

  /* Other state... */
  guint argb32 : 1;
  guint received_damage : 1;
  guint size_changed : 1;

  guint unredirected   : 1;
};
typedef struct _MetaSurfaceActorX11Private MetaSurfaceActorX11Private;

G_DEFINE_TYPE_WITH_PRIVATE (MetaSurfaceActorX11, meta_surface_actor_x11, META_TYPE_SURFACE_ACTOR)

static void
free_damage (MetaSurfaceActorX11 *self)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);
  MetaDisplay *display = priv->display;
  Display *xdisplay = meta_display_get_xdisplay (display);

  if (priv->damage == None)
    return;

  meta_error_trap_push (display);
  XDamageDestroy (xdisplay, priv->damage);
  priv->damage = None;
  meta_error_trap_pop (display);
}

static void
detach_pixmap (MetaSurfaceActorX11 *self)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);
  MetaDisplay *display = priv->display;
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaShapedTexture *stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));

  if (priv->pixmap == None)
    return;

  /* Get rid of all references to the pixmap before freeing it; it's unclear whether
   * you are supposed to be able to free a GLXPixmap after freeing the underlying
   * pixmap, but it certainly doesn't work with current DRI/Mesa
   */
  meta_shaped_texture_set_texture (stex, NULL);
  cogl_flush ();

  meta_error_trap_push (display);
  XFreePixmap (xdisplay, priv->pixmap);
  priv->pixmap = None;
  meta_error_trap_pop (display);

  cogl_object_unref (priv->texture);
  priv->texture = NULL;
}

static void
set_pixmap (MetaSurfaceActorX11 *self,
            Pixmap               pixmap)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);

  CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  MetaShapedTexture *stex = meta_surface_actor_get_texture (META_SURFACE_ACTOR (self));
  CoglTexture *texture;

  g_assert (priv->pixmap == None);
  priv->pixmap = pixmap;

  texture = COGL_TEXTURE (cogl_texture_pixmap_x11_new (ctx, priv->pixmap, FALSE, NULL));

  if (G_UNLIKELY (!cogl_texture_pixmap_x11_is_using_tfp_extension (COGL_TEXTURE_PIXMAP_X11 (texture))))
    g_warning ("NOTE: Not using GLX TFP!\n");

  priv->texture = texture;
  meta_shaped_texture_set_texture (stex, texture);
}

static void
update_pixmap (MetaSurfaceActorX11 *self)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);
  MetaDisplay *display = priv->display;
  Display *xdisplay = meta_display_get_xdisplay (display);

  if (priv->size_changed)
    {
      detach_pixmap (self);
      priv->size_changed = FALSE;
    }

  if (priv->pixmap == None)
    {
      Pixmap new_pixmap;
      Window xwindow = meta_window_get_toplevel_xwindow (priv->window);

      meta_error_trap_push (display);
      new_pixmap = XCompositeNameWindowPixmap (xdisplay, xwindow);

      if (meta_error_trap_pop_with_return (display) != Success)
        {
          /* Probably a BadMatch if the window isn't viewable; we could
           * GrabServer/GetWindowAttributes/NameWindowPixmap/UngrabServer/Sync
           * to avoid this, but there's no reason to take two round trips
           * when one will do. (We need that Sync if we want to handle failures
           * for any reason other than !viewable. That's unlikely, but maybe
           * we'll BadAlloc or something.)
           */
          new_pixmap = None;
        }

      if (new_pixmap == None)
        {
          meta_verbose ("Unable to get named pixmap for %s\n",
                        meta_window_get_description (priv->window));
          return;
        }

      set_pixmap (self, new_pixmap);
    }
}

static gboolean
is_visible (MetaSurfaceActorX11 *self)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);
  return (priv->pixmap != None) && !priv->unredirected;
}

static void
damage_area (MetaSurfaceActorX11 *self,
             int x, int y, int width, int height)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);

  if (!is_visible (self))
    return;

  cogl_texture_pixmap_x11_update_area (priv->texture, x, y, width, height);
  meta_surface_actor_update_area (META_SURFACE_ACTOR (self), x, y, width, height);
}

static void
meta_surface_actor_x11_process_damage (MetaSurfaceActor *actor,
                                       int x, int y, int width, int height)
{
  MetaSurfaceActorX11 *self = META_SURFACE_ACTOR_X11 (actor);
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);

  priv->received_damage = TRUE;

  if (meta_window_is_fullscreen (priv->window) && !priv->unredirected && !priv->does_full_damage)
    {
      MetaRectangle window_rect;
      meta_window_get_frame_rect (priv->window, &window_rect);

      if (window_rect.x == x &&
          window_rect.y == y &&
          window_rect.width == width &&
          window_rect.height == height)
        priv->full_damage_frames_count++;
      else
        priv->full_damage_frames_count = 0;

      if (priv->full_damage_frames_count >= 100)
        priv->does_full_damage = TRUE;
    }

  /* Drop damage event for unredirected windows */
  if (priv->unredirected)
    return;

  damage_area (self, x, y, width, height);
}

static void
meta_surface_actor_x11_pre_paint (MetaSurfaceActor *actor)
{
  MetaSurfaceActorX11 *self = META_SURFACE_ACTOR_X11 (actor);
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);
  MetaDisplay *display = priv->display;
  Display *xdisplay = meta_display_get_xdisplay (display);

  if (priv->received_damage)
    {
      meta_error_trap_push (display);
      XDamageSubtract (xdisplay, priv->damage, None, None);
      meta_error_trap_pop (display);

      /* We need to make sure that any X drawing that happens before the
       * XDamageSubtract() above is visible to subsequent GL rendering;
       * the only standardized way to do this is EXT_x11_sync_object,
       * which isn't yet widely available. For now, we count on details
       * of Xorg and the open source drivers, and hope for the best
       * otherwise.
       *
       * Xorg and open source driver specifics:
       *
       * The X server makes sure to flush drawing to the kernel before
       * sending out damage events, but since we use DamageReportBoundingBox
       * there may be drawing between the last damage event and the
       * XDamageSubtract() that needs to be flushed as well.
       *
       * Xorg always makes sure that drawing is flushed to the kernel
       * before writing events or responses to the client, so any round trip
       * request at this point is sufficient to flush the GLX buffers.
       */
      XSync (xdisplay, False);

      priv->received_damage = FALSE;
    }

  update_pixmap (self);
}

static void
update_is_argb32 (MetaSurfaceActorX11 *self)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);
  MetaDisplay *display = priv->display;
  Display *xdisplay = meta_display_get_xdisplay (display);

  XRenderPictFormat *format;
  format = XRenderFindVisualFormat (xdisplay, priv->window->xvisual);

  priv->argb32 = (format && format->type == PictTypeDirect && format->direct.alphaMask);
}

static gboolean
meta_surface_actor_x11_is_argb32 (MetaSurfaceActor *actor)
{
  MetaSurfaceActorX11 *self = META_SURFACE_ACTOR_X11 (actor);
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);

  return priv->argb32;
}

static gboolean
meta_surface_actor_x11_is_visible (MetaSurfaceActor *actor)
{
  MetaSurfaceActorX11 *self = META_SURFACE_ACTOR_X11 (actor);
  return is_visible (self);
}

static gboolean
meta_surface_actor_x11_should_unredirect (MetaSurfaceActor *actor)
{
  MetaSurfaceActorX11 *self = META_SURFACE_ACTOR_X11 (actor);
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);

  MetaWindow *window = priv->window;

  if (meta_window_requested_dont_bypass_compositor (window))
    return FALSE;

  if (window->opacity != 0xFF)
    return FALSE;

  if (window->shape_region != NULL)
    return FALSE;

  if (priv->argb32 && !meta_window_requested_bypass_compositor (window))
    return FALSE;

  if (!meta_window_is_monitor_sized (window))
    return FALSE;

  if (meta_window_requested_bypass_compositor (window))
    return TRUE;

  if (meta_window_is_override_redirect (window))
    return TRUE;

  if (priv->does_full_damage)
    return TRUE;

  return FALSE;
}

static void
sync_unredirected (MetaSurfaceActorX11 *self)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);
  MetaDisplay *display = priv->display;
  Display *xdisplay = meta_display_get_xdisplay (display);
  Window xwindow = meta_window_get_toplevel_xwindow (priv->window);

  meta_error_trap_push (display);

  if (priv->unredirected)
    {
      detach_pixmap (self);
      XCompositeUnredirectWindow (xdisplay, xwindow, CompositeRedirectManual);
    }
  else
    {
      XCompositeRedirectWindow (xdisplay, xwindow, CompositeRedirectManual);
    }

  meta_error_trap_pop (display);
}

static void
meta_surface_actor_x11_set_unredirected (MetaSurfaceActor *actor,
                                         gboolean          unredirected)
{
  MetaSurfaceActorX11 *self = META_SURFACE_ACTOR_X11 (actor);
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);

  if (priv->unredirected == unredirected)
    return;

  priv->unredirected = unredirected;
  sync_unredirected (self);
}

static gboolean
meta_surface_actor_x11_is_unredirected (MetaSurfaceActor *actor)
{
  MetaSurfaceActorX11 *self = META_SURFACE_ACTOR_X11 (actor);
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);

  return priv->unredirected;
}

static void
meta_surface_actor_x11_dispose (GObject *object)
{
  MetaSurfaceActorX11 *self = META_SURFACE_ACTOR_X11 (object);

  detach_pixmap (self);
  free_damage (self);

  G_OBJECT_CLASS (meta_surface_actor_x11_parent_class)->dispose (object);
}

static void
meta_surface_actor_x11_class_init (MetaSurfaceActorX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MetaSurfaceActorClass *surface_actor_class = META_SURFACE_ACTOR_CLASS (klass);

  object_class->dispose = meta_surface_actor_x11_dispose;

  surface_actor_class->process_damage = meta_surface_actor_x11_process_damage;
  surface_actor_class->pre_paint = meta_surface_actor_x11_pre_paint;
  surface_actor_class->is_argb32 = meta_surface_actor_x11_is_argb32;
  surface_actor_class->is_visible = meta_surface_actor_x11_is_visible;

  surface_actor_class->should_unredirect = meta_surface_actor_x11_should_unredirect;
  surface_actor_class->set_unredirected = meta_surface_actor_x11_set_unredirected;
  surface_actor_class->is_unredirected = meta_surface_actor_x11_is_unredirected;
}

static void
meta_surface_actor_x11_init (MetaSurfaceActorX11 *self)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);

  priv->last_width = -1;
  priv->last_height = -1;
}

static void
create_damage (MetaSurfaceActorX11 *self)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);
  Display *xdisplay = meta_display_get_xdisplay (priv->display);
  Window xwindow = meta_window_get_toplevel_xwindow (priv->window);

  priv->damage = XDamageCreate (xdisplay, xwindow, XDamageReportBoundingBox);
}

static void
window_decorated_notify (MetaWindow *window,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  MetaSurfaceActorX11 *self = META_SURFACE_ACTOR_X11 (user_data);

  free_damage (self);
  create_damage (self);
}

MetaSurfaceActor *
meta_surface_actor_x11_new (MetaWindow *window)
{
  MetaSurfaceActorX11 *self = g_object_new (META_TYPE_SURFACE_ACTOR_X11, NULL);
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);
  MetaDisplay *display = meta_window_get_display (window);

  g_assert (!meta_is_wayland_compositor ());

  priv->window = window;
  priv->display = display;

  create_damage (self);
  g_signal_connect_object (priv->window, "notify::decorated",
                           G_CALLBACK (window_decorated_notify), self, 0);

  update_is_argb32 (self);

  priv->unredirected = FALSE;
  sync_unredirected (self);

  return META_SURFACE_ACTOR (self);
}

void
meta_surface_actor_x11_set_size (MetaSurfaceActorX11 *self,
                                 int width, int height)
{
  MetaSurfaceActorX11Private *priv = meta_surface_actor_x11_get_instance_private (self);

  if (priv->last_width == width &&
      priv->last_height == height)
    return;

  priv->size_changed = TRUE;
  priv->last_width = width;
  priv->last_height = height;
}
