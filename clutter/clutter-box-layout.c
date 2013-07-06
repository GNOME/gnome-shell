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
 *   #ClutterBoxLayout:orientation property;</para></listitem>
 *   <listitem><para>the order of the packing is determined by the
 *   #ClutterBoxLayout:pack-start boolean property;</para></listitem>
 *   <listitem><para>each child will be allocated to its natural
 *   size or, if #ClutterActor:x-expand/#ClutterActor:y-expand
 *   is set, the available size;</para></listitem>
 *   <listitem><para>honours the #ClutterActor's #ClutterActor:x-align
 *   and #ClutterActor:y-align properties to fill the available
 *   size;</para></listitem>
 *   <listitem><para>if the #ClutterBoxLayout:homogeneous boolean property
 *   is set, then all widgets will get the same size, ignoring expand
 *   settings and the preferred sizes</para></listitem>
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
 * #ClutterBoxLayout is available since Clutter 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-container.h"
#include "deprecated/clutter-alpha.h"

#include "clutter-box-layout.h"

#include "clutter-actor-private.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-layout-meta.h"
#include "clutter-private.h"
#include "clutter-types.h"

#define CLUTTER_TYPE_BOX_CHILD          (clutter_box_child_get_type ())
#define CLUTTER_BOX_CHILD(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BOX_CHILD, ClutterBoxChild))
#define CLUTTER_IS_BOX_CHILD(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BOX_CHILD))

typedef struct _ClutterBoxChild         ClutterBoxChild;
typedef struct _ClutterLayoutMetaClass  ClutterBoxChildClass;

struct _ClutterBoxLayoutPrivate
{
  ClutterContainer *container;

  guint spacing;

  gulong easing_mode;
  guint easing_duration;

  ClutterOrientation orientation;

  guint is_pack_start  : 1;
  guint use_animations : 1;
  guint is_homogeneous : 1;
};

struct _ClutterBoxChild
{
  ClutterLayoutMeta parent_instance;

  ClutterBoxAlignment x_align;
  ClutterBoxAlignment y_align;

  guint x_fill              : 1;
  guint y_fill              : 1;
  guint expand              : 1;
};

enum
{
  PROP_CHILD_0,

  PROP_CHILD_X_ALIGN,
  PROP_CHILD_Y_ALIGN,
  PROP_CHILD_X_FILL,
  PROP_CHILD_Y_FILL,
  PROP_CHILD_EXPAND,

  PROP_CHILD_LAST
};

enum
{
  PROP_0,

  PROP_SPACING,
  PROP_VERTICAL,
  PROP_HOMOGENEOUS,
  PROP_PACK_START,
  PROP_USE_ANIMATIONS,
  PROP_EASING_MODE,
  PROP_EASING_DURATION,
  PROP_ORIENTATION,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

GType clutter_box_child_get_type (void);

G_DEFINE_TYPE (ClutterBoxChild,
               clutter_box_child,
               CLUTTER_TYPE_LAYOUT_META)

G_DEFINE_TYPE_WITH_PRIVATE (ClutterBoxLayout,
                            clutter_box_layout,
                            CLUTTER_TYPE_LAYOUT_MANAGER)


typedef struct _RequestedSize
{
  ClutterActor *actor;

  gfloat minimum_size;
  gfloat natural_size;
} RequestedSize;

static gint distribute_natural_allocation (gint                  extra_space,
					   guint                 n_requested_sizes,
					   RequestedSize        *sizes);
static void count_expand_children         (ClutterLayoutManager *layout,
					   ClutterContainer     *container,
					   gint                 *visible_children,
					   gint                 *expand_children);

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
                                P_("Expand"),
                                P_("Allocate extra space for the child"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_EXPAND, pspec);

