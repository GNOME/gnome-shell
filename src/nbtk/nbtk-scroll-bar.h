/*
 * nbtk-scroll-bar.h: Scroll bar actor
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

#ifndef __NBTK_SCROLL_BAR_H__
#define __NBTK_SCROLL_BAR_H__

#include <nbtk/nbtk-adjustment.h>
#include <nbtk/nbtk-bin.h>

G_BEGIN_DECLS

#define NBTK_TYPE_SCROLL_BAR            (nbtk_scroll_bar_get_type())
#define NBTK_SCROLL_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_SCROLL_BAR, NbtkScrollBar))
#define NBTK_IS_SCROLL_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_SCROLL_BAR))
#define NBTK_SCROLL_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_SCROLL_BAR, NbtkScrollBarClass))
#define NBTK_IS_SCROLL_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_SCROLL_BAR))
#define NBTK_SCROLL_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_SCROLL_BAR, NbtkScrollBarClass))

typedef struct _NbtkScrollBar          NbtkScrollBar;
typedef struct _NbtkScrollBarPrivate   NbtkScrollBarPrivate;
typedef struct _NbtkScrollBarClass     NbtkScrollBarClass;

/**
 * NbtkScrollBar:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _NbtkScrollBar
{
  /*< private >*/
  NbtkBin parent_instance;

  NbtkScrollBarPrivate *priv;
};

struct _NbtkScrollBarClass
{
  NbtkBinClass parent_class;

  /* signals */
  void (*scroll_start) (NbtkScrollBar *bar);
  void (*scroll_stop)  (NbtkScrollBar *bar);
};

GType nbtk_scroll_bar_get_type (void) G_GNUC_CONST;

NbtkWidget *    nbtk_scroll_bar_new            (NbtkAdjustment *adjustment);
void            nbtk_scroll_bar_set_adjustment (NbtkScrollBar  *bar,
                                                NbtkAdjustment *adjustment);
NbtkAdjustment *nbtk_scroll_bar_get_adjustment (NbtkScrollBar  *bar);

G_END_DECLS

#endif /* __NBTK_SCROLL_BAR_H__ */
