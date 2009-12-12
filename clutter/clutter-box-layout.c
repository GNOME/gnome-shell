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
 *
 * Based on the NBTK NbtkBoxLayout actor by:
 *   Thomas Wood <thomas.wood@intel.com>
 */

/**
 * SECTION:clutter-box-layout
 * @short_description: A layout manager arranging children on a single line
 *
 * The #ClutterBoxLayout is a #ClutterLayoutManager implementing the
 * following layout policy:
 * <itemizedlist>
 *   <listitem><para>all children are arranged on a single
 *   line;</para></listitem>
 *   <listitem><para>the axis used is controlled by the
 *   #ClutterBoxLayout:vertical boolean property;</para></listitem>
 *   <listitem><para>the order of the packing is determined by the
 *   #ClutterBoxLayout:pack-start boolean property;</para></listitem>
 *   <listitem><para>each child will be allocated to its natural
 *   size or, if set to expand, the available size;</para></listitem>
 *   <listitem><para>if a child is set to fill on either (or both)
 *   axis, its allocation will match all the available size; the
 *   fill layout property only makes sense if the expand property is
 *   also set;</para></listitem>
 *   <listitem><para>if a child is set to expand but not to fill then
 *   it is possible to control the alignment using the X and Y alignment
 *   layout properties.</para></listitem>
 * </itemizedlist>
 *
 *  <figure id="box-layout">
 *   <title>Box layout</title>
 *   <para>The image shows a #ClutterBoxLayout with the
 *   #ClutterBoxLayout:vertical property set to %FALSE.</para>
 *   <graphic fileref="box-layout.png" format="PNG"/>
 * </figure>
 *
 * It is possible to control the spacing between children of a
 * #ClutterBoxLayout by using clutter_box_layout_set_spacing().
 *
 * In order to set the layout properties when packing an actor inside a
 * #ClutterBoxLayout you should use the clutter_box_layout_pack()
 * function.
 *
 * #ClutterBoxLayout is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-box-layout.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-layout-meta.h"
#include "clutter-private.h"
#include "clutter-types.h"

#define CLUTTER_TYPE_BOX_CHILD          (clutter_box_child_get_type ())
#define CLUTTER_BOX_CHILD(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BOX_CHILD, ClutterBoxChild))
#define CLUTTER_IS_BOX_CHILD(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BOX_CHILD))

#define CLUTTER_BOX_LAYOUT_GET_PRIVATE(obj)     (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_BOX_LAYOUT, ClutterBoxLayoutPrivate))

typedef struct _ClutterBoxChild         ClutterBoxChild;
typedef struct _ClutterLayoutMetaClass  ClutterBoxChildClass;

struct _ClutterBoxLayoutPrivate
{
  ClutterContainer *container;

  guint spacing;

  GHashTable *allocations;

  guint is_vertical   : 1;
  guint is_pack_start : 1;
  guint is_animating  : 1;
};

struct _ClutterBoxChild
{
  ClutterLayoutMeta parent_instance;

  ClutterBoxAlignment x_align;
  ClutterBoxAlignment y_align;

  guint x_fill : 1;
  guint y_fill : 1;

  guint expand : 1;
};

enum
{
  PROP_CHILD_0,

  PROP_CHILD_X_ALIGN,
  PROP_CHILD_Y_ALIGN,
  PROP_CHILD_X_FILL,
  PROP_CHILD_Y_FILL,
  PROP_CHILD_EXPAND
};

enum
{
  PROP_0,

  PROP_SPACING,
  PROP_VERTICAL,
  PROP_PACK_START
};

G_DEFINE_TYPE (ClutterBoxChild,
               clutter_box_child,
               CLUTTER_TYPE_LAYOUT_META);

G_DEFINE_TYPE (ClutterBoxLayout,
               clutter_box_layout,
               CLUTTER_TYPE_LAYOUT_MANAGER);

/*
 * ClutterBoxChild
 */

static void
box_child_set_align (ClutterBoxChild     *self,
                     ClutterBoxAlignment  x_align,
                     ClutterBoxAlignment  y_align)
{
  gboolean x_changed = FALSE, y_changed = FALSE;

  if (self->x_align != x_align)
    {
      self->x_align = x_align;

      x_changed = TRUE;
    }

  if (self->y_align != y_align)
    {
      self->y_align = y_align;

      y_changed = TRUE;
    }

  if (x_changed || y_changed)
    {
      ClutterLayoutManager *layout;

      layout = clutter_layout_meta_get_manager (CLUTTER_LAYOUT_META (self));
      clutter_layout_manager_begin_animation (layout, 500, CLUTTER_EASE_OUT_CUBIC);
      clutter_layout_manager_layout_changed (layout);

      if (x_changed)
        g_object_notify (G_OBJECT (self), "x-align");

      if (y_changed)
        g_object_notify (G_OBJECT (self), "y-align");
    }
}

