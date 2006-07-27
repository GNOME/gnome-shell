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
 * SECTION:clutter-main
 * @short_description: Various 'global' clutter functions.
 *
 * Functions to retrieve various global Clutter resources and other utility
 * functions for mainloops, events and threads
 */

#ifndef _HAVE_CLUTTER_FEATURE_H
#define _HAVE_CLUTTER_FEATURE_H

#include <glib.h>
#include <GL/glx.h>
#include <GL/gl.h>

G_END_DECLS

typedef enum 
{
  CLUTTER_FEATURE_TEXTURE_RECTANGLE = (1 << 1)
} ClutterFeatureFlags;

gboolean            clutter_feature_available (ClutterFeatureFlags flags);
ClutterFeatureFlags clutter_feature_get_all   (void);

G_END_DECLS

#endif

