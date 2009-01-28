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

#ifndef __MUTTER_SHAPED_TEXTURE_H__
#define __MUTTER_SHAPED_TEXTURE_H__

#include <clutter/clutter.h>
#ifdef HAVE_GLX_TEXTURE_PIXMAP
#include <clutter/glx/clutter-glx.h>
#endif /* HAVE_GLX_TEXTURE_PIXMAP */

G_BEGIN_DECLS

#define MUTTER_TYPE_SHAPED_TEXTURE			    \
  (mutter_shaped_texture_get_type())
#define MUTTER_SHAPED_TEXTURE(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                       \
                               MUTTER_TYPE_SHAPED_TEXTURE,  \
                               MutterShapedTexture))
#define MUTTER_SHAPED_TEXTURE_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),			    \
                            MUTTER_TYPE_SHAPED_TEXTURE,	    \
                            MutterShapedTextureClass))
#define MUTTER_IS_SHAPED_TEXTURE(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),			    \
                               MUTTER_TYPE_SHAPED_TEXTURE))
#define MUTTER_IS_SHAPED_TEXTURE_CLASS(klass)		    \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),			    \
                            MUTTER_TYPE_SHAPED_TEXTURE))
#define MUTTER_SHAPED_TEXTURE_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                        \
                              MUTTER_TYPE_SHAPED_TEXTURE,   \
                              MutterShapedTextureClass))

typedef struct _MutterShapedTexture        MutterShapedTexture;
typedef struct _MutterShapedTextureClass   MutterShapedTextureClass;
typedef struct _MutterShapedTexturePrivate MutterShapedTexturePrivate;

struct _MutterShapedTextureClass
{
#ifdef HAVE_GLX_TEXTURE_PIXMAP
  ClutterGLXTexturePixmapClass parent_class;
#else
  ClutterX11TexturePixmapClass parent_class;
#endif
};

struct _MutterShapedTexture
{
#ifdef HAVE_GLX_TEXTURE_PIXMAP
  ClutterGLXTexturePixmap parent;
#else
  ClutterX11TexturePixmap parent;
#endif

  MutterShapedTexturePrivate *priv;
};

GType mutter_shaped_texture_get_type (void) G_GNUC_CONST;

ClutterActor *mutter_shaped_texture_new (void);

void mutter_shaped_texture_clear_rectangles (MutterShapedTexture *stex);

void mutter_shaped_texture_add_rectangle (MutterShapedTexture *stex,
					  const XRectangle *rect);
void mutter_shaped_texture_add_rectangles (MutterShapedTexture *stex,
					   size_t num_rects,
					   const XRectangle *rects);

G_END_DECLS

#endif /* __MUTTER_SHAPED_TEXTURE_H__ */
