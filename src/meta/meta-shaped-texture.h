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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __META_SHAPED_TEXTURE_H__
#define __META_SHAPED_TEXTURE_H__

#include <clutter/clutter.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

#define META_TYPE_SHAPED_TEXTURE            (meta_shaped_texture_get_type())
#define META_SHAPED_TEXTURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),META_TYPE_SHAPED_TEXTURE, MetaShapedTexture))
#define META_SHAPED_TEXTURE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_SHAPED_TEXTURE, MetaShapedTextureClass))
#define META_IS_SHAPED_TEXTURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_SHAPED_TEXTURE))
#define META_IS_SHAPED_TEXTURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_SHAPED_TEXTURE))
#define META_SHAPED_TEXTURE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_SHAPED_TEXTURE, MetaShapedTextureClass))

typedef struct _MetaShapedTexture        MetaShapedTexture;
typedef struct _MetaShapedTextureClass   MetaShapedTextureClass;
typedef struct _MetaShapedTexturePrivate MetaShapedTexturePrivate;

struct _MetaShapedTextureClass
{
  /*< private >*/
  ClutterActorClass parent_class;
};

/**
 * MetaShapedTexture:
 *
 * The <structname>MetaShapedTexture</structname> structure contains
 * only private data and should be accessed using the provided API
 */
struct _MetaShapedTexture
{
  /*< private >*/
  ClutterActor parent;

  MetaShapedTexturePrivate *priv;
};

GType meta_shaped_texture_get_type (void) G_GNUC_CONST;

ClutterActor *meta_shaped_texture_new (void);

void meta_shaped_texture_set_create_mipmaps (MetaShapedTexture *stex,
					     gboolean           create_mipmaps);

gboolean meta_shaped_texture_update_area (MetaShapedTexture *stex,
                                          int                x,
                                          int                y,
                                          int                width,
                                          int                height,
                                          cairo_region_t    *unobscured_region);

void meta_shaped_texture_set_pixmap (MetaShapedTexture *stex,
                                     Pixmap             pixmap);

CoglTexture * meta_shaped_texture_get_texture (MetaShapedTexture *stex);

void meta_shaped_texture_set_mask_texture (MetaShapedTexture *stex,
                                           CoglTexture       *mask_texture);
void meta_shaped_texture_set_input_shape_region (MetaShapedTexture *stex,
                                                 cairo_region_t    *shape_region);

void meta_shaped_texture_set_opaque_region (MetaShapedTexture *stex,
                                            cairo_region_t    *opaque_region);

cairo_surface_t * meta_shaped_texture_get_image (MetaShapedTexture     *stex,
                                                 cairo_rectangle_int_t *clip);

G_END_DECLS

#endif /* __META_SHAPED_TEXTURE_H__ */