  pspec = g_param_spec_boolean ("x-fill",
                                P_("Horizontal Fill"),
                                P_("Whether the child should receive priority "
                                   "when the container is allocating spare space "
                                   "on the horizontal axis"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_X_FILL, pspec);

  pspec = g_param_spec_boolean ("y-fill",
                                P_("Vertical Fill"),
                                P_("Whether the child should receive priority "
                                   "when the container is allocating spare space "
                                   "on the vertical axis"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_Y_FILL, pspec);

  pspec = g_param_spec_enum ("x-align",
                             P_("Horizontal Alignment"),
                             P_("Horizontal alignment of the actor within "
                                "the cell"),
                             CLUTTER_TYPE_BOX_ALIGNMENT,
                             CLUTTER_BOX_ALIGNMENT_CENTER,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CHILD_X_ALIGN, pspec);

  pspec = g_param_spec_enum ("y-align",
                             P_("Vertical Alignment"),
                             P_("Vertical alignment of the actor within "
                                "the cell"),
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

static gdouble
get_box_alignment_factor (ClutterBoxAlignment alignment)
{
  switch (alignment)
    {
    case CLUTTER_BOX_ALIGNMENT_CENTER:
      return 0.5;

    case CLUTTER_BOX_ALIGNMENT_START:
      return 0.0;

    case CLUTTER_BOX_ALIGNMENT_END:
      return 1.0;
    }

  return 0.0;
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
  ClutterLayoutManagerClass *parent_class;

  priv->container = container;

  if (priv->container != NULL)
    {
      ClutterRequestMode request_mode;

      /* we need to change the :request-mode of the container
       * to match the orientation
       */
      request_mode = priv->orientation == CLUTTER_ORIENTATION_VERTICAL
                   ? CLUTTER_REQUEST_HEIGHT_FOR_WIDTH
                   : CLUTTER_REQUEST_WIDTH_FOR_HEIGHT;
      clutter_actor_set_request_mode (CLUTTER_ACTOR (priv->container),
                                      request_mode);
    }

  parent_class = CLUTTER_LAYOUT_MANAGER_CLASS (clutter_box_layout_parent_class);
  parent_class->set_container (layout, container);
}

static void
get_child_size (ClutterActor       *actor,
		ClutterOrientation  orientation,
		gfloat              for_size,
		gfloat             *min_size_p,
		gfloat             *natural_size_p)
{
  if (orientation == CLUTTER_ORIENTATION_HORIZONTAL)
    clutter_actor_get_preferred_width (actor, for_size, min_size_p, natural_size_p);
  else
    clutter_actor_get_preferred_height (actor, for_size, min_size_p, natural_size_p);
}

/* Handle the request in the orientation of the box (i.e. width request of horizontal box) */
static void
get_preferred_size_for_orientation (ClutterBoxLayout   *self,
				    ClutterActor       *container,
				    gfloat              for_size,
				    gfloat             *min_size_p,
				    gfloat             *natural_size_p)
{
  ClutterBoxLayoutPrivate *priv = self->priv;
  ClutterActorIter iter;
  ClutterActor *child;
  gint n_children = 0;
  gfloat minimum, natural;

  minimum = natural = 0;

  clutter_actor_iter_init (&iter, container);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat child_min = 0, child_nat = 0;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
	continue;

      n_children++;

      get_child_size (child, priv->orientation,
		      for_size, &child_min, &child_nat);

      minimum += child_min;
      natural += child_nat;
    }

  if (n_children > 1)
    {
      minimum += priv->spacing * (n_children - 1);
      natural += priv->spacing * (n_children - 1);
    }

  if (min_size_p)
    *min_size_p = minimum;

  if (natural_size_p)
    *natural_size_p = natural;
}

static void
get_base_size_for_opposite_orientation (ClutterBoxLayout   *self,
					ClutterActor       *container,
					gfloat             *min_size_p,
					gfloat             *natural_size_p)
{
  ClutterBoxLayoutPrivate *priv = self->priv;
  ClutterActorIter iter;
  ClutterActor *child;
  gint n_children = 0;
  gfloat minimum, natural;
  ClutterOrientation opposite_orientation =
    priv->orientation == CLUTTER_ORIENTATION_HORIZONTAL
    ? CLUTTER_ORIENTATION_VERTICAL
    : CLUTTER_ORIENTATION_HORIZONTAL;

  minimum = natural = 0;

  clutter_actor_iter_init (&iter, container);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat child_min = 0, child_nat = 0;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
	continue;

      n_children++;

      get_child_size (child, opposite_orientation, -1, &child_min, &child_nat);

      minimum = MAX (minimum, child_min);
      natural = MAX (natural, child_nat);
    }

  if (min_size_p)
    *min_size_p = minimum;

  if (natural_size_p)
    *natural_size_p = natural;
}


/* Handle the request in the opposite orientation of the box
 * (i.e. height request of horizontal box)
 *
 * This operation requires a virtual allocation in the natural
 * orientation of the box, after that each element must be asked
 * for the size-for-virtually-allocated-size and the maximums of
 * each child sample will be reported as the overall
 * "size-for-size-in-opposite-orientation"
 */
static void
get_preferred_size_for_opposite_orientation (ClutterBoxLayout   *self,
					     ClutterActor       *container,
					     gfloat              for_size,
					     gfloat             *min_size_p,
					     gfloat             *natural_size_p)
{
  ClutterLayoutManager *layout = CLUTTER_LAYOUT_MANAGER (self);
  ClutterBoxLayoutPrivate *priv = self->priv;
  ClutterContainer *real_container = CLUTTER_CONTAINER (container);
  ClutterActor *child;
  ClutterActorIter iter;
  gint nvis_children = 0, n_extra_widgets = 0;
  gint nexpand_children = 0, i;
  RequestedSize *sizes;
  gfloat minimum, natural, size, extra = 0;
  ClutterOrientation opposite_orientation =
    priv->orientation == CLUTTER_ORIENTATION_HORIZONTAL
    ? CLUTTER_ORIENTATION_VERTICAL
    : CLUTTER_ORIENTATION_HORIZONTAL;

  minimum = natural = 0;

  count_expand_children (layout, real_container,
			 &nvis_children, &nexpand_children);

  if (nvis_children < 1)
    {
      if (min_size_p)
	*min_size_p = 0;

      if (natural_size_p)
	*natural_size_p = 0;

      return;
    }

  /* First collect the requested sizes in the natural orientation of the box */
  sizes  = g_newa (RequestedSize, nvis_children);
  size   = for_size;

  i = 0;
  clutter_actor_iter_init (&iter, container);
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
	continue;

      get_child_size (child, priv->orientation, -1,
		      &sizes[i].minimum_size,
		      &sizes[i].natural_size);

      size -= sizes[i].minimum_size;
      i++;
    }

  if (priv->is_homogeneous)
    {
      size            = for_size - (nvis_children - 1) * priv->spacing;
      extra           = size / nvis_children;
      n_extra_widgets = ((gint)size) % nvis_children;
    }
  else
    {
      /* Bring children up to size first */
      size = distribute_natural_allocation (MAX (0, size), nvis_children, sizes);

      /* Calculate space which hasn't distributed yet,
       * and is available for expanding children.
       */
      if (nexpand_children > 0)
        {
          extra = size / nexpand_children;
          n_extra_widgets = ((gint)size) % nexpand_children;
        }
    }

  /* Distribute expand space to children */
  i = 0;
  clutter_actor_iter_init (&iter, container);
  while (clutter_actor_iter_next (&iter, &child))
    {
      ClutterLayoutMeta *meta;
      ClutterBoxChild   *box_child;

      /* If widget is not visible, skip it. */
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      meta      = clutter_layout_manager_get_child_meta (layout, real_container, child);
      box_child = CLUTTER_BOX_CHILD (meta);

      if (priv->is_homogeneous)
	{
	  sizes[i].minimum_size = extra;

          if (n_extra_widgets > 0)
            {
              sizes[i].minimum_size++;
              n_extra_widgets--;
            }
	}
      else
	{
          if (clutter_actor_needs_expand (child, priv->orientation) || box_child->expand)
            {
              sizes[i].minimum_size += extra;

              if (n_extra_widgets > 0)
                {
                  sizes[i].minimum_size++;
                  n_extra_widgets--;
                }
            }
	}
      i++;
    }

  /* Virtual allocation finished, now we can finally ask for the right size-for-size */
  i = 0;
  clutter_actor_iter_init (&iter, container);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gfloat child_min = 0, child_nat = 0;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      get_child_size (child, opposite_orientation,
		      sizes[i].minimum_size,
		      &child_min, &child_nat);

      minimum = MAX (minimum, child_min);
      natural = MAX (natural, child_nat);

      i++;
    }

