/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * this file Copyright (C) 2001 Havoc Pennington 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_DISCLOSURE_BOX_H__
#define __GTK_DISCLOSURE_BOX_H__


#include <gdk/gdk.h>
#include <gtk/gtkframe.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_DISCLOSURE_BOX             (gtk_disclosure_box_get_type ())
#define GTK_DISCLOSURE_BOX(obj)             (GTK_CHECK_CAST ((obj), GTK_TYPE_DISCLOSURE_BOX, GtkDisclosureBox))
#define GTK_DISCLOSURE_BOX_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_DISCLOSURE_BOX, GtkDisclosureBoxClass))
#define GTK_IS_DISCLOSURE_BOX(obj)          (GTK_CHECK_TYPE ((obj), GTK_TYPE_DISCLOSURE_BOX))
#define GTK_IS_DISCLOSURE_BOX_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_DISCLOSURE_BOX))
#define GTK_DISCLOSURE_BOX_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_DISCLOSURE_BOX, GtkDisclosureBoxClass))
 
typedef struct _GtkDisclosureBox       GtkDisclosureBox;
typedef struct _GtkDisclosureBoxClass  GtkDisclosureBoxClass;

struct _GtkDisclosureBox
{
  GtkFrame parent_instance;

  guint disclosed : 1;
};

struct _GtkDisclosureBoxClass
{
  GtkFrameClass parent_class;
};

GType gtk_disclosure_box_get_type (void) G_GNUC_CONST;

GtkWidget* gtk_disclosure_box_new (const char *label);

void     gtk_disclosure_box_set_disclosed (GtkDisclosureBox *box,
                                           gboolean          disclosed);
gboolean gtk_disclosure_box_get_disclosed (GtkDisclosureBox *box);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_FRAME_H__ */
