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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

/**
 * SECTION:clutter-feature
 * @short_description: Run-time detection of Clutter features
 *
 * Parts of Clutter depend on the underlying platform, including the
 * capabilities of the backend used and the OpenGL features exposed through the
 * Clutter and COGL API.
 *
 * It is possible to ask whether Clutter has support for specific features at
 * run-time.
 *
 * See also cogl_get_features() and #CoglFeatureFlags
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "clutter-feature.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-debug.h"

#include "cogl/cogl.h"

typedef struct ClutterFeatures
{
  ClutterFeatureFlags flags;
  guint               features_set : 1;
} ClutterFeatures;

static ClutterFeatures* __features = NULL;

ClutterFeatureFlags
_clutter_features_from_cogl (guint cogl_flags)
{
  ClutterFeatureFlags clutter_flags = 0;
  
  if (cogl_flags & COGL_FEATURE_TEXTURE_NPOT)
    clutter_flags |= CLUTTER_FEATURE_TEXTURE_NPOT;

  if (cogl_flags & COGL_FEATURE_TEXTURE_YUV)
    clutter_flags |= CLUTTER_FEATURE_TEXTURE_YUV;
  
  if (cogl_flags & COGL_FEATURE_TEXTURE_READ_PIXELS)
    clutter_flags |= CLUTTER_FEATURE_TEXTURE_READ_PIXELS;
  
  if (cogl_flags & COGL_FEATURE_SHADERS_GLSL)
    clutter_flags |= CLUTTER_FEATURE_SHADERS_GLSL;
  
  if (cogl_flags & COGL_FEATURE_OFFSCREEN)
    clutter_flags |= CLUTTER_FEATURE_OFFSCREEN;
  
  return clutter_flags;
}

void
_clutter_feature_init (void)
{
  ClutterMainContext *context;

  CLUTTER_NOTE (MISC, "checking features");

  if (!__features)
    {
      CLUTTER_NOTE (MISC, "allocating features data");
      __features = g_new0 (ClutterFeatures, 1);
      __features->features_set = FALSE; /* don't rely on zero-ing */
    }

  if (__features->features_set)
    return;

  context = _clutter_context_get_default ();

  /* makes sure we have a GL context; if we have, this is a no-op */
  _clutter_backend_create_context (context->backend, NULL);

  __features->flags = (_clutter_features_from_cogl (cogl_get_features ())
                    | _clutter_backend_get_features (context->backend));

  __features->features_set = TRUE;

  CLUTTER_NOTE (MISC, "features checked");
}

/**
 * clutter_feature_available:
 * @feature: a #ClutterFeatureFlags
 *
 * Checks whether @feature is available.  @feature can be a logical
 * OR of #ClutterFeatureFlags.
 *
 * Return value: %TRUE if a feature is available
 *
 * Since: 0.1.1
 */
gboolean
clutter_feature_available (ClutterFeatureFlags feature)
{
  if (G_UNLIKELY (!__features))
    return FALSE;

  return (__features->flags & feature);
}

/**
 * clutter_feature_get_all:
 *
 * Returns all the supported features.
 *
 * Return value: a logical OR of all the supported features.
 *
 * Since: 0.1.1
 */
ClutterFeatureFlags
clutter_feature_get_all (void)
{
  return __features->flags;
}

