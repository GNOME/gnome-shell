/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-types.h"
#include "cogl-util.h"
#include "cogl-error-private.h"

#include <glib.h>

void
cogl_error_free (CoglError *error)
{
  g_error_free ((GError *)error);
}

CoglError *
cogl_error_copy (CoglError *error)
{
  return (CoglError *)g_error_copy ((GError *)error);
}

CoglBool
cogl_error_matches (CoglError *error,
                    uint32_t domain,
                    int code)
{
  return g_error_matches ((GError *)error, domain, code);
}

#define ERROR_OVERWRITTEN_WARNING \
  "CoglError set over the top of a previous CoglError or " \
  "uninitialized memory.\nThis indicates a bug in someone's " \
  "code. You must ensure an error is NULL before it's set.\n" \
  "The overwriting error message was: %s"

void
_cogl_set_error (CoglError **error,
                 uint32_t domain,
                 int code,
                 const char *format,
                 ...)
{
  GError *new;

  va_list args;

  va_start (args, format);

  if (error == NULL)
    {
      g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, format, args);
      va_end (args);
      return;
    }

  new = g_error_new_valist (domain, code, format, args);
  va_end (args);

  if (*error == NULL)
    *error = (CoglError *)new;
  else
    g_warning (ERROR_OVERWRITTEN_WARNING, new->message);
}

void
_cogl_set_error_literal (CoglError **error,
                         uint32_t domain,
                         int code,
                         const char *message)
{
  _cogl_set_error (error, domain, code, "%s", message);
}

void
_cogl_propagate_error (CoglError **dest,
                       CoglError *src)
{
  _COGL_RETURN_IF_FAIL (src != NULL);

  if (dest == NULL)
    {
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "%s", src->message);
      cogl_error_free (src);
    }
  else if (*dest)
    g_warning (ERROR_OVERWRITTEN_WARNING, src->message);
  else
    *dest = src;
}

/* This function is only used from the gdk-pixbuf image backend so it
 * should only be called if we are using the system GLib. It would be
 * difficult to get this to work without the system glib because we
 * would need to somehow call the same g_error_free function that
 * gdk-pixbuf is using */
#ifdef COGL_HAS_GLIB_SUPPORT
void
_cogl_propagate_gerror (CoglError **dest,
                        GError *src)
{
  _cogl_propagate_error (dest, (CoglError *) src);
}
#endif /* COGL_HAS_GLIB_SUPPORT */
