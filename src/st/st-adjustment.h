/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-adjustment.h: Adjustment object
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#pragma once

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define ST_TYPE_ADJUSTMENT (st_adjustment_get_type())
G_DECLARE_DERIVABLE_TYPE (StAdjustment, st_adjustment, ST, ADJUSTMENT, GObject)

/**
 * StAdjustmentClass:
 * @changed: Class handler for the ::changed signal.
 *
 * Base class for #StAdjustment.
 */
struct _StAdjustmentClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  void (* changed) (StAdjustment *adjustment);
};

StAdjustment *st_adjustment_new         (ClutterActor *actor,
                                         gdouble       value,
                                         gdouble       lower,
                                         gdouble       upper,
                                         gdouble       step_increment,
                                         gdouble       page_increment,
                                         gdouble       page_size);
gdouble       st_adjustment_get_value   (StAdjustment *adjustment);
void          st_adjustment_set_value   (StAdjustment *adjustment,
                                         gdouble       value);
double        st_adjustment_get_lower   (StAdjustment *adjustment);
void          st_adjustment_set_lower   (StAdjustment *adjustment,
                                         gdouble       lower);
double        st_adjustment_get_upper   (StAdjustment *adjustment);
void          st_adjustment_set_upper   (StAdjustment *adjustment,
                                         gdouble       upper);
double        st_adjustment_get_step_increment (StAdjustment *adjustment);
void          st_adjustment_set_step_increment (StAdjustment *adjustment,
                                                gdouble       step);
double        st_adjustment_get_page_increment (StAdjustment *adjustment);
void          st_adjustment_set_page_increment (StAdjustment *adjustment,
                                                gdouble       page);
double        st_adjustment_get_page_size (StAdjustment *adjustment);
void          st_adjustment_set_page_size (StAdjustment *adjustment,
                                           gdouble       size);
void          st_adjustment_clamp_page  (StAdjustment *adjustment,
                                         gdouble       lower,
                                         gdouble       upper);
void          st_adjustment_set_values  (StAdjustment *adjustment,
                                         gdouble       value,
                                         gdouble       lower,
                                         gdouble       upper,
                                         gdouble       step_increment,
                                         gdouble       page_increment,
                                         gdouble       page_size);
void          st_adjustment_get_values  (StAdjustment *adjustment,
                                         gdouble      *value,
                                         gdouble      *lower,
                                         gdouble      *upper,
                                         gdouble      *step_increment,
                                         gdouble      *page_increment,
                                         gdouble      *page_size);

void          st_adjustment_adjust_for_scroll_event (StAdjustment *adjustment,
                                                     gdouble       delta);

ClutterTransition * st_adjustment_get_transition    (StAdjustment      *adjustment,
                                                     const char        *name);
void                st_adjustment_add_transition    (StAdjustment      *adjustment,
                                                     const char        *name,
                                                     ClutterTransition *transition);
void                st_adjustment_remove_transition (StAdjustment      *adjustment,
                                                     const char        *name);

ClutterActor * st_adjustment_get_actor (StAdjustment *adjustment);
void           st_adjustment_set_actor (StAdjustment *adjustment,
                                        ClutterActor *actor);

G_END_DECLS
