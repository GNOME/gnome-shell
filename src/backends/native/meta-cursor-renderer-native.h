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

#ifndef META_CURSOR_RENDERER_NATIVE_H
#define META_CURSOR_RENDERER_NATIVE_H

#include "meta-cursor-renderer.h"

#define META_TYPE_CURSOR_RENDERER_NATIVE             (meta_cursor_renderer_native_get_type ())
#define META_CURSOR_RENDERER_NATIVE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_CURSOR_RENDERER_NATIVE, MetaCursorRendererNative))
#define META_CURSOR_RENDERER_NATIVE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_CURSOR_RENDERER_NATIVE, MetaCursorRendererNativeClass))
#define META_IS_CURSOR_RENDERER_NATIVE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_CURSOR_RENDERER_NATIVE))
#define META_IS_CURSOR_RENDERER_NATIVE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_CURSOR_RENDERER_NATIVE))
#define META_CURSOR_RENDERER_NATIVE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_CURSOR_RENDERER_NATIVE, MetaCursorRendererNativeClass))

typedef struct _MetaCursorRendererNative        MetaCursorRendererNative;
typedef struct _MetaCursorRendererNativeClass   MetaCursorRendererNativeClass;

struct _MetaCursorRendererNative
{
  MetaCursorRenderer parent;
};

struct _MetaCursorRendererNativeClass
{
  MetaCursorRendererClass parent_class;
};

GType meta_cursor_renderer_native_get_type (void) G_GNUC_CONST;

struct gbm_device * meta_cursor_renderer_native_get_gbm_device (MetaCursorRendererNative *renderer);
void meta_cursor_renderer_native_force_update (MetaCursorRendererNative *renderer);

#endif /* META_CURSOR_RENDERER_NATIVE_H */
