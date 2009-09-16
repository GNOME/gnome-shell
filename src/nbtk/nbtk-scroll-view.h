/*
 * nbtk-scroll-view.h: Container with scroll-bars
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

#ifndef __NBTK_SCROLL_VIEW_H__
#define __NBTK_SCROLL_VIEW_H__

#include <nbtk/nbtk-bin.h>

G_BEGIN_DECLS

#define NBTK_TYPE_SCROLL_VIEW            (nbtk_scroll_view_get_type())
#define NBTK_SCROLL_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_SCROLL_VIEW, NbtkScrollView))
#define NBTK_IS_SCROLL_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_SCROLL_VIEW))
#define NBTK_SCROLL_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_SCROLL_VIEW, NbtkScrollViewClass))
#define NBTK_IS_SCROLL_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_SCROLL_VIEW))
#define NBTK_SCROLL_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_SCROLL_VIEW, NbtkScrollViewClass))

typedef struct _NbtkScrollView          NbtkScrollView;
typedef struct _NbtkScrollViewPrivate   NbtkScrollViewPrivate;
typedef struct _NbtkScrollViewClass     NbtkScrollViewClass;

/**
 * NbtkScrollView:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _NbtkScrollView
{
  /*< private >*/
  NbtkBin parent_instance;

  NbtkScrollViewPrivate *priv;
};

struct _NbtkScrollViewClass
{
  NbtkBinClass parent_class;
};

GType nbtk_scroll_view_get_type (void) G_GNUC_CONST;

NbtkWidget *  nbtk_scroll_view_new             (void);

ClutterActor *  nbtk_scroll_view_get_hscroll_bar (NbtkScrollView *scroll);
ClutterActor *  nbtk_scroll_view_get_vscroll_bar (NbtkScrollView *scroll);

gfloat     nbtk_scroll_view_get_column_size (NbtkScrollView *scroll);
void            nbtk_scroll_view_set_column_size (NbtkScrollView *scroll,
                                                  gfloat          column_size);

gfloat     nbtk_scroll_view_get_row_size    (NbtkScrollView *scroll);
void            nbtk_scroll_view_set_row_size    (NbtkScrollView *scroll,
                                                  gfloat          row_size);

void nbtk_scroll_view_set_mouse_scrolling (NbtkScrollView *scroll, gboolean enabled);
gboolean nbtk_scroll_view_get_mouse_scrolling (NbtkScrollView *scroll);

G_END_DECLS

#endif /* __NBTK_SCROLL_VIEW_H__ */
