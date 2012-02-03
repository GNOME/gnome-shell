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

#ifndef __CLUTTER_PAINT_NODE_PRIVATE_H__
#define __CLUTTER_PAINT_NODE_PRIVATE_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <clutter/clutter-paint-node.h>

G_BEGIN_DECLS

#define CLUTTER_PAINT_NODE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_PAINT_NODE, ClutterPaintNodeClass))
#define CLUTTER_IS_PAINT_NODE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_PAINT_NODE))
#define CLUTTER_PAINT_NODE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_PAINT_NODE, ClutterPaintNodeClass))

typedef struct _ClutterPaintOperation   ClutterPaintOperation;

struct _ClutterPaintNode
{
  GTypeInstance parent_instance;

  ClutterPaintNode *parent;

  ClutterPaintNode *first_child;
  ClutterPaintNode *prev_sibling;
  ClutterPaintNode *next_sibling;
  ClutterPaintNode *last_child;

  guint n_children;

  GArray *operations;

  gchar *name;

  volatile int ref_count;
};

struct _ClutterPaintNodeClass
{
  GTypeClass base_class;

  void     (* finalize)  (ClutterPaintNode *node);

  gboolean (* pre_draw)  (ClutterPaintNode *node);
  void     (* draw)      (ClutterPaintNode *node);
  void     (* post_draw) (ClutterPaintNode *node);

  JsonNode*(* serialize) (ClutterPaintNode *node);
};

typedef enum {
  PAINT_OP_INVALID = 0,
  PAINT_OP_TEX_RECT,
  PAINT_OP_PATH,
  PAINT_OP_PRIMITIVE
} PaintOpCode;

struct _ClutterPaintOperation
{
  PaintOpCode opcode;

  union {
    float texrect[8];

    CoglPath *path;

    CoglPrimitive *primitive;
  } op;
};

GType _clutter_root_node_get_type (void) G_GNUC_CONST;
GType _clutter_transform_node_get_type (void) G_GNUC_CONST;
GType _clutter_dummy_node_get_type (void) G_GNUC_CONST;

void                    _clutter_paint_operation_paint_rectangle        (const ClutterPaintOperation *op);
void                    _clutter_paint_operation_clip_rectangle         (const ClutterPaintOperation *op);
void                    _clutter_paint_operation_paint_path             (const ClutterPaintOperation *op);
void                    _clutter_paint_operation_clip_path              (const ClutterPaintOperation *op);
void                    _clutter_paint_operation_paint_primitive        (const ClutterPaintOperation *op);

void                    _clutter_paint_node_init_types                  (void);
gpointer                _clutter_paint_node_internal                    (GType gtype);

ClutterPaintNode *      _clutter_root_node_new                          (CoglFramebuffer             *framebuffer,
                                                                         const ClutterColor          *clear_color,
                                                                         CoglBufferBit                clear_flags,
                                                                         const CoglMatrix            *matrix);
ClutterPaintNode *      _clutter_transform_node_new                     (const CoglMatrix            *matrix);
ClutterPaintNode *      _clutter_dummy_node_new                         (void);

void                    _clutter_paint_node_paint                       (ClutterPaintNode            *root);
void                    _clutter_paint_node_dump_tree                   (ClutterPaintNode            *root);

G_END_DECLS

#endif /* __CLUTTER_PAINT_NODE_PRIVATE_H__ */
