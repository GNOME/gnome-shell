/*
 * nbtk-adjustment.h: Adjustment object
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
 *
 * Written by: Chris Lord <chris@openedhand.com>, inspired by GtkAdjustment
 * Port to Nbtk by: Robert Staudinger <robsta@openedhand.com>
 *
 */

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_ADJUSTMENT_H__
#define __NBTK_ADJUSTMENT_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define NBTK_TYPE_ADJUSTMENT            (nbtk_adjustment_get_type())
#define NBTK_ADJUSTMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_ADJUSTMENT, NbtkAdjustment))
#define NBTK_IS_ADJUSTMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_ADJUSTMENT))
#define NBTK_ADJUSTMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_ADJUSTMENT, NbtkAdjustmentClass))
#define NBTK_IS_ADJUSTMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_ADJUSTMENT))
#define NBTK_ADJUSTMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_ADJUSTMENT, NbtkAdjustmentClass))

typedef struct _NbtkAdjustment          NbtkAdjustment;
typedef struct _NbtkAdjustmentPrivate   NbtkAdjustmentPrivate;
typedef struct _NbtkAdjustmentClass     NbtkAdjustmentClass;

/**
 * NbtkAdjustment:
 *
 * Class for handling an interval between to values. The contents of
 * the #NbtkAdjustment are private and should be accessed using the
 * public API.
 */
struct _NbtkAdjustment
{
  /*< private >*/
  GObject parent_instance;

  NbtkAdjustmentPrivate *priv;
};

/**
 * NbtkAdjustmentClass
 * @changed: Class handler for the ::changed signal.
 *
 * Base class for #NbtkAdjustment.
 */
struct _NbtkAdjustmentClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  void (* changed) (NbtkAdjustment *adjustment);
};

GType nbtk_adjustment_get_type (void) G_GNUC_CONST;

NbtkAdjustment *nbtk_adjustment_new          (gdouble         value,
                                              gdouble         lower,
                                              gdouble         upper,
                                              gdouble         step_increment,
                                              gdouble         page_increment,
                                              gdouble         page_size);
gdouble         nbtk_adjustment_get_value    (NbtkAdjustment *adjustment);
void            nbtk_adjustment_set_value    (NbtkAdjustment *adjustment,
                                              gdouble         value);
void            nbtk_adjustment_clamp_page   (NbtkAdjustment *adjustment,
                                              gdouble         lower,
                                              gdouble         upper);
void            nbtk_adjustment_set_values   (NbtkAdjustment *adjustment,
                                              gdouble         value,
                                              gdouble         lower,
                                              gdouble         upper,
                                              gdouble         step_increment,
                                              gdouble         page_increment,
                                              gdouble         page_size);
void            nbtk_adjustment_get_values   (NbtkAdjustment *adjustment,
                                              gdouble        *value,
                                              gdouble        *lower,
                                              gdouble        *upper,
                                              gdouble        *step_increment,
                                              gdouble        *page_increment,
                                              gdouble        *page_size);

void            nbtk_adjustment_interpolate  (NbtkAdjustment *adjustment,
                                              gdouble         value,
                                              guint           duration);

gboolean        nbtk_adjustment_get_elastic  (NbtkAdjustment *adjustment);
void            nbtk_adjustment_set_elastic  (NbtkAdjustment *adjustment,
                                              gboolean        elastic);

gboolean        nbtk_adjustment_clamp        (NbtkAdjustment *adjustment,
                                              gboolean        interpolate,
                                              guint           duration);

G_END_DECLS

#endif /* __NBTK_ADJUSTMENT_H__ */
