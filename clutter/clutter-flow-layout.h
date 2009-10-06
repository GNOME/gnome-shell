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
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_FLOW_LAYOUT_H__
#define __CLUTTER_FLOW_LAYOUT_H__

#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_FLOW_LAYOUT                (clutter_flow_layout_get_type ())
#define CLUTTER_FLOW_LAYOUT(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_FLOW_LAYOUT, ClutterFlowLayout))
#define CLUTTER_IS_FLOW_LAYOUT(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_FLOW_LAYOUT))
#define CLUTTER_FLOW_LAYOUT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_FLOW_LAYOUT, ClutterFlowLayoutClass))
#define CLUTTER_IS_FLOW_LAYOUT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_FLOW_LAYOUT))
#define CLUTTER_FLOW_LAYOUT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_FLOW_LAYOUT, ClutterFlowLayoutClass))

typedef struct _ClutterFlowLayout               ClutterFlowLayout;
typedef struct _ClutterFlowLayoutPrivate        ClutterFlowLayoutPrivate;
typedef struct _ClutterFlowLayoutClass          ClutterFlowLayoutClass;

/**
 * ClutterFlowOrientation:
 * @CLUTTER_FLOW_HORIZONTAL: Arrange the children of the flow layout
 *   horizontally first
 * @CLUTTER_FLOW_VERTICAL: Arrange the children of the flow layout
 *   vertically first
 *
 * The direction of the arrangement of the children inside
 * a #ClutterFlowLayout
 *
 * Since: 1.2
 */
typedef enum { /*< prefix=CLUTTER_FLOW >*/
  CLUTTER_FLOW_HORIZONTAL,
  CLUTTER_FLOW_VERTICAL
} ClutterFlowOrientation;

/**
 * ClutterFlowLayout:
 *
 * The #ClutterFlowLayout structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterFlowLayout
{
  /*< private >*/
  ClutterLayoutManager parent_instance;

  ClutterFlowLayoutPrivate *priv;
};

/**
 * ClutterFlowLayoutClass:
 *
 * The #ClutterFlowLayoutClass structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterFlowLayoutClass
{
  /*< private >*/
  ClutterLayoutManagerClass parent_class;
};

GType clutter_flow_layout_get_type (void) G_GNUC_CONST;

ClutterLayoutManager * clutter_flow_layout_new                (ClutterFlowOrientation  orientation);

void                   clutter_flow_layout_set_orientation    (ClutterFlowLayout      *layout,
                                                               ClutterFlowOrientation  orientation);
ClutterFlowOrientation clutter_flow_layout_get_orientation    (ClutterFlowLayout      *layout);
void                   clutter_flow_layout_set_wrap           (ClutterFlowLayout      *layout,
                                                               gboolean                wrap);
gboolean               clutter_flow_layout_get_wrap           (ClutterFlowLayout      *layout);
void                   clutter_flow_layout_set_homogeneous    (ClutterFlowLayout      *layout,
                                                               gboolean                homogeneous);
gboolean               clutter_flow_layout_get_homogeneous    (ClutterFlowLayout      *layout);

void                   clutter_flow_layout_set_column_spacing (ClutterFlowLayout      *layout,
                                                               gfloat                  spacing);
gfloat                 clutter_flow_layout_get_column_spacing (ClutterFlowLayout      *layout);
void                   clutter_flow_layout_set_row_spacing    (ClutterFlowLayout      *layout,
                                                               gfloat                  spacing);
gfloat                 clutter_flow_layout_get_row_spacing    (ClutterFlowLayout      *layout);

void                   clutter_flow_layout_set_column_width   (ClutterFlowLayout      *layout,
                                                               gfloat                  min_width,
                                                               gfloat                  max_width);
void                   clutter_flow_layout_get_column_width   (ClutterFlowLayout      *layout,
                                                               gfloat                 *min_width,
                                                               gfloat                 *max_width);
void                   clutter_flow_layout_set_row_height     (ClutterFlowLayout      *layout,
                                                               gfloat                  min_height,
                                                               gfloat                  max_height);
void                   clutter_flow_layout_get_row_height     (ClutterFlowLayout      *layout,
                                                               gfloat                 *min_height,
                                                               gfloat                 *max_height);

G_END_DECLS

#endif /* __CLUTTER_FLOW_LAYOUT_H__ */
