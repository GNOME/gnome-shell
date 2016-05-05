/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 * SECTION:clutter-align-constraint
 * @Title: ClutterAlignConstraint
 * @Short_Description: A constraint aligning the position of an actor
 *
 * #ClutterAlignConstraint is a #ClutterConstraint that aligns the position
 * of the #ClutterActor to which it is applied to the size of another
 * #ClutterActor using an alignment factor
 *
 * #ClutterAlignConstraint is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-align-constraint.h"

#include "clutter-actor-meta-private.h"
#include "clutter-actor-private.h"
#include "clutter-constraint.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"

#include <math.h>

#define CLUTTER_ALIGN_CONSTRAINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ALIGN_CONSTRAINT, ClutterAlignConstraintClass))
#define CLUTTER_IS_ALIGN_CONSTRAINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ALIGN_CONSTRAINT))
#define CLUTTER_ALIGN_CONSTRAINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ALIGN_CONSTRAINT, ClutterAlignConstraintClass))

struct _ClutterAlignConstraint
{
  ClutterConstraint parent_instance;

  ClutterActor *actor;
  ClutterActor *source;
  ClutterAlignAxis align_axis;
  gfloat factor;
};

struct _ClutterAlignConstraintClass
{
  ClutterConstraintClass parent_class;
};

enum
{
  PROP_0,

