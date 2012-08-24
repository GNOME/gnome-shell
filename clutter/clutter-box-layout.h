/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 *
 * Based on the NBTK NbtkBoxLayout actor by:
 *   Thomas Wood <thomas.wood@intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BOX_LAYOUT_H__
#define __CLUTTER_BOX_LAYOUT_H__

#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BOX_LAYOUT                 (clutter_box_layout_get_type ())
#define CLUTTER_BOX_LAYOUT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BOX_LAYOUT, ClutterBoxLayout))
#define CLUTTER_IS_BOX_LAYOUT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BOX_LAYOUT))
#define CLUTTER_BOX_LAYOUT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BOX_LAYOUT, ClutterBoxLayoutClass))
#define CLUTTER_IS_BOX_LAYOUT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BOX_LAYOUT))
#define CLUTTER_BOX_LAYOUT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BOX_LAYOUT, ClutterBoxLayoutClass))

typedef struct _ClutterBoxLayout                ClutterBoxLayout;
typedef struct _ClutterBoxLayoutPrivate         ClutterBoxLayoutPrivate;
typedef struct _ClutterBoxLayoutClass           ClutterBoxLayoutClass;

/**
 * ClutterBoxLayout:
 *
 * The #ClutterBoxLayout structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterBoxLayout
{
  /*< private >*/
  ClutterLayoutManager parent_instance;

  ClutterBoxLayoutPrivate *priv;
};

/**
 * ClutterBoxLayoutClass:
 *
 * The #ClutterBoxLayoutClass structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterBoxLayoutClass
{
  /*< private >*/
  ClutterLayoutManagerClass parent_class;
};

GType clutter_box_layout_get_type (void) G_GNUC_CONST;

ClutterLayoutManager *  clutter_box_layout_new                 (void);

CLUTTER_AVAILABLE_IN_1_12
void                    clutter_box_layout_set_orientation      (ClutterBoxLayout    *layout,
                                                                 ClutterOrientation   orientation);
CLUTTER_AVAILABLE_IN_1_12
ClutterOrientation      clutter_box_layout_get_orientation      (ClutterBoxLayout    *layout);

void                    clutter_box_layout_set_spacing          (ClutterBoxLayout    *layout,
                                                                 guint                spacing);
guint                   clutter_box_layout_get_spacing          (ClutterBoxLayout    *layout);
void                    clutter_box_layout_set_homogeneous      (ClutterBoxLayout    *layout,
                                                                 gboolean             homogeneous);
gboolean                clutter_box_layout_get_homogeneous      (ClutterBoxLayout    *layout);
void                    clutter_box_layout_set_pack_start       (ClutterBoxLayout    *layout,
                                                                 gboolean             pack_start);
gboolean                clutter_box_layout_get_pack_start       (ClutterBoxLayout    *layout);

G_END_DECLS

#endif /* __CLUTTER_BOX_LAYOUT_H__ */
