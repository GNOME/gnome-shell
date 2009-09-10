/*
 * nbtk-scrollable.c: Scrollable interface
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
 * Written by: Chris Lord <chris@openedhand.com>
 * Port to Nbtk by: Robert Staudinger <robsta@openedhand.com>
 *
 */

#include "nbtk-scrollable.h"

static void
nbtk_scrollable_base_init (gpointer g_iface)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      g_object_interface_install_property (g_iface,
                                   g_param_spec_object ("hadjustment",
                                                        "NbtkAdjustment",
                                                        "Horizontal adjustment",
                                                        NBTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

      g_object_interface_install_property (g_iface,
                                   g_param_spec_object ("vadjustment",
                                                        "NbtkAdjustment",
                                                        "Vertical adjustment",
                                                        NBTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

      initialized = TRUE;
    }
}

GType
nbtk_scrollable_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      static const GTypeInfo info =
        {
          sizeof (NbtkScrollableInterface),
          nbtk_scrollable_base_init,        /* base_init */
          NULL,
        };
      type = g_type_register_static (G_TYPE_INTERFACE,
                                     "NbtkScrollable", &info, 0);
    }
  return type;
}

void
nbtk_scrollable_set_adjustments (NbtkScrollable *scrollable,
                                 NbtkAdjustment *hadjustment,
                                 NbtkAdjustment *vadjustment)
{
  NBTK_SCROLLABLE_GET_INTERFACE (scrollable)->set_adjustments (scrollable,
                                                               hadjustment,
                                                               vadjustment);
}

void
nbtk_scrollable_get_adjustments (NbtkScrollable *scrollable,
                                 NbtkAdjustment **hadjustment,
                                 NbtkAdjustment **vadjustment)
{
  NBTK_SCROLLABLE_GET_INTERFACE (scrollable)->get_adjustments (scrollable,
                                                               hadjustment,
                                                               vadjustment);
}
