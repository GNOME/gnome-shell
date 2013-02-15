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

typedef struct _MetaBarrierEvent   MetaBarrierEvent;

/**
 * MetaBarrier:
 *
 * The <structname>MetaBarrier</structname> structure contains
 * only private data and should be accessed using the provided API
 *
 **/
struct _MetaBarrier
{
  GObject parent;

  MetaBarrierPrivate *priv;
};

/**
 * MetaBarrierClass:
 *
 * The <structname>MetaBarrierClass</structname> structure contains only
 * private data.
 */
struct _MetaBarrierClass
{
  /*< private >*/
  GObjectClass parent_class;
};

GType meta_barrier_get_type (void) G_GNUC_CONST;

gboolean meta_barrier_is_active (MetaBarrier *barrier);
void meta_barrier_destroy (MetaBarrier *barrier);
void meta_barrier_release (MetaBarrier      *barrier,
                           MetaBarrierEvent *event);

/**
 * MetaBarrierDirection:
 * @META_BARRIER_DIRECTION_POSITIVE_X: Positive direction in the X axis
 * @META_BARRIER_DIRECTION_POSITIVE_Y: Positive direction in the Y axis
 * @META_BARRIER_DIRECTION_NEGATIVE_X: Negative direction in the X axis
 * @META_BARRIER_DIRECTION_NEGATIVE_Y: Negative direction in the Y axis
 */

/* Keep in sync with XFixes */
typedef enum {
  META_BARRIER_DIRECTION_POSITIVE_X = 1 << 0,
  META_BARRIER_DIRECTION_POSITIVE_Y = 1 << 1,
  META_BARRIER_DIRECTION_NEGATIVE_X = 1 << 2,
  META_BARRIER_DIRECTION_NEGATIVE_Y = 1 << 3,
} MetaBarrierDirection;

/**
 * MetaBarrierEvent:
 * @event_id: A unique integer ID identifying a
 * consecutive series of motions at or along the barrier
 * @time: Server time, in milliseconds
 * @dt: Server time, in milliseconds, since the last event
 * sent for this barrier
 * @x: The cursor X position in screen coordinates
 * @y: The cursor Y position in screen coordinates.
 * @dx: If the cursor hadn't been constrained, the delta
 * of X movement past the barrier, in screen coordinates
 * @dy: If the cursor hadn't been constrained, the delta
 * of X movement past the barrier, in screen coordinates
 * @released: A boolean flag, %TRUE if this event generated
 * by the pointer leaving the barrier as a result of a client
 * calling meta_barrier_release() (will be set only for
 * MetaBarrier::leave signals)
 * @grabbed: A boolean flag, %TRUE if the pointer was grabbed
 * at the time this event was sent
 */
struct _MetaBarrierEvent {
  /* < private > */
  volatile guint ref_count;

  /* < public > */
  int event_id;
  int dt;
  guint32 time;
  double x;
  double y;
  double dx;
  double dy;
  gboolean released;
  gboolean grabbed;
};

#define META_TYPE_BARRIER_EVENT (meta_barrier_event_get_type ())
GType meta_barrier_event_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __META_BARRIER_H__ */
