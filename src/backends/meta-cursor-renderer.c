/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "meta-cursor-renderer.h"

#include <meta/meta-backend.h>
#include <meta/util.h>

#include <cogl/cogl.h>
#include <clutter/clutter.h>

#include "meta-stage.h"

struct _MetaCursorRendererPrivate
{
  int current_x, current_y;
  MetaRectangle current_rect;

  MetaCursorSprite *displayed_cursor;
  gboolean handled_by_backend;
};
typedef struct _MetaCursorRendererPrivate MetaCursorRendererPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaCursorRenderer, meta_cursor_renderer, G_TYPE_OBJECT);

static void
queue_redraw (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);
  MetaBackend *backend = meta_get_backend ();
  ClutterActor *stage = meta_backend_get_stage (backend);
  CoglTexture *texture;

  /* During early initialization, we can have no stage */
  if (!stage)
    return;

  if (priv->displayed_cursor && !priv->handled_by_backend)
    texture = meta_cursor_sprite_get_cogl_texture (priv->displayed_cursor,
                                                   NULL, NULL);
  else
    texture = NULL;

  meta_stage_set_cursor (META_STAGE (stage), texture, &priv->current_rect);
}

static gboolean
meta_cursor_renderer_real_update_cursor (MetaCursorRenderer *renderer)
{
  return FALSE;
}

static void
meta_cursor_renderer_class_init (MetaCursorRendererClass *klass)
{
  klass->update_cursor = meta_cursor_renderer_real_update_cursor;
}

static void
meta_cursor_renderer_init (MetaCursorRenderer *renderer)
{
}

static void
update_cursor (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);
  gboolean handled_by_backend;
  gboolean should_redraw = FALSE;

  if (priv->displayed_cursor)
    {
      CoglTexture *texture;
      int hot_x, hot_y;

      texture = meta_cursor_sprite_get_cogl_texture (priv->displayed_cursor,
                                                     &hot_x, &hot_y);

      priv->current_rect.x = priv->current_x - hot_x;
      priv->current_rect.y = priv->current_y - hot_y;
      priv->current_rect.width = cogl_texture_get_width (COGL_TEXTURE (texture));
      priv->current_rect.height = cogl_texture_get_height (COGL_TEXTURE (texture));
    }
  else
    {
      priv->current_rect.x = 0;
      priv->current_rect.y = 0;
      priv->current_rect.width = 0;
      priv->current_rect.height = 0;
    }

  handled_by_backend = META_CURSOR_RENDERER_GET_CLASS (renderer)->update_cursor (renderer);
  if (handled_by_backend != priv->handled_by_backend)
    {
      priv->handled_by_backend = handled_by_backend;
      should_redraw = TRUE;
    }

  if (!handled_by_backend)
    should_redraw = TRUE;

  if (should_redraw)
    queue_redraw (renderer);
}

MetaCursorRenderer *
meta_cursor_renderer_new (void)
{
  return g_object_new (META_TYPE_CURSOR_RENDERER, NULL);
}

void
meta_cursor_renderer_set_cursor (MetaCursorRenderer *renderer,
                                 MetaCursorSprite   *cursor_sprite)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  if (priv->displayed_cursor == cursor_sprite)
    return;

  priv->displayed_cursor = cursor_sprite;
  update_cursor (renderer);
}

void
meta_cursor_renderer_force_update (MetaCursorRenderer *renderer)
{
  update_cursor (renderer);
  queue_redraw (renderer);
}

void
meta_cursor_renderer_set_position (MetaCursorRenderer *renderer,
                                   int x, int y)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  g_assert (meta_is_wayland_compositor ());

  priv->current_x = x;
  priv->current_y = y;

  update_cursor (renderer);
}

MetaCursorSprite *
meta_cursor_renderer_get_cursor (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  return priv->displayed_cursor;
}

const MetaRectangle *
meta_cursor_renderer_get_rect (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  return &priv->current_rect;
}

#ifdef HAVE_WAYLAND
void
meta_cursor_renderer_realize_cursor_from_wl_buffer (MetaCursorRenderer *renderer,
                                                    MetaCursorSprite   *cursor_sprite,
                                                    struct wl_resource *buffer)
{

  MetaCursorRendererClass *renderer_class = META_CURSOR_RENDERER_GET_CLASS (renderer);

  if (renderer_class->realize_cursor_from_wl_buffer)
    renderer_class->realize_cursor_from_wl_buffer (renderer, cursor_sprite, buffer);
}
#endif

void
meta_cursor_renderer_realize_cursor_from_xcursor (MetaCursorRenderer *renderer,
                                                  MetaCursorSprite   *cursor_sprite,
                                                  XcursorImage       *xc_image)
{
  MetaCursorRendererClass *renderer_class = META_CURSOR_RENDERER_GET_CLASS (renderer);

  if (renderer_class->realize_cursor_from_xcursor)
    renderer_class->realize_cursor_from_xcursor (renderer, cursor_sprite, xc_image);
}
