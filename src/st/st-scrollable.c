/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-scrollable.c: Scrollable interface
 *
 * Copyright 2008 OpenedHand
 * Copyright 2009 Intel Corporation.
 * Copyright 2010 Red Hat, Inc.
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

#include "st-private.h"
#include "st-scrollable.h"

G_DEFINE_INTERFACE (StScrollable, st_scrollable, G_TYPE_OBJECT)

/**
 * SECTION:st-scrollable
 * @short_description: A #ClutterActor that can be scrolled
 *
 * The #StScrollable interface is exposed by actors that support scrolling.
 *
 * The interface contains methods for getting and setting the adjustments
 * for scrolling; these adjustments will be used to hook the scrolled
 * position up to scrollbars or other external controls. When a #StScrollable
 * is added to a parent container, the parent container is responsible
 * for setting the adjustments. The parent container then sets the adjustments
 * back to %NULL when the scrollable is removed.
 *
 * For #StScrollable supporting height-for-width size negotiation, size
 * negotiation works as follows:
 *
 * In response to get_preferred_width(), the scrollable should report
 * the minimum width at which horizontal scrolling is needed for the
 * preferred width, and natural width of the actor when not
 * horizontally scrolled as the natural width.
 *
 * The for_width passed into get_preferred_height() is the width at which
 * the scrollable will be allocated; this will be smaller than the minimum
 * width when scrolling horizontally, so the scrollable may want to adjust
 * it up to the minimum width before computing a preferred height. (Other
 * scrollables may want to fit as much content into the allocated area
 * as possible and only scroll what absolutely needs to scroll - consider,
 * for example, the line-wrapping behavior of a text editor where there
 * is a long line without any spaces.) As for width, get_preferred_height()
 * should return the minimum size at which no scrolling is needed for the
 * minimum height, and the natural size of the actor when not vertically scrolled
 * as the natural height.
 *
 * In allocate() the allocation box passed in will be actual allocated
 * size of the actor so will be smaller than the reported minimum
 * width and/or height when scrolling is present. Any scrollable actor
 * must support being allocated at any size down to 0x0 without
 * crashing, however if the actor has content around the scrolled area
 * and has an absolute minimum size that's bigger than 0x0 its
 * acceptable for it to misdraw between 0x0 and the absolute minimum
 * size. It's up to the application author to avoid letting the user
 * resize the scroll view small enough so that the scrolled area
 * vanishes.
 *
 * In response to allocate, in addition to normal handling, the
 * scrollable should also set the limits of the the horizontal and
 * vertical adjustments that were set on it earlier. The standard
 * settings are:
 *
 *  lower: 0
 *  page_size: allocated size (width or height)
 *  upper: MAX (total size of the scrolled area,allocated_size)
 *  step_increment: natural row/column height or a fixed fraction of the page size
 *  page_increment: page_size - step_increment
 */
static void
st_scrollable_default_init (StScrollableInterface *g_iface)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      /**
       * StScrollable:hadjustment:
       *
       * The horizontal #StAdjustment used by the #StScrollable.
       *
       * Implementations should override this property to provide read-write
       * access to the #StAdjustment.
       *
       * JavaScript code may override this as demonstrated below:
       *
       * |[<!-- language="JavaScript" -->
       * var MyScrollable = GObject.registerClass({
       *     Properties: {
       *         'hadjustment': GObject.ParamSpec.override(
       *             'hadjustment',
       *             St.Scrollable
       *         )
       *     }
       * }, class MyScrollable extends St.Scrollable {
       *
       *     get hadjustment() {
       *         return this._hadjustment || null;
       *     }
       *
       *     set hadjustment(adjustment) {
       *         if (this.hadjustment === adjustment)
       *             return;
       *
       *         this._hadjustment = adjustment;
       *         this.notify('hadjustment');
       *     }
       * });
       * ]|
       */
      g_object_interface_install_property (g_iface,
                                           g_param_spec_object ("hadjustment",
                                                                "StAdjustment",
                                                                "Horizontal adjustment",
                                                                ST_TYPE_ADJUSTMENT,
                                                                ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

      /**
       * StScrollable:vadjustment:
       *
       * The vertical #StAdjustment used by the #StScrollable.
       *
       * Implementations should override this property to provide read-write
       * access to the #StAdjustment.
       *
       * See #StScrollable:hadjustment for an example of how to override this
       * property in JavaScript code.
       */
      g_object_interface_install_property (g_iface,
                                           g_param_spec_object ("vadjustment",
                                                                "StAdjustment",
                                                                "Vertical adjustment",
                                                                ST_TYPE_ADJUSTMENT,
                                                                ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

      initialized = TRUE;
    }
}

/**
 * st_scrollable_set_adjustments:
 * @scrollable: a #StScrollable
 * @hadjustment: the horizontal #StAdjustment
 * @vadjustment: the vertical #StAdjustment
 *
 * This method should be implemented by classes implementing the #StScrollable
 * interface.
 *
 * JavaScript code should do this by overriding the `vfunc_set_adjustments()`
 * method.
 */
void
st_scrollable_set_adjustments (StScrollable *scrollable,
                               StAdjustment *hadjustment,
                               StAdjustment *vadjustment)
{
  ST_SCROLLABLE_GET_IFACE (scrollable)->set_adjustments (scrollable,
                                                         hadjustment,
                                                         vadjustment);
}

/**
 * st_scroll_bar_get_adjustments:
 * @hadjustment: (transfer none) (out) (optional): location to store the horizontal adjustment, or %NULL
 * @vadjustment: (transfer none) (out) (optional): location to store the vertical adjustment, or %NULL
 *
 * Gets the adjustment objects that store the offsets of the scrollable widget
 * into its possible scrolling area.
 *
 * This method should be implemented by classes implementing the #StScrollable
 * interface.
 *
 * JavaScript code should do this by overriding the `vfunc_get_adjustments()`
 * method.
 */
void
st_scrollable_get_adjustments (StScrollable  *scrollable,
                               StAdjustment **hadjustment,
                               StAdjustment **vadjustment)
{
  ST_SCROLLABLE_GET_IFACE (scrollable)->get_adjustments (scrollable,
                                                         hadjustment,
                                                         vadjustment);
}
