/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity hacked-up GtkAccelLabel */
/* Copyright (C) 2002 Red Hat, Inc. */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * MetaAccelLabel: GtkLabel with accelerator monitoring facilities.
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __META_ACCEL_LABEL_H__
#define __META_ACCEL_LABEL_H__

#include <gtk/gtk.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define META_TYPE_ACCEL_LABEL		(meta_accel_label_get_type ())
#define META_ACCEL_LABEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_ACCEL_LABEL, MetaAccelLabel))
#define META_ACCEL_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_ACCEL_LABEL, MetaAccelLabelClass))
#define META_IS_ACCEL_LABEL(obj)	 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_ACCEL_LABEL))
#define META_IS_ACCEL_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_ACCEL_LABEL))
#define META_ACCEL_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_ACCEL_LABEL, MetaAccelLabelClass))


typedef struct _MetaAccelLabel	    MetaAccelLabel;
typedef struct _MetaAccelLabelClass  MetaAccelLabelClass;

struct _MetaAccelLabel
{
  GtkLabel label;

  MetaVirtualModifier accel_mods;
  guint accel_key;
  guint accel_padding;
  gchar *accel_string;
  guint16 accel_string_width;
};

struct _MetaAccelLabelClass
{
  GtkLabelClass	 parent_class;

  gchar		*signal_quote1;
  gchar		*signal_quote2;
  gchar		*mod_name_shift;
  gchar		*mod_name_control;
  gchar		*mod_name_alt;
  gchar		*mod_name_meta;
  gchar		*mod_name_super;
  gchar		*mod_name_hyper;
  gchar		*mod_name_mod2;
  gchar		*mod_name_mod3;
  gchar		*mod_name_mod4;
  gchar		*mod_name_mod5;
  gchar		*mod_separator;
  gchar		*accel_seperator;
  guint		 latin1_to_char : 1;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType	   meta_accel_label_get_type          (void) G_GNUC_CONST;
GtkWidget* meta_accel_label_new_with_mnemonic (const gchar            *string);
void       meta_accel_label_set_accelerator   (MetaAccelLabel         *accel_label,
                                               guint                   accelerator_key,
                                               MetaVirtualModifier     accelerator_mods);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __META_ACCEL_LABEL_H__ */
