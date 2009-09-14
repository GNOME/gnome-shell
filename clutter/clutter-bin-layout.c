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
 *
 * #ClutterBinLayout is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor.h"
#include "clutter-animatable.h"
#include "clutter-bin-layout.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"

#define CLUTTER_BIN_LAYOUT_GET_PRIVATE(obj)     (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_BIN_LAYOUT, ClutterBinLayoutPrivate))

struct _ClutterBinLayoutPrivate
{
  ClutterBinAlignment x_align;
  ClutterBinAlignment y_align;

  gdouble x_factor;
  gdouble y_factor;
};

enum
{
  PROP_0,

  PROP_X_ALIGN,
  PROP_Y_ALIGN
};

G_DEFINE_TYPE (ClutterBinLayout,
               clutter_bin_layout,
               CLUTTER_TYPE_LAYOUT_MANAGER);


static void
set_x_align (ClutterBinLayout    *self,
             ClutterBinAlignment  alignment)
{
  ClutterBinLayoutPrivate *priv = self->priv;

  if (priv->x_align != alignment)
    {
      ClutterLayoutManager *manager;

      priv->x_align = alignment;

      manager = CLUTTER_LAYOUT_MANAGER (self);
      clutter_layout_manager_layout_changed (manager);

      g_object_notify (G_OBJECT (self), "x-align");
    }
}

static void
set_y_align (ClutterBinLayout    *self,
             ClutterBinAlignment  alignment)
{
  ClutterBinLayoutPrivate *priv = self->priv;

  if (priv->y_align != alignment)
    {
      ClutterLayoutManager *manager;

      priv->y_align = alignment;

      manager = CLUTTER_LAYOUT_MANAGER (self);
      clutter_layout_manager_layout_changed (manager);

      g_object_notify (G_OBJECT (self), "y-align");
    }
}

