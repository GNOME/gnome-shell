/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation.
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

#ifndef __CLUTTER_PAINT_NODE_H__
#define __CLUTTER_PAINT_NODE_H__

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#include <cogl/cogl.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_PAINT_NODE                 (clutter_paint_node_get_type ())
#define CLUTTER_PAINT_NODE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_PAINT_NODE, ClutterPaintNode))
#define CLUTTER_IS_PAINT_NODE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_PAINT_NODE))

typedef struct _ClutterPaintNodePrivate ClutterPaintNodePrivate;
typedef struct _ClutterPaintNodeClass   ClutterPaintNodeClass;

CLUTTER_AVAILABLE_IN_1_10
GType clutter_paint_node_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_10
ClutterPaintNode *      clutter_paint_node_ref                          (ClutterPaintNode      *node);
CLUTTER_AVAILABLE_IN_1_10
void                    clutter_paint_node_unref                        (ClutterPaintNode      *node);

CLUTTER_AVAILABLE_IN_1_10
void                    clutter_paint_node_set_name                     (ClutterPaintNode      *node,
                                                                         const char            *name);

CLUTTER_AVAILABLE_IN_1_10
void                    clutter_paint_node_add_child                    (ClutterPaintNode      *node,
                                                                         ClutterPaintNode      *child);
CLUTTER_AVAILABLE_IN_1_10
void                    clutter_paint_node_add_rectangle                (ClutterPaintNode      *node,
                                                                         const ClutterActorBox *rect);
CLUTTER_AVAILABLE_IN_1_10
void                    clutter_paint_node_add_texture_rectangle        (ClutterPaintNode      *node,
                                                                         const ClutterActorBox *rect,
                                                                         float                  x_1,
                                                                         float                  y_1,
                                                                         float                  x_2,
                                                                         float                  y_2);

CLUTTER_AVAILABLE_IN_1_10
void                    clutter_paint_node_add_path                     (ClutterPaintNode      *node,
                                                                         CoglPath              *path);
CLUTTER_AVAILABLE_IN_1_10
void                    clutter_paint_node_add_primitive                (ClutterPaintNode      *node,
                                                                         CoglPrimitive         *primitive);

/**
 * CLUTTER_VALUE_HOLDS_PAINT_NODE:
 * @value: a #GValue
 *
 * Evaluates to %TRUE if the @value has been initialized to hold
 * a #ClutterPaintNode.
 *
 * Since: 1.10
 */
#define CLUTTER_VALUE_HOLDS_PAINT_NODE(value)   (G_VALUE_HOLDS (value, CLUTTER_TYPE_PAINT_NODE))

CLUTTER_AVAILABLE_IN_1_10
void                    clutter_value_set_paint_node                    (GValue                *value,
                                                                         gpointer               node);
CLUTTER_AVAILABLE_IN_1_10
void                    clutter_value_take_paint_node                   (GValue                *value,
                                                                         gpointer               node);
CLUTTER_AVAILABLE_IN_1_10
gpointer                clutter_value_get_paint_node                    (const GValue          *value);
CLUTTER_AVAILABLE_IN_1_10
gpointer                clutter_value_dup_paint_node                    (const GValue          *value);

G_END_DECLS

#endif /* __CLUTTER_PAINT_NODE_H__ */