  if (min_size_p)
    *min_size_p = minimum;

  if (natural_size_p)
    *natural_size_p = natural;
}


static void
allocate_box_child (ClutterBoxLayout       *self,
                    ClutterContainer       *container,
                    ClutterActor           *child,
                    ClutterActorBox        *child_box,
                    ClutterAllocationFlags  flags)
{
  ClutterBoxLayoutPrivate *priv = self->priv;
  ClutterBoxChild *box_child;
  ClutterLayoutMeta *meta;

  meta = clutter_layout_manager_get_child_meta (CLUTTER_LAYOUT_MANAGER (self),
                                                container,
                                                child);
  box_child = CLUTTER_BOX_CHILD (meta);

  CLUTTER_NOTE (LAYOUT, "Allocation for %s { %.2f, %.2f, %.2f, %.2f }",
                _clutter_actor_get_debug_name (child),
                child_box->x1, child_box->y1,
                child_box->x2 - child_box->x1,
                child_box->y2 - child_box->y1);

  if (priv->use_animations)
    {
      clutter_actor_save_easing_state (child);
      clutter_actor_set_easing_mode (child, priv->easing_mode);
      clutter_actor_set_easing_duration (child, priv->easing_duration);
    }

  /* call allocate() instead of allocate_align_fill() if the actor needs
   * expand in either direction. this will honour the actors alignment settings
   */
  if (clutter_actor_needs_expand (child, CLUTTER_ORIENTATION_HORIZONTAL) ||
      clutter_actor_needs_expand (child, CLUTTER_ORIENTATION_VERTICAL))
    clutter_actor_allocate (child, child_box, flags);
  else
    clutter_actor_allocate_align_fill (child, child_box,
                                       get_box_alignment_factor (box_child->x_align),
                                       get_box_alignment_factor (box_child->y_align),
                                       box_child->x_fill,
                                       box_child->y_fill,
                                       flags);

  if (priv->use_animations)
    clutter_actor_restore_easing_state (child);
}

static void
clutter_box_layout_get_preferred_width (ClutterLayoutManager *layout,
                                        ClutterContainer     *container,
                                        gfloat                for_height,
                                        gfloat               *min_width_p,
                                        gfloat               *natural_width_p)
{
  ClutterBoxLayout        *self = CLUTTER_BOX_LAYOUT (layout);
  ClutterBoxLayoutPrivate *priv = self->priv;

  if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
    {
      if (for_height < 0)
	get_base_size_for_opposite_orientation (self, CLUTTER_ACTOR (container),
						min_width_p, natural_width_p);
      else
	get_preferred_size_for_opposite_orientation (self, CLUTTER_ACTOR (container), for_height,
						     min_width_p, natural_width_p);
    }
  else
    get_preferred_size_for_orientation (self, CLUTTER_ACTOR (container), for_height,
					min_width_p, natural_width_p);
}

static void
clutter_box_layout_get_preferred_height (ClutterLayoutManager *layout,
                                         ClutterContainer     *container,
                                         gfloat                for_width,
                                         gfloat               *min_height_p,
                                         gfloat               *natural_height_p)
{
  ClutterBoxLayout        *self = CLUTTER_BOX_LAYOUT (layout);
  ClutterBoxLayoutPrivate *priv = self->priv;

  if (priv->orientation == CLUTTER_ORIENTATION_HORIZONTAL)
    {
      if (for_width < 0)
	get_base_size_for_opposite_orientation (self, CLUTTER_ACTOR (container),
						min_height_p, natural_height_p);
      else
	get_preferred_size_for_opposite_orientation (self, CLUTTER_ACTOR (container), for_width,
						     min_height_p, natural_height_p);
    }
  else
    get_preferred_size_for_orientation (self, CLUTTER_ACTOR (container), for_width,
					min_height_p, natural_height_p);
}

