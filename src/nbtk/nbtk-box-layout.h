/*
 * nbtk-box-layout.h: box layout actor
 *
 * Copyright 2009 Intel Corporation.
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
 *
 */


#ifndef _NBTK_BOX_LAYOUT_H
#define _NBTK_BOX_LAYOUT_H

#include <nbtk/nbtk-widget.h>

G_BEGIN_DECLS

#define NBTK_TYPE_BOX_LAYOUT nbtk_box_layout_get_type()

#define NBTK_BOX_LAYOUT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  NBTK_TYPE_BOX_LAYOUT, NbtkBoxLayout))

#define NBTK_BOX_LAYOUT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  NBTK_TYPE_BOX_LAYOUT, NbtkBoxLayoutClass))

#define NBTK_IS_BOX_LAYOUT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  NBTK_TYPE_BOX_LAYOUT))

#define NBTK_IS_BOX_LAYOUT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  NBTK_TYPE_BOX_LAYOUT))

#define NBTK_BOX_LAYOUT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  NBTK_TYPE_BOX_LAYOUT, NbtkBoxLayoutClass))

typedef struct _NbtkBoxLayout NbtkBoxLayout;
typedef struct _NbtkBoxLayoutClass NbtkBoxLayoutClass;
typedef struct _NbtkBoxLayoutPrivate NbtkBoxLayoutPrivate;

/**
 * NbtkBoxLayout:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _NbtkBoxLayout
{
  /*< private >*/
  NbtkWidget parent;

  NbtkBoxLayoutPrivate *priv;
};

struct _NbtkBoxLayoutClass
{
  NbtkWidgetClass parent_class;
};

GType nbtk_box_layout_get_type (void);

NbtkWidget *nbtk_box_layout_new (void);

void nbtk_box_layout_set_vertical (NbtkBoxLayout *box, gboolean vertical);
gboolean nbtk_box_layout_get_vertical (NbtkBoxLayout *box);

void nbtk_box_layout_set_pack_start (NbtkBoxLayout *box, gboolean pack_start);
gboolean nbtk_box_layout_get_pack_start (NbtkBoxLayout *box);

void nbtk_box_layout_set_spacing (NbtkBoxLayout *box, guint spacing);
guint nbtk_box_layout_get_spacing (NbtkBoxLayout *box);

G_END_DECLS

#endif /* _NBTK_BOX_LAYOUT_H */