static void
clutter_bin_layout_get_preferred_width (ClutterLayoutManager *manager,
                                        ClutterContainer     *container,
                                        gfloat                for_height,
                                        gfloat               *min_width_p,
                                        gfloat               *nat_width_p)
{
  GList *children = clutter_container_get_children (container);
  GList *l;
  gfloat min_width, nat_width;

  min_width = nat_width = 0.0;

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat minimum, natural;

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
  GList *children = clutter_container_get_children (container);
  GList *l;
  gfloat min_height, nat_height;

  min_height = nat_height = 0.0;

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      gfloat minimum, natural;

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

static gdouble
get_bin_alignment_factor (ClutterBinAlignment alignment)
{
  switch (alignment)
    {
    case CLUTTER_BIN_ALIGNMENT_CENTER:
      return 0.5;

    case CLUTTER_BIN_ALIGNMENT_START:
      return 0.0;

    case CLUTTER_BIN_ALIGNMENT_END:
      return 1.0;

    case CLUTTER_BIN_ALIGNMENT_FIXED:
    case CLUTTER_BIN_ALIGNMENT_FILL:
      return 0.0;
    }

  return 0.0;
}

static void
clutter_bin_layout_allocate (ClutterLayoutManager   *manager,
                             ClutterContainer       *container,
                             const ClutterActorBox  *allocation,
                             ClutterAllocationFlags  flags)
{
  ClutterBinLayoutPrivate *priv = CLUTTER_BIN_LAYOUT (manager)->priv;
  GList *children = clutter_container_get_children (container);
  GList *l;
  gfloat available_w, available_h;

  available_w = clutter_actor_box_get_width (allocation);
  available_h = clutter_actor_box_get_height (allocation);

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      ClutterActorBox child_alloc = { 0, };
      gfloat child_width, child_height;
      ClutterRequestMode request;

      if (priv->x_align == CLUTTER_BIN_ALIGNMENT_FILL)
        {
          child_alloc.x1 = (int) 0;
          child_alloc.x2 = (int) available_w;
        }

      if (priv->y_align == CLUTTER_BIN_ALIGNMENT_FILL)
        {
          child_alloc.y1 = (int) 0;
          child_alloc.y2 = (int) available_h;
        }

      /* if we are filling horizontally and vertically then we
       * can break here because we already have a full allocation
       */
      if (priv->x_align == CLUTTER_BIN_ALIGNMENT_FILL &&
          priv->y_align == CLUTTER_BIN_ALIGNMENT_FILL)
        {
          clutter_actor_allocate (child, &child_alloc, flags);
          break;
        }

      request = CLUTTER_REQUEST_HEIGHT_FOR_WIDTH;
      g_object_get (G_OBJECT (child), "request-mode", &request, NULL);
      if (request == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
        {
          gfloat min_width, nat_width;
          gfloat min_height, nat_height;

          clutter_actor_get_preferred_width (child, available_h,
                                             &min_width,
                                             &nat_width);
          child_width = CLAMP (nat_width, min_width, available_w);

          clutter_actor_get_preferred_height (child, child_width,
                                              &min_height,
                                              &nat_height);
          child_height = CLAMP (nat_height, min_height, available_h);
        }
      else if (request == CLUTTER_REQUEST_WIDTH_FOR_HEIGHT)
        {
          gfloat min_width, nat_width;
          gfloat min_height, nat_height;

          clutter_actor_get_preferred_height (child, available_w,
                                              &min_height,
                                              &nat_height);
          child_height = CLAMP (nat_height, min_height, available_h);

          clutter_actor_get_preferred_width (child, child_height,
                                             &min_width,
                                             &nat_width);
          child_width = CLAMP (nat_width, min_width, available_w);
        }

      if (priv->x_align == CLUTTER_BIN_ALIGNMENT_FIXED)
        {
          child_alloc.x1 = (int) clutter_actor_get_x (child);
          child_alloc.x2 = (int) child_alloc.x1 + child_width;
        }
      else
        {
          gdouble x_align = get_bin_alignment_factor (priv->x_align);

          if (priv->x_align != CLUTTER_BIN_ALIGNMENT_FILL)
            {
              child_alloc.x1 = (int) ((available_w - child_width) * x_align);
              child_alloc.x2 = (int) child_alloc.x1 + child_width;
            }
        }

      if (priv->y_align == CLUTTER_BIN_ALIGNMENT_FIXED)
        {
          child_alloc.y1 = (int) clutter_actor_get_y (child);
          child_alloc.y2 = (int) child_alloc.y1 + child_height;
        }
      else
        {
          gdouble y_align = get_bin_alignment_factor (priv->y_align);

          if (priv->y_align != CLUTTER_BIN_ALIGNMENT_FILL)
            {
              child_alloc.y1 = (int) ((available_h - child_height) * y_align);
              child_alloc.y2 = (int) child_alloc.y1 + child_height;
            }
        }

      clutter_actor_allocate (child, &child_alloc, flags);
    }

  g_list_free (children);
}

static void
clutter_bin_layout_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_X_ALIGN:
      set_x_align (CLUTTER_BIN_LAYOUT (gobject),
                   g_value_get_enum (value));
      break;

    case PROP_Y_ALIGN:
      set_y_align (CLUTTER_BIN_LAYOUT (gobject),
                   g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_bin_layout_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ClutterBinLayoutPrivate *priv;

  priv = CLUTTER_BIN_LAYOUT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_X_ALIGN:
      g_value_set_enum (value, priv->x_align);
      break;

    case PROP_Y_ALIGN:
      g_value_set_enum (value, priv->y_align);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_bin_layout_class_init (ClutterBinLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterLayoutManagerClass *layout_class =
    CLUTTER_LAYOUT_MANAGER_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (ClutterBinLayoutPrivate));

  gobject_class->set_property = clutter_bin_layout_set_property;
  gobject_class->get_property = clutter_bin_layout_get_property;

  /**
   * ClutterBinLayout:x-align:
   *
   * The horizontal alignment policy for actors managed by the
   * #ClutterBinLayout
   *
   * Since: 1.2
   */
  pspec = g_param_spec_enum ("x-align",
                             "X Align",
                             "Horizontal alignment for the actors "
                             "inside the layout manager",
                             CLUTTER_TYPE_BIN_ALIGNMENT,
                             CLUTTER_BIN_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_X_ALIGN, pspec);

  /**
   * ClutterBinLayout:y-align:
   *
   * The vertical alignment policy for actors managed by the
   * #ClutterBinLayout
   *
   * Since: 1.2
   */
  pspec = g_param_spec_enum ("y-align",
                             "Y Align",
                             "Vertical alignment for the actors "
                             "inside the layout manager",
                             CLUTTER_TYPE_BIN_ALIGNMENT,
                             CLUTTER_BIN_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_Y_ALIGN, pspec);

  layout_class->get_preferred_width =
    clutter_bin_layout_get_preferred_width;
  layout_class->get_preferred_height =
    clutter_bin_layout_get_preferred_height;
  layout_class->allocate =
    clutter_bin_layout_allocate;
}

static void
clutter_bin_layout_init (ClutterBinLayout *self)
{
  self->priv = CLUTTER_BIN_LAYOUT_GET_PRIVATE (self);

  self->priv->x_align = CLUTTER_BIN_ALIGNMENT_CENTER;
  self->priv->y_align = CLUTTER_BIN_ALIGNMENT_CENTER;
}

/**
 * clutter_bin_layout_new:
 * @x_align: the #ClutterBinAlignment policy to be used on the
 *   horizontal axis
 * @y_align: the #ClutterBinAlignment policy to be used on the
 *   vertical axis
 *
 * Creates a new #ClutterBinLayout layout manager
 *
 * Return value: the newly created layout manager
 *
 * Since: 1.2
 */
ClutterLayoutManager *
clutter_bin_layout_new (ClutterBinAlignment x_align,
                        ClutterBinAlignment y_align)
{
  return g_object_new (CLUTTER_TYPE_BIN_LAYOUT,
                       "x-align", x_align,
                       "y-align", y_align,
                       NULL);
}

/**
 * clutter_bin_layout_set_alignment:
 * @self: a #ClutterBinLayout
 * @x_align: the #ClutterBinAlignment policy to be used on the
 *   horizontal axis
 * @y_align: the #ClutterBinAlignment policy to be used on the
 *   vertical axis
 *
 * Sets the alignment policies on the horizontal and vertical
 * axis for @self
 *
 * Since: 1.2
 */
void
clutter_bin_layout_set_alignment (ClutterBinLayout    *self,
                                  ClutterBinAlignment  x_align,
                                  ClutterBinAlignment  y_align)
{
  g_return_if_fail (CLUTTER_IS_BIN_LAYOUT (self));

  g_object_freeze_notify (G_OBJECT (self));

  set_x_align (self, x_align);
  set_y_align (self, y_align);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_bin_layout_get_alignment:
 * @self: a #ClutterBinLayout
 * @x_align: (out) (allow-none): return location for the horizontal
 *   alignment policy
 * @y_align: (out) (allow-none): return location for the vertical
 *   alignment policy
 *
 * Retrieves the horizontal and vertical alignment policies for @self
 *
 * Since: 1.2
 */
void
clutter_bin_layout_get_alignment (ClutterBinLayout    *self,
                                  ClutterBinAlignment *x_align,
                                  ClutterBinAlignment *y_align)
{
  g_return_if_fail (CLUTTER_IS_BIN_LAYOUT (self));

  if (x_align)
    *x_align = self->priv->x_align;

  if (y_align)
    *y_align = self->priv->y_align;
}
