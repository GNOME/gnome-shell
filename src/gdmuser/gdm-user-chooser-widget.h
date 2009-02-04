/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __GDM_USER_CHOOSER_WIDGET_H
#define __GDM_USER_CHOOSER_WIDGET_H

#include <glib-object.h>

#include "gdm-chooser-widget.h"

G_BEGIN_DECLS

#define GDM_TYPE_USER_CHOOSER_WIDGET         (gdm_user_chooser_widget_get_type ())
#define GDM_USER_CHOOSER_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDM_TYPE_USER_CHOOSER_WIDGET, GdmUserChooserWidget))
#define GDM_USER_CHOOSER_WIDGET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDM_TYPE_USER_CHOOSER_WIDGET, GdmUserChooserWidgetClass))
#define GDM_IS_USER_CHOOSER_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDM_TYPE_USER_CHOOSER_WIDGET))
#define GDM_IS_USER_CHOOSER_WIDGET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDM_TYPE_USER_CHOOSER_WIDGET))
#define GDM_USER_CHOOSER_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDM_TYPE_USER_CHOOSER_WIDGET, GdmUserChooserWidgetClass))

typedef struct GdmUserChooserWidgetPrivate GdmUserChooserWidgetPrivate;

typedef struct
{
        GdmChooserWidget            parent;
        GdmUserChooserWidgetPrivate *priv;
} GdmUserChooserWidget;

typedef struct
{
        GdmChooserWidgetClass   parent_class;
} GdmUserChooserWidgetClass;

#define GDM_USER_CHOOSER_USER_OTHER "__other"
#define GDM_USER_CHOOSER_USER_GUEST "__guest"
#define GDM_USER_CHOOSER_USER_AUTO  "__auto"

GType                  gdm_user_chooser_widget_get_type                   (void);
GtkWidget *            gdm_user_chooser_widget_new                        (void);

char *                 gdm_user_chooser_widget_get_chosen_user_name       (GdmUserChooserWidget *widget);
void                   gdm_user_chooser_widget_set_chosen_user_name       (GdmUserChooserWidget *widget,
                                                                           const char           *user_name);
void                   gdm_user_chooser_widget_set_show_only_chosen       (GdmUserChooserWidget *widget,
                                                                           gboolean              show_only);
void                   gdm_user_chooser_widget_set_show_user_other        (GdmUserChooserWidget *widget,
                                                                           gboolean              show);
void                   gdm_user_chooser_widget_set_show_user_guest        (GdmUserChooserWidget *widget,
                                                                           gboolean              show);
void                   gdm_user_chooser_widget_set_show_user_auto         (GdmUserChooserWidget *widget,
                                                                           gboolean              show);
G_END_DECLS

#endif /* __GDM_USER_CHOOSER_WIDGET_H */
