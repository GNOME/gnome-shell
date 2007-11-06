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
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * ClutterLayout: interface to be implemented by actors providing
 *                extended layouts.
 *
 * Author: Emmanuele Bassi <ebassi@openedhand.com>
 */

#ifndef __CLUTTER_LAYOUT_H__
#define __CLUTTER_LAYOUT_H__

#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_LAYOUT             (clutter_layout_get_type ())
#define CLUTTER_LAYOUT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_LAYOUT, ClutterLayout))
#define CLUTTER_IS_LAYOUT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_LAYOUT))
#define CLUTTER_LAYOUT_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CLUTTER_TYPE_LAYOUT, ClutterLayoutIface))

#define CLUTTER_LAYOUT_HAS(layout,f)                    \
        (CLUTTER_IS_LAYOUT ((layout)) && \
         (clutter_layout_get_layout_flags ((ClutterLayout *) (layout)) & (f)))

#define CLUTTER_LAYOUT_HAS_WIDTH_FOR_HEIGHT(layout)     \
        (CLUTTER_LAYOUT_HAS ((layout), CLUTTER_LAYOUT_WIDTH_FOR_HEIGHT))
#define CLUTTER_LAYOUT_HAS_HEIGHT_FOR_WIDTH(layout)     \
        (CLUTTER_LAYOUT_HAS ((layout), CLUTTER_LAYOUT_HEIGHT_FOR_WIDTH))
#define CLUTTER_LAYOUT_HAS_NATURAL_SIZE(layout)         \
        (CLUTTER_LAYOUT_HAS ((layout), CLUTTER_LAYOUT_NATURAL))
#define CLUTTER_LAYOUT_HAS_TUNABLE_SIZE(layout)         \
        (CLUTTER_LAYOUT_HAS ((layout), CLUTTER_LAYOUT_TUNABLE))

/**
 * ClutterLayoutFlags
 * @CLUTTER_LAYOUT_NONE: No layout (default behaviour)
 * @CLUTTER_LAYOUT_WIDTH_FOR_HEIGHT: Width-for-height
 * @CLUTTER_LAYOUT_HEIGHT_FOR_WIDTH: Height-for-width
 * @CLUTTER_LAYOUT_NATURAL: Natural size request
 * @CLUTTER_LAYOUT_TUNABLE: Tunable size request
 *
 * Type of layouts supported by an actor.
 *
 * Since: 0.4
 */
typedef enum {
  CLUTTER_LAYOUT_NONE             = 0,
  CLUTTER_LAYOUT_WIDTH_FOR_HEIGHT = 1 << 0,
  CLUTTER_LAYOUT_HEIGHT_FOR_WIDTH = 1 << 1,
  CLUTTER_LAYOUT_NATURAL          = 1 << 2,
  CLUTTER_LAYOUT_TUNABLE          = 1 << 3
} ClutterLayoutFlags;

typedef struct _ClutterLayout           ClutterLayout; /* dummy */
typedef struct _ClutterLayoutIface      ClutterLayoutIface;

/**
 * ClutterLayoutIface:
 * @get_layout_flags: Retrieve the layout mode used by the actor
 * @width_for_height: Compute width for a given height
 * @height_for_width: Compute height for a given width
 * @natural_request: Natural size of an actor
 * @tune_request: Iterative size allocation
 *
 * Interface for extended layout support in actors.
 *
 * Since: 0.4
 */
struct _ClutterLayoutIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  /* Retrieve the layout mode used by the actor */
  ClutterLayoutFlags (* get_layout_flags) (ClutterLayout *layout);
  
  /* Width-for-Height and Height-for-Width: one size is known
   * and the other is queried. useful for labels and unidirectional
   * containers, like vertical and horizontal boxes.
   */
  void               (* width_for_height) (ClutterLayout *layout,
                                           ClutterUnit   *width,
                                           ClutterUnit    height);
  void               (* height_for_width) (ClutterLayout *layout,
                                           ClutterUnit    width,
                                           ClutterUnit   *height);

  /* Natural size request: the actor is queried for its natural
   * size and the container can decide to either scale the actor
   * or to resize itself to make it fit. useful for textures
   * or shapes.
   */
  void               (* natural_request)  (ClutterLayout *layout,
                                           ClutterUnit   *width,
                                           ClutterUnit   *height);

  /* Iterative allocation: the actor is iteratively queried
   * for its size, until it finds it.
   */
  gboolean           (* tune_request)     (ClutterLayout *layout,
                                           ClutterUnit    given_width,
                                           ClutterUnit    given_height,
                                           ClutterUnit   *width,
                                           ClutterUnit   *height);
};

GType              clutter_layout_get_type         (void) G_GNUC_CONST;

ClutterLayoutFlags clutter_layout_get_layout_flags (ClutterLayout *layout);
void               clutter_layout_width_for_height (ClutterLayout *layout,
                                                    gint          *width,
                                                    gint           height);
void               clutter_layout_height_for_width (ClutterLayout *layout,
                                                    gint           width,
                                                    gint          *height);
void               clutter_layout_natural_request  (ClutterLayout *layout,
                                                    gint          *width,
                                                    gint          *height);
void               clutter_layout_tune_request     (ClutterLayout *layout,
                                                    gint           given_width,
                                                    gint           given_height,
                                                    gint          *width,
                                                    gint          *height);

G_END_DECLS

#endif /* __CLUTTER_LAYOUT_H__ */