static void
count_expand_children (ClutterLayoutManager *layout,
                       ClutterContainer     *container,
                       gint                 *visible_children,
                       gint                 *expand_children)
{
  ClutterBoxLayoutPrivate *priv = CLUTTER_BOX_LAYOUT (layout)->priv;
  ClutterActor *actor, *child;
  ClutterActorIter iter;

  actor = CLUTTER_ACTOR (container);

  *visible_children = *expand_children = 0;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        {
          ClutterLayoutMeta *meta;

          *visible_children += 1;

          meta = clutter_layout_manager_get_child_meta (layout,
                                                        container,
                                                        child);

          if (clutter_actor_needs_expand (child, priv->orientation) ||
              CLUTTER_BOX_CHILD (meta)->expand)
            *expand_children += 1;
        }
    }
}

/* Pulled from gtksizerequest.c from Gtk+ */
static gint
compare_gap (gconstpointer p1,
             gconstpointer p2,
             gpointer      data)
{
  RequestedSize *sizes = data;
  const guint *c1 = p1;
  const guint *c2 = p2;

  const gint d1 = MAX (sizes[*c1].natural_size -
                       sizes[*c1].minimum_size,
                       0);
  const gint d2 = MAX (sizes[*c2].natural_size -
                       sizes[*c2].minimum_size,
                       0);

  gint delta = (d2 - d1);

  if (0 == delta)
    delta = (*c2 - *c1);

  return delta;
}

/*
 * distribute_natural_allocation:
 * @extra_space: Extra space to redistribute among children after subtracting
 *   minimum sizes and any child padding from the overall allocation
 * @n_requested_sizes: Number of requests to fit into the allocation
 * @sizes: An array of structs with a client pointer and a minimum/natural size
 *   in the orientation of the allocation.
 *
 * Distributes @extra_space to child @sizes by bringing smaller
 * children up to natural size first.
 *
 * The remaining space will be added to the @minimum_size member of the
 * RequestedSize struct. If all sizes reach their natural size then
 * the remaining space is returned.
 *
 * Returns: The remainder of @extra_space after redistributing space
 * to @sizes.
 *
 * Pulled from gtksizerequest.c from Gtk+
 */
static gint
distribute_natural_allocation (gint           extra_space,
                               guint          n_requested_sizes,
                               RequestedSize *sizes)
{
  guint *spreading;
  gint   i;

  g_return_val_if_fail (extra_space >= 0, 0);

  spreading = g_newa (guint, n_requested_sizes);

  for (i = 0; i < n_requested_sizes; i++)
    spreading[i] = i;

  /* Distribute the container's extra space c_gap. We want to assign
   * this space such that the sum of extra space assigned to children
   * (c^i_gap) is equal to c_cap. The case that there's not enough
   * space for all children to take their natural size needs some
   * attention. The goals we want to achieve are:
   *
   *   a) Maximize number of children taking their natural size.
   *   b) The allocated size of children should be a continuous
   *   function of c_gap.  That is, increasing the container size by
   *   one pixel should never make drastic changes in the distribution.
   *   c) If child i takes its natural size and child j doesn't,
   *   child j should have received at least as much gap as child i.
   *
   * The following code distributes the additional space by following
   * these rules.
   */

  /* Sort descending by gap and position. */
  g_qsort_with_data (spreading,
                     n_requested_sizes, sizeof (guint),
                     compare_gap, sizes);

  /* Distribute available space.
   * This master piece of a loop was conceived by Behdad Esfahbod.
   */
  for (i = n_requested_sizes - 1; extra_space > 0 && i >= 0; --i)
    {
      /* Divide remaining space by number of remaining children.
       * Sort order and reducing remaining space by assigned space
       * ensures that space is distributed equally.
       */
      gint glue = (extra_space + i) / (i + 1);
      gint gap = sizes[(spreading[i])].natural_size
               - sizes[(spreading[i])].minimum_size;

      gint extra = MIN (glue, gap);

      sizes[spreading[i]].minimum_size += extra;

      extra_space -= extra;
    }

  return extra_space;
}

/* Pulled from gtkbox.c from Gtk+ */

