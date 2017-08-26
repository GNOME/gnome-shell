/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib-object.h>

#include <meta/errors.h>
#include "display-private.h"
#include "x11/meta-x11-display-private.h"
#include "screen-private.h"
#include "startup-notification-private.h"

/* This should be fairly long, as it should never be required unless
 * apps or .desktop files are buggy, and it's confusing if
 * OpenOffice or whatever seems to stop launching - people
 * might decide they need to launch it again.
 */
#define STARTUP_TIMEOUT 15000000

typedef struct _MetaStartupNotificationSequence MetaStartupNotificationSequence;
typedef struct _MetaStartupNotificationSequenceClass MetaStartupNotificationSequenceClass;

enum {
  PROP_SN_0,
  PROP_SN_DISPLAY,
  N_SN_PROPS
};

enum {
  PROP_SEQ_0,
  PROP_SEQ_ID,
  PROP_SEQ_TIMESTAMP,
  N_SEQ_PROPS
};

enum {
  SN_CHANGED,
  N_SN_SIGNALS
};

static guint sn_signals[N_SN_SIGNALS];
static GParamSpec *sn_props[N_SN_PROPS];
static GParamSpec *seq_props[N_SEQ_PROPS];

typedef struct
{
  GSList *list;
  gint64 now;
} CollectTimedOutData;

struct _MetaStartupNotification
{
  GObject parent_instance;
  MetaDisplay *display;

#ifdef HAVE_STARTUP_NOTIFICATION
  SnDisplay *sn_display;
  SnMonitorContext *sn_context;
#endif

  GSList *startup_sequences;
  guint startup_sequence_timeout;
};

#define META_TYPE_STARTUP_NOTIFICATION_SEQUENCE \
  (meta_startup_notification_sequence_get_type ())

G_DECLARE_DERIVABLE_TYPE (MetaStartupNotificationSequence,
                          meta_startup_notification_sequence,
                          META, STARTUP_NOTIFICATION_SEQUENCE,
                          GObject)

typedef struct {
  gchar *id;
  gint64 timestamp;
} MetaStartupNotificationSequencePrivate;

struct _MetaStartupNotificationSequenceClass {
  GObjectClass parent_class;

  void (* complete) (MetaStartupNotificationSequence *sequence);
};

G_DEFINE_TYPE (MetaStartupNotification,
               meta_startup_notification,
               G_TYPE_OBJECT)
G_DEFINE_TYPE_WITH_PRIVATE (MetaStartupNotificationSequence,
                            meta_startup_notification_sequence,
                            G_TYPE_OBJECT)

#ifdef HAVE_STARTUP_NOTIFICATION

enum {
  PROP_SEQ_X11_0,
  PROP_SEQ_X11_SEQ,
  N_SEQ_X11_PROPS
};

struct _MetaStartupNotificationSequenceX11 {
  MetaStartupNotificationSequence parent_instance;
  SnStartupSequence *seq;
};

static GParamSpec *seq_x11_props[N_SEQ_X11_PROPS];

#define META_TYPE_STARTUP_NOTIFICATION_SEQUENCE_X11 \
  (meta_startup_notification_sequence_x11_get_type ())

G_DECLARE_FINAL_TYPE (MetaStartupNotificationSequenceX11,
                      meta_startup_notification_sequence_x11,
                      META, STARTUP_NOTIFICATION_SEQUENCE_X11,
                      MetaStartupNotificationSequence)

G_DEFINE_TYPE (MetaStartupNotificationSequenceX11,
               meta_startup_notification_sequence_x11,
               META_TYPE_STARTUP_NOTIFICATION_SEQUENCE)

static void meta_startup_notification_ensure_timeout  (MetaStartupNotification *sn);

#endif

static void
meta_startup_notification_update_feedback (MetaStartupNotification *sn)
{
  MetaScreen *screen = sn->display->screen;

  if (sn->startup_sequences != NULL)
    {
      meta_topic (META_DEBUG_STARTUP,
                  "Setting busy cursor\n");
      meta_screen_set_cursor (screen, META_CURSOR_BUSY);
    }
  else
    {
      meta_topic (META_DEBUG_STARTUP,
                  "Setting default cursor\n");
      meta_screen_set_cursor (screen, META_CURSOR_DEFAULT);
    }
}

