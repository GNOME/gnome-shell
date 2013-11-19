/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 *   Jose Dapena Paz <jdapena@igalia.com>
 *
 * Based on the MX MxTable actor by:
 *   Thomas Wood <thomas.wood@intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_TABLE_LAYOUT_H__
#define __CLUTTER_TABLE_LAYOUT_H__

#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TABLE_LAYOUT                 (clutter_table_layout_get_type ())
#define CLUTTER_TABLE_LAYOUT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TABLE_LAYOUT, ClutterTableLayout))
#define CLUTTER_IS_TABLE_LAYOUT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TABLE_LAYOUT))
#define CLUTTER_TABLE_LAYOUT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TABLE_LAYOUT, ClutterTableLayoutClass))
#define CLUTTER_IS_TABLE_LAYOUT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TABLE_LAYOUT))
#define CLUTTER_TABLE_LAYOUT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TABLE_LAYOUT, ClutterTableLayoutClass))

typedef struct _ClutterTableLayout                ClutterTableLayout;
typedef struct _ClutterTableLayoutPrivate         ClutterTableLayoutPrivate;
typedef struct _ClutterTableLayoutClass           ClutterTableLayoutClass;

/**
 * ClutterTableLayout:
 *
 * The #ClutterTableLayout structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.4
 *
 * Deprecated: 1.18: Use #ClutterGridLayout instead
 */
struct _ClutterTableLayout
{
  /*< private >*/
  ClutterLayoutManager parent_instance;

  ClutterTableLayoutPrivate *priv;
};

/**
 * ClutterTableLayoutClass:
 *
 * The #ClutterTableLayoutClass structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.4
 *
 * Deprecated: 1.18: Use #ClutterGridLayout instead
 */
struct _ClutterTableLayoutClass
{
  /*< private >*/
  ClutterLayoutManagerClass parent_class;
};

CLUTTER_DEPRECATED_IN_1_18_FOR (clutter_grid_layout_get_type)
GType clutter_table_layout_get_type (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_18_FOR (clutter_grid_layout_new)
ClutterLayoutManager *clutter_table_layout_new                 (void);

CLUTTER_DEPRECATED_IN_1_18_FOR (clutter_grid_layout_attach)
void                  clutter_table_layout_pack                (ClutterTableLayout    *layout,
                                                                ClutterActor          *actor,
                                                                gint                   column,
                                                                gint                   row);

CLUTTER_DEPRECATED_IN_1_18_FOR (clutter_grid_layout_set_column_spacing)
void                  clutter_table_layout_set_column_spacing  (ClutterTableLayout    *layout,
                                                                guint                  spacing);
CLUTTER_DEPRECATED_IN_1_18_FOR (clutter_grid_layout_set_row_spacing)
void                  clutter_table_layout_set_row_spacing     (ClutterTableLayout    *layout,
                                                                guint                  spacing);
CLUTTER_DEPRECATED_IN_1_18_FOR (clutter_grid_layout_get_column_spacing)
guint                 clutter_table_layout_get_column_spacing  (ClutterTableLayout    *layout);
CLUTTER_DEPRECATED_IN_1_18_FOR (clutter_grid_layout_get_row_spacing)
guint                 clutter_table_layout_get_row_spacing     (ClutterTableLayout    *layout);

CLUTTER_DEPRECATED_IN_1_18
void                  clutter_table_layout_set_span            (ClutterTableLayout    *layout,
                                                                ClutterActor          *actor,
                                                                gint                   column_span,
                                                                gint                   row_span);
CLUTTER_DEPRECATED_IN_1_18
void                  clutter_table_layout_get_span            (ClutterTableLayout    *layout,
                                                                ClutterActor          *actor,
                                                                gint                  *column_span,
                                                                gint                  *row_span);

CLUTTER_DEPRECATED_IN_1_12
void                  clutter_table_layout_set_alignment       (ClutterTableLayout    *layout,
                                                                ClutterActor          *actor,
                                                                ClutterTableAlignment  x_align,
                                                                ClutterTableAlignment  y_align);
CLUTTER_DEPRECATED_IN_1_12
void                  clutter_table_layout_get_alignment       (ClutterTableLayout    *layout,
                                                                ClutterActor          *actor,
                                                                ClutterTableAlignment *x_align,
                                                                ClutterTableAlignment *y_align);
CLUTTER_DEPRECATED_IN_1_12
void                  clutter_table_layout_set_fill            (ClutterTableLayout    *layout,
                                                                ClutterActor          *actor,
                                                                gboolean               x_fill,
                                                                gboolean               y_fill);
CLUTTER_DEPRECATED_IN_1_12
void                  clutter_table_layout_get_fill            (ClutterTableLayout    *layout,
                                                                ClutterActor          *actor,
                                                                gboolean              *x_fill,
                                                                gboolean              *y_fill);
CLUTTER_DEPRECATED_IN_1_12
void                  clutter_table_layout_set_expand          (ClutterTableLayout    *layout,
                                                                ClutterActor          *actor,
                                                                gboolean               x_expand,
                                                                gboolean               y_expand);
CLUTTER_DEPRECATED_IN_1_12
void                  clutter_table_layout_get_expand          (ClutterTableLayout    *layout,
                                                                ClutterActor          *actor,
                                                                gboolean              *x_expand,
                                                                gboolean              *y_expand);

CLUTTER_DEPRECATED_IN_1_18
gint                  clutter_table_layout_get_row_count       (ClutterTableLayout    *layout);
CLUTTER_DEPRECATED_IN_1_18
gint                  clutter_table_layout_get_column_count    (ClutterTableLayout    *layout);

CLUTTER_DEPRECATED_IN_1_12
void                  clutter_table_layout_set_use_animations  (ClutterTableLayout    *layout,
                                                                gboolean               animate);
CLUTTER_DEPRECATED_IN_1_12
gboolean              clutter_table_layout_get_use_animations  (ClutterTableLayout    *layout);
CLUTTER_DEPRECATED_IN_1_12
void                  clutter_table_layout_set_easing_mode     (ClutterTableLayout    *layout,
                                                                gulong                 mode);
CLUTTER_DEPRECATED_IN_1_12
gulong                clutter_table_layout_get_easing_mode     (ClutterTableLayout    *layout);
CLUTTER_DEPRECATED_IN_1_12
void                  clutter_table_layout_set_easing_duration (ClutterTableLayout    *layout,
                                                                guint                  msecs);
CLUTTER_DEPRECATED_IN_1_12
guint                 clutter_table_layout_get_easing_duration (ClutterTableLayout    *layout);

G_END_DECLS

#endif /* __CLUTTER_TABLE_LAYOUT_H__ */
