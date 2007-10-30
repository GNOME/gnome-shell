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
 *
 * ClutterLayout: interface to be implemented by actors providing
 *                extended layouts.
 *
 * Author: Emmanuele Bassi <ebassi@openedhand.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-layout.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-units.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"

#define MAX_TUNE_REQUESTS       3

/**
 * SECTION:clutter-layout
 * @short_description: An interface for implementing layouts
 *
 * #ClutterLayout is an interface that #ClutterActor<!-- -->s might
 * implement to provide complex or extended layouts. The default
 * size allocation of a #ClutterActor inside a #ClutterGroup is to
 * make the group size allocation grow enough to contain the actor.
 * A #ClutterActor implementing the #ClutterLayout interface will
 * be queried for its size when it is added to a #ClutterGroup subclass
 * that honours the #ClutterLayout interface; the resulting size
 * allocation will depend on the #ClutterLayoutFlags that the actor
 * supports.
 *
 * There are various types of layout available for actors implementing
 * the #ClutterLayout interface: %CLUTTER_LAYOUT_WIDTH_FOR_HEIGHT will
 * ask the actor for its width given the height allocated by the
 * container; %CLUTTER_LAYOUT_HEIGHT_FOR_WIDTH will ask the actor for
 * its height given the width allocated by the container. These two
 * layout types are especially useful for labels and unidirectional
 * container types, like vertical and horizontal boxes.
 *
 * Another layout available is %CLUTTER_LAYOUT_NATURAL, which will
 * query the actor for its natural (default) width and height; the
 * container actor will then try to allocate as much as it can,
 * and might resort to scaling the actor to fit the allocation. This
 * layout type is suited for #ClutterTexture<!-- -->s and shapes.
 *
 * Finally, the %CLUTTER_LAYOUT_TUNABLE is an iterative layout. An actor
 * will be queried multiple times until it's satisfied with the size
 * given.
 *
 * A #ClutterContainer implementation that honours the #ClutterLayout
 * interface should check whether an actor is implementing this interface
 * when adding it, by using the %CLUTTER_IS_LAYOUT type check macro. If the
 * actor does implement the interface, the #ClutterContainer should get
 * the supported layouts using clutter_layout_get_layout_flags() and
 * verify which layout is compatible with the group's own layout; for
 * instance, vertical containers should check for actors implementing the
 * %CLUTTER_LAYOUT_WIDTH_FOR_HEIGHT layout management, while horizontal
 * containers should check for actors implementing the
 * %CLUTTER_LAYOUT_HEIGHT_FOR_WIDTH layout management. If the actor
 * satisfies the layout requirements, the container actor should query
 * the actor for a geometry request using the appropriate function and
 * allocate space for the newly added actor accordingly.
 *
 * #ClutterLayout is available since Clutter 0.4
 */

static void
clutter_layout_base_init (gpointer g_iface)
{
  static gboolean initialised = FALSE;

  if (G_UNLIKELY (!initialised))
    {
      initialised = TRUE;

      /**
       * ClutterLayout:layout-flags:
       *
       * The layout types that the #ClutterLayout supports.
       *
       * Since: 0.4
       */
      g_object_interface_install_property (g_iface,
                                           g_param_spec_flags ("layout-flags",
                                                               "Layout Flags",
                                                               "Supported layouts",
                                                               CLUTTER_TYPE_LAYOUT_FLAGS,
                                                               CLUTTER_LAYOUT_NONE,
                                                               CLUTTER_PARAM_READABLE));
  }
}

GType
clutter_layout_get_type (void)
{
  static GType layout_type = 0;

  if (!layout_type)
    {
      GTypeInfo layout_info =
      {
        sizeof (ClutterLayoutIface),
        clutter_layout_base_init,
        NULL,
      };

      layout_type = g_type_register_static (G_TYPE_INTERFACE, "ClutterLayout",
                                            &layout_info, 0);
      g_type_interface_add_prerequisite (layout_type, CLUTTER_TYPE_ACTOR);
    }

  return layout_type;
}

/*
 * Public API
 */

/**
 * clutter_layout_get_layout_flags:
 * @layout: a #ClutterLayout
 *
 * Retrieves the supported layout types from the #ClutterLayout
 *
 * Return value: bitwise or of #ClutterLayoutFlags
 *
 * Since: 0.4
 */
ClutterLayoutFlags
clutter_layout_get_layout_flags (ClutterLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_LAYOUT (layout), CLUTTER_LAYOUT_NONE);

  if (CLUTTER_LAYOUT_GET_IFACE (layout)->get_layout_flags)
    return CLUTTER_LAYOUT_GET_IFACE (layout)->get_layout_flags (layout);

  return CLUTTER_LAYOUT_NONE;
}

/**
 * clutter_layout_width_for_height:
 * @layout: a #ClutterLayout
 * @width: return location for the width
 * @height: height allocated by the parent
 *
 * Queries a #ClutterLayout actor for its width with a known height.
 *
 * Since: 0.4
 */
