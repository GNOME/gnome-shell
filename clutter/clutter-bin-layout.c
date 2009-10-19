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
 * <example id="example-clutter-bin-layout">
 *  <title>How to pack actors inside a BinLayout</title>
 *  <para>The following code shows how to build a composite actor with
 *  a texture and a background, and add controls overlayed on top. The
 *  background is set to fill the whole allocation, whilst the texture
 *  is centered; there is a control in the top right corner and a label
 *  in the bottom, filling out the whole allocated width.</para>
 *  <programlisting>
 *  ClutterLayoutManager *manager;
 *  ClutterActor *box;
 *
 *  /&ast; create the layout first &ast;/
 *  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
 *                                   CLUTTER_BIN_ALIGNMENT_CENTER);
 *  box = clutter_box_new (layout); /&ast; then the container &ast;/
 *
 *  /&ast; we can use the layout object to add actors &ast;/
 *  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (layout), background,
 *                          CLUTTER_BIN_ALIGNMENT_FILL,
 *                          CLUTTER_BIN_ALIGNMENT_FILL);
 *  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (layout), icon,
 *                          CLUTTER_BIN_ALIGNMENT_CENTER,
 *                          CLUTTER_BIN_ALIGNMENT_CENTER);
 *
 *  /&ast; align to the bottom left &ast;/
 *  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (layout), label,
 *                          CLUTTER_BIN_ALIGNMENT_START,
 *                          CLUTTER_BIN_ALIGNMENT_END);
 *  /&ast; align to the top right &ast;/
 *  clutter_bin_layout_add (CLUTTER_BIN_LAYOUT (layout), button,
 *                          CLUTTER_BIN_ALIGNMENT_END,
 *                          CLUTTER_BIN_ALIGNMENT_START);
 *  </programlisting>
 * </example>
 *
 * #ClutterBinLayout is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-actor.h"
#include "clutter-animatable.h"
#include "clutter-bin-layout.h"
#include "clutter-child-meta.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-layout-meta.h"
#include "clutter-private.h"

#define CLUTTER_TYPE_BIN_LAYER          (clutter_bin_layer_get_type ())
#define CLUTTER_BIN_LAYER(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BIN_LAYER, ClutterBinLayer))
#define CLUTTER_IS_BIN_LAYER(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BIN_LAYER))

#define CLUTTER_BIN_LAYOUT_GET_PRIVATE(obj)     (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_BIN_LAYOUT, ClutterBinLayoutPrivate))

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
  PROP_LAYER_Y_ALIGN
};

enum
{
  PROP_0,

  PROP_X_ALIGN,
  PROP_Y_ALIGN
};

G_DEFINE_TYPE (ClutterBinLayer,
               clutter_bin_layer,
               CLUTTER_TYPE_LAYOUT_META);

