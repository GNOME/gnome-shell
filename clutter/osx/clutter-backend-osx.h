/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend - initial entry point
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */
#ifndef __CLUTTER_BACKEND_OSX_H__
#define __CLUTTER_BACKEND_OSX_H__

#include <clutter/clutter-backend.h>

@class NSOpenGLPixelFormat, NSOpenGLContext;

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_OSX             (clutter_backend_osx_get_type())
#define CLUTTER_BACKEND_OSX(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),CLUTTER_TYPE_BACKEND_OSX,ClutterBackendOSX))
#define CLUTTER_BACKEND_OSX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),CLUTTER_TYPE_BACKEND_OSX,ClutterBackend))
#define CLUTTER_IS_BACKEND_OSX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),CLUTTER_TYPE_BACKEND_OSX))
#define CLUTTER_IS_BACKEND_OSX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),CLUTTER_TYPE_BACKEND_OSX))
#define CLUTTER_BACKEND_OSX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),CLUTTER_TYPE_BACKEND_OSX,ClutterBackendOSXClass))

typedef struct _ClutterBackendOSX      ClutterBackendOSX;
typedef struct _ClutterBackendOSXClass ClutterBackendOSXClass;

struct _ClutterBackendOSX
{
  ClutterBackend parent;

  NSOpenGLPixelFormat *pixel_format;
  NSOpenGLContext     *context;
};

struct _ClutterBackendOSXClass
{
  ClutterBackendClass parent_class;
};

GType        clutter_backend_osx_get_type    (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_BACKEND_OSX_H__ */
