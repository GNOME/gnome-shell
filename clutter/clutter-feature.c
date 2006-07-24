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

static gulong __features;

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

gboolean
clutter_feature_available (gulong query)
{
  return (__features & query);
}

gulong 
clutter_feature_all (void)
{
  return __features;
}

void
clutter_feature_init (void)
{
  if (__features)
    return;

  __features = 0;

  if (check_gl_extension ("GL_ARB_texture_rectangle"))
      __features |= CLUTTER_FEATURE_TEXTURE_RECTANGLE;

}