static void
box_child_set_fill (ClutterBoxChild *self,
                    gboolean         x_fill,
                    gboolean         y_fill)
{
  gboolean x_changed = FALSE, y_changed = FALSE;

  if (self->x_fill != x_fill)
    {
      self->x_fill = x_fill;

      x_changed = TRUE;
    }

  if (self->y_fill != y_fill)
    {
      self->y_fill = y_fill;

      y_changed = TRUE;
    }

  if (x_changed || y_changed)
    {
      ClutterLayoutManager *layout;

      layout = clutter_layout_meta_get_manager (CLUTTER_LAYOUT_META (self));
      clutter_layout_manager_begin_animation (layout, 500, CLUTTER_EASE_OUT_CUBIC);
      clutter_layout_manager_layout_changed (layout);

      if (x_changed)
        g_object_notify (G_OBJECT (self), "x-fill");

      if (y_changed)
        g_object_notify (G_OBJECT (self), "y-fill");
    }
}

static void
box_child_set_expand (ClutterBoxChild *self,
                      gboolean         expand)
{
  if (self->expand != expand)
    {
      ClutterLayoutManager *layout;

      self->expand = expand;

      layout = clutter_layout_meta_get_manager (CLUTTER_LAYOUT_META (self));
      clutter_layout_manager_begin_animation (layout, 500, CLUTTER_EASE_OUT_CUBIC);
      clutter_layout_manager_layout_changed (layout);

      g_object_notify (G_OBJECT (self), "expand");
    }
}

