/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-bin-layout
 * @short_description: A simple layout manager
 *
 * #ClutterBinLayout is a layout manager which implements the following
 * policy:
 *
 * <itemizedlist>
 *   <listitem><simpara>the preferred size is the maximum preferred size
 *   between all the children of the container using the
 *   layout;</simpara></listitem>
 *   <listitem><simpara>each child is allocated in "layers", on on top
 *   of the other;</simpara></listitem>
 *   <listitem><simpara>for each layer there are horizontal and vertical
 *   alignment policies.</simpara></listitem>
 * </itemizedlist>
 *
 * <figure id="bin-layout">
 *   <title>Bin layout</title>
 *   <para>The image shows a #ClutterBinLayout with three layers:
 *   a background #ClutterCairoTexture, set to fill on both the X
 *   and Y axis; a #ClutterTexture, set to center on both the X and
 *   Y axis; and a #ClutterRectangle, set to %CLUTTER_BIN_ALIGNMENT_END
 *   on both the X and Y axis.</para>
 *   <graphic fileref="bin-layout.png" format="PNG"/>
 * </figure>
 *
 * <example id="example-clutter-bin-layout">
 *  <title>How to pack actors inside a BinLayout</title>
 *  <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/bin-layout.c">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 *  </programlisting>
 * </example>
 *
 * #ClutterBinLayout is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-actor-private.h"
#include "clutter-animatable.h"
#include "clutter-bin-layout.h"
#include "clutter-child-meta.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-layout-meta.h"
#include "clutter-private.h"

#define CLUTTER_BIN_LAYOUT_GET_PRIVATE(obj)     (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_BIN_LAYOUT, ClutterBinLayoutPrivate))

typedef struct _ClutterLayoutMetaClass  ClutterBinLayerClass;

struct _ClutterBinLayoutPrivate
{
  ClutterContainer *container;
};

G_DEFINE_TYPE (ClutterBinLayout, clutter_bin_layout, CLUTTER_TYPE_LAYOUT_MANAGER)

static void
clutter_bin_layout_get_preferred_width (ClutterLayoutManager *manager,
                                        ClutterContainer     *container,
                                        gfloat                for_height,
                                        gfloat               *min_width_p,
                                        gfloat               *nat_width_p)
{
  ClutterActor *actor = CLUTTER_ACTOR (container);
  ClutterActorIter iter;
  ClutterActor *child;
  gfloat min_width, nat_width;

  min_width = nat_width = 0.0;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat minimum, natural;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      clutter_actor_get_preferred_width (child, for_height,
                                         &minimum,
                                         &natural);

      min_width = MAX (min_width, minimum);
      nat_width = MAX (nat_width, natural);
    }

  if (min_width_p)
    *min_width_p = min_width;

  if (nat_width_p)
    *nat_width_p = nat_width;
}

static void
clutter_bin_layout_get_preferred_height (ClutterLayoutManager *manager,
                                         ClutterContainer     *container,
                                         gfloat                for_width,
                                         gfloat               *min_height_p,
                                         gfloat               *nat_height_p)
{
  ClutterActor *actor = CLUTTER_ACTOR (container);
  ClutterActorIter iter;
  ClutterActor *child;
  gfloat min_height, nat_height;

  min_height = nat_height = 0.0;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat minimum, natural;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      clutter_actor_get_preferred_height (child, for_width,
                                          &minimum,
                                          &natural);

      min_height = MAX (min_height, minimum);
      nat_height = MAX (nat_height, natural);
    }

  if (min_height_p)
    *min_height_p = min_height;

  if (nat_height_p)
    *nat_height_p = nat_height;
}

static void
clutter_bin_layout_allocate (ClutterLayoutManager   *manager,
                             ClutterContainer       *container,
                             const ClutterActorBox  *allocation,
                             ClutterAllocationFlags  flags)
{
  gfloat allocation_x, allocation_y;
  gfloat available_w, available_h;
  ClutterActor *actor, *child;
  ClutterActorIter iter;

  clutter_actor_box_get_origin (allocation, &allocation_x, &allocation_y);
  clutter_actor_box_get_size (allocation, &available_w, &available_h);

  actor = CLUTTER_ACTOR (container);

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      ClutterActorBox child_alloc = { 0, };
      gboolean is_fixed_position_set;
      float fixed_x, fixed_y;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      fixed_x = fixed_y = 0.f;
      g_object_get (child,
                    "fixed-position-set", &is_fixed_position_set,
                    "fixed-x", &fixed_x,
                    "fixed-y", &fixed_y,
                    NULL);

      if (is_fixed_position_set)
	{
          child_alloc.x1 = fixed_x;
          child_alloc.y1 = fixed_y;
	}
      else
        {
          child_alloc.x1 = allocation_x;
          child_alloc.y1 = allocation_y;
        }

      child_alloc.x2 = available_w;
      child_alloc.y2 = available_h;

      clutter_actor_allocate (child, &child_alloc, flags);
    }
}

static void
clutter_bin_layout_set_container (ClutterLayoutManager *manager,
                                  ClutterContainer     *container)
{
  ClutterBinLayoutPrivate *priv;
  ClutterLayoutManagerClass *parent_class;

  priv = CLUTTER_BIN_LAYOUT (manager)->priv;
  priv->container = container;

  parent_class = CLUTTER_LAYOUT_MANAGER_CLASS (clutter_bin_layout_parent_class);
  parent_class->set_container (manager, container);
}

static void
clutter_bin_layout_class_init (ClutterBinLayoutClass *klass)
{
  ClutterLayoutManagerClass *layout_class =
    CLUTTER_LAYOUT_MANAGER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterBinLayoutPrivate));

  layout_class->get_preferred_width = clutter_bin_layout_get_preferred_width;
  layout_class->get_preferred_height = clutter_bin_layout_get_preferred_height;
  layout_class->allocate = clutter_bin_layout_allocate;
  layout_class->set_container = clutter_bin_layout_set_container;
}

static void
clutter_bin_layout_init (ClutterBinLayout *self)
{
  self->priv = CLUTTER_BIN_LAYOUT_GET_PRIVATE (self);
}

/**
 * clutter_bin_layout_new:
 * @x_align: the default alignment policy to be used on the
 *   horizontal axis
 * @y_align: the default alignment policy to be used on the
 *   vertical axis
 *
 * Creates a new #ClutterBinLayout layout manager
 *
 * Return value: the newly created layout manager
 *
 * Since: 1.2
 */
ClutterLayoutManager *
clutter_bin_layout_new (void)
{
  return g_object_new (CLUTTER_TYPE_BIN_LAYOUT, NULL);
}
