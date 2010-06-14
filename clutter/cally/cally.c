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

#include "cally.h"

#include "cally-factory.h"
#include "cally-util.h"

extern void gnome_accessibility_module_init     (void);
extern void gnome_accessibility_module_shutdown (void);

static int cally_initialized = FALSE;

/* factories initialization*/
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_ACTOR, cally_actor, cally_actor_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_GROUP, cally_group, cally_group_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_STAGE, cally_stage, cally_stage_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_TEXT, cally_text, cally_text_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_TEXTURE, cally_texture, cally_texture_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_RECTANGLE, cally_rectangle, cally_rectangle_new)
CALLY_ACCESSIBLE_FACTORY (CALLY_TYPE_CLONE, cally_clone, cally_clone_new)

void
cally_accessibility_module_init(void)
{
  if (cally_initialized)
    return;

  cally_initialized = TRUE;

  /* setting the factories */
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_ACTOR, cally_actor);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_GROUP, cally_group);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_STAGE, cally_stage);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_TEXT, cally_text);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_TEXTURE, cally_texture);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_RECTANGLE, cally_rectangle);
  CALLY_ACTOR_SET_FACTORY (CLUTTER_TYPE_CLONE, cally_clone);

  /* Initialize the CallyUtility class */
  g_type_class_unref (g_type_class_ref (CALLY_TYPE_UTIL));

  g_message ("Clutter Accessibility Module initialized");
}


/**
 * gnome_accessibility_module_shutdown:
 * @void:
 *
 * Common gnome hook to be used in order to activate the module
 **/
void
gnome_accessibility_module_init                 (void)
{
  cally_accessibility_module_init ();
}

/**
 * gnome_accessibility_module_shutdown:
 * @void:
 *
 * Common gnome hook to be used in order to de-activate the module
 **/
void
gnome_accessibility_module_shutdown             (void)
{
  if (!cally_initialized)
    return;

  cally_initialized = FALSE;

  g_message ("Clutter Accessibility Module shutdown");
}
