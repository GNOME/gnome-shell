/*
 * shaped texture
 *
 * An actor to draw a texture clipped to a list of rectangles
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2008 Intel Corporation
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

#ifndef __META_SHAPED_TEXTURE_H__
#define __META_SHAPED_TEXTURE_H__

#include <clutter/clutter-texture.h>

G_BEGIN_DECLS

#define META_TYPE_SHAPED_TEXTURE                                     \
  (meta_shaped_texture_get_type())
#define META_SHAPED_TEXTURE(obj)                                     \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                \
                               META_TYPE_SHAPED_TEXTURE,             \
                               MetaShapedTexture))
#define META_SHAPED_TEXTURE_CLASS(klass)                             \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                 \
                            META_TYPE_SHAPED_TEXTURE,                \
                            MetaShapedTextureClass))
#define META_IS_SHAPED_TEXTURE(obj)                                  \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                \
                               META_TYPE_SHAPED_TEXTURE))
#define META_IS_SHAPED_TEXTURE_CLASS(klass)                          \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                 \
                            META_TYPE_SHAPED_TEXTURE))
#define META_SHAPED_TEXTURE_GET_CLASS(obj)                           \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                 \
                              META_TYPE_SHAPED_TEXTURE,              \
                              MetaShapedTextureClass))

typedef struct _MetaShapedTexture        MetaShapedTexture;
typedef struct _MetaShapedTextureClass   MetaShapedTextureClass;
typedef struct _MetaShapedTexturePrivate MetaShapedTexturePrivate;

struct _MetaShapedTextureClass
{
  ClutterTextureClass parent_class;
};

struct _MetaShapedTexture
{
  ClutterTexture parent;

  MetaShapedTexturePrivate *priv;
};

GType meta_shaped_texture_get_type (void) G_GNUC_CONST;

ClutterActor *meta_shaped_texture_new (void);

void meta_shaped_texture_clear_rectangles (MetaShapedTexture *stex);

void meta_shaped_texture_add_rectangle (MetaShapedTexture *stex,
                                        const ClutterGeometry *rect);
void meta_shaped_texture_add_rectangles (MetaShapedTexture *stex,
                                         size_t num_rects,
                                         const ClutterGeometry *rects);

G_END_DECLS

#endif /* __META_SHAPED_TEXTURE_H__ */