  PROP_SOURCE,
  PROP_ALIGN_AXIS,
  PROP_FACTOR,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (ClutterAlignConstraint,
               clutter_align_constraint,
               CLUTTER_TYPE_CONSTRAINT);

static void
source_position_changed (ClutterActor           *actor,
                         const ClutterActorBox  *allocation,
                         ClutterAllocationFlags  flags,
                         ClutterAlignConstraint *align)
{
  if (align->actor != NULL)
    clutter_actor_queue_relayout (align->actor);
}

static void
source_destroyed (ClutterActor           *actor,
                  ClutterAlignConstraint *align)
{
  align->source = NULL;
}

static void
clutter_align_constraint_set_actor (ClutterActorMeta *meta,
                                    ClutterActor     *new_actor)
{
  ClutterAlignConstraint *align = CLUTTER_ALIGN_CONSTRAINT (meta);
  ClutterActorMetaClass *parent;

  if (new_actor != NULL &&
      align->source != NULL &&
      clutter_actor_contains (new_actor, align->source))
    {
      g_warning (G_STRLOC ": The source actor '%s' is contained "
                 "by the actor '%s' associated to the constraint "
                 "'%s'",
                 _clutter_actor_get_debug_name (align->source),
                 _clutter_actor_get_debug_name (new_actor),
                 _clutter_actor_meta_get_debug_name (meta));
      return;
    }

  /* store the pointer to the actor, for later use */
  align->actor = new_actor;

  parent = CLUTTER_ACTOR_META_CLASS (clutter_align_constraint_parent_class);
  parent->set_actor (meta, new_actor);
}

static void
clutter_align_constraint_update_allocation (ClutterConstraint *constraint,
                                            ClutterActor      *actor,
                                            ClutterActorBox   *allocation)
{
  ClutterAlignConstraint *align = CLUTTER_ALIGN_CONSTRAINT (constraint);
  gfloat source_width, source_height;
  gfloat actor_width, actor_height;
  gfloat source_x, source_y;

  if (align->source == NULL)
    return;

  clutter_actor_box_get_size (allocation, &actor_width, &actor_height);

  clutter_actor_get_position (align->source, &source_x, &source_y);
  clutter_actor_get_size (align->source, &source_width, &source_height);

  switch (align->align_axis)
    {
    case CLUTTER_ALIGN_X_AXIS:
      allocation->x1 = ((source_width - actor_width) * align->factor)
                     + source_x;
      allocation->x2 = allocation->x1 + actor_width;
      break;

    case CLUTTER_ALIGN_Y_AXIS:
      allocation->y1 = ((source_height - actor_height) * align->factor)
                     + source_y;
      allocation->y2 = allocation->y1 + actor_height;
      break;

    case CLUTTER_ALIGN_BOTH:
      allocation->x1 = ((source_width - actor_width) * align->factor)
                     + source_x;
      allocation->y1 = ((source_height - actor_height) * align->factor)
                     + source_y;
      allocation->x2 = allocation->x1 + actor_width;
      allocation->y2 = allocation->y1 + actor_height;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  clutter_actor_box_clamp_to_pixel (allocation);
}

static void
clutter_align_constraint_dispose (GObject *gobject)
{
  ClutterAlignConstraint *align = CLUTTER_ALIGN_CONSTRAINT (gobject);

  if (align->source != NULL)
    {
      g_signal_handlers_disconnect_by_func (align->source,
                                            G_CALLBACK (source_destroyed),
                                            align);
      g_signal_handlers_disconnect_by_func (align->source,
                                            G_CALLBACK (source_position_changed),
                                            align);
      align->source = NULL;
    }

  G_OBJECT_CLASS (clutter_align_constraint_parent_class)->dispose (gobject);
}

static void
clutter_align_constraint_set_property (GObject      *gobject,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ClutterAlignConstraint *align = CLUTTER_ALIGN_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      clutter_align_constraint_set_source (align, g_value_get_object (value));
      break;

    case PROP_ALIGN_AXIS:
      clutter_align_constraint_set_align_axis (align, g_value_get_enum (value));
      break;

    case PROP_FACTOR:
      clutter_align_constraint_set_factor (align, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_align_constraint_get_property (GObject    *gobject,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ClutterAlignConstraint *align = CLUTTER_ALIGN_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, align->source);
      break;

    case PROP_ALIGN_AXIS:
      g_value_set_enum (value, align->align_axis);
      break;

    case PROP_FACTOR:
      g_value_set_float (value, align->factor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_align_constraint_class_init (ClutterAlignConstraintClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  ClutterConstraintClass *constraint_class = CLUTTER_CONSTRAINT_CLASS (klass);

  meta_class->set_actor = clutter_align_constraint_set_actor;

  constraint_class->update_allocation = clutter_align_constraint_update_allocation;

  /**
   * ClutterAlignConstraint:source:
   *
   * The #ClutterActor used as the source for the alignment.
   *
   * The #ClutterActor must not be a child or a grandchild of the actor
   * using the constraint.
   *
   * Since: 1.4
   */
  obj_props[PROP_SOURCE] =
    g_param_spec_object ("source",
                           P_("Source"),
                           P_("The source of the alignment"),
                           CLUTTER_TYPE_ACTOR,
                           CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /**
   * ClutterAlignConstraint:align-axis:
   *
   * The axis to be used to compute the alignment
   *
   * Since: 1.4
   */
  obj_props[PROP_ALIGN_AXIS] =
    g_param_spec_enum ("align-axis",
                       P_("Align Axis"),
                       P_("The axis to align the position to"),
                       CLUTTER_TYPE_ALIGN_AXIS,
                       CLUTTER_ALIGN_X_AXIS,
                       CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /**
   * ClutterAlignConstraint:factor:
   *
   * The alignment factor, as a normalized value between 0.0 and 1.0
   *
   * The factor depends on the #ClutterAlignConstraint:align-axis property:
   * with an align-axis value of %CLUTTER_ALIGN_X_AXIS, 0.0 means left and
   * 1.0 means right; with a value of %CLUTTER_ALIGN_Y_AXIS, 0.0 means top
   * and 1.0 means bottom.
   *
   * Since: 1.4
   */
  obj_props[PROP_FACTOR] =
    g_param_spec_float ("factor",
                        P_("Factor"),
                        P_("The alignment factor, between 0.0 and 1.0"),
                        0.0, 1.0,
                        0.0,
                        CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  gobject_class->dispose = clutter_align_constraint_dispose;
  gobject_class->set_property = clutter_align_constraint_set_property;
  gobject_class->get_property = clutter_align_constraint_get_property;
  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_align_constraint_init (ClutterAlignConstraint *self)
{
  self->actor = NULL;
  self->source = NULL;
  self->align_axis = CLUTTER_ALIGN_X_AXIS;
  self->factor = 0.0f;
}

/**
 * clutter_align_constraint_new:
 * @source: (allow-none): the #ClutterActor to use as the source of the
 *   alignment, or %NULL
 * @axis: the axis to be used to compute the alignment
 * @factor: the alignment factor, between 0.0 and 1.0
 *
 * Creates a new constraint, aligning a #ClutterActor's position with
 * regards of the size of the actor to @source, with the given
 * alignment @factor
 *
 * Return value: the newly created #ClutterAlignConstraint
 *
 * Since: 1.4
 */
ClutterConstraint *
clutter_align_constraint_new (ClutterActor     *source,
                              ClutterAlignAxis  axis,
                              gfloat            factor)
{
  g_return_val_if_fail (source == NULL || CLUTTER_IS_ACTOR (source), NULL);

  return g_object_new (CLUTTER_TYPE_ALIGN_CONSTRAINT,
                       "source", source,
                       "align-axis", axis,
                       "factor", factor,
                       NULL);
}

/**
 * clutter_align_constraint_set_source:
 * @align: a #ClutterAlignConstraint
 * @source: (allow-none): a #ClutterActor, or %NULL to unset the source
 *
 * Sets the source of the alignment constraint
 *
 * Since: 1.4
 */
void
clutter_align_constraint_set_source (ClutterAlignConstraint *align,
                                     ClutterActor           *source)
{
  ClutterActor *old_source, *actor;
  ClutterActorMeta *meta;

  g_return_if_fail (CLUTTER_IS_ALIGN_CONSTRAINT (align));
  g_return_if_fail (source == NULL || CLUTTER_IS_ACTOR (source));

  if (align->source == source)
    return;

  meta = CLUTTER_ACTOR_META (align);
  actor = clutter_actor_meta_get_actor (meta);
  if (actor != NULL && source != NULL)
    {
      if (clutter_actor_contains (actor, source))
        {
          g_warning (G_STRLOC ": The source actor '%s' is contained "
                     "by the actor '%s' associated to the constraint "
                     "'%s'",
                     _clutter_actor_get_debug_name (source),
                     _clutter_actor_get_debug_name (actor),
                     _clutter_actor_meta_get_debug_name (meta));
          return;
        }
    }

  old_source = align->source;
  if (old_source != NULL)
    {
      g_signal_handlers_disconnect_by_func (old_source,
                                            G_CALLBACK (source_destroyed),
                                            align);
      g_signal_handlers_disconnect_by_func (old_source,
                                            G_CALLBACK (source_position_changed),
                                            align);
    }

  align->source = source;
  if (align->source != NULL)
    {
      g_signal_connect (align->source, "allocation-changed",
                        G_CALLBACK (source_position_changed),
                        align);
      g_signal_connect (align->source, "destroy",
                        G_CALLBACK (source_destroyed),
                        align);

      if (align->actor != NULL)
        clutter_actor_queue_relayout (align->actor);
    }

  g_object_notify_by_pspec (G_OBJECT (align), obj_props[PROP_SOURCE]);
}

/**
 * clutter_align_constraint_get_source:
 * @align: a #ClutterAlignConstraint
 *
 * Retrieves the source of the alignment
 *
 * Return value: (transfer none): the #ClutterActor used as the source
 *   of the alignment
 *
 * Since: 1.4
 */
ClutterActor *
clutter_align_constraint_get_source (ClutterAlignConstraint *align)
{
  g_return_val_if_fail (CLUTTER_IS_ALIGN_CONSTRAINT (align), NULL);

  return align->source;
}

/**
 * clutter_align_constraint_set_align_axis:
 * @align: a #ClutterAlignConstraint
 * @axis: the axis to which the alignment refers to
 *
 * Sets the axis to which the alignment refers to
 *
 * Since: 1.4
 */
void
clutter_align_constraint_set_align_axis (ClutterAlignConstraint *align,
                                         ClutterAlignAxis        axis)
{
  g_return_if_fail (CLUTTER_IS_ALIGN_CONSTRAINT (align));

  if (align->align_axis == axis)
    return;

  align->align_axis = axis;

  if (align->actor != NULL)
    clutter_actor_queue_relayout (align->actor);

  g_object_notify_by_pspec (G_OBJECT (align), obj_props[PROP_ALIGN_AXIS]);
}

/**
 * clutter_align_constraint_get_align_axis:
 * @align: a #ClutterAlignConstraint
 *
 * Retrieves the value set using clutter_align_constraint_set_align_axis()
 *
 * Return value: the alignment axis
 *
 * Since: 1.4
 */
ClutterAlignAxis
clutter_align_constraint_get_align_axis (ClutterAlignConstraint *align)
{
  g_return_val_if_fail (CLUTTER_IS_ALIGN_CONSTRAINT (align),
                        CLUTTER_ALIGN_X_AXIS);

  return align->align_axis;
}

/**
 * clutter_align_constraint_set_factor:
 * @align: a #ClutterAlignConstraint
 * @factor: the alignment factor, between 0.0 and 1.0
 *
 * Sets the alignment factor of the constraint
 *
 * The factor depends on the #ClutterAlignConstraint:align-axis property
 * and it is a value between 0.0 (meaning left, when
 * #ClutterAlignConstraint:align-axis is set to %CLUTTER_ALIGN_X_AXIS; or
 * meaning top, when #ClutterAlignConstraint:align-axis is set to
 * %CLUTTER_ALIGN_Y_AXIS) and 1.0 (meaning right, when
 * #ClutterAlignConstraint:align-axis is set to %CLUTTER_ALIGN_X_AXIS; or
 * meaning bottom, when #ClutterAlignConstraint:align-axis is set to
 * %CLUTTER_ALIGN_Y_AXIS). A value of 0.5 aligns in the middle in either
 * cases
 *
 * Since: 1.4
 */
void
clutter_align_constraint_set_factor (ClutterAlignConstraint *align,
                                     gfloat                  factor)
{
  g_return_if_fail (CLUTTER_IS_ALIGN_CONSTRAINT (align));

  align->factor = CLAMP (factor, 0.0, 1.0);

  if (align->actor != NULL)
    clutter_actor_queue_relayout (align->actor);

  g_object_notify_by_pspec (G_OBJECT (align), obj_props[PROP_FACTOR]);
}

/**
 * clutter_align_constraint_get_factor:
 * @align: a #ClutterAlignConstraint
 *
 * Retrieves the factor set using clutter_align_constraint_set_factor()
 *
 * Return value: the alignment factor
 *
 * Since: 1.4
 */
gfloat
clutter_align_constraint_get_factor (ClutterAlignConstraint *align)
{
  g_return_val_if_fail (CLUTTER_IS_ALIGN_CONSTRAINT (align), 0.0);

  return align->factor;
}