static void
clutter_box_layout_allocate (ClutterLayoutManager   *layout,
                             ClutterContainer       *container,
                             const ClutterActorBox  *box,
                             ClutterAllocationFlags  flags)
{
  ClutterBoxLayoutPrivate *priv = CLUTTER_BOX_LAYOUT (layout)->priv;
  ClutterActor *actor, *child;
  gint nvis_children;
  gint nexpand_children;
  gboolean is_rtl;
  ClutterActorIter iter;

  ClutterActorBox child_allocation;
  RequestedSize *sizes;

  gint size;
  gint extra;
  gint n_extra_widgets = 0; /* Number of widgets that receive 1 extra px */
  gint x = 0, y = 0, i;
  gint child_size;

  count_expand_children (layout, container, &nvis_children, &nexpand_children);

  CLUTTER_NOTE (LAYOUT, "BoxLayout for %s: visible=%d, expand=%d",
                _clutter_actor_get_debug_name (CLUTTER_ACTOR (container)),
                nvis_children,
                nexpand_children);

  /* If there is no visible child, simply return. */
  if (nvis_children <= 0)
    return;

  sizes = g_newa (RequestedSize, nvis_children);

  if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
    size = box->y2 - box->y1 - (nvis_children - 1) * priv->spacing;
  else
    size = box->x2 - box->x1 - (nvis_children - 1) * priv->spacing;

  actor = CLUTTER_ACTOR (container);

  /* Retrieve desired size for visible children. */
  i = 0;
  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
        clutter_actor_get_preferred_height (child,
                                            box->x2 - box->x1,
                                            &sizes[i].minimum_size,
                                            &sizes[i].natural_size);
      else
        clutter_actor_get_preferred_width (child,
                                           box->y2 - box->y1,
                                           &sizes[i].minimum_size,
                                           &sizes[i].natural_size);


      /* Assert the api is working properly */
      if (sizes[i].minimum_size < 0)
        g_error ("ClutterBoxLayout child %s minimum %s: %f < 0 for %s %f",
                 _clutter_actor_get_debug_name (child),
                 priv->orientation == CLUTTER_ORIENTATION_VERTICAL
                   ? "height"
                   : "width",
                 sizes[i].minimum_size,
                 priv->orientation == CLUTTER_ORIENTATION_VERTICAL
                   ? "width"
                   : "height",
                 priv->orientation == CLUTTER_ORIENTATION_VERTICAL
                   ? box->x2 - box->x1
                   : box->y2 - box->y1);

      if (sizes[i].natural_size < sizes[i].minimum_size)
        g_error ("ClutterBoxLayout child %s natural %s: %f < minimum %f for %s %f",
                 _clutter_actor_get_debug_name (child),
                 priv->orientation == CLUTTER_ORIENTATION_VERTICAL
                   ? "height"
                   : "width",
                 sizes[i].natural_size,
                 sizes[i].minimum_size,
                 priv->orientation == CLUTTER_ORIENTATION_VERTICAL
                   ? "width"
                   : "height",
                 priv->orientation == CLUTTER_ORIENTATION_VERTICAL
                   ? box->x2 - box->x1
                   : box->y2 - box->y1);

      size -= sizes[i].minimum_size;

      sizes[i].actor = child;

      i += 1;
    }

  if (priv->is_homogeneous)
    {
      /* If were homogenous we still need to run the above loop to get the
       * minimum sizes for children that are not going to fill
       */
      if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
        size = box->y2 - box->y1 - (nvis_children - 1) * priv->spacing;
      else
        size = box->x2 - box->x1 - (nvis_children - 1) * priv->spacing;

      extra = size / nvis_children;
      n_extra_widgets = size % nvis_children;
    }
  else
    {
      /* Bring children up to size first */
      size = distribute_natural_allocation (MAX (0, size), nvis_children, sizes);

      /* Calculate space which hasn't distributed yet,
       * and is available for expanding children.
       */
      if (nexpand_children > 0)
        {
          extra = size / nexpand_children;
          n_extra_widgets = size % nexpand_children;
        }
      else
        extra = 0;
    }

  if (priv->orientation == CLUTTER_ORIENTATION_HORIZONTAL)
    {
      ClutterTextDirection text_dir;

      text_dir = clutter_actor_get_text_direction (CLUTTER_ACTOR (container));
      is_rtl = (text_dir == CLUTTER_TEXT_DIRECTION_RTL) ? TRUE : FALSE;
    }
  else
    is_rtl = FALSE;

  /* Allocate child positions. */
  if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
    {
      child_allocation.x1 = box->x1;
      child_allocation.x2 = MAX (1.0, box->x2);
      if (priv->is_pack_start)
        y = box->y2 - box->y1;
      else
        y = box->y1;
    }
  else
    {
      child_allocation.y1 = box->y1;
      child_allocation.y2 = MAX (1.0, box->y2);
      if (priv->is_pack_start)
        x = box->x2 - box->x1;
      else
        x = box->x1;
    }

  i = 0;
  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      ClutterLayoutMeta *meta;
      ClutterBoxChild *box_child;

      /* If widget is not visible, skip it. */
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      meta = clutter_layout_manager_get_child_meta (layout,
                                                    container,
                                                    child);
      box_child = CLUTTER_BOX_CHILD (meta);

      /* Assign the child's size. */
      if (priv->is_homogeneous)
        {
          child_size = extra;

          if (n_extra_widgets > 0)
            {
              child_size++;
              n_extra_widgets--;
            }
        }
      else
        {
          child_size = sizes[i].minimum_size;

          if (clutter_actor_needs_expand (child, priv->orientation) ||
              box_child->expand)
            {
              child_size += extra;

              if (n_extra_widgets > 0)
                {
                  child_size++;
                  n_extra_widgets--;
                }
            }
        }

      /* Assign the child's position. */
      if (priv->orientation == CLUTTER_ORIENTATION_VERTICAL)
        {
          if (clutter_actor_needs_expand (child, priv->orientation) ||
              box_child->expand)
            {
              child_allocation.y1 = y;
              child_allocation.y2 = child_allocation.y1 + MAX (1.0, child_size);
            }
          else
            {
              child_allocation.y1 = y + (child_size - sizes[i].minimum_size) / 2;
              child_allocation.y2 = child_allocation.y1 + sizes[i].minimum_size;
            }

          if (priv->is_pack_start)
            {
              y -= child_size + priv->spacing;

              child_allocation.y1 -= child_size;
              child_allocation.y2 -= child_size;
            }
          else
            {
              y += child_size + priv->spacing;
            }
        }
      else /* CLUTTER_ORIENTATION_HORIZONTAL */
        {
          if (clutter_actor_needs_expand (child, priv->orientation) ||
              box_child->expand)
            {
              child_allocation.x1 = x;
              child_allocation.x2 = child_allocation.x1 + MAX (1.0, child_size);
            }
          else
            {
              child_allocation.x1 = x + (child_size - sizes[i].minimum_size) / 2;
              child_allocation.x2 = child_allocation.x1 + sizes[i].minimum_size;
            }

          if (priv->is_pack_start)
            {
              x -= child_size + priv->spacing;

              child_allocation.x1 -= child_size;
              child_allocation.x2 -= child_size;
            }
          else
            {
              x += child_size + priv->spacing;
            }

          if (is_rtl)
            {
              gfloat width = child_allocation.x2 - child_allocation.x1;

              child_allocation.x1 = box->x2 - box->x1
                                  - child_allocation.x1
                                  - (child_allocation.x2 - child_allocation.x1);
              child_allocation.x2 = child_allocation.x1 + width;
            }

        }

        allocate_box_child (CLUTTER_BOX_LAYOUT (layout),
                            container,
                            child,
                            &child_allocation,
                            flags);

        i += 1;
    }
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

    case PROP_ORIENTATION:
      clutter_box_layout_set_orientation (self, g_value_get_enum (value));
      break;

    case PROP_HOMOGENEOUS:
      clutter_box_layout_set_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_SPACING:
      clutter_box_layout_set_spacing (self, g_value_get_uint (value));
      break;

    case PROP_PACK_START:
      clutter_box_layout_set_pack_start (self, g_value_get_boolean (value));
      break;

    case PROP_USE_ANIMATIONS:
      clutter_box_layout_set_use_animations (self, g_value_get_boolean (value));
      break;

    case PROP_EASING_MODE:
      clutter_box_layout_set_easing_mode (self, g_value_get_ulong (value));
      break;

    case PROP_EASING_DURATION:
      clutter_box_layout_set_easing_duration (self, g_value_get_uint (value));
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
      g_value_set_boolean (value,
                           priv->orientation == CLUTTER_ORIENTATION_VERTICAL);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, priv->is_homogeneous);
      break;

    case PROP_SPACING:
      g_value_set_uint (value, priv->spacing);
      break;

    case PROP_PACK_START:
      g_value_set_boolean (value, priv->is_pack_start);
      break;

    case PROP_USE_ANIMATIONS:
      g_value_set_boolean (value, priv->use_animations);
      break;

    case PROP_EASING_MODE:
      g_value_set_ulong (value, priv->easing_mode);
      break;

    case PROP_EASING_DURATION:
      g_value_set_uint (value, priv->easing_duration);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_box_layout_class_init (ClutterBoxLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterLayoutManagerClass *layout_class;

  layout_class = CLUTTER_LAYOUT_MANAGER_CLASS (klass);

  layout_class->get_preferred_width = clutter_box_layout_get_preferred_width;
  layout_class->get_preferred_height = clutter_box_layout_get_preferred_height;
  layout_class->allocate = clutter_box_layout_allocate;
  layout_class->set_container = clutter_box_layout_set_container;
  layout_class->get_child_meta_type = clutter_box_layout_get_child_meta_type;

  /**
   * ClutterBoxLayout:vertical:
   *
   * Whether the #ClutterBoxLayout should arrange its children
   * alongside the Y axis, instead of alongside the X axis
   *
   * Since: 1.2
   *
   * Deprecated: 1.12: Use #ClutterBoxLayout:orientation instead.
   */
  obj_props[PROP_VERTICAL] =
    g_param_spec_boolean ("vertical",
                          P_("Vertical"),
                          P_("Whether the layout should be vertical, "
                             "rather than horizontal"),
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_DEPRECATED);

  /**
   * ClutterBoxLayout:orientation:
   *
   * The orientation of the #ClutterBoxLayout, either horizontal
   * or vertical
   *
   * Since: 1.12
   */
  obj_props[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation",
                       P_("Orientation"),
                       P_("The orientation of the layout"),
                       CLUTTER_TYPE_ORIENTATION,
                       CLUTTER_ORIENTATION_HORIZONTAL,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  /**
   * ClutterBoxLayout:homogeneous:
   *
   * Whether the #ClutterBoxLayout should arrange its children
   * homogeneously, i.e. all childs get the same size
   *
   * Since: 1.4
   */
  obj_props[PROP_HOMOGENEOUS] =
    g_param_spec_boolean ("homogeneous",
                          P_("Homogeneous"),
                          P_("Whether the layout should be homogeneous, "
                             "i.e. all childs get the same size"),
                          FALSE,
                          CLUTTER_PARAM_READWRITE);

  /**
   * ClutterBoxLayout:pack-start:
   *
   * Whether the #ClutterBoxLayout should pack items at the start
   * or append them at the end
   *
   * Since: 1.2
   */
  obj_props[PROP_PACK_START] =
    g_param_spec_boolean ("pack-start",
                          P_("Pack Start"),
                          P_("Whether to pack items at the start of the box"),
                          FALSE,
                          CLUTTER_PARAM_READWRITE);

  /**
   * ClutterBoxLayout:spacing:
   *
   * The spacing between children of the #ClutterBoxLayout, in pixels
   *
   * Since: 1.2
   */
  obj_props[PROP_SPACING] =
    g_param_spec_uint ("spacing",
                       P_("Spacing"),
                       P_("Spacing between children"),
                       0, G_MAXUINT, 0,
                       CLUTTER_PARAM_READWRITE);

  /**
   * ClutterBoxLayout:use-animations:
   *
   * Whether the #ClutterBoxLayout should animate changes in the
   * layout, overriding the easing state of the children.
   *
   * Since: 1.2
   *
   * Deprecated: 1.12: #ClutterBoxLayout will honour the easing state
   *   of the children when allocating them.
   */
  obj_props[PROP_USE_ANIMATIONS] =
    g_param_spec_boolean ("use-animations",
                          P_("Use Animations"),
                          P_("Whether layout changes should be animated"),
                          FALSE,
                          CLUTTER_PARAM_READWRITE);

  /**
   * ClutterBoxLayout:easing-mode:
   *
   * The easing mode for the animations, in case
   * #ClutterBoxLayout:use-animations is set to %TRUE.
   *
   * The easing mode has the same semantics of #ClutterAnimation:mode: it can
   * either be a value from the #ClutterAnimationMode enumeration, like
   * %CLUTTER_EASE_OUT_CUBIC, or a logical id as returned by
   * clutter_alpha_register_func().
   *
   * The default value is %CLUTTER_EASE_OUT_CUBIC.
   *
   * Since: 1.2
   *
   * Deprecated: 1.12: The #ClutterBoxLayout will honour the easing state of
   *   the children when allocating them.
   */
  obj_props[PROP_EASING_MODE] =
    g_param_spec_ulong ("easing-mode",
                        P_("Easing Mode"),
                        P_("The easing mode of the animations"),
                        0, G_MAXULONG,
                        CLUTTER_EASE_OUT_CUBIC,
                        CLUTTER_PARAM_READWRITE);

  /**
   * ClutterBoxLayout:easing-duration:
   *
   * The duration of the animations, in case #ClutterBoxLayout:use-animations
   * is set to %TRUE.
   *
   * The duration is expressed in milliseconds.
   *
   * Since: 1.2
   *
   * Deprecated: 1.12: The #ClutterBoxLayout will honour the easing state of
   *   the children when allocating them.
   */
  obj_props[PROP_EASING_DURATION] =
    g_param_spec_uint ("easing-duration",
                       P_("Easing Duration"),
                       P_("The duration of the animations"),
                       0, G_MAXUINT,
                       500,
                       CLUTTER_PARAM_READWRITE);

  gobject_class->set_property = clutter_box_layout_set_property;
  gobject_class->get_property = clutter_box_layout_get_property;
  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_box_layout_init (ClutterBoxLayout *self)
{
  self->priv = clutter_box_layout_get_instance_private (self);

  self->priv->orientation = CLUTTER_ORIENTATION_HORIZONTAL;
  self->priv->is_homogeneous = FALSE;
  self->priv->is_pack_start = FALSE;
  self->priv->spacing = 0;

  self->priv->use_animations = FALSE;
  self->priv->easing_mode = CLUTTER_EASE_OUT_CUBIC;
  self->priv->easing_duration = 500;
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

      clutter_layout_manager_layout_changed (manager);

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
 *
 * Deprecated: 1.12: Use clutter_box_layout_set_orientation() instead.
 */
void
clutter_box_layout_set_vertical (ClutterBoxLayout *layout,
                                 gboolean          vertical)
{
  ClutterOrientation new_orientation, old_orientation;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));

  old_orientation = layout->priv->orientation;
  new_orientation = vertical
                  ? CLUTTER_ORIENTATION_VERTICAL
                  : CLUTTER_ORIENTATION_HORIZONTAL;
  clutter_box_layout_set_orientation (layout, new_orientation);

  if (old_orientation != new_orientation)
    g_object_notify_by_pspec (G_OBJECT (layout), obj_props[PROP_VERTICAL]);
}

/**
 * clutter_box_layout_set_orientation:
 * @layout: a #ClutterBoxLayout
 * @orientation: the orientation of the #ClutterBoxLayout
 *
 * Sets the orientation of the #ClutterBoxLayout layout manager.
 *
 * Since: 1.12
 */
void
clutter_box_layout_set_orientation (ClutterBoxLayout   *layout,
                                    ClutterOrientation  orientation)
{
  ClutterBoxLayoutPrivate *priv;
  ClutterLayoutManager *manager;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));

  priv = layout->priv;

  if (priv->orientation == orientation)
    return;

  priv->orientation = orientation;

  manager = CLUTTER_LAYOUT_MANAGER (layout);

  clutter_layout_manager_layout_changed (manager);

  g_object_notify_by_pspec (G_OBJECT (layout), obj_props[PROP_ORIENTATION]);
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
 *
 * Deprecated: 1.12: Use clutter_box_layout_get_orientation() instead
 */
