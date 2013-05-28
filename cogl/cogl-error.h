/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ERROR_H__
#define __COGL_ERROR_H__

#include "cogl-types.h"

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-error
 * @short_description: A way for Cogl to throw exceptions
 *
 * As a general rule Cogl shields non-recoverable errors from
 * developers, such as most heap allocation failures (unless for
 * exceptionally large resources which we might reasonably expect to
 * fail) and this reduces the burden on developers.
 *
 * There are some Cogl apis though that can fail for exceptional
 * reasons that can also potentially be recovered from at runtime
 * and for these apis we use a standard convention for reporting
 * runtime recoverable errors.
 *
 * As an example if we look at the cogl_context_new() api which
 * takes an error argument:
 * |[
 *   CoglContext *
 *   cogl_context_new (CoglDisplay *display, CoglError **error);
 * ]|
 *
 * A caller interested in catching any runtime error when creating a
 * new #CoglContext would pass the address of a #CoglError pointer
 * that has first been initialized to %NULL as follows:
 *
 * |[
 *   CoglError *error = NULL;
 *   CoglContext *context;
 *
 *   context = cogl_context_new (NULL, &error);
 * ]|
 *
 * The return status should usually be enough to determine if there
 * was an error set (in this example we can check if context == %NULL)
 * but if it's not possible to tell from the function's return status
 * you can instead look directly at the error pointer which you
 * initialized to %NULL. In this example we now check the error,
 * report any error to the user, free the error and then simply
 * abort without attempting to recover.
 *
 * |[
 *   if (context == NULL)
 *     {
 *       fprintf (stderr, "Failed to create a Cogl context: %s\n",
 *                error->message);
 *       cogl_error_free (error);
 *       abort ();
 *     }
 * ]|
 *
 * All Cogl APIs that accept an error argument can also be passed a
 * %NULL pointer. In this case if an exceptional error condition is hit
 * then Cogl will simply log the error message and abort the
 * application. This can be compared to language execeptions where the
 * developer has not attempted to catch the exception. This means the
 * above example is essentially redundant because it's what Cogl would
 * have done automatically and so, similarly, if your application has
 * no way to recover from a particular error you might just as well
 * pass a %NULL #CoglError pointer to save a bit of typing.
 *
 * <note>If you are used to using the GLib API you will probably
 * recognize that #CoglError is just like a #GError. In fact if Cogl
 * has been built with --enable-glib then it is safe to cast a
 * #CoglError to a #GError.</note>
 *
 * <note>An important detail to be aware of if you are used to using
 * GLib's GError API is that Cogl deviates from the GLib GError
 * conventions in one noteable way which is that a %NULL error pointer
 * does not mean you want to ignore the details of an error, it means
 * you are not trying to catch any exceptional errors the function might
 * throw which will result in the program aborting with a log message
 * if an error is thrown.</note>
 */

#ifdef COGL_HAS_GLIB_SUPPORT
#define CoglError GError
#else
/**
 * CoglError:
 * @domain: A high-level domain identifier for the error
 * @code: A specific error code within a specified domain
 * @message: A human readable error message
 */
typedef struct _CoglError {
  uint32_t domain;
  int code;
  char *message;
} CoglError;
#endif /* COGL_HAS_GLIB_SUPPORT */

/**
 * cogl_error_free:
 * @error: A #CoglError thrown by the Cogl api
 *
 * Frees a #CoglError and associated resources.
 */
void
cogl_error_free (CoglError *error);

/**
 * cogl_error_copy:
 * @error: A #CoglError thrown by the Cogl api
 *
 * Makes a copy of @error which can later be freed using
 * cogl_error_free().
 *
 * Return value: A newly allocated #CoglError initialized to match the
 *               contents of @error.
 */
CoglError *
cogl_error_copy (CoglError *error);

/**
 * cogl_error_matches:
 * @error: A #CoglError thrown by the Cogl api or %NULL
 * @domain: The error domain
 * @code: The error code
 *
 * Returns %TRUE if error matches @domain and @code, %FALSE otherwise.
 * In particular, when error is %NULL, FALSE will be returned.
 *
 * Return value: whether the @error corresponds to the given @domain
 *               and @code.
 */
CoglBool
cogl_error_matches (CoglError *error,
                    uint32_t domain,
                    int code);

/**
 * COGL_GLIB_ERROR:
 * @COGL_ERROR: A #CoglError thrown by the Cogl api or %NULL
 *
 * Simply casts a #CoglError to a #CoglError
 *
 * If Cogl is built with GLib support then it can safely be assumed
 * that a CoglError is a GError and can be used directly with the
 * GError api.
 */
#ifdef COGL_HAS_GLIB_SUPPORT
#define COGL_GLIB_ERROR(COGL_ERROR) ((CoglError *)COGL_ERROR)
#endif

COGL_END_DECLS

#endif /* __COGL_ERROR_H__ */
