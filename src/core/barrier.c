/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; c-basic-offset: 2; -*- */

#include "config.h"

#include <glib-object.h>

#include <X11/extensions/Xfixes.h>
#include <meta/util.h>
#include <meta/barrier.h>
#include "display-private.h"
#include "mutter-enum-types.h"
#include "core.h"

G_DEFINE_TYPE (MetaBarrier, meta_barrier, G_TYPE_OBJECT)

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

struct _MetaBarrierPrivate
{
  MetaDisplay *display;

  int x1;
  int y1;
  int x2;
  int y2;

  MetaBarrierDirection directions;

  PointerBarrier xbarrier;
};

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
      g_value_set_int (value, priv->x1);
      break;
    case PROP_Y1:
      g_value_set_int (value, priv->y1);
      break;
    case PROP_X2:
      g_value_set_int (value, priv->x2);
      break;
    case PROP_Y2:
      g_value_set_int (value, priv->y2);
      break;
    case PROP_DIRECTIONS:
      g_value_set_flags (value, priv->directions);
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
      priv->x1 = g_value_get_int (value);
      break;
    case PROP_Y1:
      priv->y1 = g_value_get_int (value);
      break;
    case PROP_X2:
      priv->x2 = g_value_get_int (value);
      break;
    case PROP_Y2:
      priv->y2 = g_value_get_int (value);
      break;
    case PROP_DIRECTIONS:
      priv->directions = g_value_get_flags (value);
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
  MetaBarrierPrivate *priv = barrier->priv;

  if (meta_barrier_is_active (barrier))
    {
      meta_bug ("MetaBarrier wrapper %p for X barrier %ld was destroyed"
                " while the X barrier is still active.",
                barrier, priv->xbarrier);
    }

  G_OBJECT_CLASS (meta_barrier_parent_class)->dispose (object);
}

gboolean
meta_barrier_is_active (MetaBarrier *barrier)
{
  return barrier->priv->xbarrier != 0;
}

static void
meta_barrier_constructed (GObject *object)
{
  MetaBarrier *barrier = META_BARRIER (object);
  MetaBarrierPrivate *priv = barrier->priv;
  Display *dpy;
  Window root;

  g_return_if_fail (priv->x1 == priv->x2 || priv->y1 == priv->y2);

  if (priv->display == NULL)
    {
      g_warning ("A display must be provided when constructing a barrier.");
      return;
    }

  dpy = priv->display->xdisplay;
  root = DefaultRootWindow (dpy);

  priv->xbarrier = XFixesCreatePointerBarrier (dpy, root,
                                               priv->x1, priv->y1,
                                               priv->x2, priv->y2,
                                               priv->directions, 0, NULL);

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

  g_type_class_add_private (object_class, sizeof(MetaBarrierPrivate));
}

void
meta_barrier_destroy (MetaBarrier *barrier)
{
  MetaBarrierPrivate *priv = barrier->priv;
  Display *dpy;

  if (priv->display == NULL)
    return;

  dpy = priv->display->xdisplay;

  if (!meta_barrier_is_active (barrier))
    return;

  XFixesDestroyPointerBarrier (dpy, priv->xbarrier);
  priv->xbarrier = 0;
}

static void
meta_barrier_init (MetaBarrier *barrier)
{
  barrier->priv = G_TYPE_INSTANCE_GET_PRIVATE (barrier, META_TYPE_BARRIER, MetaBarrierPrivate);
}
