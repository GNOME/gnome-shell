/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
