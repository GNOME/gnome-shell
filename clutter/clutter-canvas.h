/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
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

#ifndef __CLUTTER_CANVAS_H__
#define __CLUTTER_CANVAS_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CANVAS             (clutter_canvas_get_type ())
#define CLUTTER_CANVAS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CANVAS, ClutterCanvas))
#define CLUTTER_IS_CANVAS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CANVAS))
#define CLUTTER_CANVAS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_CANVAS, ClutterCanvasClass))
#define CLUTTER_IS_CANVAS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_CANVAS))
#define CLUTTER_CANVAS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_CANVAS, ClutterCanvasClass))

typedef struct _ClutterCanvas           ClutterCanvas;
typedef struct _ClutterCanvasPrivate    ClutterCanvasPrivate;
typedef struct _ClutterCanvasClass      ClutterCanvasClass;

/**
 * ClutterCanvas:
 *
 * The <structname>ClutterCanvas</structname> structure contains
 * private data and should only be accessed using the provided
 * API.
 *
 * Since: 1.10
 */
struct _ClutterCanvas
{
  /*< private >*/
  GObject parent_instance;

  ClutterCanvasPrivate *priv;
};

/**
 * ClutterCanvasClass:
 * @draw: class handler for the #ClutterCanvas::draw signal
 *
 * The <structname>ClutterCanvasClass</structname> structure contains
 * private data.
 *
 * Since: 1.10
 */
struct _ClutterCanvasClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  gboolean (* draw) (ClutterCanvas *canvas,
                     cairo_t       *cr,
                     int            width,
                     int            height);

  /*< private >*/
  gpointer _padding[16];
};

CLUTTER_AVAILABLE_IN_1_10
GType clutter_canvas_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_10
ClutterContent *        clutter_canvas_new                      (void);
CLUTTER_AVAILABLE_IN_1_10
gboolean                clutter_canvas_set_size                 (ClutterCanvas *canvas,
                                                                 int            width,
                                                                 int            height);

CLUTTER_AVAILABLE_IN_1_18
void                    clutter_canvas_set_scale_factor         (ClutterCanvas *canvas,
                                                                 int            scale);
CLUTTER_AVAILABLE_IN_1_18
int                     clutter_canvas_get_scale_factor         (ClutterCanvas *canvas);

G_END_DECLS

#endif /* __CLUTTER_CANVAS_H__ */
