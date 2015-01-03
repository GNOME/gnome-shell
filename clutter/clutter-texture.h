/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_TEXTURE_H__
#define __CLUTTER_TEXTURE_H__

#include <cogl/cogl.h>
#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TEXTURE            (clutter_texture_get_type ())
#define CLUTTER_TEXTURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TEXTURE, ClutterTexture))
#define CLUTTER_TEXTURE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TEXTURE, ClutterTextureClass))
#define CLUTTER_IS_TEXTURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TEXTURE))
#define CLUTTER_IS_TEXTURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TEXTURE))
#define CLUTTER_TEXTURE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TEXTURE, ClutterTextureClass))

/**
 * ClutterTextureError:
 * @CLUTTER_TEXTURE_ERROR_OUT_OF_MEMORY: OOM condition
 * @CLUTTER_TEXTURE_ERROR_NO_YUV: YUV operation attempted but no YUV support
 *   found
 * @CLUTTER_TEXTURE_ERROR_BAD_FORMAT: The requested format for
 * clutter_texture_set_from_rgb_data or
 * clutter_texture_set_from_yuv_data is unsupported.
 *
 * Error enumeration for #ClutterTexture
 *
 * Since: 0.4
 */
typedef enum {
  CLUTTER_TEXTURE_ERROR_OUT_OF_MEMORY,
  CLUTTER_TEXTURE_ERROR_NO_YUV,
  CLUTTER_TEXTURE_ERROR_BAD_FORMAT
} ClutterTextureError;

/**
 * CLUTTER_TEXTURE_ERROR:
 *
 * Error domain for #ClutterTexture errors
 *
 * Since: 0.4
 */
#define CLUTTER_TEXTURE_ERROR   (clutter_texture_error_quark ())
CLUTTER_AVAILABLE_IN_ALL
GQuark clutter_texture_error_quark (void);

typedef struct _ClutterTexture        ClutterTexture;
typedef struct _ClutterTextureClass   ClutterTextureClass;
typedef struct _ClutterTexturePrivate ClutterTexturePrivate;

/**
 * ClutterTexture:
 *
 * The #ClutterTexture structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 0.1
 */
struct _ClutterTexture
{
  /*< private >*/
  ClutterActor         parent;

  ClutterTexturePrivate *priv;
};

/**
 * ClutterTextureClass:
 * @size_change: handler for the #ClutterTexture::size-change signal
 * @pixbuf_change: handler for the #ClutterTexture::pixbuf-change signal
 * @load_finished: handler for the #ClutterTexture::load-finished signal
 *
 * The #ClutterTextureClass structure contains only private data
 *
 * Since: 0.1
 */
struct _ClutterTextureClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /*< public >*/
  void (* size_change)   (ClutterTexture *texture,
                          gint            width,
                          gint            height);
  void (* pixbuf_change) (ClutterTexture *texture);
  void (* load_finished) (ClutterTexture *texture,
                          const GError   *error);

  /*< private >*/
  /* padding, for future expansion */
  void (*_clutter_texture1) (void);
  void (*_clutter_texture2) (void);
  void (*_clutter_texture3) (void);
  void (*_clutter_texture4) (void);
  void (*_clutter_texture5) (void);
};

CLUTTER_AVAILABLE_IN_ALL
GType clutter_texture_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_TEXTURE_H__ */
