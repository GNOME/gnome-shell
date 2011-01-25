/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-evolution-event-source.h"


struct _ShellEvolutionEventSourceClass
{
  GObjectClass parent_class;
};

struct _ShellEvolutionEventSource {
  GObject parent;

};

/* Signals */
enum
{
  CHANGED_SIGNAL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ShellEvolutionEventSource, shell_evolution_event_source, G_TYPE_OBJECT);

static void
shell_evolution_event_source_init (ShellEvolutionEventSource *source)
{
}

static void
shell_evolution_event_source_class_init (ShellEvolutionEventSourceClass *klass)
{
  signals[CHANGED_SIGNAL] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

}

ShellEvolutionEventSource *
shell_evolution_event_source_new (void)
{
  return SHELL_EVOLUTION_EVENT_SOURCE (g_object_new (SHELL_TYPE_EVOLUTION_EVENT_SOURCE, NULL));
}

/**
 * shell_evolution_event_source_get_events:
 * @source: A #ShellEvolutionEventSource.
 * @date_begin: Start date (milli-seconds since Epoch).
 * @date_end: End date (milli-seconds since Epoch).
 *
 * Gets all events that occur between @date_begin and @date_end.
 *
 * Returns: (element-type ShellEvolutionEvent) (transfer full): List of events.
 */
GList *
shell_evolution_event_source_get_events  (ShellEvolutionEventSource *source,
                                          gint64                     date_begin,
                                          gint64                     date_end)
{
  GList *result;

  g_print ("get_events\n");
  g_print (" date_begin = %" G_GINT64_FORMAT "\n", date_begin);
  g_print (" date_end = %" G_GINT64_FORMAT "\n", date_end);

  result = NULL;

  gint64 event_time = 1295931631000 + 32 * 3600 * 1000;
  if (event_time >= date_begin && event_time <= date_end)
    {
      ShellEvolutionEvent *event;
      event = shell_evolution_event_new ("Stuff", FALSE, event_time);
      result = g_list_prepend (result, event);
    }

  return result;
}

G_DEFINE_BOXED_TYPE (ShellEvolutionEvent,
                     shell_evolution_event,
                     shell_evolution_event_copy,
                     shell_evolution_event_free);

void
shell_evolution_event_free (ShellEvolutionEvent *event)
{
  g_free (event->summary);
  g_free (event);
}

ShellEvolutionEvent *
shell_evolution_event_copy (ShellEvolutionEvent *event)
{
  ShellEvolutionEvent *copy;
  copy = g_memdup (event, sizeof (ShellEvolutionEvent));
  copy->summary = g_strdup (event->summary);
  return copy;
}

ShellEvolutionEvent *
shell_evolution_event_new (const gchar *summary,
                           gboolean     all_day,
                           gint64       date)
{
  ShellEvolutionEvent *event;
  event = g_new0 (ShellEvolutionEvent, 1);
  event->summary = g_strdup (summary);
  event->all_day = all_day;
  event->date = date;
  return event;
}