void
clutter_layout_width_for_height (ClutterLayout *layout,
                                 gint          *width,
                                 gint           height)
{
  ClutterLayoutFlags layout_type;

  g_return_if_fail (CLUTTER_IS_LAYOUT (layout));

  layout_type = clutter_layout_get_layout_flags (layout);
  if (layout_type & CLUTTER_LAYOUT_WIDTH_FOR_HEIGHT)
    {
      ClutterUnit u_width, u_height;

      u_height = CLUTTER_UNITS_FROM_INT (height);
      CLUTTER_LAYOUT_GET_IFACE (layout)->width_for_height (layout,
                                                           &u_width,
                                                           u_height);

      if (width)
        *width = CLUTTER_UNITS_TO_INT (u_width);
    }
  else
    {
      g_warning ("Actor queried for width with a given height, but "
                 "actors of type `%s' do not support width-for-height "
                 "layouts.",
                 g_type_name (G_OBJECT_TYPE (layout)));

      if (width)
        *width = -1;
    }
}

/**
 * clutter_layout_height_for_width:
 * @layout: a #ClutterLayout
 * @width: width allocated by the parent
 * @height: return location for the height
 *
 * Queries a #ClutterLayout actor for its height with a known width.
 *
 * Since: 0.4
 */
void
clutter_layout_height_for_width (ClutterLayout *layout,
                                 gint           width,
                                 gint          *height)
{
  ClutterLayoutFlags layout_type;

  g_return_if_fail (CLUTTER_IS_LAYOUT (layout));

  layout_type = clutter_layout_get_layout_flags (layout);
  if (layout_type & CLUTTER_LAYOUT_HEIGHT_FOR_WIDTH)
    {
      ClutterUnit u_width, u_height;

      u_width = CLUTTER_UNITS_FROM_INT (width);
      CLUTTER_LAYOUT_GET_IFACE (layout)->height_for_width (layout,
                                                           u_width,
                                                           &u_height);

      if (height)
        *height = CLUTTER_UNITS_TO_INT (u_height);
    }
  else
    {
      g_warning ("Actor queried for height with a given width, but "
                 "actors of type `%s' do not support height-for-width "
                 "layouts.",
                 g_type_name (G_OBJECT_TYPE (layout)));

      if (height)
        *height = -1;
    }
}

/**
 * clutter_layout_natural_request:
 * @layout: a #ClutterLayout
 * @width: return location for the natural width
 * @height: return location for the natural height
 *
 * Queries a #ClutterLayout actor for its natural (default) width
 * and height.
 *
 * Since: 0.4
 */
void
clutter_layout_natural_request (ClutterLayout *layout,
                                gint          *width,
                                gint          *height)
{
  ClutterLayoutFlags layout_type;

  g_return_if_fail (CLUTTER_IS_LAYOUT (layout));

  layout_type = clutter_layout_get_layout_flags (layout);
  if (layout_type & CLUTTER_LAYOUT_NATURAL)
    {
      ClutterUnit u_width, u_height;

      CLUTTER_LAYOUT_GET_IFACE (layout)->natural_request (layout,
                                                          &u_width,
                                                          &u_height);

      if (width)
        *width = CLUTTER_UNITS_TO_INT (u_width);

      if (height)
        *height = CLUTTER_UNITS_TO_INT (u_height);
    }
  else
    {
      g_warning ("Actor queried for natural size, but actors of type `%s' "
                 "do not support natural-size layouts.",
                 g_type_name (G_OBJECT_TYPE (layout)));

      if (width)
        *width = -1;
      if (height)
        *height = -1;
    }
}

/**
 * clutter_layout_tune_request:
 * @layout: a #ClutterLayout
 * @given_width: width allocated by the parent
 * @given_height: height allocated by the parent
 * @width: return location for the new width
 * @height: return location for the new height
 *
 * Iteratively queries a #ClutterLayout actor until it finds
 * its desired size, given a width and height tuple.
 *
 * Since: 0.4
 */
void
clutter_layout_tune_request (ClutterLayout *layout,
                             gint           given_width,
                             gint           given_height,
                             gint          *width,
                             gint          *height)
{
  ClutterLayoutFlags layout_type;
  gint tries;
  ClutterUnit try_width, try_height;
  ClutterUnit new_width, new_height;

  g_return_if_fail (CLUTTER_IS_LAYOUT (layout));

  layout_type = clutter_layout_get_layout_flags (layout);
  if ((layout_type & CLUTTER_LAYOUT_TUNABLE) == 0)
    {
      g_warning ("Actor queried for tunable size size but actors of "
                 "type `%s' do not support tunable layouts.",
                 g_type_name (G_OBJECT_TYPE (layout)));

      if (width)
        *width = -1;

      if (height)
        *height = -1;

      return;
    }

  tries = 0;
  try_width = CLUTTER_UNITS_FROM_INT (given_width);
  try_height = CLUTTER_UNITS_FROM_INT (given_height);
  new_width = new_height = 0;

  do
    {
      gboolean res;

      res = CLUTTER_LAYOUT_GET_IFACE (layout)->tune_request (layout,
                                                             try_width,
                                                             try_height,
                                                             &new_width,
                                                             &new_height);

      if (res)
        break;

      if (new_width)
        try_width = new_width;

      if (new_height)
        try_height = new_height;

      new_width = new_height = 0;

      tries += 1;
    }
  while (tries <= MAX_TUNE_REQUESTS);

  if (width)
    *width = CLUTTER_UNITS_TO_INT (new_width);

  if (height)
    *height = CLUTTER_UNITS_TO_INT (new_height);
}