static void
clutter_box_child_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ClutterBoxChild *self = CLUTTER_BOX_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_X_ALIGN:
      box_child_set_align (self,
                           g_value_get_enum (value),
                           self->y_align);
      break;

    case PROP_CHILD_Y_ALIGN:
      box_child_set_align (self,
                           self->x_align,
                           g_value_get_enum (value));
      break;

    case PROP_CHILD_X_FILL:
      box_child_set_fill (self,
                          g_value_get_boolean (value),
                          self->y_fill);
      break;

    case PROP_CHILD_Y_FILL:
      box_child_set_fill (self,
                          self->x_fill,
                          g_value_get_boolean (value));
      break;

    case PROP_CHILD_EXPAND:
      box_child_set_expand (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_box_child_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ClutterBoxChild *self = CLUTTER_BOX_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_X_ALIGN:
      g_value_set_enum (value, self->x_align);
      break;

    case PROP_CHILD_Y_ALIGN:
      g_value_set_enum (value, self->y_align);
      break;

    case PROP_CHILD_X_FILL:
      g_value_set_boolean (value, self->x_fill);
      break;

    case PROP_CHILD_Y_FILL:
      g_value_set_boolean (value, self->y_fill);
      break;

    case PROP_CHILD_EXPAND:
      g_value_set_boolean (value, self->expand);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_box_child_class_init (ClutterBoxChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_box_child_set_property;
  gobject_class->get_property = clutter_box_child_get_property;

  pspec = g_param_spec_boolean ("expand",
                                "Expand",
                                "Allocate extra space for the child",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_EXPAND, pspec);

  pspec = g_param_spec_boolean ("x-fill",
                                "Horizontal Fill",
                                "Whether the child should receive priority "
                                "when the container is allocating spare space "
                                "on the horizontal axis",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_X_FILL, pspec);

  pspec = g_param_spec_boolean ("y-fill",
                                "Vertical Fill",
                                "Whether the child should receive priority "
                                "when the container is allocating spare space "
                                "on the vertical axis",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_Y_FILL, pspec);

  pspec = g_param_spec_enum ("x-align",
                             "Horizontal Alignment",
                             "Horizontal alignment of the actor within "
                             "the cell",
                             CLUTTER_TYPE_BOX_ALIGNMENT,
                             CLUTTER_BOX_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_X_ALIGN, pspec);

  pspec = g_param_spec_enum ("y-align",
                             "Vertical Alignment",
                             "Vertical alignment of the actor within "
                             "the cell",
                             CLUTTER_TYPE_BOX_ALIGNMENT,
                             CLUTTER_BOX_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_Y_ALIGN, pspec);
}

static void
clutter_box_child_init (ClutterBoxChild *self)
{
  self->x_align = CLUTTER_BOX_ALIGNMENT_CENTER;
  self->y_align = CLUTTER_BOX_ALIGNMENT_CENTER;

  self->x_fill = self->y_fill = FALSE;

  self->expand = FALSE;
}

static inline void
allocate_fill (ClutterActor    *child,
               ClutterActorBox *childbox,
               ClutterBoxChild *box_child)
{
  gfloat natural_width, natural_height;
  gfloat min_width, min_height;
  gfloat child_width, child_height;
  gfloat available_width, available_height;
  ClutterRequestMode request;
  ClutterActorBox allocation = { 0, };
  gdouble x_align, y_align;

  if (box_child->x_align == CLUTTER_BOX_ALIGNMENT_START)
    x_align = 0.0;
  else if (box_child->x_align == CLUTTER_BOX_ALIGNMENT_CENTER)
    x_align = 0.5;
  else
    x_align = 1.0;

  if (box_child->y_align == CLUTTER_BOX_ALIGNMENT_START)
    y_align = 0.0;
  else if (box_child->y_align == CLUTTER_BOX_ALIGNMENT_CENTER)
    y_align = 0.5;
  else
    y_align = 1.0;

  available_width  = childbox->x2 - childbox->x1;
  available_height = childbox->y2 - childbox->y1;

  if (available_width < 0)
    available_width = 0;

  if (available_height < 0)
    available_height = 0;

  if (box_child->x_fill)
    {
      allocation.x1 = childbox->x1;
      allocation.x2 = ceilf (allocation.x1 + available_width);
    }

  if (box_child->y_fill)
    {
      allocation.y1 = childbox->y1;
      allocation.y2 = ceilf (allocation.y1 + available_height);
    }

  /* if we are filling horizontally and vertically then we're done */
  if (box_child->x_fill && box_child->y_fill)
    {
      *childbox = allocation;
      return;
    }

  request = clutter_actor_get_request_mode (child);
  if (request == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
    {
      clutter_actor_get_preferred_width (child, available_height,
                                         &min_width,
                                         &natural_width);

      child_width = CLAMP (natural_width, min_width, available_width);

      clutter_actor_get_preferred_height (child, child_width,
                                          &min_height,
                                          &natural_height);

      child_height = CLAMP (natural_height, min_height, available_height);
    }
  else
    {
      clutter_actor_get_preferred_height (child, available_width,
                                          &min_height,
                                          &natural_height);

      child_height = CLAMP (natural_height, min_height, available_height);

      clutter_actor_get_preferred_width (child, child_height,
                                         &min_width,
                                         &natural_width);

      child_width = CLAMP (natural_width, min_width, available_width);
    }

  if (!box_child->x_fill)
    {
      allocation.x1 = ceilf (childbox->x1
                    + ((available_width - child_width) * x_align));
      allocation.x2 = ceilf (allocation.x1 + child_width);
    }

  if (!box_child->y_fill)
    {
      allocation.y1 = ceilf (childbox->y1
                    + ((available_height - child_height) * y_align));
      allocation.y2 = ceilf (allocation.y1 + child_height);
    }

  *childbox = allocation;
}

static GType
clutter_box_layout_get_child_meta_type (ClutterLayoutManager *manager)
{
  return CLUTTER_TYPE_BOX_CHILD;
}

static void
clutter_box_layout_set_container (ClutterLayoutManager *layout,
                                  ClutterContainer     *container)
{
  ClutterBoxLayoutPrivate *priv = CLUTTER_BOX_LAYOUT (layout)->priv;

  priv->container = container;

  if (priv->container != NULL)
    {
      ClutterRequestMode request_mode;

      /* we need to change the :request-mode of the container
       * to match the orientation
       */
      request_mode = (priv->is_vertical)
                   ? CLUTTER_REQUEST_WIDTH_FOR_HEIGHT
                   : CLUTTER_REQUEST_HEIGHT_FOR_WIDTH;
      clutter_actor_set_request_mode (CLUTTER_ACTOR (priv->container),
                                      request_mode);
    }
}

static void
get_preferred_width (ClutterBoxLayout *self,
                     ClutterContainer *container,
                     GList            *children,
                     gfloat            for_height,
                     gfloat           *min_width_p,
                     gfloat           *natural_width_p)
{
  ClutterBoxLayoutPrivate *priv = self->priv;
  gint n_children = 0;
  gboolean is_rtl;
  GList *l;

  if (min_width_p)
    *min_width_p = 0;

  if (natural_width_p)
    *natural_width_p = 0;

  if (!priv->is_vertical)
    {
      ClutterTextDirection text_dir;

      text_dir = clutter_actor_get_text_direction (CLUTTER_ACTOR (container));
      is_rtl = (text_dir == CLUTTER_TEXT_DIRECTION_RTL) ? TRUE : FALSE;
    }
  else
    is_rtl = FALSE;

  for (l = (is_rtl) ? g_list_last (children) : children;
       l != NULL;
       l = (is_rtl) ? l->prev : l->next)
    {
      ClutterActor *child = l->data;
      gfloat child_min = 0, child_nat = 0;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      n_children++;

      clutter_actor_get_preferred_width (child,
                                         (!priv->is_vertical)
                                           ? for_height
                                           : -1,
                                         &child_min,
                                         &child_nat);

      if (priv->is_vertical)
        {
          if (min_width_p)
            *min_width_p = MAX (child_min, *min_width_p);

          if (natural_width_p)
            *natural_width_p = MAX (child_nat, *natural_width_p);
        }
      else
        {
          if (min_width_p)
            *min_width_p += child_min;

          if (natural_width_p)
            *natural_width_p += child_nat;
        }
    }


  if (!priv->is_vertical && n_children > 1)
    {
      if (min_width_p)
        *min_width_p += priv->spacing * (n_children - 1);

      if (natural_width_p)
        *natural_width_p += priv->spacing * (n_children - 1);
    }
}

static void
get_preferred_height (ClutterBoxLayout *self,
                      ClutterContainer *container,
                      GList            *children,
                      gfloat            for_height,
                      gfloat           *min_height_p,
                      gfloat           *natural_height_p)
{
  ClutterBoxLayoutPrivate *priv = self->priv;
  gint n_children = 0;
  gboolean is_rtl;
  GList *l;

  if (min_height_p)
    *min_height_p = 0;

  if (natural_height_p)
    *natural_height_p = 0;

  if (!priv->is_vertical)
    {
      ClutterTextDirection text_dir;

      text_dir = clutter_actor_get_text_direction (CLUTTER_ACTOR (container));
      is_rtl = (text_dir == CLUTTER_TEXT_DIRECTION_RTL) ? TRUE : FALSE;
    }
  else
    is_rtl = FALSE;

  for (l = (is_rtl) ? g_list_last (children) : children;
       l != NULL;
       l = (is_rtl) ? l->prev : l->next)
    {
      ClutterActor *child = l->data;
      gfloat child_min = 0, child_nat = 0;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      n_children++;

      clutter_actor_get_preferred_height (child,
                                          (priv->is_vertical)
                                            ? for_height
                                            : -1,
                                          &child_min,
                                          &child_nat);

      if (!priv->is_vertical)
        {
          if (min_height_p)
            *min_height_p = MAX (child_min, *min_height_p);

          if (natural_height_p)
            *natural_height_p = MAX (child_nat, *natural_height_p);
        }
      else
        {
          if (min_height_p)
            *min_height_p += child_min;

          if (natural_height_p)
            *natural_height_p += child_nat;
        }
    }

  if (priv->is_vertical && n_children > 1)
    {
      if (min_height_p)
        *min_height_p += priv->spacing * (n_children - 1);

      if (natural_height_p)
        *natural_height_p += priv->spacing * (n_children - 1);
    }
}

static void
allocate_box_child (ClutterBoxLayout       *self,
                    ClutterContainer       *container,
                    ClutterActor           *child,
                    gfloat                 *position,
                    gfloat                  avail_width,
                    gfloat                  avail_height,
                    gfloat                  extra_space,
                    ClutterAllocationFlags  flags)
{
  ClutterBoxLayoutPrivate *priv = self->priv;
  ClutterActorBox child_box;
  ClutterBoxChild *box_child;
  ClutterLayoutMeta *meta;
  gfloat child_nat;

  if (!CLUTTER_ACTOR_IS_VISIBLE (child))
    return;

  meta = clutter_layout_manager_get_child_meta (CLUTTER_LAYOUT_MANAGER (self),
                                                container,
                                                child);
  box_child = CLUTTER_BOX_CHILD (meta);

  if (priv->is_vertical)
    {
      clutter_actor_get_preferred_height (child, avail_width,
                                          NULL, &child_nat);

      child_box.y1 = floorf (*position + 0.5);

      if (box_child->expand)
        child_box.y2 = floorf (*position + child_nat + extra_space + 0.5);
      else
        child_box.y2 = floorf (*position + child_nat + 0.5);

      child_box.x1 = 0;
      child_box.x2 = floorf (avail_width + 0.5);
    }
  else
    {
      clutter_actor_get_preferred_width (child, avail_height,
                                         NULL, &child_nat);

      child_box.x1 = floorf (*position + 0.5);

      if (box_child->expand)
        child_box.x2 = floorf (*position + child_nat + extra_space + 0.5);
      else
        child_box.x2 = floorf (*position + child_nat + 0.5);

      child_box.y1 = 0;
      child_box.y2 = floorf (avail_height + 0.5);
    }

  allocate_fill (child, &child_box, box_child);

  if (priv->is_animating)
    {
      ClutterLayoutManager *manager = CLUTTER_LAYOUT_MANAGER (self);
      ClutterActorBox *start = NULL;
      ClutterActorBox end = { 0, };
      gdouble p;

      p = clutter_layout_manager_get_animation_progress (manager);

      start = g_hash_table_lookup (priv->allocations, child);
      if (start == NULL)
        {
          /* if there is no allocation available then the child has just
           * been added to the container; we put it in the final state
           * and store its allocation for later
           */
          start = clutter_actor_box_copy (&child_box);
          g_hash_table_insert (priv->allocations, child, start);

          goto do_allocate;
        }

      end = child_box;

      /* interpolate between the initial and final values */
      clutter_actor_box_interpolate (start, &end, p, &child_box);

      CLUTTER_NOTE (ANIMATION,
                    "Animate { %.1f, %.1f, %.1f, %.1f }\t"
                     "%.3f * { %.1f, %.1f, %.1f, %.1f }\t"
                         "-> { %.1f, %.1f, %.1f, %.1f }",
                    start->x1, start->y1,
                    start->x2, start->y2,
                    p,
                    child_box.x1, child_box.y1,
                    child_box.x2, child_box.y2,
                    end.x1, end.y1,
                    end.x2, end.y2);
    }
  else
    {
      ClutterActorBox *start = clutter_actor_box_copy (&child_box);

      /* store the allocation for later animations */
      g_hash_table_replace (priv->allocations, child, start);
    }

do_allocate:
  clutter_actor_allocate (child, &child_box, flags);

  if (box_child->expand)
    *position += (child_nat + priv->spacing + extra_space);
  else
    *position += (child_nat + priv->spacing);
}

static void
clutter_box_layout_get_preferred_width (ClutterLayoutManager *layout,
                                        ClutterContainer     *container,
                                        gfloat                for_height,
                                        gfloat               *min_width_p,
                                        gfloat               *natural_width_p)
{
  ClutterBoxLayout *self = CLUTTER_BOX_LAYOUT (layout);
  GList *children;

  children = clutter_container_get_children (container);

  get_preferred_width (self, container, children, for_height,
                       min_width_p,
                       natural_width_p);

  g_list_free (children);
}

static void
clutter_box_layout_get_preferred_height (ClutterLayoutManager *layout,
                                         ClutterContainer     *container,
                                         gfloat                for_width,
                                         gfloat               *min_height_p,
                                         gfloat               *natural_height_p)
{
  ClutterBoxLayout *self = CLUTTER_BOX_LAYOUT (layout);
  GList *children;

  children = clutter_container_get_children (container);

  get_preferred_height (self, container, children, for_width,
                        min_height_p,
                        natural_height_p);

  g_list_free (children);
}

static void
clutter_box_layout_allocate (ClutterLayoutManager   *layout,
                             ClutterContainer       *container,
                             const ClutterActorBox  *box,
                             ClutterAllocationFlags  flags)
{
  ClutterBoxLayoutPrivate *priv = CLUTTER_BOX_LAYOUT (layout)->priv;
  gfloat avail_width, avail_height, pref_width, pref_height;
  gint n_expand_children, extra_space;
  GList *children, *l;
  gfloat position;
  gboolean is_rtl;

  children = clutter_container_get_children (container);
  if (children == NULL)
    return;

  clutter_actor_box_get_size (box, &avail_width, &avail_height);

  if (priv->is_vertical)
    {
      get_preferred_height (CLUTTER_BOX_LAYOUT (layout),
                            container,
                            children, avail_width,
                            NULL,
                            &pref_height);

      pref_width = avail_width;
    }
  else
    {
      get_preferred_width (CLUTTER_BOX_LAYOUT (layout),
                           container,
                           children, avail_height,
                           NULL,
                           &pref_width);

      pref_height = avail_height;
    }

  /* count the number of children with expand set to TRUE */
  n_expand_children = 0;
  for (l = children; l; l = l->next)
    {
      ClutterLayoutMeta *meta;

      meta = clutter_layout_manager_get_child_meta (layout,
                                                    container,
                                                    l->data);

      if (CLUTTER_BOX_CHILD (meta)->expand)
        n_expand_children++;
    }

  if (n_expand_children == 0)
    {
      extra_space = 0;
      n_expand_children = 1;
    }
  else
    {
      if (priv->is_vertical)
        extra_space = (avail_height - pref_height) / n_expand_children;
      else
        extra_space = (avail_width - pref_width) / n_expand_children;

      /* don't shrink anything */
      if (extra_space < 0)
        extra_space = 0;
    }

  position = 0;

  if (!priv->is_vertical)
    {
      ClutterTextDirection text_dir;

      text_dir = clutter_actor_get_text_direction (CLUTTER_ACTOR (container));
      is_rtl = (text_dir == CLUTTER_TEXT_DIRECTION_RTL) ? TRUE : FALSE;
    }
  else
    is_rtl = FALSE;

  if (is_rtl)
    {
      for (l = (priv->is_pack_start) ? children : g_list_last (children);
           l != NULL;
           l = (priv->is_pack_start) ? l->next : l->prev)
        {
          ClutterActor *child = l->data;

          allocate_box_child (CLUTTER_BOX_LAYOUT (layout), container, child,
                              &position,
                              avail_width,
                              avail_height,
                              extra_space, flags);
        }
    }
  else
    {
      for (l = (priv->is_pack_start) ? g_list_last (children) : children;
           l != NULL;
           l = (priv->is_pack_start) ? l->prev : l->next)
        {
          ClutterActor *child = l->data;

          allocate_box_child (CLUTTER_BOX_LAYOUT (layout), container, child,
                              &position,
                              avail_width,
                              avail_height,
                              extra_space, flags);
        }
    }

  g_list_free (children);
}

static void
clutter_box_layout_begin_animation (ClutterLayoutManager *manager,
                                    guint                 duration,
                                    gulong                easing)
{
  ClutterBoxLayoutPrivate *priv = CLUTTER_BOX_LAYOUT (manager)->priv;
  ClutterLayoutManagerClass *parent_class;

  if (priv->is_animating)
    return;

  priv->is_animating = TRUE;

  /* we want the default implementation */
  parent_class = CLUTTER_LAYOUT_MANAGER_CLASS (clutter_box_layout_parent_class);
  parent_class->begin_animation (manager, duration, easing);
}

static void
clutter_box_layout_end_animation (ClutterLayoutManager *manager)
{
  ClutterBoxLayoutPrivate *priv = CLUTTER_BOX_LAYOUT (manager)->priv;
  ClutterLayoutManagerClass *parent_class;

  priv->is_animating = FALSE;

  /* we want the default implementation */
  parent_class = CLUTTER_LAYOUT_MANAGER_CLASS (clutter_box_layout_parent_class);
  parent_class->end_animation (manager);
}

static void
clutter_box_layout_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterBoxLayout *self = CLUTTER_BOX_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_VERTICAL:
      clutter_box_layout_set_vertical (self, g_value_get_boolean (value));
      break;

    case PROP_SPACING:
      clutter_box_layout_set_spacing (self, g_value_get_uint (value));
      break;

    case PROP_PACK_START:
      clutter_box_layout_set_pack_start (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_box_layout_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ClutterBoxLayoutPrivate *priv = CLUTTER_BOX_LAYOUT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_VERTICAL:
      g_value_set_boolean (value, priv->is_vertical);
      break;

    case PROP_SPACING:
      g_value_set_uint (value, priv->spacing);
      break;

    case PROP_PACK_START:
      g_value_set_boolean (value, priv->is_pack_start);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_box_layout_finalize (GObject *gobject)
{
  ClutterBoxLayoutPrivate *priv = CLUTTER_BOX_LAYOUT (gobject)->priv;

  g_hash_table_destroy (priv->allocations);

  G_OBJECT_CLASS (clutter_box_layout_parent_class)->finalize (gobject);
}

static void
clutter_box_layout_class_init (ClutterBoxLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterLayoutManagerClass *layout_class;
  GParamSpec *pspec;

  layout_class = CLUTTER_LAYOUT_MANAGER_CLASS (klass);

  gobject_class->set_property = clutter_box_layout_set_property;
  gobject_class->get_property = clutter_box_layout_get_property;
  gobject_class->finalize = clutter_box_layout_finalize;

  layout_class->get_preferred_width =
    clutter_box_layout_get_preferred_width;
  layout_class->get_preferred_height =
    clutter_box_layout_get_preferred_height;
  layout_class->allocate = clutter_box_layout_allocate;
  layout_class->set_container = clutter_box_layout_set_container;
  layout_class->get_child_meta_type =
    clutter_box_layout_get_child_meta_type;
  layout_class->begin_animation = clutter_box_layout_begin_animation;
  layout_class->end_animation = clutter_box_layout_end_animation;

  g_type_class_add_private (klass, sizeof (ClutterBoxLayoutPrivate));

  /**
   * ClutterBoxLayout:vertical:
   *
   * Whether the #ClutterBoxLayout should arrange its children
   * alongside the Y axis, instead of alongside the X axis
   *
   * Since: 1.2
   */
  pspec = g_param_spec_boolean ("vertical",
                                "Vertical",
                                "Whether the layout should be vertical, rather"
                                "than horizontal",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_VERTICAL, pspec);

  /**
   * ClutterBoxLayout:pack-start:
   *
   * Whether the #ClutterBoxLayout should pack items at the start
   * or append them at the end
   *
   * Since: 1.2
   */
  pspec = g_param_spec_boolean ("pack-start",
                                "Pack Start",
                                "Whether to pack items at the start of the box",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_PACK_START, pspec);

  /**
   * ClutterBoxLayout:spacing:
   *
   * The spacing between children of the #ClutterBoxLayout, in pixels
   *
   * Since: 1.2
   */
  pspec = g_param_spec_uint ("spacing",
                             "Spacing",
                             "Spacing between children",
                             0, G_MAXUINT, 0,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_SPACING, pspec);
}

static void
clutter_box_layout_init (ClutterBoxLayout *layout)
{
  ClutterBoxLayoutPrivate *priv;

  layout->priv = priv = CLUTTER_BOX_LAYOUT_GET_PRIVATE (layout);

  priv->is_vertical = FALSE;
  priv->is_pack_start = FALSE;
  priv->spacing = 0;

  priv->allocations =
    g_hash_table_new_full (NULL, NULL,
                           NULL,
                           (GDestroyNotify) clutter_actor_box_free);
}

/**
 * clutter_box_layout_new:
 *
 * Creates a new #ClutterBoxLayout layout manager
 *
 * Return value: the newly created #ClutterBoxLayout
 *
 * Since: 1.2
 */
ClutterLayoutManager *
clutter_box_layout_new (void)
{
  return g_object_new (CLUTTER_TYPE_BOX_LAYOUT, NULL);
}

/**
 * clutter_box_layout_set_spacing:
 * @layout: a #ClutterBoxLayout
 * @spacing: the spacing between children of the layout, in pixels
 *
 * Sets the spacing between children of @layout
 *
 * Since: 1.2
 */
void
clutter_box_layout_set_spacing (ClutterBoxLayout *layout,
                                guint             spacing)
{
  ClutterBoxLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));

  priv = layout->priv;

  if (priv->spacing != spacing)
    {
      ClutterLayoutManager *manager;

      priv->spacing = spacing;

      manager = CLUTTER_LAYOUT_MANAGER (layout);
      clutter_layout_manager_begin_animation (manager, 500, CLUTTER_EASE_OUT_CUBIC);

      g_object_notify (G_OBJECT (layout), "spacing");
    }
}

/**
 * clutter_box_layout_get_spacing:
 * @layout: a #ClutterBoxLayout
 *
 * Retrieves the spacing set using clutter_box_layout_set_spacing()
 *
 * Return value: the spacing between children of the #ClutterBoxLayout
 *
 * Since: 1.2
 */
guint
clutter_box_layout_get_spacing (ClutterBoxLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout), 0);

  return layout->priv->spacing;
}