gboolean
clutter_box_layout_get_vertical (ClutterBoxLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout), FALSE);

  return layout->priv->orientation == CLUTTER_ORIENTATION_VERTICAL;
}

/**
 * clutter_box_layout_get_orientation:
 * @layout: a #ClutterBoxLayout
 *
 * Retrieves the orientation of the @layout.
 *
 * Return value: the orientation of the layout
 *
 * Since: 1.12
 */
ClutterOrientation
clutter_box_layout_get_orientation (ClutterBoxLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout),
                        CLUTTER_ORIENTATION_HORIZONTAL);

  return layout->priv->orientation;
}

/**
 * clutter_box_layout_set_homogeneous:
 * @layout: a #ClutterBoxLayout
 * @homogeneous: %TRUE if the layout should be homogeneous
 *
 * Sets whether the size of @layout children should be
 * homogeneous
 *
 * Since: 1.4
 */
void
clutter_box_layout_set_homogeneous (ClutterBoxLayout *layout,
				    gboolean          homogeneous)
{
  ClutterBoxLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));

  priv = layout->priv;

  if (priv->is_homogeneous != homogeneous)
    {
      ClutterLayoutManager *manager;

      priv->is_homogeneous = !!homogeneous;

      manager = CLUTTER_LAYOUT_MANAGER (layout);

      clutter_layout_manager_layout_changed (manager);

      g_object_notify (G_OBJECT (layout), "homogeneous");
    }
}

