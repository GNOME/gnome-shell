/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 Intel Corporation
 * Copyright (C) 2013 Erick Pérez Castellanos <erick.red@gmail.com>
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

#include <cairo.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_PATH               (clutter_path_get_type ())
#define CLUTTER_TYPE_PATH_NODE          (clutter_path_node_get_type ())
#define CLUTTER_PATH(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_PATH, ClutterPath))
#define CLUTTER_PATH_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_PATH, ClutterPathClass))
#define CLUTTER_IS_PATH(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_PATH))
#define CLUTTER_IS_PATH_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_PATH))
#define CLUTTER_PATH_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_PATH, ClutterPathClass))

typedef struct _ClutterPathClass   ClutterPathClass;
typedef struct _ClutterPathPrivate ClutterPathPrivate;

/**
 * ClutterPathCallback:
 * @node: the node
 * @data: (closure): optional data passed to the function
 *
 * This function is passed to clutter_path_foreach() and will be
 * called for each node contained in the path.
 *
 *
 */
typedef void (* ClutterPathCallback) (const ClutterPathNode *node,
                                      gpointer               data);

/**
 * ClutterPath:
 *
 * The #ClutterPath struct contains only private data and should
 * be accessed with the functions below.
 *
 *
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
 *
 */
struct _ClutterPathClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;
};

GType clutter_path_get_type (void) G_GNUC_CONST;

ClutterPath *clutter_path_new                  (void);
ClutterPath *clutter_path_new_with_description (const gchar           *desc);
void         clutter_path_add_move_to          (ClutterPath           *path,
                                                gfloat                 x,
                                                gfloat                 y);
void         clutter_path_add_rel_move_to      (ClutterPath           *path,
                                                gfloat                 x,
                                                gfloat                 y);
void         clutter_path_add_line_to          (ClutterPath           *path,
                                                gfloat                 x,
                                                gfloat                 y);
void         clutter_path_add_rel_line_to      (ClutterPath           *path,
                                                gfloat                 x,
                                                gfloat                 y);
void         clutter_path_add_curve_to         (ClutterPath           *path,
                                                gfloat                 x_1,
                                                gfloat                 y_1,
                                                gfloat                 x_2,
                                                gfloat                 y_2,
                                                gfloat                 x_3,
                                                gfloat                 y_3);
void         clutter_path_add_rel_curve_to     (ClutterPath           *path,
                                                gfloat                 x_1,
                                                gfloat                 y_1,
                                                gfloat                 x_2,
                                                gfloat                 y_2,
                                                gfloat                 x_3,
                                                gfloat                 y_3);
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
                                                ClutterPoint          *position);
guint        clutter_path_get_length           (ClutterPath           *path);

G_END_DECLS

#endif /* __CLUTTER_PATH_H__ */
