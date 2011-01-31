/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "calendar-client/calendar-client.h"
#include "shell-evolution-event-source.h"


struct _ShellEvolutionEventSourceClass
{
  GObjectClass parent_class;
};

struct _ShellEvolutionEventSource {
  GObject parent;
  CalendarClient *client;
  /* The month that we are currently requesting events from */
  gint req_year;
  gint req_mon; /* starts at 1, not zero */
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
on_tasks_changed (CalendarClient *client,
                  gpointer        user_data)
{
  ShellEvolutionEventSource *source = SHELL_EVOLUTION_EVENT_SOURCE (user_data);
  /* g_print ("on tasks changed\n"); */
  g_signal_emit (source, signals[CHANGED_SIGNAL], 0);
}

static void
on_appointments_changed (CalendarClient *client,
                         gpointer        user_data)
{
  ShellEvolutionEventSource *source = SHELL_EVOLUTION_EVENT_SOURCE (user_data);
  /* g_print ("on appointments changed\n"); */
  g_signal_emit (source, signals[CHANGED_SIGNAL], 0);
}

static void
shell_evolution_event_source_init (ShellEvolutionEventSource *source)
{
  source->client = calendar_client_new ();
  g_signal_connect (source->client,
                    "tasks-changed",
                    G_CALLBACK (on_tasks_changed),
                    source);
  g_signal_connect (source->client,
                    "appointments-changed",
                    G_CALLBACK (on_appointments_changed),
                    source);
}

static void
shell_evolution_event_source_finalize (GObject *object)
{
  ShellEvolutionEventSource *source = SHELL_EVOLUTION_EVENT_SOURCE (object);
  g_object_unref (source->client);
  G_OBJECT_CLASS (shell_evolution_event_source_parent_class)->finalize (object);
}

static void
shell_evolution_event_source_class_init (ShellEvolutionEventSourceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shell_evolution_event_source_finalize;

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

void
shell_evolution_event_source_request_range (ShellEvolutionEventSource *source,
                                            gint64                     msec_begin,
                                            gint64                     msec_end)
{
  GDateTime *middle_utc, *middle;

  /* The CalendarClient type is a convenience wrapper on top of
   * Evolution Data Server. It is based on the assumption that only
   * a single month is shown at a time.
   *
   * To avoid reimplemting all the work already done in CalendarClient
   * we make the same assumption. This means that we only show events
   * in the month that is in the middle of @msec_begin and
   * @msec_end. Since the Shell displays a month at a time (plus the
   * days before and after) it works out just fine.
   */

  middle_utc = g_date_time_new_from_unix_utc ((msec_begin + msec_end) / 2 / 1000);
  /* CalendarClient uses localtime rather than UTC */
  middle = g_date_time_to_local (middle_utc);
  g_date_time_unref (middle_utc);
  g_date_time_get_ymd (middle, &source->req_year, &source->req_mon, NULL);
  g_date_time_unref (middle);
  calendar_client_select_month (source->client, source->req_mon - 1, source->req_year);
}

static gint
event_cmp (gconstpointer a,
           gconstpointer b)
{
  const ShellEvolutionEvent *ea;
  const ShellEvolutionEvent *eb;

  ea = a;
  eb = b;
  if (ea->msec_begin < eb->msec_begin)
    return -1;
  else if (ea->msec_begin > eb->msec_begin)
    return 1;
  else
    return 0;
}

/**
 * shell_evolution_event_source_get_events:
 * @source: A #ShellEvolutionEventSource.
 * @msec_begin: Start date (milli-seconds since Epoch).
 * @msec_end: End date (milli-seconds since Epoch).
 *
 * Gets all events that occur between @msec_begin and @msec_end.
 *
 * Returns: (element-type ShellEvolutionEvent) (transfer full): List of events.
 */
GList *
shell_evolution_event_source_get_events  (ShellEvolutionEventSource *source,
                                          gint64                     msec_begin,
                                          gint64                     msec_end)
{
  GList *result;
  GDateTime *cur_date;
  GDateTime *begin_date_utc, *begin_date;
  GDateTime *end_date_utc, *end_date;

  g_return_val_if_fail (msec_begin <= msec_end, NULL);

  result = NULL;

  begin_date_utc = g_date_time_new_from_unix_utc (msec_begin / 1000);
  end_date_utc = g_date_time_new_from_unix_utc (msec_end / 1000);

  /* CalendarClient uses localtime rather than UTC */
  begin_date = g_date_time_to_local (begin_date_utc);
  end_date = g_date_time_to_local (end_date_utc);
  g_date_time_unref (begin_date_utc);
  g_date_time_unref (end_date_utc);

  cur_date = g_date_time_ref (begin_date);
  do
    {
      gint year, mon, day;
      GDateTime *next_date;

      g_date_time_get_ymd (cur_date, &year, &mon, &day);
      /* g_print ("y=%04d m=%02d d=%02d\n", year, mon, day); */

      /* Silently drop events not in range (see comment in
       * shell_evolution_event_source_request_range() above)
       */
      if (!(year == source->req_year && mon == source->req_mon))
        {
          /* g_print ("skipping day\n"); */
        }
      else
        {
          GSList *events;
          GSList *l;
          calendar_client_select_day (source->client, day);
          events = calendar_client_get_events (source->client, CALENDAR_EVENT_APPOINTMENT);
          /* g_print ("num_events: %d\n", g_slist_length (events)); */
          for (l = events; l; l = l->next)
            {
              CalendarAppointment *appointment = l->data;
              ShellEvolutionEvent *event;
              gint64 start_time;

              if (appointment->is_all_day)
                {
                  start_time = g_date_time_to_unix (cur_date) * G_GINT64_CONSTANT (1000);
                }
              else
                {
                  start_time = appointment->start_time * G_GINT64_CONSTANT (1000);
                }
              event = shell_evolution_event_new (appointment->summary,
                                                 appointment->is_all_day,
                                                 start_time);
              result = g_list_prepend (result, event);
            }
          g_slist_foreach (events, (GFunc) calendar_event_free, NULL);
          g_slist_free (events);
        }

      next_date = g_date_time_add_days (cur_date, 1);
      g_date_time_unref (cur_date);
      cur_date = next_date;
    }
  while (g_date_time_difference (end_date, cur_date) > 0);
  g_date_time_unref (begin_date);
  g_date_time_unref (end_date);

  result = g_list_sort (result, event_cmp);

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
                           gint64       msec_begin)
{
  ShellEvolutionEvent *event;
  event = g_new0 (ShellEvolutionEvent, 1);
  event->summary = g_strdup (summary);
  event->all_day = all_day;
  event->msec_begin = msec_begin;
  return event;
}
