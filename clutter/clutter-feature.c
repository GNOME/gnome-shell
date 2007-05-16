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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-feature
 * @short_description: functions to query available GL features ay runtime 
 *
 * Functions to query available GL features ay runtime 
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "clutter-feature.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-debug.h"

#include "cogl.h"

typedef struct ClutterFeatures
{
  ClutterFeatureFlags flags;
  guint               features_set : 1;
} ClutterFeatures;

static ClutterFeatures* __features = NULL;
G_LOCK_DEFINE_STATIC (__features);

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

  context = clutter_context_get_default ();

  __features->flags = cogl_get_features()
                          |_clutter_backend_get_features (context->backend);

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

