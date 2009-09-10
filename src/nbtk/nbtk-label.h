/*
 * nbtk-label.h: Plain label actor
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
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Thomas Wood <thomas@linux.intel.com>
 *
 */

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_LABEL_H__
#define __NBTK_LABEL_H__

G_BEGIN_DECLS

#include <nbtk/nbtk-widget.h>

#define NBTK_TYPE_LABEL                (nbtk_label_get_type ())
#define NBTK_LABEL(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_LABEL, NbtkLabel))
#define NBTK_IS_LABEL(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_LABEL))
#define NBTK_LABEL_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_LABEL, NbtkLabelClass))
#define NBTK_IS_LABEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_LABEL))
#define NBTK_LABEL_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_LABEL, NbtkLabelClass))

typedef struct _NbtkLabel              NbtkLabel;
typedef struct _NbtkLabelPrivate       NbtkLabelPrivate;
typedef struct _NbtkLabelClass         NbtkLabelClass;

/**
 * NbtkLabel:
 *
 * The contents of this structure is private and should only be accessed using
 * the provided API.
 */
struct _NbtkLabel
{
  /*< private >*/
  NbtkWidget parent_instance;

  NbtkLabelPrivate *priv;
};

struct _NbtkLabelClass
{
  NbtkWidgetClass parent_class;
};

GType nbtk_label_get_type (void) G_GNUC_CONST;

NbtkWidget *          nbtk_label_new              (const gchar *text);
G_CONST_RETURN gchar *nbtk_label_get_text         (NbtkLabel   *label);
void                  nbtk_label_set_text         (NbtkLabel   *label,
                                                   const gchar *text);
ClutterActor *        nbtk_label_get_clutter_text (NbtkLabel   *label);

G_END_DECLS

#endif /* __NBTK_LABEL_H__ */
