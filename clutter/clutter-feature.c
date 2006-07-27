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
#include "clutter-feature.h"
#include "string.h"

G_LOCK_DEFINE_STATIC (__features);
static ClutterFeatureFlags __features;

/* Note must be called after context created */
static gboolean 
check_gl_extension (const gchar *name)
{
  const gchar *ext;
  gchar       *end;
  gint         name_len, n;

  ext = (const gchar*)glGetString(GL_EXTENSIONS);

  if (name == NULL || ext == NULL)
    return FALSE;

  end = (gchar*)(ext + strlen(ext));

  name_len = strlen(name);

  while (ext < end) 
    {
      n = strcspn(ext, " ");

      if ((name_len == n) && (!strncmp(name, ext, n)))
	return TRUE;
      ext += (n + 1);
    }

  return FALSE;
}

/* HOLDS: __features lock */
static void
clutter_feature_init_do (void)
{
  if (__features)
    return;

  __features = 0;

  if (check_gl_extension ("GL_ARB_texture_rectangle"))
      __features |= CLUTTER_FEATURE_TEXTURE_RECTANGLE;
}

void
clutter_feature_init (void)
{
  G_LOCK (__features);
  clutter_feature_init_do ();
  G_UNLOCK (__features);
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
  clutter_feature_init ();
  return (__features & feature);
}

/**
 * clutter_feature_all:
 *
 * Returns all the suppoerted features.
 *
 * Return value: a logical OR of all the supported features.
 *
 * Since: 0.1.1
 */
ClutterFeatureFlags
clutter_feature_all (void)
{
  clutter_feature_init ();
  return __features;
}
