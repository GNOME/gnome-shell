/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 Intel Corporation
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
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_PATH_H__
#define __CLUTTER_PATH_H__

#include <glib-object.h>
#include <clutter/clutter-types.h>
#include <cairo.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_PATH               (clutter_path_get_type ())
#define CLUTTER_TYPE_PATH_NODE          (clutter_path_node_get_type ())
#define CLUTTER_PATH(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_PATH, ClutterPath))
#define CLUTTER_PATH_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_PATH, ClutterPathClass))
#define CLUTTER_IS_PATH(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_PATH))
#define CLUTTER_IS_PATH_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_PATH))
#define CLUTTER_PATH_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_PATH, ClutterPathClass))

#define CLUTTER_PATH_RELATIVE           (32)

/**
 * ClutterPathNodeType:
 * @CLUTTER_PATH_MOVE_TO: jump to the given position
 * @CLUTTER_PATH_LINE_TO: create a line from the last node to the
 *   given position
 * @CLUTTER_PATH_CURVE_TO: bezier curve using the last position and
 *   three control points.
 * @CLUTTER_PATH_CLOSE: create a line from the last node to the last
 *   %CLUTTER_PATH_MOVE_TO node.
 * @CLUTTER_PATH_REL_MOVE_TO: same as %CLUTTER_PATH_MOVE_TO but with
 *   coordinates relative to the last node.
 * @CLUTTER_PATH_REL_LINE_TO: same as %CLUTTER_PATH_LINE_TO but with
 *   coordinates relative to the last node.
 * @CLUTTER_PATH_REL_CURVE_TO: same as %CLUTTER_PATH_CURVE_TO but with
 *   coordinates relative to the last node.
 *
 * Types of nodes in a #ClutterPath.
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_PATH_MOVE_TO      = 0,
  CLUTTER_PATH_LINE_TO      = 1,
  CLUTTER_PATH_CURVE_TO     = 2,
  CLUTTER_PATH_CLOSE        = 3,

  CLUTTER_PATH_REL_MOVE_TO  = CLUTTER_PATH_MOVE_TO | CLUTTER_PATH_RELATIVE,
  CLUTTER_PATH_REL_LINE_TO  = CLUTTER_PATH_LINE_TO | CLUTTER_PATH_RELATIVE,
  CLUTTER_PATH_REL_CURVE_TO = CLUTTER_PATH_CURVE_TO | CLUTTER_PATH_RELATIVE
} ClutterPathNodeType;

typedef struct _ClutterPath        ClutterPath;
typedef struct _ClutterPathClass   ClutterPathClass;
typedef struct _ClutterPathPrivate ClutterPathPrivate;
typedef struct _ClutterPathNode    ClutterPathNode;

/**
 * ClutterPathCallback:
 * @node: the node
 * @data: optional data passed to the function
 *
 * This function is passed to clutter_path_foreach() and will be
 * called for each node contained in the path.
 *
 * Since: 1.0
 */
typedef void (* ClutterPathCallback) (const ClutterPathNode *node,
                                      gpointer               data);

/**
 * ClutterPath:
 *
 * The #ClutterPath struct contains only private data and should
 * be accessed with the functions below.
 *
 * Since: 1.0
 */
struct _ClutterPath
{
  /*< private >*/
  GInitiallyUnowned parent;

  ClutterPathPrivate *priv;
};

/**
 * ClutterPathClass:
 *
 * The #ClutterPathClass struct contains only private data.
 *
 * Since: 1.0
 */
struct _ClutterPathClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;
};

/**
 * ClutterPathNode:
 * @type: the node's type
 * @points: the coordinates of the node
 *
 * Represents a single node of a #ClutterPath.
 *
 * Some of the coordinates in @points may be unused for some node
 * types. %CLUTTER_PATH_MOVE_TO and %CLUTTER_PATH_LINE_TO use only one
 * pair of coordinates, %CLUTTER_PATH_CURVE_TO uses all three and
 * %CLUTTER_PATH_CLOSE uses none.
 *
 * Since: 1.0
 */
struct _ClutterPathNode
{
  ClutterPathNodeType type;

  ClutterKnot points[3];
};

GType clutter_path_get_type (void) G_GNUC_CONST;
GType clutter_path_node_get_type (void) G_GNUC_CONST;

ClutterPath *clutter_path_new                  (void);
ClutterPath *clutter_path_new_with_description (const gchar           *desc);
void         clutter_path_add_move_to          (ClutterPath           *path,
                                                gint                   x,
                                                gint                   y);
void         clutter_path_add_rel_move_to      (ClutterPath           *path,
                                                gint                   x,
                                                gint                   y);
void         clutter_path_add_line_to          (ClutterPath           *path,
                                                gint                   x,
                                                gint                   y);
void         clutter_path_add_rel_line_to      (ClutterPath           *path,
                                                gint                   x,
                                                gint                   y);
void         clutter_path_add_curve_to         (ClutterPath           *path,
                                                gint                   x_1,
                                                gint                   y_1,
                                                gint                   x_2,
                                                gint                   y_2,
                                                gint                   x_3,
                                                gint                   y_3);
void         clutter_path_add_rel_curve_to     (ClutterPath           *path,
                                                gint                   x_1,
                                                gint                   y_1,
                                                gint                   x_2,
                                                gint                   y_2,
                                                gint                   x_3,
                                                gint                   y_3);
void         clutter_path_add_close            (ClutterPath           *path);
gboolean     clutter_path_add_string           (ClutterPath           *path,
                                                const gchar           *str);
void         clutter_path_add_node             (ClutterPath           *path,
                                                const ClutterPathNode *node);
void         clutter_path_add_cairo_path       (ClutterPath           *path,
                                                const cairo_path_t    *cpath);
guint        clutter_path_get_n_nodes          (ClutterPath           *path);
void         clutter_path_get_node             (ClutterPath           *path,
                                                guint                  index_,
                                                ClutterPathNode       *node);
GSList *     clutter_path_get_nodes            (ClutterPath           *path);
void         clutter_path_foreach              (ClutterPath           *path,
                                                ClutterPathCallback    callback,
                                                gpointer               user_data);
void         clutter_path_insert_node          (ClutterPath           *path,
                                                gint                   index_,
                                                const ClutterPathNode *node);
void         clutter_path_remove_node          (ClutterPath           *path,
                                                guint                  index_);
void         clutter_path_replace_node         (ClutterPath           *path,
                                                guint                  index_,
                                                const ClutterPathNode *node);
gchar *      clutter_path_get_description      (ClutterPath           *path);
gboolean     clutter_path_set_description      (ClutterPath           *path,
                                                const gchar           *str);
void         clutter_path_clear                (ClutterPath           *path);
void         clutter_path_to_cairo_path        (ClutterPath           *path,
                                                cairo_t               *cr);
guint        clutter_path_get_position         (ClutterPath           *path,
                                                gdouble                progress,
                                                ClutterKnot           *position);
guint        clutter_path_get_length           (ClutterPath           *path);

ClutterPathNode *clutter_path_node_copy  (const ClutterPathNode *node);
void             clutter_path_node_free  (ClutterPathNode       *node);
gboolean         clutter_path_node_equal (const ClutterPathNode *node_a,
                                          const ClutterPathNode *node_b);

G_END_DECLS

#endif /* __CLUTTER_PATH_H__ */
