#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-bind-constraint.h"

#include "clutter-constraint.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"

#define CLUTTER_BIND_CONSTRAINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BIND_CONSTRAINT, ClutterBindConstraintClass))
#define CLUTTER_IS_BIND_CONSTRAINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BIND_CONSTRAINT))
#define CLUTTER_BIND_CONSTRAINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BIND_CONSTRAINT, ClutterBindConstraintClass))

typedef struct _ClutterBindConstraintClass      ClutterBindConstraintClass;

struct _ClutterBindConstraint
{
  ClutterConstraint parent_instance;

  ClutterActor *source;
  ClutterBindAxis bind_axis;
  gfloat offset;
};

struct _ClutterBindConstraintClass
{
  ClutterConstraintClass parent_class;
};

enum
{
  PROP_0,

  PROP_SOURCE,
  PROP_BIND_AXIS,
  PROP_OFFSET
};

G_DEFINE_TYPE (ClutterBindConstraint,
               clutter_bind_constraint,
               CLUTTER_TYPE_CONSTRAINT);

static void
update_actor_position (ClutterBindConstraint *bind)
{
  ClutterVertex source_position;
  ClutterActor *actor;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (bind)))
    return;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (bind));
  if (actor == NULL)
    return;

  source_position.x = clutter_actor_get_x (bind->source);
  source_position.y = clutter_actor_get_y (bind->source);
  source_position.z = clutter_actor_get_depth (bind->source);

  switch (bind->bind_axis)
    {
    case CLUTTER_BIND_X_AXIS:
      clutter_actor_set_x (actor, source_position.x + bind->offset);
      break;

    case CLUTTER_BIND_Y_AXIS:
      clutter_actor_set_y (actor, source_position.y + bind->offset);
      break;

    case CLUTTER_BIND_Z_AXIS:
      clutter_actor_set_depth (actor, source_position.z + bind->offset);
      break;
    }
}

static void
source_position_changed (GObject               *gobject,
                         GParamSpec            *pspec,
                         ClutterBindConstraint *bind)
{
  if (strcmp (pspec->name, "x") == 0 ||
      strcmp (pspec->name, "y") == 0 ||
      strcmp (pspec->name, "depth") == 0)
    {
      update_actor_position (bind);
    }
}

static void
source_destroyed (ClutterActor *actor,
                  ClutterBindConstraint *bind)
{
  bind->source = NULL;
}

static void
_clutter_bind_constraint_set_source (ClutterBindConstraint *bind,
                                     ClutterActor          *source)
{
  ClutterActor *old_source = bind->source;

  if (old_source != NULL)
    {
      g_signal_handlers_disconnect_by_func (old_source,
                                            G_CALLBACK (source_destroyed),
                                            bind);
      g_signal_handlers_disconnect_by_func (old_source,
                                            G_CALLBACK (source_position_changed),
                                            bind);
    }

  bind->source = source;
  g_signal_connect (bind->source, "notify",
                    G_CALLBACK (source_position_changed),
                    bind);
  g_signal_connect (bind->source, "destroy",
                    G_CALLBACK (source_destroyed),
                    bind);

  update_actor_position (bind);

  g_object_notify (G_OBJECT (bind), "source");
}

static void
_clutter_bind_constraint_set_bind_axis (ClutterBindConstraint *bind,
                                        ClutterBindAxis        axis)
{
  if (bind->bind_axis == axis)
    return;

  bind->bind_axis = axis;

  update_actor_position (bind);

  g_object_notify (G_OBJECT (bind), "bind-axis");
}

static void
_clutter_bind_constraint_set_offset (ClutterBindConstraint *bind,
                                     gfloat                 offset)
{
  if (fabs (bind->offset - offset) < 0.00001f)
    return;

  bind->offset = offset;

  update_actor_position (bind);

  g_object_notify (G_OBJECT (bind), "offset");
}

static void
clutter_bind_constraint_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ClutterBindConstraint *bind = CLUTTER_BIND_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      _clutter_bind_constraint_set_source (bind, g_value_get_object (value));
      break;

    case PROP_BIND_AXIS:
      _clutter_bind_constraint_set_bind_axis (bind, g_value_get_enum (value));
      break;

    case PROP_OFFSET:
      _clutter_bind_constraint_set_offset (bind, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_bind_constraint_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ClutterBindConstraint *bind = CLUTTER_BIND_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, bind->source);
      break;

    case PROP_BIND_AXIS:
      g_value_set_enum (value, bind->bind_axis);
      break;

    case PROP_OFFSET:
      g_value_set_float (value, bind->offset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_bind_constraint_class_init (ClutterBindConstraintClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_bind_constraint_set_property;
  gobject_class->get_property = clutter_bind_constraint_get_property;

  pspec = g_param_spec_object ("source",
                               "Source",
                               "The source of the binding",
                               CLUTTER_TYPE_ACTOR,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_CONSTRUCT);
  g_object_class_install_property (gobject_class, PROP_SOURCE, pspec);

  pspec = g_param_spec_enum ("bind-axis",
                             "Bind Axis",
                             "The axis to bind the position from",
                             CLUTTER_TYPE_BIND_AXIS,
                             CLUTTER_BIND_X_AXIS,
                             CLUTTER_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT);
  g_object_class_install_property (gobject_class, PROP_BIND_AXIS, pspec);

  pspec = g_param_spec_float ("offset",
                              "Offset",
                              "The offset in pixels to apply to the binding",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0.0f,
                              CLUTTER_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT);
  g_object_class_install_property (gobject_class, PROP_OFFSET, pspec);
}

static void
clutter_bind_constraint_init (ClutterBindConstraint *self)
{
  self->source = NULL;
  self->bind_axis = CLUTTER_BIND_X_AXIS;
  self->offset = 0.0f;
}

ClutterConstraint *
clutter_bind_constraint_new (ClutterActor    *source,
                             ClutterBindAxis  axis,
                             gfloat           offset)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (source), NULL);

  return g_object_new (CLUTTER_TYPE_BIND_CONSTRAINT,
                       "source", source,
                       "bind-axis", axis,
                       "offset", offset,
                       NULL);
}
