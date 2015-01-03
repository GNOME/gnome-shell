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
 *   - the preferred size is the maximum preferred size
 *   between all the children of the container using the
 *   layout;
 *   - each child is allocated in "layers", on on top
 *   of the other;
 *   - for each layer there are horizontal and vertical
 *   alignment policies.
 *
 * The [bin-layout example](https://git.gnome.org/browse/clutter/tree/examples/bin-layout.c?h=clutter-1.18)
 * shows how to pack actors inside a #ClutterBinLayout.
 *
 * #ClutterBinLayout is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-container.h"
#include "deprecated/clutter-bin-layout.h"

#include "clutter-actor-private.h"
#include "clutter-animatable.h"
#include "clutter-child-meta.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-layout-meta.h"
#include "clutter-private.h"

#define CLUTTER_TYPE_BIN_LAYER          (clutter_bin_layer_get_type ())
#define CLUTTER_BIN_LAYER(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BIN_LAYER, ClutterBinLayer))
#define CLUTTER_IS_BIN_LAYER(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BIN_LAYER))

typedef struct _ClutterBinLayer         ClutterBinLayer;
typedef struct _ClutterLayoutMetaClass  ClutterBinLayerClass;

struct _ClutterBinLayoutPrivate
{
  ClutterBinAlignment x_align;
  ClutterBinAlignment y_align;

  ClutterContainer *container;
};

struct _ClutterBinLayer
{
  ClutterLayoutMeta parent_instance;

  ClutterBinAlignment x_align;
  ClutterBinAlignment y_align;
};

enum
{
  PROP_LAYER_0,

  PROP_LAYER_X_ALIGN,
  PROP_LAYER_Y_ALIGN,

  PROP_LAYER_LAST
};

enum
{
  PROP_0,

  PROP_X_ALIGN,
  PROP_Y_ALIGN,

  PROP_LAST
};

static GParamSpec *layer_props[PROP_LAYER_LAST] = { NULL, };
static GParamSpec *bin_props[PROP_LAST] = { NULL, };

GType clutter_bin_layer_get_type (void);

G_DEFINE_TYPE (ClutterBinLayer,
               clutter_bin_layer,
               CLUTTER_TYPE_LAYOUT_META)

G_DEFINE_TYPE_WITH_PRIVATE (ClutterBinLayout,
                            clutter_bin_layout,
                            CLUTTER_TYPE_LAYOUT_MANAGER)

/*
 * ClutterBinLayer
 */

static void
set_layer_x_align (ClutterBinLayer     *self,
                   ClutterBinAlignment  alignment)
{
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  if (self->x_align == alignment)
    return;

  self->x_align = alignment;

  meta = CLUTTER_LAYOUT_META (self);
  manager = clutter_layout_meta_get_manager (meta);
  clutter_layout_manager_layout_changed (manager);

  g_object_notify_by_pspec (G_OBJECT (self), layer_props[PROP_LAYER_X_ALIGN]);
}

static void
set_layer_y_align (ClutterBinLayer     *self,
                   ClutterBinAlignment  alignment)
{
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  if (self->y_align == alignment)
    return;

  self->y_align = alignment;

  meta = CLUTTER_LAYOUT_META (self);
  manager = clutter_layout_meta_get_manager (meta);
  clutter_layout_manager_layout_changed (manager);

  g_object_notify_by_pspec (G_OBJECT (self), layer_props[PROP_LAYER_Y_ALIGN]);
}

static void
clutter_bin_layer_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ClutterBinLayer *layer = CLUTTER_BIN_LAYER (gobject);

  switch (prop_id)
    {
    case PROP_LAYER_X_ALIGN:
      set_layer_x_align (layer, g_value_get_enum (value));
      break;

    case PROP_LAYER_Y_ALIGN:
      set_layer_y_align (layer, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_bin_layer_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ClutterBinLayer *layer = CLUTTER_BIN_LAYER (gobject);

  switch (prop_id)
    {
    case PROP_LAYER_X_ALIGN:
      g_value_set_enum (value, layer->x_align);
      break;

    case PROP_LAYER_Y_ALIGN:
      g_value_set_enum (value, layer->y_align);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_bin_layer_class_init (ClutterBinLayerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_bin_layer_set_property;
  gobject_class->get_property = clutter_bin_layer_get_property;

  layer_props[PROP_LAYER_X_ALIGN] =
    g_param_spec_enum ("x-align",
                       P_("Horizontal Alignment"),
                       P_("Horizontal alignment for the actor "
                          "inside the layout manager"),
                       CLUTTER_TYPE_BIN_ALIGNMENT,
                       CLUTTER_BIN_ALIGNMENT_CENTER,
                       CLUTTER_PARAM_READWRITE);

  layer_props[PROP_LAYER_Y_ALIGN] =
    g_param_spec_enum ("y-align",
                       P_("Vertical Alignment"),
                       P_("Vertical alignment for the actor "
                          "inside the layout manager"),
                       CLUTTER_TYPE_BIN_ALIGNMENT,
                       CLUTTER_BIN_ALIGNMENT_CENTER,
                       CLUTTER_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class,
                                     PROP_LAYER_LAST,
                                     layer_props);
}

static void
clutter_bin_layer_init (ClutterBinLayer *layer)
{
  layer->x_align = CLUTTER_BIN_ALIGNMENT_CENTER;
  layer->y_align = CLUTTER_BIN_ALIGNMENT_CENTER;
}

/*
 * ClutterBinLayout
 */

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

      g_object_notify_by_pspec (G_OBJECT (self), bin_props[PROP_X_ALIGN]);
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

      g_object_notify_by_pspec (G_OBJECT (self), bin_props[PROP_Y_ALIGN]);
    }
}

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

static gdouble
get_bin_alignment_factor (ClutterBinAlignment alignment,
                          ClutterTextDirection text_dir)
{
  switch (alignment)
    {
    case CLUTTER_BIN_ALIGNMENT_CENTER:
      return 0.5;

    case CLUTTER_BIN_ALIGNMENT_START:
      return text_dir == CLUTTER_TEXT_DIRECTION_LTR ? 0.0 : 1.0;

    case CLUTTER_BIN_ALIGNMENT_END:
      return text_dir == CLUTTER_TEXT_DIRECTION_LTR ? 1.0 : 0.0;

    case CLUTTER_BIN_ALIGNMENT_FIXED:
    case CLUTTER_BIN_ALIGNMENT_FILL:
      return 0.0;
    }

  return 0.0;
}

static gdouble
get_actor_align_factor (ClutterActorAlign alignment)
{
  switch (alignment)
    {
    case CLUTTER_ACTOR_ALIGN_CENTER:
      return 0.5;

    case CLUTTER_ACTOR_ALIGN_START:
      return 0.0;

    case CLUTTER_ACTOR_ALIGN_END:
      return 1.0;

    case CLUTTER_ACTOR_ALIGN_FILL:
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
      ClutterLayoutMeta *meta;
      ClutterBinLayer *layer;
      ClutterActorBox child_alloc = { 0, };
      gdouble x_align, y_align;
      gboolean x_fill, y_fill, is_fixed_position_set;
      float fixed_x, fixed_y;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      meta = clutter_layout_manager_get_child_meta (manager,
                                                    container,
                                                    child);
      layer = CLUTTER_BIN_LAYER (meta);

      fixed_x = fixed_y = 0.f;
      g_object_get (child,
                    "fixed-position-set", &is_fixed_position_set,
                    "fixed-x", &fixed_x,
                    "fixed-y", &fixed_y,
                    NULL);

      /* XXX:2.0 - remove the FIXED alignment, and just use the fixed position
       * of the actor if one is set
       */
      if (is_fixed_position_set ||
          layer->x_align == CLUTTER_BIN_ALIGNMENT_FIXED)
	{
          if (is_fixed_position_set)
            child_alloc.x1 = fixed_x;
          else
            child_alloc.x1 = clutter_actor_get_x (child);
	}
      else
        child_alloc.x1 = allocation_x;

      if (is_fixed_position_set ||
          layer->y_align == CLUTTER_BIN_ALIGNMENT_FIXED)
	{
	  if (is_fixed_position_set)
            child_alloc.y1 = fixed_y;
          else
	    child_alloc.y1 = clutter_actor_get_y (child);
	}
      else
        child_alloc.y1 = allocation_y;

      child_alloc.x2 = allocation_x + available_w;
      child_alloc.y2 = allocation_y + available_h;

      if (clutter_actor_needs_expand (child, CLUTTER_ORIENTATION_HORIZONTAL))
        {
          ClutterActorAlign align;

          align = clutter_actor_get_x_align (child);
          x_fill = align == CLUTTER_ACTOR_ALIGN_FILL;
          x_align = get_actor_align_factor (align);
        }
      else
        {
          ClutterTextDirection text_dir;

          x_fill = (layer->x_align == CLUTTER_BIN_ALIGNMENT_FILL);

          text_dir = clutter_actor_get_text_direction (child);

          if (!is_fixed_position_set)
            x_align = get_bin_alignment_factor (layer->x_align, text_dir);
          else
            x_align = 0.0;
        }

      if (clutter_actor_needs_expand (child, CLUTTER_ORIENTATION_VERTICAL))
        {
          ClutterActorAlign align;

          align = clutter_actor_get_y_align (child);
          y_fill = align == CLUTTER_ACTOR_ALIGN_FILL;
          y_align = get_actor_align_factor (align);
        }
      else
        {
          y_fill = (layer->y_align == CLUTTER_BIN_ALIGNMENT_FILL);

          if (!is_fixed_position_set)
            y_align = get_bin_alignment_factor (layer->y_align,
                                                CLUTTER_TEXT_DIRECTION_LTR);
          else
            y_align = 0.0;
        }

      clutter_actor_allocate_align_fill (child, &child_alloc,
                                         x_align, y_align,
                                         x_fill, y_fill,
                                         flags);
    }
}

static GType
clutter_bin_layout_get_child_meta_type (ClutterLayoutManager *manager)
{
  return CLUTTER_TYPE_BIN_LAYER;
}

static ClutterLayoutMeta *
clutter_bin_layout_create_child_meta (ClutterLayoutManager *manager,
                                      ClutterContainer     *container,
                                      ClutterActor         *actor)
{
  ClutterBinLayoutPrivate *priv;

  priv = CLUTTER_BIN_LAYOUT (manager)->priv;

  return g_object_new (CLUTTER_TYPE_BIN_LAYER,
                       "container", container,
                       "actor", actor,
                       "manager", manager,
                       "x-align", priv->x_align,
                       "y_align", priv->y_align,
                       NULL);
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
clutter_bin_layout_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterBinLayout *layout = CLUTTER_BIN_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_X_ALIGN:
      set_x_align (layout, g_value_get_enum (value));
      break;

    case PROP_Y_ALIGN:
      set_y_align (layout, g_value_get_enum (value));
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

  /**
   * ClutterBinLayout:x-align:
   *
   * The default horizontal alignment policy for actors managed
   * by the #ClutterBinLayout
   *
   * Since: 1.2
   *
   * Deprecated: 1.12: Use the #ClutterActor:x-expand and the
   *   #ClutterActor:x-align properties on #ClutterActor instead.
   */
  bin_props[PROP_X_ALIGN] =
    g_param_spec_enum ("x-align",
                       P_("Horizontal Alignment"),
                       P_("Default horizontal alignment for the actors "
                          "inside the layout manager"),
                       CLUTTER_TYPE_BIN_ALIGNMENT,
                       CLUTTER_BIN_ALIGNMENT_CENTER,
                       CLUTTER_PARAM_READWRITE);

  /**
   * ClutterBinLayout:y-align:
   *
   * The default vertical alignment policy for actors managed
   * by the #ClutterBinLayout
   *
   * Since: 1.2
   *
   * Deprecated: 1.12: Use the #ClutterActor:y-expand and the
   *   #ClutterActor:y-align properties on #ClutterActor instead.
   */
  bin_props[PROP_Y_ALIGN] =
    g_param_spec_enum ("y-align",
                       P_("Vertical Alignment"),
                       P_("Default vertical alignment for the actors "
                          "inside the layout manager"),
                       CLUTTER_TYPE_BIN_ALIGNMENT,
                       CLUTTER_BIN_ALIGNMENT_CENTER,
                       CLUTTER_PARAM_READWRITE);

  gobject_class->set_property = clutter_bin_layout_set_property;
  gobject_class->get_property = clutter_bin_layout_get_property;
  g_object_class_install_properties (gobject_class, PROP_LAST, bin_props);

  layout_class->get_preferred_width = clutter_bin_layout_get_preferred_width;
  layout_class->get_preferred_height = clutter_bin_layout_get_preferred_height;
  layout_class->allocate = clutter_bin_layout_allocate;
  layout_class->create_child_meta = clutter_bin_layout_create_child_meta;
  layout_class->get_child_meta_type = clutter_bin_layout_get_child_meta_type;
  layout_class->set_container = clutter_bin_layout_set_container;
}

static void
clutter_bin_layout_init (ClutterBinLayout *self)
{
  self->priv = clutter_bin_layout_get_instance_private (self);

  self->priv->x_align = CLUTTER_BIN_ALIGNMENT_CENTER;
  self->priv->y_align = CLUTTER_BIN_ALIGNMENT_CENTER;
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
 * @child: (allow-none): a child of @container
 * @x_align: the horizontal alignment policy to be used for the @child
 *   inside @container
 * @y_align: the vertical aligment policy to be used on the @child
 *   inside @container
 *
 * Sets the horizontal and vertical alignment policies to be applied
 * to a @child of @self
 *
 * If @child is %NULL then the @x_align and @y_align values will
 * be set as the default alignment policies
 *
 * Since: 1.2
 *
 * Deprecated: 1.12: Use the #ClutterActor:x-align and
 *   #ClutterActor:y-align properties of #ClutterActor instead.
 */
void
clutter_bin_layout_set_alignment (ClutterBinLayout    *self,
                                  ClutterActor        *child,
                                  ClutterBinAlignment  x_align,
                                  ClutterBinAlignment  y_align)
{
  ClutterBinLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_BIN_LAYOUT (self));
  g_return_if_fail (child == NULL || CLUTTER_IS_ACTOR (child));

  priv = self->priv;

  if (priv->container == NULL)
    {
      if (child == NULL)
        {
          set_x_align (self, x_align);
          set_y_align (self, y_align);
        }
      else
        g_warning ("The layout of type '%s' must be associated to "
                   "a ClutterContainer before setting the alignment "
                   "on its children",
                   G_OBJECT_TYPE_NAME (self));

      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (self);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                child);
  g_assert (CLUTTER_IS_BIN_LAYER (meta));

  set_layer_x_align (CLUTTER_BIN_LAYER (meta), x_align);
  set_layer_y_align (CLUTTER_BIN_LAYER (meta), y_align);
}

/**
 * clutter_bin_layout_get_alignment:
 * @self: a #ClutterBinLayout
 * @child: (allow-none): a child of @container
 * @x_align: (out) (allow-none): return location for the horizontal
 *   alignment policy
 * @y_align: (out) (allow-none): return location for the vertical
 *   alignment policy
 *
 * Retrieves the horizontal and vertical alignment policies for
 * a child of @self
 *
 * If @child is %NULL the default alignment policies will be returned
 * instead
 *
 * Since: 1.2
 *
 * Deprecated: 1.12: Use the #ClutterActor:x-align and the
 *   #ClutterActor:y-align properties of #ClutterActor instead.
 */
void
clutter_bin_layout_get_alignment (ClutterBinLayout    *self,
                                  ClutterActor        *child,
                                  ClutterBinAlignment *x_align,
                                  ClutterBinAlignment *y_align)
{
  ClutterBinLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;
  ClutterBinLayer *layer;

  g_return_if_fail (CLUTTER_IS_BIN_LAYOUT (self));

  priv = self->priv;

  if (priv->container == NULL)
    {
      if (child == NULL)
        {
          if (x_align)
            *x_align = priv->x_align;

          if (y_align)
            *y_align = priv->y_align;
        }
      else
        g_warning ("The layout of type '%s' must be associated to "
                   "a ClutterContainer before getting the alignment "
                   "of its children",
                   G_OBJECT_TYPE_NAME (self));

      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (self);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                child);
  g_assert (CLUTTER_IS_BIN_LAYER (meta));

  layer = CLUTTER_BIN_LAYER (meta);

  if (x_align)
    *x_align = layer->x_align;

  if (y_align)
    *y_align = layer->y_align;
}

/**
 * clutter_bin_layout_add:
 * @self: a #ClutterBinLayout
 * @child: a #ClutterActor
 * @x_align: horizontal alignment policy for @child
 * @y_align: vertical alignment policy for @child
 *
 * Adds a #ClutterActor to the container using @self and
 * sets the alignment policies for it
 *
 * This function is equivalent to clutter_container_add_actor()
 * and clutter_layout_manager_child_set_property() but it does not
 * require a pointer to the #ClutterContainer associated to the
 * #ClutterBinLayout
 *
 * Since: 1.2
 *
 * Deprecated: 1.12: Use clutter_actor_add_child() instead.
 */
void
clutter_bin_layout_add (ClutterBinLayout    *self,
                        ClutterActor        *child,
                        ClutterBinAlignment  x_align,
                        ClutterBinAlignment  y_align)
{
  ClutterBinLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_BIN_LAYOUT (self));
  g_return_if_fail (CLUTTER_IS_ACTOR (child));

  priv = self->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before adding children",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  clutter_container_add_actor (priv->container, child);

  manager = CLUTTER_LAYOUT_MANAGER (self);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                child);
  g_assert (CLUTTER_IS_BIN_LAYER (meta));

  set_layer_x_align (CLUTTER_BIN_LAYER (meta), x_align);
  set_layer_y_align (CLUTTER_BIN_LAYER (meta), y_align);
}
