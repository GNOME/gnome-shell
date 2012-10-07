/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
 *               2011 Giovanni Campagna <scampa.giovanni@gmail.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __CLUTTER_STAGE_GDK_H__
#define __CLUTTER_STAGE_GDK_H__

#include <clutter/clutter-stage.h>
#include <gdk/gdk.h>

#include "clutter-backend-gdk.h"
#include "cogl/clutter-stage-cogl.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE_GDK                  (_clutter_stage_gdk_get_type ())
#define CLUTTER_STAGE_GDK(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_GDK, ClutterStageGdk))
#define CLUTTER_IS_STAGE_GDK(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_GDK))
#define CLUTTER_STAGE_GDK_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_GDK, ClutterStageGdkClass))
#define CLUTTER_IS_STAGE_GDK_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_GDK))
#define CLUTTER_STAGE_GDK_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_GDK, ClutterStageGdkClass))

typedef struct _ClutterStageGdk         ClutterStageGdk;
typedef struct _ClutterStageGdkClass    ClutterStageGdkClass;

struct _ClutterStageGdk
{
  ClutterStageCogl parent_instance;

  GdkWindow *window;
  GdkCursor *blank_cursor;

  gboolean foreign_window;
};

struct _ClutterStageGdkClass
{
  ClutterStageCoglClass parent_class;
};

#define CLUTTER_STAGE_GDK_EVENT_MASK \
  (GDK_STRUCTURE_MASK |		     \
   GDK_FOCUS_CHANGE_MASK |	     \
   GDK_EXPOSURE_MASK |		     \
   GDK_PROPERTY_CHANGE_MASK |	     \
   GDK_ENTER_NOTIFY_MASK |	     \
   GDK_LEAVE_NOTIFY_MASK |	     \
   GDK_KEY_PRESS_MASK |		     \
   GDK_KEY_RELEASE_MASK |	     \
   GDK_BUTTON_PRESS_MASK |	     \
   GDK_BUTTON_RELEASE_MASK |	     \
   GDK_POINTER_MOTION_MASK |         \
   GDK_SCROLL_MASK)

GType _clutter_stage_gdk_get_type (void) G_GNUC_CONST;

void _clutter_stage_gdk_update_foreign_event_mask (CoglOnscreen *onscreen,
						   guint32 event_mask,
						   void *user_data);

G_END_DECLS

#endif /* __CLUTTER_STAGE_H__ */
