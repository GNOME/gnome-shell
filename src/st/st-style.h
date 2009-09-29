/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
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

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_STYLE_H__
#define __ST_STYLE_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define ST_TYPE_STYLE                 (st_style_get_type ())
#define ST_STYLE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ST_TYPE_STYLE, StStyle))
#define ST_IS_STYLE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ST_TYPE_STYLE))
#define ST_STYLE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_STYLE, StStyleClass))
#define ST_IS_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_STYLE))
#define ST_STYLE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_STYLE, StStyleClass))

typedef struct _StStyle               StStyle;
typedef struct _StStylePrivate        StStylePrivate;
typedef struct _StStyleClass          StStyleClass;

/* forward declaration */
typedef struct _StStylable            StStylable; /* dummy typedef */
typedef struct _StStylableIface       StStylableIface;

typedef enum { /*< prefix=ST_STYLE_ERROR >*/
  ST_STYLE_ERROR_INVALID_FILE
} StStyleError;

/**
 * StStyle:
 *
 * The contents of this structure is private and should only be accessed using
 * the provided API.
 */
struct _StStyle
{
  /*< private >*/
  GObject parent_instance;

  StStylePrivate *priv;
};

struct _StStyleClass
{
  GObjectClass parent_class;

  void (* changed) (StStyle *style);
};

GType st_style_get_type (void) G_GNUC_CONST;

StStyle *st_style_get_default (void);
StStyle *st_style_new         (void);

gboolean st_style_load_from_file (StStyle      *style,
                                  const gchar  *filename,
                                  GError      **error);
void     st_style_get_property   (StStyle      *style,
                                  StStylable   *stylable,
                                  GParamSpec   *pspec,
                                  GValue       *value);
void     st_style_get            (StStyle      *style,
                                  StStylable   *stylable,
                                  const gchar  *first_property_name,
                                  ...) G_GNUC_NULL_TERMINATED;
void     st_style_get_valist     (StStyle      *style,
                                  StStylable   *stylable,
                                  const gchar  *first_property_name,
                                  va_list       va_args);

G_END_DECLS

#endif /* __ST_STYLE_H__ */