/**
 * clutter_box_layout_set_vertical:
 * @layout: a #ClutterBoxLayout
 * @vertical: %TRUE if the layout should be vertical
 *
 * Sets whether @layout should arrange its children vertically alongside
 * the Y axis, instead of horizontally alongside the X axis
 *
 * Since: 1.2
 */
void
clutter_box_layout_set_vertical (ClutterBoxLayout *layout,
                                 gboolean          vertical)
{
  ClutterBoxLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));

  priv = layout->priv;

  if (priv->is_vertical != vertical)
    {
      ClutterLayoutManager *manager;

      priv->is_vertical = vertical ? TRUE : FALSE;

      manager = CLUTTER_LAYOUT_MANAGER (layout);
      clutter_layout_manager_begin_animation (manager, 500, CLUTTER_EASE_OUT_CUBIC);

      g_object_notify (G_OBJECT (layout), "vertical");
    }
}

/**
 * clutter_box_layout_get_vertical:
 * @layout: a #ClutterBoxLayout
 *
 * Retrieves the orientation of the @layout as set using the
 * clutter_box_layout_set_vertical() function
 *
 * Return value: %TRUE if the #ClutterBoxLayout is arranging its children
 *   vertically, and %FALSE otherwise
 *
 * Since: 1.2
 */
