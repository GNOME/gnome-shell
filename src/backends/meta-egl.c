/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#include "config.h"

#include "backends/meta-egl.h"

#include <EGL/egl.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>

struct _MetaEgl
{
  GObject parent;
};

G_DEFINE_TYPE (MetaEgl, meta_egl, G_TYPE_OBJECT)

static const char *
get_egl_error_str (void)
{
  EGLint error_number;

  error_number = eglGetError ();

  switch (error_number)
    {
    case EGL_SUCCESS:
      return "The last function succeeded without error.";
      break;
    case EGL_NOT_INITIALIZED:
      return "EGL is not initialized, or could not be initialized, for the specified EGL display connection.";
      break;
    case EGL_BAD_ACCESS:
      return "EGL cannot access a requested resource (for example a context is bound in another thread).";
      break;
    case EGL_BAD_ALLOC:
      return "EGL failed to allocate resources for the requested operation.";
      break;
    case EGL_BAD_ATTRIBUTE:
      return "An unrecognized attribute or attribute value was passed in the attribute list.";
      break;
    case EGL_BAD_CONTEXT:
      return "An EGLContext argument does not name a valid EGL rendering context.";
      break;
    case EGL_BAD_CONFIG:
      return "An EGLConfig argument does not name a valid EGL frame buffer configuration.";
      break;
    case EGL_BAD_CURRENT_SURFACE:
      return "The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid.";
      break;
    case EGL_BAD_DISPLAY:
      return "An EGLDisplay argument does not name a valid EGL display connection.";
      break;
    case EGL_BAD_SURFACE:
      return "An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering.";
      break;
    case EGL_BAD_MATCH:
      return "Arguments are inconsistent (for example, a valid context requires buffers not supplied by a valid surface).";
      break;
    case EGL_BAD_PARAMETER:
      return "One or more argument values are invalid.";
      break;
    case EGL_BAD_NATIVE_PIXMAP:
      return "A NativePixmapType argument does not refer to a valid native pixmap.";
      break;
    case EGL_BAD_NATIVE_WINDOW:
      return "A NativeWindowType argument does not refer to a valid native window.";
      break;
    case EGL_CONTEXT_LOST:
      return "A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering. ";
      break;
    default:
      return "Unknown error";
      break;
    }
}

static void
set_egl_error (GError **error)
{
  const char *error_str;

  error_str = get_egl_error_str ();
  g_set_error (error, G_IO_ERROR,
               G_IO_ERROR_FAILED,
               error_str);
}

EGLDisplay
meta_egl_get_display (MetaEgl             *egl,
                      EGLNativeDisplayType display_id,
                      GError             **error)
{
  EGLDisplay display;

  display = eglGetDisplay (display_id);
  if (display == EGL_NO_DISPLAY)
    {
      set_egl_error (error);
      return EGL_NO_DISPLAY;
    }

  return display;
}

static void
meta_egl_init (MetaEgl *egl)
{
}

static void
meta_egl_class_init (MetaEglClass *klass)
{
}