static void
meta_startup_notification_sequence_init (MetaStartupNotificationSequence *seq)
{
}

static void
meta_startup_notification_sequence_finalize (GObject *object)
{
  MetaStartupNotificationSequence *seq;
  MetaStartupNotificationSequencePrivate *priv;

  seq = META_STARTUP_NOTIFICATION_SEQUENCE (object);
  priv = meta_startup_notification_sequence_get_instance_private (seq);
  g_free (priv->id);

  G_OBJECT_CLASS (meta_startup_notification_sequence_parent_class)->finalize (object);
}

static void
meta_startup_notification_sequence_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  MetaStartupNotificationSequence *seq;
  MetaStartupNotificationSequencePrivate *priv;

  seq = META_STARTUP_NOTIFICATION_SEQUENCE (object);
  priv = meta_startup_notification_sequence_get_instance_private (seq);

  switch (prop_id)
    {
    case PROP_SEQ_ID:
      priv->id = g_value_dup_string (value);
      break;
    case PROP_SEQ_TIMESTAMP:
      priv->timestamp = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_startup_notification_sequence_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  MetaStartupNotificationSequence *seq;
  MetaStartupNotificationSequencePrivate *priv;

  seq = META_STARTUP_NOTIFICATION_SEQUENCE (object);
  priv = meta_startup_notification_sequence_get_instance_private (seq);

  switch (prop_id)
    {
    case PROP_SEQ_ID:
      g_value_set_string (value, priv->id);
      break;
    case PROP_SEQ_TIMESTAMP:
      g_value_set_int64 (value, priv->timestamp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_startup_notification_sequence_class_init (MetaStartupNotificationSequenceClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_startup_notification_sequence_finalize;
  object_class->set_property = meta_startup_notification_sequence_set_property;
  object_class->get_property = meta_startup_notification_sequence_get_property;

  seq_props[PROP_SEQ_ID] =
    g_param_spec_string ("id",
                         "ID",
                         "ID",
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);
  seq_props[PROP_SEQ_TIMESTAMP] =
    g_param_spec_int64 ("timestamp",
                        "Timestamp",
                        "Timestamp",
                        G_MININT64, G_MAXINT64, 0,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_SEQ_PROPS, seq_props);
}

static const gchar *
meta_startup_notification_sequence_get_id (MetaStartupNotificationSequence *seq)
{
  MetaStartupNotificationSequencePrivate *priv;

  priv = meta_startup_notification_sequence_get_instance_private (seq);
  return priv->id;
}

#ifdef HAVE_STARTUP_NOTIFICATION
static gint64
meta_startup_notification_sequence_get_timestamp (MetaStartupNotificationSequence *seq)
{
  MetaStartupNotificationSequencePrivate *priv;

  priv = meta_startup_notification_sequence_get_instance_private (seq);
  return priv->timestamp;
}

static void
meta_startup_notification_sequence_complete (MetaStartupNotificationSequence *seq)
{
  MetaStartupNotificationSequenceClass *klass;

  klass = META_STARTUP_NOTIFICATION_SEQUENCE_GET_CLASS (seq);

  if (klass->complete)
    klass->complete (seq);
}
#endif

#ifdef HAVE_STARTUP_NOTIFICATION
static void
meta_startup_notification_sequence_x11_complete (MetaStartupNotificationSequence *seq)
{
  MetaStartupNotificationSequenceX11 *seq_x11;

  seq_x11 = META_STARTUP_NOTIFICATION_SEQUENCE_X11 (seq);
  sn_startup_sequence_complete (seq_x11->seq);
}

static void
meta_startup_notification_sequence_x11_finalize (GObject *object)
{
  MetaStartupNotificationSequenceX11 *seq;

  seq = META_STARTUP_NOTIFICATION_SEQUENCE_X11 (object);
  sn_startup_sequence_unref (seq->seq);

  G_OBJECT_CLASS (meta_startup_notification_sequence_x11_parent_class)->finalize (object);
}

static void
meta_startup_notification_sequence_x11_set_property (GObject      *object,
                                                     guint         prop_id,
                                                     const GValue *value,
                                                     GParamSpec   *pspec)
{
  MetaStartupNotificationSequenceX11 *seq;

  seq = META_STARTUP_NOTIFICATION_SEQUENCE_X11 (object);

  switch (prop_id)
    {
    case PROP_SEQ_X11_SEQ:
      seq->seq = g_value_get_pointer (value);
      sn_startup_sequence_ref (seq->seq);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_startup_notification_sequence_x11_get_property (GObject    *object,
                                                     guint       prop_id,
                                                     GValue     *value,
                                                     GParamSpec *pspec)
{
  MetaStartupNotificationSequenceX11 *seq;

  seq = META_STARTUP_NOTIFICATION_SEQUENCE_X11 (object);

  switch (prop_id)
    {
    case PROP_SEQ_X11_SEQ:
      g_value_set_pointer (value, seq->seq);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_startup_notification_sequence_x11_init (MetaStartupNotificationSequenceX11 *seq)
{
}

static void
meta_startup_notification_sequence_x11_class_init (MetaStartupNotificationSequenceX11Class *klass)
{
  MetaStartupNotificationSequenceClass *seq_class;
  GObjectClass *object_class;

  seq_class = META_STARTUP_NOTIFICATION_SEQUENCE_CLASS (klass);
  seq_class->complete = meta_startup_notification_sequence_x11_complete;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_startup_notification_sequence_x11_finalize;
  object_class->set_property = meta_startup_notification_sequence_x11_set_property;
  object_class->get_property = meta_startup_notification_sequence_x11_get_property;

  seq_x11_props[PROP_SEQ_X11_SEQ] =
    g_param_spec_pointer ("seq",
                          "Sequence",
                          "Sequence",
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_SEQ_X11_PROPS,
                                     seq_x11_props);
}

static MetaStartupNotificationSequence *
meta_startup_notification_sequence_x11_new (SnStartupSequence *seq)
{
  gint64 timestamp;

  timestamp = sn_startup_sequence_get_timestamp (seq) * 1000;
  return g_object_new (META_TYPE_STARTUP_NOTIFICATION_SEQUENCE_X11,
                       "id", sn_startup_sequence_get_id (seq),
                       "timestamp", timestamp,
                       "seq", seq,
                       NULL);
}

static void
meta_startup_notification_add_sequence_internal (MetaStartupNotification         *sn,
                                                 MetaStartupNotificationSequence *seq)
{
  sn->startup_sequences = g_slist_prepend (sn->startup_sequences,
                                           g_object_ref (seq));

  meta_startup_notification_ensure_timeout (sn);
  meta_startup_notification_update_feedback (sn);
}

static void
collect_timed_out_foreach (void *element,
                           void *data)
{
  MetaStartupNotificationSequence *sequence = element;
  CollectTimedOutData *ctod = data;
  gint64 elapsed, timestamp;

  timestamp = meta_startup_notification_sequence_get_timestamp (sequence);
  elapsed = ctod->now - timestamp;

  meta_topic (META_DEBUG_STARTUP,
              "Sequence used %" G_GINT64_FORMAT " ms vs. %d max: %s\n",
              elapsed, STARTUP_TIMEOUT,
              meta_startup_notification_sequence_get_id (sequence));

  if (elapsed > STARTUP_TIMEOUT)
    ctod->list = g_slist_prepend (ctod->list, sequence);
}

static gboolean
startup_sequence_timeout (void *data)
{
  MetaStartupNotification *sn = data;
  CollectTimedOutData ctod;
  GSList *l;

  ctod.list = NULL;
  ctod.now = g_get_monotonic_time ();
  g_slist_foreach (sn->startup_sequences,
                   collect_timed_out_foreach,
                   &ctod);

  for (l = ctod.list; l != NULL; l = l->next)
    {
      MetaStartupNotificationSequence *sequence = l->data;

      meta_topic (META_DEBUG_STARTUP,
                  "Timed out sequence %s\n",
                  meta_startup_notification_sequence_get_id (sequence));

      meta_startup_notification_sequence_complete (sequence);
    }

  g_slist_free (ctod.list);

  if (sn->startup_sequences != NULL)
    {
      return TRUE;
    }
  else
    {
      /* remove */
      sn->startup_sequence_timeout = 0;
      return FALSE;
    }
}

static void
meta_startup_notification_ensure_timeout (MetaStartupNotification *sn)
{
  if (sn->startup_sequence_timeout != 0)
    return;

  /* our timeout just polls every second, instead of bothering
   * to compute exactly when we may next time out
   */
  sn->startup_sequence_timeout = g_timeout_add_seconds (1,
                                                        startup_sequence_timeout,
                                                        sn);
  g_source_set_name_by_id (sn->startup_sequence_timeout,
                           "[mutter] startup_sequence_timeout");
}
#endif

static void
meta_startup_notification_remove_sequence_internal (MetaStartupNotification         *sn,
                                                    MetaStartupNotificationSequence *seq)
{
  sn->startup_sequences = g_slist_remove (sn->startup_sequences, seq);
  meta_startup_notification_update_feedback (sn);

  if (sn->startup_sequences == NULL &&
      sn->startup_sequence_timeout != 0)
    {
      g_source_remove (sn->startup_sequence_timeout);
      sn->startup_sequence_timeout = 0;
    }

  g_object_unref (seq);
}

static MetaStartupNotificationSequence *
meta_startup_notification_lookup_sequence (MetaStartupNotification *sn,
                                           const gchar             *id)
{
  MetaStartupNotificationSequence *seq;
  const gchar *seq_id;
  GSList *l;

  for (l = sn->startup_sequences; l; l = l->next)
    {
      seq = l->data;
      seq_id = meta_startup_notification_sequence_get_id (seq);

      if (g_str_equal (seq_id, id))
        return l->data;
    }

  return NULL;
}

static void
meta_startup_notification_init (MetaStartupNotification *sn)
{
}

static void
meta_startup_notification_finalize (GObject *object)
{
  MetaStartupNotification *sn = META_STARTUP_NOTIFICATION (object);

#ifdef HAVE_STARTUP_NOTIFICATION
  sn_monitor_context_unref (sn->sn_context);
  sn_display_unref (sn->sn_display);
#endif

  if (sn->startup_sequence_timeout)
    g_source_remove (sn->startup_sequence_timeout);

  g_slist_foreach (sn->startup_sequences, (GFunc) g_object_unref, NULL);
  g_slist_free (sn->startup_sequences);
  sn->startup_sequences = NULL;

  G_OBJECT_CLASS (meta_startup_notification_parent_class)->finalize (object);
}

static void
meta_startup_notification_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  MetaStartupNotification *sn = META_STARTUP_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_SN_DISPLAY:
      sn->display = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_startup_notification_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  MetaStartupNotification *sn = META_STARTUP_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_SN_DISPLAY:
      g_value_set_object (value, sn->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

#ifdef HAVE_STARTUP_NOTIFICATION
static void
sn_error_trap_push (SnDisplay *sn_display,
                    Display   *xdisplay)
{
  MetaDisplay *display;
  display = meta_display_for_x_display (xdisplay);
  if (display != NULL)
    meta_error_trap_push (display->x11_display);
}

static void
sn_error_trap_pop (SnDisplay *sn_display,
                   Display   *xdisplay)
{
  MetaDisplay *display;
  display = meta_display_for_x_display (xdisplay);
  if (display != NULL)
    meta_error_trap_pop (display->x11_display);
}

static void
meta_startup_notification_sn_event (SnMonitorEvent *event,
                                    void           *user_data)
{
  MetaStartupNotification *sn = user_data;
  MetaStartupNotificationSequence *seq;
  SnStartupSequence *sequence;

  sequence = sn_monitor_event_get_startup_sequence (event);

  sn_startup_sequence_ref (sequence);

  switch (sn_monitor_event_get_type (event))
    {
    case SN_MONITOR_EVENT_INITIATED:
      {
        const char *wmclass;

        wmclass = sn_startup_sequence_get_wmclass (sequence);

        meta_topic (META_DEBUG_STARTUP,
                    "Received startup initiated for %s wmclass %s\n",
                    sn_startup_sequence_get_id (sequence),
                    wmclass ? wmclass : "(unset)");

        seq = meta_startup_notification_sequence_x11_new (sequence);
        meta_startup_notification_add_sequence_internal (sn, seq);
        g_object_unref (seq);
      }
      break;

    case SN_MONITOR_EVENT_COMPLETED:
      {
        meta_topic (META_DEBUG_STARTUP,
                    "Received startup completed for %s\n",
                    sn_startup_sequence_get_id (sequence));

        meta_startup_notification_remove_sequence (sn, sn_startup_sequence_get_id (sequence));
      }
      break;

    case SN_MONITOR_EVENT_CHANGED:
      meta_topic (META_DEBUG_STARTUP,
                  "Received startup changed for %s\n",
                  sn_startup_sequence_get_id (sequence));
      break;

    case SN_MONITOR_EVENT_CANCELED:
      meta_topic (META_DEBUG_STARTUP,
                  "Received startup canceled for %s\n",
                  sn_startup_sequence_get_id (sequence));
      break;
    }

  g_signal_emit (sn, sn_signals[SN_CHANGED], 0, sequence);

  sn_startup_sequence_unref (sequence);
}
#endif

static void
meta_startup_notification_constructed (GObject *object)
{
  MetaStartupNotification *sn = META_STARTUP_NOTIFICATION (object);

  g_assert (sn->display != NULL);

#ifdef HAVE_STARTUP_NOTIFICATION
  sn->sn_display = sn_display_new (sn->display->x11_display->xdisplay,
                                   sn_error_trap_push,
                                   sn_error_trap_pop);
  sn->sn_context =
    sn_monitor_context_new (sn->sn_display,
                            meta_ui_get_screen_number (),
                            meta_startup_notification_sn_event,
                            sn,
                            NULL);
#endif
  sn->startup_sequences = NULL;
  sn->startup_sequence_timeout = 0;
}

static void
meta_startup_notification_class_init (MetaStartupNotificationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = meta_startup_notification_constructed;
  object_class->finalize = meta_startup_notification_finalize;
  object_class->set_property = meta_startup_notification_set_property;
  object_class->get_property = meta_startup_notification_get_property;

  sn_props[PROP_SN_DISPLAY] =
    g_param_spec_object ("display",
                         "Display",
                         "Display",
                         META_TYPE_DISPLAY,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  sn_signals[SN_CHANGED] =
    g_signal_new ("changed",
                  META_TYPE_STARTUP_NOTIFICATION,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  g_object_class_install_properties (object_class, N_SN_PROPS, sn_props);
}

MetaStartupNotification *
meta_startup_notification_get (MetaDisplay *display)
{
  static MetaStartupNotification *notification = NULL;

  if (!notification)
    notification = g_object_new (META_TYPE_STARTUP_NOTIFICATION,
                                 "display", display,
                                 NULL);

  return notification;
}

void
meta_startup_notification_remove_sequence (MetaStartupNotification *sn,
                                           const gchar             *id)
{
  MetaStartupNotificationSequence *seq;

  seq = meta_startup_notification_lookup_sequence (sn, id);
  if (seq)
    meta_startup_notification_remove_sequence_internal (sn, seq);
}

gboolean
meta_startup_notification_handle_xevent (MetaStartupNotification *sn,
                                         XEvent                  *xevent)
{
#ifdef HAVE_STARTUP_NOTIFICATION
  return sn_display_process_event (sn->sn_display, xevent);
#endif
  return FALSE;
}

GSList *
meta_startup_notification_get_sequences (MetaStartupNotification *sn)
{
  GSList *sequences = NULL;
#ifdef HAVE_STARTUP_NOTIFICATION
  GSList *l;

  /* We return a list of SnStartupSequences here */
  for (l = sn->startup_sequences; l; l = l->next)
    {
      MetaStartupNotificationSequenceX11 *seq_x11;

      if (!META_IS_STARTUP_NOTIFICATION_SEQUENCE_X11 (l->data))
        continue;

      seq_x11 = META_STARTUP_NOTIFICATION_SEQUENCE_X11 (l->data);
      sequences = g_slist_prepend (sequences, seq_x11->seq);
    }
#endif

  return sequences;
}