gboolean
clutter_box_layout_get_vertical (ClutterBoxLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout), FALSE);

  return layout->priv->is_vertical;
}

/**
 * clutter_box_layout_set_pack_start:
 * @layout: a #ClutterBoxLayout
 * @pack_start: %TRUE if the @layout should pack children at the
 *   beginning of the layout
 *
 * Sets whether children of @layout should be layed out by appending
 * them or by prepending them
 *
 * Since: 1.2
 */
void
clutter_box_layout_set_pack_start (ClutterBoxLayout *layout,
                                   gboolean          pack_start)
{
  ClutterBoxLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));

  priv = layout->priv;

  if (priv->is_pack_start != pack_start)
    {
      ClutterLayoutManager *manager;

      priv->is_pack_start = pack_start ? TRUE : FALSE;

      manager = CLUTTER_LAYOUT_MANAGER (layout);
      clutter_layout_manager_begin_animation (manager, 500, CLUTTER_EASE_OUT_CUBIC);

      g_object_notify (G_OBJECT (layout), "pack-start");
    }
}

/**
 * clutter_box_layout_get_pack_start:
 * @layout: a #ClutterBoxLayout
 *
 * Retrieves the value set using clutter_box_layout_set_pack_start()
 *
 * Return value: %TRUE if the #ClutterBoxLayout should pack children
 *  at the beginning of the layout, and %FALSE otherwise
 *
 * Since: 1.2
 */
