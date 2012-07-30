/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; c-basic-offset: 2; -*- */

#ifndef __META_BARRIER_H__
#define __META_BARRIER_H__

#include <glib-object.h>

#include <meta/display.h>

G_BEGIN_DECLS

#define META_TYPE_BARRIER            (meta_barrier_get_type ())
#define META_BARRIER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BARRIER, MetaBarrier))
#define META_BARRIER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_BARRIER, MetaBarrierClass))
#define META_IS_BARRIER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BARRIER))
#define META_IS_BARRIER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_BARRIER))
#define META_BARRIER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_BARRIER, MetaBarrierClass))

typedef struct _MetaBarrier        MetaBarrier;
typedef struct _MetaBarrierClass   MetaBarrierClass;
typedef struct _MetaBarrierPrivate MetaBarrierPrivate;

struct _MetaBarrier
{
  GObject parent;

  MetaBarrierPrivate *priv;
};

struct _MetaBarrierClass
{
  GObjectClass parent_class;
};

GType meta_barrier_get_type (void) G_GNUC_CONST;

gboolean meta_barrier_is_active (MetaBarrier *barrier);
void meta_barrier_destroy (MetaBarrier *barrier);

/* Keep in sync with XFixes */
typedef enum {
  META_BARRIER_DIRECTION_POSITIVE_X = 1 << 0,
  META_BARRIER_DIRECTION_POSITIVE_Y = 1 << 1,
  META_BARRIER_DIRECTION_NEGATIVE_X = 1 << 2,
  META_BARRIER_DIRECTION_NEGATIVE_Y = 1 << 3,
} MetaBarrierDirection;

G_END_DECLS

#endif /* __META_BARRIER_H__ */
