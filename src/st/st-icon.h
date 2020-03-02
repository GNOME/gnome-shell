/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-icon.h: icon widget
 *
 * Copyright 2009, 2010 Intel Corporation.
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
 * Written by: Thomas Wood <thomas.wood@intel.com>
 *
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef _ST_ICON
#define _ST_ICON

#include <glib-object.h>
#include <gio/gio.h>
#include <st/st-widget.h>

#include <st/st-types.h>

G_BEGIN_DECLS

#define ST_TYPE_ICON st_icon_get_type()
G_DECLARE_FINAL_TYPE (StIcon, st_icon, ST, ICON, StWidget)

typedef struct _StIconPrivate       StIconPrivate;

/**
 * StIcon:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _StIcon {
  /*< private >*/
  StWidget parent;

  StIconPrivate *priv;
};

ClutterActor* st_icon_new (void);

GIcon       *st_icon_get_gicon (StIcon *icon);
void         st_icon_set_gicon (StIcon *icon,
                                GIcon  *gicon);

GIcon       *st_icon_get_fallback_gicon (StIcon *icon);
void         st_icon_set_fallback_gicon (StIcon *icon,
                                         GIcon  *fallback_gicon);

const gchar *st_icon_get_icon_name (StIcon      *icon);
void         st_icon_set_icon_name (StIcon      *icon,
                                    const gchar *icon_name);

const gchar *st_icon_get_fallback_icon_name (StIcon      *icon);
void         st_icon_set_fallback_icon_name (StIcon      *icon,
                                             const gchar *fallback_icon_name);

gint         st_icon_get_icon_size (StIcon *icon);
void         st_icon_set_icon_size (StIcon *icon,
                                    gint    size);

G_END_DECLS

#endif /* _ST_ICON */