gboolean
clutter_box_layout_get_pack_start (ClutterBoxLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout), FALSE);

  return layout->priv->is_pack_start;
}

/**
 * clutter_box_layout_pack:
 * @layout: a #ClutterBoxLayout
 * @actor: a #ClutterActor
 * @expand: whether the @actor should expand
 * @x_fill: whether the @actor should fill horizontally
 * @y_fill: whether the @actor should fill vertically
 * @x_align: the horizontal alignment policy for @actor
 * @y_align: the vertical alignment policy for @actor
 *
 * Packs @actor inside the #ClutterContainer associated to @layout
 * and sets the layout properties
 *
 * Since: 1.2
 */
void
clutter_box_layout_pack (ClutterBoxLayout    *layout,
                         ClutterActor        *actor,
                         gboolean             expand,
                         gboolean             x_fill,
                         gboolean             y_fill,
                         ClutterBoxAlignment  x_align,
                         ClutterBoxAlignment  y_align)
{
  ClutterBoxLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before adding children",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  clutter_container_add_actor (priv->container, actor);

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  g_assert (CLUTTER_IS_BOX_CHILD (meta));

  box_child_set_align (CLUTTER_BOX_CHILD (meta), x_align, y_align);
  box_child_set_fill (CLUTTER_BOX_CHILD (meta), x_fill, y_fill);
  box_child_set_expand (CLUTTER_BOX_CHILD (meta), expand);
}

