/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef _HAVE_CLUTTER_STAGE_H
#define _HAVE_CLUTTER_STAGE_H

#include <glib-object.h>

#include <clutter/clutter-group.h>
#include <clutter/clutter-color.h>
#include <clutter/clutter-event.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE clutter_stage_get_type()

#define CLUTTER_STAGE_WIDTH() \
 clutter_element_get_width(CLUTTER_ELEMENT(clutter_stage()))

#define CLUTTER_STAGE_HEIGHT() \
 clutter_element_get_height(CLUTTER_ELEMENT(clutter_stage()))


#define CLUTTER_STAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_STAGE, ClutterStage))

#define CLUTTER_STAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_STAGE, ClutterStageClass))

#define CLUTTER_IS_STAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_STAGE))

#define CLUTTER_IS_STAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_STAGE))

#define CLUTTER_STAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_STAGE, ClutterStageClass))

typedef struct ClutterStagePrivate ClutterStagePrivate;
typedef struct _ClutterStage       ClutterStage;
typedef struct _ClutterStageClass  ClutterStageClass;

struct _ClutterStage
{
  ClutterGroup         parent;
  ClutterStagePrivate *priv;
}; 

struct _ClutterStageClass 
{
  ClutterGroupClass parent_class;

  void (*input_event) (ClutterStage *stage,
		       ClutterEvent *event);
}; 

GType clutter_stage_get_type (void);

/* FIXME: no need for below to take stage arg ? 
 *        convert to defines also ?
*/

Window
clutter_stage_get_xwindow (ClutterStage *stage);

void
clutter_stage_set_color (ClutterStage *stage,
			 ClutterColor  color);

ClutterColor
clutter_stage_get_color (ClutterStage *stage);

GdkPixbuf*
clutter_stage_snapshot (ClutterStage *stage,
			gint          x,
			gint          y,
			guint         width,
			guint         height);

ClutterElement*
clutter_stage_pick (ClutterStage *stage, gint x, gint y);

G_END_DECLS

#endif
