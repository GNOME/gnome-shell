/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend - integration with NSWindow and NSView
 *
 * Copyright (C) 2007  Tommi Komulainen <tommi.komulainen@iki.fi>
 * Copyright (C) 2007  OpenedHand Ltd.
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
#ifndef __CLUTTER_STAGE_OSX_H__
#define __CLUTTER_STAGE_OSX_H__

#include <clutter/clutter-stage.h>
#include <clutter/clutter-backend.h>

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

G_BEGIN_DECLS

/* convenience macros */
#define CLUTTER_TYPE_STAGE_OSX             (clutter_stage_osx_get_type())
#define CLUTTER_STAGE_OSX(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),CLUTTER_TYPE_STAGE_OSX,ClutterStageOSX))
#define CLUTTER_STAGE_OSX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),CLUTTER_TYPE_STAGE_OSX,ClutterStage))
#define CLUTTER_IS_STAGE_OSX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),CLUTTER_TYPE_STAGE_OSX))
#define CLUTTER_IS_STAGE_OSX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),CLUTTER_TYPE_STAGE_OSX))
#define CLUTTER_STAGE_OSX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),CLUTTER_TYPE_STAGE_OSX,ClutterStageOSXClass))

typedef struct _ClutterStageOSX      ClutterStageOSX;
typedef struct _ClutterStageOSXClass ClutterStageOSXClass;

@interface ClutterGLWindow : NSWindow
{
@public
  ClutterStageOSX *stage_osx;
}
@end

struct _ClutterStageOSX
{
  ClutterGroup parent;

  ClutterBackend *backend;
  ClutterStage   *wrapper;

  NSWindow *window;
  NSOpenGLView *view;

  gboolean haveNormalFrame;
  NSRect normalFrame;

  gint requisition_width;
  gint requisition_height;

  ClutterStageState stage_state;
};

struct _ClutterStageOSXClass
{
  ClutterGroupClass parent_class;
};

GType           clutter_stage_osx_get_type    (void) G_GNUC_CONST;

ClutterStageWindow*   clutter_stage_osx_new (ClutterBackend *backend,
                                             ClutterStage   *wrapper);

G_END_DECLS

#endif /* __CLUTTER_STAGE_OSX_H__ */
