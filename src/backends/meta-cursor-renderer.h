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

#ifndef META_CURSOR_RENDERER_H
#define META_CURSOR_RENDERER_H

#include <glib-object.h>

#include <meta/screen.h>
#include "meta-cursor.h"

#include <gbm.h>

#define META_TYPE_CURSOR_RENDERER            (meta_cursor_renderer_get_type ())
#define META_CURSOR_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_CURSOR_RENDERER, MetaCursorRenderer))
#define META_CURSOR_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_CURSOR_RENDERER, MetaCursorRendererClass))
#define META_IS_CURSOR_RENDERER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_CURSOR_RENDERER))
#define META_IS_CURSOR_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_CURSOR_RENDERER))
#define META_CURSOR_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_CURSOR_RENDERER, MetaCursorRendererClass))

typedef struct _MetaCursorRenderer        MetaCursorRenderer;
typedef struct _MetaCursorRendererClass   MetaCursorRendererClass;

struct _MetaCursorRenderer
{
  GObject parent;
};

struct _MetaCursorRendererClass
{
  GObjectClass parent_class;
};

GType meta_cursor_renderer_get_type (void) G_GNUC_CONST;

MetaCursorRenderer * meta_cursor_renderer_new (MetaScreen *screen);

void meta_cursor_renderer_set_cursor (MetaCursorRenderer  *renderer,
                                      MetaCursorReference *cursor);

void meta_cursor_renderer_set_position (MetaCursorRenderer *renderer,
                                        int x, int y);

void meta_cursor_renderer_paint (MetaCursorRenderer *renderer);

void meta_cursor_renderer_force_update (MetaCursorRenderer *renderer);

struct gbm_device * meta_cursor_renderer_get_gbm_device (MetaCursorRenderer *renderer);

#endif /* META_CURSOR_RENDERER_H */
