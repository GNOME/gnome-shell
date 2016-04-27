/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2012 Bastian Winkler <buz@netbuz.org>
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
 *   Bastian Winkler <buz@netbuz.org>
 *
 * Based on GtkGrid widget by:
 *   Matthias Clasen
 */

#ifndef __CLUTTER_GRID_LAYOUT_H__
#define __CLUTTER_GRID_LAYOUT_H__

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_GRID_LAYOUT                 (clutter_grid_layout_get_type ())
#define CLUTTER_GRID_LAYOUT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_GRID_LAYOUT, ClutterGridLayout))
#define CLUTTER_IS_GRID_LAYOUT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_GRID_LAYOUT))
#define CLUTTER_GRID_LAYOUT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_GRID_LAYOUT, ClutterGridLayoutClass))
#define CLUTTER_IS_GRID_LAYOUT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_GRID_LAYOUT))
#define CLUTTER_GRID_LAYOUT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_GRID_LAYOUT, ClutterGridLayoutClass))

typedef struct _ClutterGridLayout                ClutterGridLayout;
typedef struct _ClutterGridLayoutPrivate         ClutterGridLayoutPrivate;
typedef struct _ClutterGridLayoutClass           ClutterGridLayoutClass;

/**
 * ClutterGridLayout:
 *
 * The #ClutterGridLayout structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.12
 */
struct _ClutterGridLayout
{
  /*< private >*/
  ClutterLayoutManager parent_instance;

  ClutterGridLayoutPrivate *priv;
};

/**
 * ClutterGridLayoutClass:
 *
 * The #ClutterGridLayoutClass structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.12
 */
struct _ClutterGridLayoutClass
{
  /*< private >*/
  ClutterLayoutManagerClass parent_class;

  gpointer _padding[8];
};

CLUTTER_AVAILABLE_IN_1_12
GType clutter_grid_layout_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
ClutterLayoutManager *  clutter_grid_layout_new                 (void);

CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_attach                      (ClutterGridLayout *layout,
                                                                     ClutterActor      *child,
                                                                     gint               left,
                                                                     gint               top,
                                                                     gint               width,
                                                                     gint               height);

CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_attach_next_to              (ClutterGridLayout   *layout,
                                                                     ClutterActor        *child,
                                                                     ClutterActor        *sibling,
                                                                     ClutterGridPosition  side,
                                                                     gint                 width,
                                                                     gint                 height);

CLUTTER_AVAILABLE_IN_1_12
ClutterActor *      clutter_grid_layout_get_child_at                (ClutterGridLayout *layout,
                                                                     gint               left,
                                                                     gint               top);

CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_insert_row                  (ClutterGridLayout *layout,
                                                                     gint               position);

CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_insert_column               (ClutterGridLayout *layout,
                                                                     gint               position);

CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_insert_next_to              (ClutterGridLayout   *layout,
                                                                     ClutterActor        *sibling,
                                                                     ClutterGridPosition  side);

CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_set_orientation             (ClutterGridLayout *layout,
                                                                     ClutterOrientation orientation);

CLUTTER_AVAILABLE_IN_1_12
ClutterOrientation  clutter_grid_layout_get_orientation             (ClutterGridLayout *layout);

CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_set_column_spacing          (ClutterGridLayout *layout,
                                                                     guint              spacing);

CLUTTER_AVAILABLE_IN_1_12
guint               clutter_grid_layout_get_column_spacing          (ClutterGridLayout *layout);

CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_set_row_spacing             (ClutterGridLayout *layout,
                                                                     guint              spacing);

CLUTTER_AVAILABLE_IN_1_12
guint               clutter_grid_layout_get_row_spacing             (ClutterGridLayout *layout);

CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_set_column_homogeneous      (ClutterGridLayout *layout,
                                                                     gboolean           homogeneous);

CLUTTER_AVAILABLE_IN_1_12
gboolean            clutter_grid_layout_get_column_homogeneous      (ClutterGridLayout *layout);


CLUTTER_AVAILABLE_IN_1_12
void                clutter_grid_layout_set_row_homogeneous         (ClutterGridLayout *layout,
                                                                     gboolean           homogeneous);

CLUTTER_AVAILABLE_IN_1_12
gboolean            clutter_grid_layout_get_row_homogeneous         (ClutterGridLayout *layout);

G_END_DECLS

#endif /* __CLUTTER_GRID_LAYOUT_H__ */
