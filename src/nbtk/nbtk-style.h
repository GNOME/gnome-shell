/*
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
 */

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_STYLE_H__
#define __NBTK_STYLE_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define NBTK_TYPE_STYLE                 (nbtk_style_get_type ())
#define NBTK_STYLE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_STYLE, NbtkStyle))
#define NBTK_IS_STYLE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_STYLE))
#define NBTK_STYLE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_STYLE, NbtkStyleClass))
#define NBTK_IS_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_STYLE))
#define NBTK_STYLE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_STYLE, NbtkStyleClass))

typedef struct _NbtkStyle               NbtkStyle;
typedef struct _NbtkStylePrivate        NbtkStylePrivate;
typedef struct _NbtkStyleClass          NbtkStyleClass;

/* forward declaration */
typedef struct _NbtkStylable            NbtkStylable; /* dummy typedef */
typedef struct _NbtkStylableIface       NbtkStylableIface;

typedef enum { /*< prefix=NBTK_STYLE_ERROR >*/
  NBTK_STYLE_ERROR_INVALID_FILE
} NbtkStyleError;

/**
 * NbtkStyle:
 *
 * The contents of this structure is private and should only be accessed using
 * the provided API.
 */
struct _NbtkStyle
{
  /*< private >*/
  GObject parent_instance;

  NbtkStylePrivate *priv;
};

struct _NbtkStyleClass
{
  GObjectClass parent_class;

  void (* changed) (NbtkStyle *style);
};

GType            nbtk_style_get_type     (void) G_GNUC_CONST;

NbtkStyle *      nbtk_style_get_default  (void);
NbtkStyle *      nbtk_style_new          (void);

gboolean         nbtk_style_load_from_file (NbtkStyle     *style,
                                            const gchar   *filename,
                                            GError       **error);
void             nbtk_style_get_property   (NbtkStyle     *style,
                                            NbtkStylable  *stylable,
                                            GParamSpec    *pspec,
                                            GValue        *value);
void             nbtk_style_get            (NbtkStyle     *style,
                                            NbtkStylable  *stylable,
                                            const gchar   *first_property_name,
                                            ...) G_GNUC_NULL_TERMINATED;
void             nbtk_style_get_valist     (NbtkStyle     *style,
                                            NbtkStylable  *stylable,
                                            const gchar   *first_property_name,
                                            va_list        va_args);

G_END_DECLS

#endif /* __NBTK_STYLE_H__ */