/**
 * clutter_box_layout_set_alignment:
 * @layout: a #ClutterBoxLayout
 * @actor: a #ClutterActor child of @layout
 * @x_align: Horizontal alignment policy for @actor
 * @y_align: Vertical alignment policy for @actor
 *
 * Sets the horizontal and vertical alignment policies for @actor
 * inside @layout
 *
 * Since: 1.2
 */
void
clutter_box_layout_set_alignment (ClutterBoxLayout    *layout,
                                  ClutterActor        *actor,
                                  ClutterBoxAlignment  x_align,
                                  ClutterBoxAlignment  y_align)
{
  ClutterBoxLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_BOX_CHILD (meta));

  box_child_set_align (CLUTTER_BOX_CHILD (meta), x_align, y_align);
}

/**
 * clutter_box_layout_get_alignment:
 * @layout: a #ClutterBoxLayout
 * @actor: a #ClutterActor child of @layout
 * @x_align: (out): return location for the horizontal alignment policy
 * @y_align: (out): return location for the vertical alignment policy
 *
 * Retrieves the horizontal and vertical alignment policies for @actor
 * as set using clutter_box_layout_pack() or clutter_box_layout_set_alignment()
 *
 * Since: 1.2
 */
void
clutter_box_layout_get_alignment (ClutterBoxLayout    *layout,
                                  ClutterActor        *actor,
                                  ClutterBoxAlignment *x_align,
                                  ClutterBoxAlignment *y_align)
{
  ClutterBoxLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_BOX_CHILD (meta));

  if (x_align)
    *x_align = CLUTTER_BOX_CHILD (meta)->x_align;

  if (y_align)
    *y_align = CLUTTER_BOX_CHILD (meta)->y_align;
}

