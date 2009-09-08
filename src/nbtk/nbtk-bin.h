/*
 * nbtk-bin.h: Basic container actor
 *
 * Copyright 2009, 2008 Intel Corporation.
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
 * Written by: Emmanuele Bassi <ebassi@linux.intel.com>
 *
 */

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_BIN_H__
#define __NBTK_BIN_H__

#include <nbtk/nbtk-types.h>
#include <nbtk/nbtk-widget.h>

G_BEGIN_DECLS

#define NBTK_TYPE_BIN                   (nbtk_bin_get_type ())
#define NBTK_BIN(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_BIN, NbtkBin))
#define NBTK_IS_BIN(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_BIN))
#define NBTK_BIN_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_BIN, NbtkBinClass))
#define NBTK_IS_BIN_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_BIN))
#define NBTK_BIN_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_BIN, NbtkBinClass))

typedef struct _NbtkBin                 NbtkBin;
typedef struct _NbtkBinPrivate          NbtkBinPrivate;
typedef struct _NbtkBinClass            NbtkBinClass;

/**
 * NbtkBin:
 *
 * The #NbtkBin struct contains only private data
 */
struct _NbtkBin
{
  /*< private >*/
  NbtkWidget parent_instance;

  NbtkBinPrivate *priv;
};

/**
 * NbtkBinClass:
 *
 * The #NbtkBinClass struct contains only private data
 */
struct _NbtkBinClass
{
  /*< private >*/
  NbtkWidgetClass parent_class;
};

GType nbtk_bin_get_type (void) G_GNUC_CONST;

NbtkWidget   *nbtk_bin_new           (void);
void          nbtk_bin_set_child     (NbtkBin           *bin,
                                      ClutterActor      *child);
ClutterActor *nbtk_bin_get_child     (NbtkBin           *bin);
void          nbtk_bin_set_alignment (NbtkBin           *bin,
                                      NbtkAlignment      x_align,
                                      NbtkAlignment      y_align);
void          nbtk_bin_get_alignment (NbtkBin           *bin,
                                      NbtkAlignment     *x_align,
                                      NbtkAlignment     *y_align);
void          nbtk_bin_set_fill      (NbtkBin           *bin,
                                      gboolean           x_fill,
                                      gboolean           y_fill);
void          nbtk_bin_get_fill      (NbtkBin           *bin,
                                      gboolean          *x_fill,
                                      gboolean          *y_fill);

G_END_DECLS

#endif /* __NBTK_BIN_H__ */
