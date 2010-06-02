/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef COGL_HAS_GLX_SUPPORT
#include <GL/glx.h>
#endif

#include "cogl-context.h"
#include "cogl-feature-private.h"

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private) \
  static const CoglFeatureFunction                                      \
  cogl_winsys_feature_ ## name ## _funcs[] = {
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)                   \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglContext, winsys.pf_ ## name) },
#define COGL_WINSYS_FEATURE_END()               \
  { NULL, 0 },                                  \
    };
#include "cogl-winsys-feature-functions.h"

/* Define an array of features */
#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private) \
  { 255, 255, namespaces, extension_names,                              \
      feature_flags, feature_flags_private,                             \
      cogl_winsys_feature_ ## name ## _funcs },
#undef COGL_WINSYS_FEATURE_FUNCTION
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef COGL_WINSYS_FEATURE_END
#define COGL_WINSYS_FEATURE_END()

static const CoglFeatureData cogl_winsys_feature_data[] =
  {
#include "cogl-winsys-feature-functions.h"
  };

static const char *
_cogl_get_winsys_extensions (void)
{
#ifdef COGL_HAS_GLX_SUPPORT
  Display *display = _cogl_xlib_get_display ();

  return glXQueryExtensionsString (display, DefaultScreen (display));
#else
  return "";
#endif
}

static void
_cogl_winsys_features_init (CoglContext *context)
{
  CoglWinsysFeatureFlags flags = 0;
  const char *extensions = _cogl_get_winsys_extensions ();
  int i;

  for (i = 0; i < G_N_ELEMENTS (cogl_winsys_feature_data); i++)
    if (_cogl_feature_check ("GLX", cogl_winsys_feature_data + i, 0, 0,
                             extensions))
      flags |= cogl_winsys_feature_data[i].feature_flags;

  context->winsys.feature_flags = flags;
}

void
_cogl_create_context_winsys (CoglContext *context)
{
#ifdef COGL_HAS_XLIB_SUPPORT
  context->winsys.event_filters = NULL;
#endif

  _cogl_winsys_features_init (context);
}

#ifdef COGL_HAS_XLIB_SUPPORT

#include "cogl-xlib.h"

static void
free_xlib_filter_closure (gpointer data, gpointer user_data)
{
  g_slice_free (CoglXlibFilterClosure, data);
}

#endif

void
_cogl_destroy_context_winsys (CoglContext *context)
{
#ifdef COGL_HAS_XLIB_SUPPORT
  g_slist_foreach (context->winsys.event_filters,
                   free_xlib_filter_closure, NULL);
  g_slist_free (context->winsys.event_filters);
#endif
}