/**
 * clutter_box_layout_set_fill:
 * @layout: a #ClutterBoxLayout
 * @actor: a #ClutterActor child of @layout
 * @x_fill: whether @actor should fill horizontally the allocated space
 * @y_fill: whether @actor should fill vertically the allocated space
 *
 * Sets the horizontal and vertical fill policies for @actor
 * inside @layout
 *
 * Since: 1.2
 */
void
clutter_box_layout_set_fill (ClutterBoxLayout *layout,
                             ClutterActor     *actor,
                             gboolean          x_fill,
                             gboolean          y_fill)
{
  ClutterBoxLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_BOX_CHILD (meta));

  box_child_set_fill (CLUTTER_BOX_CHILD (meta), x_fill, y_fill);
}

/**
 * clutter_box_layout_get_fill:
 * @layout: a #ClutterBoxLayout
 * @actor: a #ClutterActor child of @layout
 * @x_fill: (out): return location for the horizontal fill policy
 * @y_fill: (out): return location for the vertical fill policy
 *
 * Retrieves the horizontal and vertical fill policies for @actor
 * as set using clutter_box_layout_pack() or clutter_box_layout_set_fill()
 *
 * Since: 1.2
 */
void
clutter_box_layout_get_fill (ClutterBoxLayout *layout,
                             ClutterActor     *actor,
                             gboolean         *x_fill,
                             gboolean         *y_fill)
{
  ClutterBoxLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_BOX_CHILD (meta));

  if (x_fill)
    *x_fill = CLUTTER_BOX_CHILD (meta)->x_fill;

  if (y_fill)
    *y_fill = CLUTTER_BOX_CHILD (meta)->y_fill;
}

/**
 * clutter_box_layout_set_expand:
 * @layout: a #ClutterBoxLayout
 * @actor: a #ClutterActor child of @layout
 * @expand: whether @actor should expand
 *
 * Sets whether @actor should expand inside @layout
 *
 * Since: 1.2
 */
void
clutter_box_layout_set_expand (ClutterBoxLayout *layout,
                               ClutterActor     *actor,
                               gboolean          expand)
{
  ClutterBoxLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return;
    }

  g_assert (CLUTTER_IS_BOX_CHILD (meta));

  box_child_set_expand (CLUTTER_BOX_CHILD (meta), expand);
}

/**
 * clutter_box_layout_get_expand:
 * @layout: a #ClutterBoxLayout
 * @actor: a #ClutterActor child of @layout
 *
 * Retrieves whether @actor should expand inside @layout
 *
 * Return value: %TRUE if the #ClutterActor should expand, %FALSE otherwise
 *
 * Since: 1.2
 */
gboolean
clutter_box_layout_get_expand (ClutterBoxLayout *layout,
                               ClutterActor     *actor)
{
  ClutterBoxLayoutPrivate *priv;
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;

  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout), FALSE);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), FALSE);

  priv = layout->priv;

  if (priv->container == NULL)
    {
      g_warning ("The layout of type '%s' must be associated to "
                 "a ClutterContainer before querying layout "
                 "properties",
                 G_OBJECT_TYPE_NAME (layout));
      return FALSE;
    }

  manager = CLUTTER_LAYOUT_MANAGER (layout);
  meta = clutter_layout_manager_get_child_meta (manager,
                                                priv->container,
                                                actor);
  if (meta == NULL)
    {
      g_warning ("No layout meta found for the child of type '%s' "
                 "inside the layout manager of type '%s'",
                 G_OBJECT_TYPE_NAME (actor),
                 G_OBJECT_TYPE_NAME (manager));
      return FALSE;
    }

  g_assert (CLUTTER_IS_BOX_CHILD (meta));

  return CLUTTER_BOX_CHILD (meta)->expand;
}