G_DEFINE_TYPE (ClutterBinLayout,
               clutter_bin_layout,
               CLUTTER_TYPE_LAYOUT_MANAGER);

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

  g_object_notify (G_OBJECT (self), "x-align");
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

  g_object_notify (G_OBJECT (self), "y-align");
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
  GParamSpec *pspec;

  gobject_class->set_property = clutter_bin_layer_set_property;
  gobject_class->get_property = clutter_bin_layer_get_property;

  pspec = g_param_spec_enum ("x-align",
                             "Horizontal Alignment",
                             "Horizontal alignment for the actor "
                             "inside the layer",
                             CLUTTER_TYPE_BIN_ALIGNMENT,
                             CLUTTER_BIN_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_LAYER_X_ALIGN,
                                   pspec);

  pspec = g_param_spec_enum ("y-align",
                             "Vertical Alignment",
                             "Vertical alignment for the actor "
                             "inside the layer manager",
                             CLUTTER_TYPE_BIN_ALIGNMENT,
                             CLUTTER_BIN_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_LAYER_Y_ALIGN,
                                   pspec);
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
  GList *children = clutter_container_get_children (container);
  GList *l;
  gfloat available_w, available_h;

  available_w = clutter_actor_box_get_width (allocation);
  available_h = clutter_actor_box_get_height (allocation);

  for (l = children; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      ClutterLayoutMeta *meta;
      ClutterBinLayer *layer;
      ClutterActorBox child_alloc = { 0, };
      gfloat child_width, child_height;
      ClutterRequestMode request;

      meta = clutter_layout_manager_get_child_meta (manager,
                                                    container,
                                                    child);
      layer = CLUTTER_BIN_LAYER (meta);

      if (layer->x_align == CLUTTER_BIN_ALIGNMENT_FILL)
        {
          child_alloc.x1 = 0;
          child_alloc.x2 = ceilf (available_w);
        }

      if (layer->y_align == CLUTTER_BIN_ALIGNMENT_FILL)
        {
          child_alloc.y1 = 0;
          child_alloc.y2 = ceilf (available_h);
        }

      /* if we are filling horizontally and vertically then we
       * can break here because we already have a full allocation
       */
      if (layer->x_align == CLUTTER_BIN_ALIGNMENT_FILL &&
          layer->y_align == CLUTTER_BIN_ALIGNMENT_FILL)
        {
          clutter_actor_allocate (child, &child_alloc, flags);
          continue;
        }

      request = clutter_actor_get_request_mode (child);
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

      if (layer->x_align == CLUTTER_BIN_ALIGNMENT_FIXED)
        {
          child_alloc.x1 = ceilf (clutter_actor_get_x (child));
          child_alloc.x2 = ceilf (child_alloc.x1 + child_width);
        }
      else
        {
          gdouble x_align = get_bin_alignment_factor (layer->x_align);

          if (layer->x_align != CLUTTER_BIN_ALIGNMENT_FILL)
            {
              child_alloc.x1 = ceilf ((available_w - child_width) * x_align);
              child_alloc.x2 = ceilf (child_alloc.x1 + child_width);
            }
        }

      if (layer->y_align == CLUTTER_BIN_ALIGNMENT_FIXED)
        {
          child_alloc.y1 = ceilf (clutter_actor_get_y (child));
          child_alloc.y2 = ceilf (child_alloc.y1 + child_height);
        }
      else
        {
          gdouble y_align = get_bin_alignment_factor (layer->y_align);

          if (layer->y_align != CLUTTER_BIN_ALIGNMENT_FILL)
            {
              child_alloc.y1 = ceilf ((available_h - child_height) * y_align);
              child_alloc.y2 = ceilf (child_alloc.y1 + child_height);
            }
        }

      clutter_actor_allocate (child, &child_alloc, flags);
    }

  g_list_free (children);
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

  priv = CLUTTER_BIN_LAYOUT (manager)->priv;
  priv->container = container;
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
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (ClutterBinLayoutPrivate));

  gobject_class->set_property = clutter_bin_layout_set_property;
  gobject_class->get_property = clutter_bin_layout_get_property;

  /**
   * ClutterBinLayout:x-align:
   *
   * The default horizontal alignment policy for actors managed
   * by the #ClutterBinLayout
   *
   * Since: 1.2
   */
  pspec = g_param_spec_enum ("x-align",
                             "Horizontal Alignment",
                             "Default horizontal alignment for the actors "
                             "inside the layout manager",
                             CLUTTER_TYPE_BIN_ALIGNMENT,
                             CLUTTER_BIN_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_X_ALIGN, pspec);

  /**
   * ClutterBinLayout:y-align:
   *
   * The default vertical alignment policy for actors managed
   * by the #ClutterBinLayout
   *
   * Since: 1.2
   */
  pspec = g_param_spec_enum ("y-align",
                             "Vertical Alignment",
                             "Default vertical alignment for the actors "
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
  layout_class->create_child_meta =
    clutter_bin_layout_create_child_meta;
  layout_class->set_container =
    clutter_bin_layout_set_container;
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
