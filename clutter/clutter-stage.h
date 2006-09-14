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
#include <GL/glx.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE (clutter_stage_get_type())

#define CLUTTER_STAGE_WIDTH() \
 clutter_actor_get_width(CLUTTER_ACTOR(clutter_stage_get_default()))

#define CLUTTER_STAGE_HEIGHT() \
 clutter_actor_get_height(CLUTTER_ACTOR(clutter_stage_get_default()))


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

typedef struct _ClutterStagePrivate ClutterStagePrivate;
typedef struct _ClutterStage        ClutterStage;
typedef struct _ClutterStageClass   ClutterStageClass;

struct _ClutterStage
{
  ClutterGroup         parent;
  
  /*< private >*/
  ClutterStagePrivate *priv;
}; 

struct _ClutterStageClass 
{
  ClutterGroupClass parent_class;

  void (*input_event)          (ClutterStage       *stage,
		                ClutterEvent       *event);
  void (*button_press_event)   (ClutterStage       *stage,
			        ClutterButtonEvent *event);
  void (*button_release_event) (ClutterStage       *stage,
		  		ClutterButtonEvent *event);
  void (*key_press_event)      (ClutterStage       *stage,
		  		ClutterKeyEvent    *event);
  void (*key_release_event)    (ClutterStage       *stage,
		  		ClutterKeyEvent    *event);
  void (*motion_event)         (ClutterStage       *stage,
		  		ClutterMotionEvent *event);

  /* padding for future expansion */
  void (*_clutter_stage1) (void);
  void (*_clutter_stage2) (void);
  void (*_clutter_stage3) (void);
  void (*_clutter_stage4) (void);
  void (*_clutter_stage5) (void);
  void (*_clutter_stage6) (void);
}; 

GType         clutter_stage_get_type            (void) G_GNUC_CONST;
ClutterActor *clutter_stage_get_default         (void);
Window        clutter_stage_get_xwindow         (ClutterStage       *stage);
gboolean      clutter_stage_set_xwindow_foreign (ClutterStage       *stage,
						 Window              xid);
void          clutter_stage_set_color           (ClutterStage       *stage,
					         const ClutterColor *color);
void          clutter_stage_get_color           (ClutterStage       *stage,
						 ClutterColor       *color);
ClutterActor *clutter_stage_get_actor_at_pos    (ClutterStage       *stage,
						 gint                x,
						 gint                y);
GdkPixbuf *   clutter_stage_snapshot            (ClutterStage       *stage,
						 gint                x,
						 gint                y,
						 gint                width,
						 gint                height);
const XVisualInfo * clutter_stage_get_xvisual   (ClutterStage *stage);

G_END_DECLS

#endif
