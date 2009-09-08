/*
 * nbtk-tooltip.h: Plain tooltip actor
 *
 * Copyright 2008, 2009 Intel Corporation.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Thomas Wood <thomas@linux.intel.com>
 *
 */

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_TOOLTIP_H__
#define __NBTK_TOOLTIP_H__

G_BEGIN_DECLS

#include <nbtk/nbtk-bin.h>

#define NBTK_TYPE_TOOLTIP                (nbtk_tooltip_get_type ())
#define NBTK_TOOLTIP(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_TOOLTIP, NbtkTooltip))
#define NBTK_IS_TOOLTIP(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_TOOLTIP))
#define NBTK_TOOLTIP_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_TOOLTIP, NbtkTooltipClass))
#define NBTK_IS_TOOLTIP_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_TOOLTIP))
#define NBTK_TOOLTIP_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_TOOLTIP, NbtkTooltipClass))

typedef struct _NbtkTooltip              NbtkTooltip;
typedef struct _NbtkTooltipPrivate       NbtkTooltipPrivate;
typedef struct _NbtkTooltipClass         NbtkTooltipClass;

/**
 * NbtkTooltip:
 *
 * The contents of this structure is private and should only be accessed using
 * the provided API.
 */
struct _NbtkTooltip
{
  /*< private >*/
  NbtkBin parent_instance;

  NbtkTooltipPrivate *priv;
};

struct _NbtkTooltipClass
{
  NbtkBinClass parent_class;
};

GType nbtk_tooltip_get_type (void) G_GNUC_CONST;

G_CONST_RETURN gchar *nbtk_tooltip_get_label (NbtkTooltip *tooltip);
void                  nbtk_tooltip_set_label (NbtkTooltip *tooltip,
                                              const gchar *text);
void                  nbtk_tooltip_show      (NbtkTooltip *tooltip);
void                  nbtk_tooltip_hide      (NbtkTooltip *tooltip);

void                  nbtk_tooltip_set_tip_area (NbtkTooltip *tooltip, const ClutterGeometry *area);
G_CONST_RETURN ClutterGeometry* nbtk_tooltip_get_tip_area (NbtkTooltip *tooltip);

G_END_DECLS

#endif /* __NBTK_TOOLTIP_H__ */