/**
 * clutter_box_layout_get_homogeneous:
 * @layout: a #ClutterBoxLayout
 *
 * Retrieves if the children sizes are allocated homogeneously.
 *
 * Return value: %TRUE if the #ClutterBoxLayout is arranging its children
 *   homogeneously, and %FALSE otherwise
 *
 * Since: 1.4
 */
gboolean
clutter_box_layout_get_homogeneous (ClutterBoxLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout), FALSE);

  return layout->priv->is_homogeneous;
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

      clutter_layout_manager_layout_changed (manager);

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
 * Deprecated: 1.12: #ClutterBoxLayout honours #ClutterActor's
 *   align and expand properties. The preferred way is adding
 *   the @actor with clutter_actor_add_child() and setting
 *   #ClutterActor:x-align, #ClutterActor:y-align,
 *   #ClutterActor:x-expand and #ClutterActor:y-expand
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
 * Deprecated: 1.12: #ClutterBoxLayout will honour #ClutterActor's
 *   #ClutterActor:x-align and #ClutterActor:y-align properies
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
 * Deprecated: 1.12: #ClutterBoxLayout will honour #ClutterActor's
 *   #ClutterActor:x-align and #ClutterActor:y-align properies
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
 * Deprecated: 1.12: #ClutterBoxLayout will honour #ClutterActor's
 *   #ClutterActor:x-align and #ClutterActor:y-align properies
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
 * Deprecated: 1.12: #ClutterBoxLayout will honour #ClutterActor's
 *   #ClutterActor:x-align and #ClutterActor:y-align properies
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
 * Deprecated: 1.12: #ClutterBoxLayout will honour #ClutterActor's
 *   #ClutterActor:x-expand and #ClutterActor:y-expand properies
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
 * Deprecated: 1.12: #ClutterBoxLayout will honour #ClutterActor's
 *   #ClutterActor:x-expand and #ClutterActor:y-expand properies
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

