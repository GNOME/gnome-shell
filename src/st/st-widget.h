/*
 * st-widget.h: Base class for St actors
 *
 * Copyright 2007 OpenedHand
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
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_WIDGET_H__
#define __ST_WIDGET_H__

#include <clutter/clutter.h>
#include <st/st-types.h>

G_BEGIN_DECLS

#define ST_TYPE_WIDGET                 (st_widget_get_type ())
#define ST_WIDGET(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ST_TYPE_WIDGET, StWidget))
#define ST_IS_WIDGET(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ST_TYPE_WIDGET))
#define ST_WIDGET_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_WIDGET, StWidgetClass))
#define ST_IS_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_WIDGET))
#define ST_WIDGET_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_WIDGET, StWidgetClass))

typedef struct _StWidget               StWidget;
typedef struct _StWidgetPrivate        StWidgetPrivate;
typedef struct _StWidgetClass          StWidgetClass;

/**
 * StWidget:
 *
 * Base class for stylable actors. The contents of the #StWidget
 * structure are private and should only be accessed through the
 * public API.
 */
struct _StWidget
{
  /*< private >*/
  ClutterActor parent_instance;

  StWidgetPrivate *priv;
};

/**
 * StWidgetClass:
 *
 * Base class for stylable actors.
 */
struct _StWidgetClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /* vfuncs */
  void (* draw_background) (StWidget           *self,
                            ClutterActor       *background,
                            const ClutterColor *color);
};

GType st_widget_get_type (void) G_GNUC_CONST;

void                  st_widget_set_style_pseudo_class (StWidget    *actor,
                                                        const gchar *pseudo_class);
G_CONST_RETURN gchar *st_widget_get_style_pseudo_class (StWidget    *actor);
void                  st_widget_set_style_class_name   (StWidget    *actor,
                                                        const gchar *style_class);
G_CONST_RETURN gchar *st_widget_get_style_class_name   (StWidget    *actor);

void         st_widget_set_has_tooltip  (StWidget    *widget,
                                         gboolean     has_tooltip);
gboolean     st_widget_get_has_tooltip  (StWidget    *widget);
void         st_widget_set_tooltip_text (StWidget    *widget,
                                         const gchar *text);
const gchar* st_widget_get_tooltip_text (StWidget    *widget);

void st_widget_show_tooltip (StWidget *widget);
void st_widget_hide_tooltip (StWidget *widget);

void st_widget_ensure_style (StWidget *widget);


/* Only to be used by sub-classes of StWidget */
ClutterActor *st_widget_get_background_image (StWidget  *actor);
ClutterActor *st_widget_get_border_image     (StWidget  *actor);
void          st_widget_get_padding          (StWidget  *widget,
                                              StPadding *padding);
void          st_widget_draw_background      (StWidget  *widget);

G_END_DECLS

#endif /* __ST_WIDGET_H__ */
