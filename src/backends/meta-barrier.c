/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; c-basic-offset: 2; -*- */

/**
 * SECTION:barrier
 * @Title: MetaBarrier
 * @Short_Description: Pointer barriers
 */

#include "config.h"

#include <glib-object.h>

#include <meta/util.h>
#include <meta/barrier.h>
#include "backends/native/meta-backend-native.h"
#include "backends/native/meta-barrier-native.h"
#include "backends/x11/meta-backend-x11.h"
#include "backends/x11/meta-barrier-x11.h"
#include <meta/meta-enum-types.h>

G_DEFINE_TYPE (MetaBarrier, meta_barrier, G_TYPE_OBJECT)
G_DEFINE_TYPE (MetaBarrierImpl, meta_barrier_impl, G_TYPE_OBJECT)

enum {
  PROP_0,

  PROP_DISPLAY,

  PROP_X1,
  PROP_Y1,
  PROP_X2,
  PROP_Y2,
  PROP_DIRECTIONS,

  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

enum {
  HIT,
  LEFT,

  LAST_SIGNAL,
};

static guint obj_signals[LAST_SIGNAL];


static void
meta_barrier_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  MetaBarrier *barrier = META_BARRIER (object);
  MetaBarrierPrivate *priv = barrier->priv;
  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    case PROP_X1:
      g_value_set_int (value, priv->border.line.a.x);
      break;
    case PROP_Y1:
      g_value_set_int (value, priv->border.line.a.y);
      break;
    case PROP_X2:
      g_value_set_int (value, priv->border.line.b.x);
      break;
    case PROP_Y2:
      g_value_set_int (value, priv->border.line.b.y);
      break;
    case PROP_DIRECTIONS:
      g_value_set_flags (value,
                         meta_border_get_allows_directions (&priv->border));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_barrier_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  MetaBarrier *barrier = META_BARRIER (object);
  MetaBarrierPrivate *priv = barrier->priv;
  switch (prop_id)
    {
    case PROP_DISPLAY:
      priv->display = g_value_get_object (value);
      break;
    case PROP_X1:
      priv->border.line.a.x = g_value_get_int (value);
      break;
    case PROP_Y1:
      priv->border.line.a.y = g_value_get_int (value);
      break;
    case PROP_X2:
      priv->border.line.b.x = g_value_get_int (value);
      break;
    case PROP_Y2:
      priv->border.line.b.y = g_value_get_int (value);
      break;
    case PROP_DIRECTIONS:
      meta_border_set_allows_directions (&priv->border,
                                         g_value_get_flags (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_barrier_dispose (GObject *object)
{
  MetaBarrier *barrier = META_BARRIER (object);

  if (meta_barrier_is_active (barrier))
    {
      meta_bug ("MetaBarrier %p was destroyed while it was still active.",
                barrier);
    }

  G_OBJECT_CLASS (meta_barrier_parent_class)->dispose (object);
}

gboolean
meta_barrier_is_active (MetaBarrier *barrier)
{
  MetaBarrierImpl *impl = barrier->priv->impl;

  if (impl)
    return META_BARRIER_IMPL_GET_CLASS (impl)->is_active (impl);
  else
    return FALSE;
}

/**
 * meta_barrier_release:
 * @barrier: The barrier to release
 * @event: The event to release the pointer for
 *
 * In XI2.3, pointer barriers provide a feature where they can
 * be temporarily released so that the pointer goes through
 * them. Pass a #MetaBarrierEvent to release the barrier for
 * this event sequence.
 */
void
meta_barrier_release (MetaBarrier      *barrier,
                      MetaBarrierEvent *event)
{
  MetaBarrierImpl *impl = barrier->priv->impl;

  if (impl)
    META_BARRIER_IMPL_GET_CLASS (impl)->release (impl, event);
}

static void
meta_barrier_constructed (GObject *object)
{
  MetaBarrier *barrier = META_BARRIER (object);
  MetaBarrierPrivate *priv = barrier->priv;

  g_return_if_fail (priv->border.line.a.x == priv->border.line.b.x ||
                    priv->border.line.a.y == priv->border.line.b.y);

#if defined(HAVE_NATIVE_BACKEND)
  if (META_IS_BACKEND_NATIVE (meta_get_backend ()))
    priv->impl = meta_barrier_impl_native_new (barrier);
#endif
#if defined(HAVE_XI23)
  if (META_IS_BACKEND_X11 (meta_get_backend ()) &&
      !meta_is_wayland_compositor ())
    priv->impl = meta_barrier_impl_x11_new (barrier);
#endif

  if (priv->impl == NULL)
    g_warning ("Created a non-working barrier");

  /* Take a ref that we'll release in destroy() so that the object stays
   * alive while active. */
  g_object_ref (barrier);

  G_OBJECT_CLASS (meta_barrier_parent_class)->constructed (object);
}

static void
meta_barrier_class_init (MetaBarrierClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = meta_barrier_get_property;
  object_class->set_property = meta_barrier_set_property;
  object_class->dispose = meta_barrier_dispose;
  object_class->constructed = meta_barrier_constructed;

  obj_props[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         "Display",
                         "The display to construct the pointer barrier on",
                         META_TYPE_DISPLAY,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_X1] =
    g_param_spec_int ("x1",
                      "X1",
                      "The first X coordinate of the barrier",
                      0, G_MAXSHORT, 0,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_Y1] =
    g_param_spec_int ("y1",
                      "Y1",
                      "The first Y coordinate of the barrier",
                      0, G_MAXSHORT, 0,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_X2] =
    g_param_spec_int ("x2",
                      "X2",
                      "The second X coordinate of the barrier",
                      0, G_MAXSHORT, G_MAXSHORT,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_Y2] =
    g_param_spec_int ("y2",
                      "Y2",
                      "The second Y coordinate of the barrier",
                      0, G_MAXSHORT, G_MAXSHORT,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_DIRECTIONS] =
    g_param_spec_flags ("directions",
                        "Directions",
                        "A set of directions to let the pointer through",
                        META_TYPE_BARRIER_DIRECTION,
                        0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);

  /**
   * MetaBarrier::hit:
   * @barrier: The #MetaBarrier that was hit
   * @event: A #MetaBarrierEvent that has the details of how
   * the barrier was hit.
   *
   * When a pointer barrier is hit, this will trigger. This
   * requires an XI2-enabled server.
   */
  obj_signals[HIT] =
    g_signal_new ("hit",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  META_TYPE_BARRIER_EVENT);

  /**
   * MetaBarrier::left:
   * @barrier: The #MetaBarrier that was left
   * @event: A #MetaBarrierEvent that has the details of how
   * the barrier was left.
   *
   * When a pointer barrier hitbox was left, this will trigger.
   * This requires an XI2-enabled server.
   */
  obj_signals[LEFT] =
    g_signal_new ("left",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  META_TYPE_BARRIER_EVENT);

  g_type_class_add_private (object_class, sizeof(MetaBarrierPrivate));
}

void
meta_barrier_destroy (MetaBarrier *barrier)
{
  MetaBarrierImpl *impl = barrier->priv->impl;

  if (impl)
    return META_BARRIER_IMPL_GET_CLASS (impl)->destroy (impl);

  g_object_unref (barrier);
}

static void
meta_barrier_init (MetaBarrier *barrier)
{
  barrier->priv = G_TYPE_INSTANCE_GET_PRIVATE (barrier, META_TYPE_BARRIER, MetaBarrierPrivate);
}

void
_meta_barrier_emit_hit_signal (MetaBarrier      *barrier,
                               MetaBarrierEvent *event)
{
  g_signal_emit (barrier, obj_signals[HIT], 0, event);
}

void
_meta_barrier_emit_left_signal (MetaBarrier      *barrier,
                                MetaBarrierEvent *event)
{
  g_signal_emit (barrier, obj_signals[LEFT], 0, event);
}

static void
meta_barrier_impl_class_init (MetaBarrierImplClass *klass)
{
  klass->is_active = NULL;
  klass->release = NULL;
  klass->destroy = NULL;
}

static void
meta_barrier_impl_init (MetaBarrierImpl *impl)
{
}

static MetaBarrierEvent *
meta_barrier_event_ref (MetaBarrierEvent *event)
{
  g_return_val_if_fail (event != NULL, NULL);
  g_return_val_if_fail (event->ref_count > 0, NULL);

  g_atomic_int_inc ((volatile int *)&event->ref_count);
  return event;
}

void
meta_barrier_event_unref (MetaBarrierEvent *event)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (event->ref_count > 0);

  if (g_atomic_int_dec_and_test ((volatile int *)&event->ref_count))
    g_slice_free (MetaBarrierEvent, event);
}

G_DEFINE_BOXED_TYPE (MetaBarrierEvent,
                     meta_barrier_event,
                     meta_barrier_event_ref,
                     meta_barrier_event_unref)
