/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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
 */
#include <stdio.h>

#include "config.h"

#include "backends/meta-gles3.h"

#include <dlfcn.h>
#include <gio/gio.h>

#include "backends/meta-gles3-table.h"

struct _MetaGles3
{
  GObject parent;

  MetaEgl *egl;

  MetaGles3Table table;
};

G_DEFINE_TYPE (MetaGles3, meta_gles3, G_TYPE_OBJECT)

MetaGles3Table *
meta_gles3_get_table (MetaGles3 *gles3)
{
  return &gles3->table;
}

void
meta_gles3_ensure_loaded (MetaGles3  *gles3,
                          gpointer   *func,
                          const char *name)
{
  GError *error = NULL;

  if (*func)
    return;

  *func = meta_egl_get_proc_address (gles3->egl, name, &error);
  if (!*func)
    g_error ("Failed to load GLES3 symbol: %s", error->message);
}

static const char *
get_gl_error_str (GLenum gl_error)
{
  switch (gl_error)
    {
    case GL_NO_ERROR:
      return "No error has been recorded.";
    case GL_INVALID_ENUM:
      return "An unacceptable value is specified for an enumerated argument.";
    case GL_INVALID_VALUE:
        return "A numeric argument is out of range.";
    case GL_INVALID_OPERATION:
        return "The specified operation is not allowed in the current state.";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "The framebuffer object is not complete.";
    case GL_OUT_OF_MEMORY:
        return "There is not enough memory left to execute the command.";
    default:
        return "Unknown error";
    }
}

void
meta_gles3_clear_error (MetaGles3 *gles3)
{
  while (TRUE)
    {
      GLenum gl_error = glGetError ();

      if (gl_error == GL_NO_ERROR)
        break;
    }
}

gboolean
meta_gles3_validate (MetaGles3 *gles3,
                     GError   **error)
{
  GLenum gl_error;

  gl_error = glGetError ();
  if (gl_error != GL_NO_ERROR)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           get_gl_error_str (gl_error));
      return FALSE;
    }

  return TRUE;
}

gboolean
meta_gles3_has_extensions (MetaGles3 *gles3,
                           char    ***missing_extensions,
                           char      *first_extension,
                           ...)
{
  va_list var_args;
  const char *extensions_str;
  gboolean has_extensions;

  extensions_str = (const char *) glGetString (GL_EXTENSIONS);
  if (!extensions_str)
    {
      g_warning ("Failed to get string: %s", get_gl_error_str (glGetError ()));
      return FALSE;
    }

  va_start (var_args, first_extension);
  has_extensions =
    meta_extensions_string_has_extensions_valist (extensions_str,
                                                  missing_extensions,
                                                  first_extension,
                                                  var_args);
  va_end (var_args);

  return has_extensions;
}

MetaGles3 *
meta_gles3_new (MetaEgl *egl)
{
  MetaGles3 *gles3;

  gles3 = g_object_new (META_TYPE_GLES3, NULL);
  gles3->egl = egl;

  return gles3;
}

static void
meta_gles3_init (MetaGles3 *gles3)
{
}

static void
meta_gles3_class_init (MetaGles3Class *klass)
{
}