/**
 * clutter_box_layout_set_use_animations:
 * @layout: a #ClutterBoxLayout
 * @animate: %TRUE if the @layout should use animations
 *
 * Sets whether @layout should animate changes in the layout properties
 *
 * The duration of the animations is controlled by
 * clutter_box_layout_set_easing_duration(); the easing mode to be used
 * by the animations is controlled by clutter_box_layout_set_easing_mode().
 *
 * Enabling animations will override the easing state of each child
 * of the actor using @layout, and will use the #ClutterBoxLayout:easing-mode
 * and #ClutterBoxLayout:easing-duration properties instead.
 *
 * Since: 1.2
 *
 * Deprecated: 1.12: The layout manager will honour the easing state
 *   of the children when allocating them.
 */
void
clutter_box_layout_set_use_animations (ClutterBoxLayout *layout,
                                       gboolean          animate)
{
  ClutterBoxLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));

  priv = layout->priv;

  if (priv->use_animations != animate)
    {
      priv->use_animations = animate;

      g_object_notify (G_OBJECT (layout), "use-animations");
    }
}

/**
 * clutter_box_layout_get_use_animations:
 * @layout: a #ClutterBoxLayout
 *
 * Retrieves whether @layout should animate changes in the layout properties.
 *
 * Return value: %TRUE if the animations should be used, %FALSE otherwise
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
gboolean
clutter_box_layout_get_use_animations (ClutterBoxLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout), FALSE);

  return layout->priv->use_animations;
}

/**
 * clutter_box_layout_set_easing_mode:
 * @layout: a #ClutterBoxLayout
 * @mode: an easing mode, either from #ClutterAnimationMode or a logical id
 *   from clutter_alpha_register_func()
 *
 * Sets the easing mode to be used by @layout when animating changes in layout
 * properties.
 *
 * Since: 1.2
 *
 * Deprecated: 1.12: The layout manager will honour the easing state
 *   of the children when allocating them.
 */
void
clutter_box_layout_set_easing_mode (ClutterBoxLayout *layout,
                                    gulong            mode)
{
  ClutterBoxLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));

  priv = layout->priv;

  if (priv->easing_mode != mode)
    {
      priv->easing_mode = mode;

      g_object_notify (G_OBJECT (layout), "easing-mode");
    }
}

/**
 * clutter_box_layout_get_easing_mode:
 * @layout: a #ClutterBoxLayout
 *
 * Retrieves the easing mode set using clutter_box_layout_set_easing_mode()
 *
 * Return value: an easing mode
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
gulong
clutter_box_layout_get_easing_mode (ClutterBoxLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout),
                        CLUTTER_EASE_OUT_CUBIC);

  return layout->priv->easing_mode;
}

/**
 * clutter_box_layout_set_easing_duration:
 * @layout: a #ClutterBoxLayout
 * @msecs: the duration of the animations, in milliseconds
 *
 * Sets the duration of the animations used by @layout when animating changes
 * in the layout properties.
 *
 * Since: 1.2
 *
 * Deprecated: 1.12: The layout manager will honour the easing state
 *   of the children when allocating them.
 */
void
clutter_box_layout_set_easing_duration (ClutterBoxLayout *layout,
                                        guint             msecs)
{
  ClutterBoxLayoutPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BOX_LAYOUT (layout));

  priv = layout->priv;

  if (priv->easing_duration != msecs)
    {
      priv->easing_duration = msecs;

      g_object_notify (G_OBJECT (layout), "easing-duration");
    }
}

/**
 * clutter_box_layout_get_easing_duration:
 * @layout: a #ClutterBoxLayout
 *
 * Retrieves the duration set using clutter_box_layout_set_easing_duration()
 *
 * Return value: the duration of the animations, in milliseconds
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
 */
guint
clutter_box_layout_get_easing_duration (ClutterBoxLayout *layout)
{
  g_return_val_if_fail (CLUTTER_IS_BOX_LAYOUT (layout), 500);

  return layout->priv->easing_duration;
}
