/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2008 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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
 * SECTION:cally
 * @Title: Cally
 * @short_description: Cally initialization methods.
 *
 * Cally initialization methods.
 *
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS

#include "cally.h"

#include "cally-actor.h"
#include "cally-group.h"
#include "cally-stage.h"
#include "cally-text.h"
#include "cally-texture.h"
#include "cally-rectangle.h"
#include "cally-clone.h"

#include "cally-factory.h"
#include "cally-util.h"

#include "clutter.h"

#include "clutter-debug.h"
#include "clutter-private.h"

/* factories initialization*/
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_ACTOR, cally_actor, cally_actor_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_GROUP, cally_group, cally_group_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_STAGE, cally_stage, cally_stage_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_TEXT, cally_text, cally_text_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_TEXTURE, cally_texture, cally_texture_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_RECTANGLE, cally_rectangle, cally_rectangle_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_CLONE, cally_clone, cally_clone_new)

/**
 * cally_accessibility_init:
 *
 * Initializes the accessibility support.
 *
 * Return value: %TRUE if accessibility support has been correctly
 * initialized.
 *
 * Since: 1.4
 */
gboolean
cally_accessibility_init (void)
{
  /* setting the factories */
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_ACTOR, cally_actor);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_GROUP, cally_group);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_STAGE, cally_stage);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_TEXT, cally_text);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_TEXTURE, cally_texture);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_RECTANGLE, cally_rectangle);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_CLONE, cally_clone);

  /* Initialize the CallyUtility class */
  _cally_util_override_atk_util ();

  CLUTTER_NOTE (MISC, "Clutter Accessibility initialized");

  return TRUE;
}

/**
 * cally_get_cally_initialized:
 *
 * Returns if the accessibility support using cally is enabled.
 *
 * Return value: %TRUE if accessibility support has been correctly
 * initialized.
 *
 * Since: 1.4
 */
gboolean cally_get_cally_initialized (void)
{
  return !g_strcmp0 (atk_get_toolkit_name (), "clutter");
}
