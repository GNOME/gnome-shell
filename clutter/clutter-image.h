/*
 * Clutter.
 *
 * An OpenGL based 'interactive image' library.
 *
 * Copyright (C) 2012  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_IMAGE_H__
#define __CLUTTER_IMAGE_H__

#include <cogl/cogl.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_IMAGE              (clutter_image_get_type ())
#define CLUTTER_IMAGE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_IMAGE, ClutterImage))
#define CLUTTER_IS_IMAGE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_IMAGE))
#define CLUTTER_IMAGE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_IMAGE, ClutterImageClass))
#define CLUTTER_IS_IMAGE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_IMAGE))
#define CLUTTER_IMAGE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_IMAGE, ClutterImageClass))

/**
 * CLUTTER_IMAGE_ERROR:
 *
 * Error domain for the #ClutterImageError enumeration.
 *
 * Since: 1.10
 */
#define CLUTTER_IMAGE_ERROR             (clutter_image_error_quark ())

typedef struct _ClutterImage           ClutterImage;
typedef struct _ClutterImagePrivate    ClutterImagePrivate;
typedef struct _ClutterImageClass      ClutterImageClass;

/**
 * ClutterImageError:
 * @CLUTTER_IMAGE_ERROR_INVALID_DATA: Invalid data passed to the
 *   clutter_image_set_data() function.
 *
 * Error enumeration for #ClutterImage.
 *
 * Since: 1.10
 */
typedef enum {
  CLUTTER_IMAGE_ERROR_INVALID_DATA
} ClutterImageError;

/**
 * ClutterImage:
 *
 * The #ClutterImage structure contains
 * private data and should only be accessed using the provided
 * API.
 *
 * Since: 1.10
 */
struct _ClutterImage
{
  /*< private >*/
  GObject parent_instance;

  ClutterImagePrivate *priv;
};

/**
 * ClutterImageClass:
 *
 * The #ClutterImageClass structure contains
 * private data.
 *
 * Since: 1.10
 */
struct _ClutterImageClass
{
  /*< private >*/
  GObjectClass parent_class;

  gpointer _padding[16];
};

CLUTTER_AVAILABLE_IN_1_10
GQuark clutter_image_error_quark (void);
CLUTTER_AVAILABLE_IN_1_10
GType clutter_image_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_10
ClutterContent *        clutter_image_new               (void);
CLUTTER_AVAILABLE_IN_1_10
gboolean                clutter_image_set_data          (ClutterImage                 *image,
                                                         const guint8                 *data,
                                                         CoglPixelFormat               pixel_format,
                                                         guint                         width,
                                                         guint                         height,
                                                         guint                         row_stride,
                                                         GError                      **error);
CLUTTER_AVAILABLE_IN_1_10
gboolean                clutter_image_set_area          (ClutterImage                 *image,
                                                         const guint8                 *data,
                                                         CoglPixelFormat               pixel_format,
                                                         const cairo_rectangle_int_t  *rect,
                                                         guint                         row_stride,
                                                         GError                      **error);
CLUTTER_AVAILABLE_IN_1_12
gboolean                clutter_image_set_bytes         (ClutterImage                 *image,
                                                         GBytes                       *data,
                                                         CoglPixelFormat               pixel_format,
                                                         guint                         width,
                                                         guint                         height,
                                                         guint                         row_stride,
                                                         GError                      **error);

#if defined(COGL_ENABLE_EXPERIMENTAL_API) && defined(CLUTTER_ENABLE_EXPERIMENTAL_API)
CLUTTER_AVAILABLE_IN_1_10
CoglTexture *           clutter_image_get_texture       (ClutterImage                 *image);
#endif

G_END_DECLS

#endif /* __CLUTTER_IMAGE_H__ */
