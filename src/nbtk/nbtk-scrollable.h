/*
 * nbtk-scrollable.h: Scrollable interface
 *
 * Copyright 2008 OpenedHand
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
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Chris Lord <chris@openedhand.com>
 * Port to Nbtk by: Robert Staudinger <robsta@openedhand.com>
 *
 */

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_SCROLLABLE_H__
#define __NBTK_SCROLLABLE_H__

#include <glib-object.h>
#include <nbtk/nbtk-adjustment.h>

G_BEGIN_DECLS

#define NBTK_TYPE_SCROLLABLE                (nbtk_scrollable_get_type ())
#define NBTK_SCROLLABLE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_SCROLLABLE, NbtkScrollable))
#define NBTK_IS_SCROLLABLE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_SCROLLABLE))
#define NBTK_SCROLLABLE_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), NBTK_TYPE_SCROLLABLE, NbtkScrollableInterface))

typedef struct _NbtkScrollable NbtkScrollable; /* Dummy object */
typedef struct _NbtkScrollableInterface NbtkScrollableInterface;

struct _NbtkScrollableInterface
{
  GTypeInterface parent;

  void (* set_adjustments) (NbtkScrollable  *scrollable,
                            NbtkAdjustment  *hadjustment,
                            NbtkAdjustment  *vadjustment);
  void (* get_adjustments) (NbtkScrollable  *scrollable,
                            NbtkAdjustment **hadjustment,
                            NbtkAdjustment **vadjustment);
};

GType nbtk_scrollable_get_type (void) G_GNUC_CONST;

void nbtk_scrollable_set_adjustments (NbtkScrollable  *scrollable,
                                      NbtkAdjustment  *hadjustment,
                                      NbtkAdjustment  *vadjustment);
void nbtk_scrollable_get_adjustments (NbtkScrollable  *scrollable,
                                      NbtkAdjustment **hadjustment,
                                      NbtkAdjustment **vadjustment);

G_END_DECLS

#endif /* __NBTK_SCROLLABLE_H__ */
