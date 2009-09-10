/*
 * nbtk-box-layout-child.h: box layout child actor
 *
 * Copyright 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Written by: Thomas Wood <thomas.wood@intel.com>
 */

#ifndef _NBTK_BOX_LAYOUT_CHILD_H
#define _NBTK_BOX_LAYOUT_CHILD_H

#include <clutter/clutter.h>
#include "nbtk-enum-types.h"
#include "nbtk-box-layout.h"

G_BEGIN_DECLS

#define NBTK_TYPE_BOX_LAYOUT_CHILD nbtk_box_layout_child_get_type()

#define NBTK_BOX_LAYOUT_CHILD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  NBTK_TYPE_BOX_LAYOUT_CHILD, NbtkBoxLayoutChild))

#define NBTK_BOX_LAYOUT_CHILD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  NBTK_TYPE_BOX_LAYOUT_CHILD, NbtkBoxLayoutChildClass))

#define NBTK_IS_BOX_LAYOUT_CHILD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  NBTK_TYPE_BOX_LAYOUT_CHILD))

#define NBTK_IS_BOX_LAYOUT_CHILD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  NBTK_TYPE_BOX_LAYOUT_CHILD))

#define NBTK_BOX_LAYOUT_CHILD_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  NBTK_TYPE_BOX_LAYOUT_CHILD, NbtkBoxLayoutChildClass))

typedef struct _NbtkBoxLayoutChild NbtkBoxLayoutChild;
typedef struct _NbtkBoxLayoutChildClass NbtkBoxLayoutChildClass;
typedef struct _NbtkBoxLayoutChildPrivate NbtkBoxLayoutChildPrivate;

/**
 * NbtkBoxLayoutChild:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _NbtkBoxLayoutChild
{
  /*< private >*/
  ClutterChildMeta parent;

  gboolean expand;
  gboolean x_fill : 1;
  gboolean y_fill : 1;
  NbtkAlign x_align;
  NbtkAlign y_align;
};

struct _NbtkBoxLayoutChildClass
{
  ClutterChildMetaClass parent_class;
};

GType nbtk_box_layout_child_get_type (void);

G_END_DECLS

#endif /* _NBTK_BOX_LAYOUT_CHILD_H */
