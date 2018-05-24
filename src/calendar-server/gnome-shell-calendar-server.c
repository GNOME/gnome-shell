/*
 * Copyright (C) 2011 Red Hat, Inc.
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
 *
 * Author: David Zeuthen <davidz@redhat.com>
 *
 * Based on code from gnome-panel's clock-applet, file calendar-client.c, with Authors:
 *
 *     Mark McLoughlin  <mark@skynet.ie>
 *     William Jon McCann  <mccann@jhu.edu>
 *     Martin Grimme  <martin@pycage.de>
 *     Christian Kellner  <gicmo@xatom.net>
 *
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <gio/gio.h>

#define HANDLE_LIBICAL_MEMORY
#include <libecal/libecal.h>

#include "calendar-sources.h"
#include "reminder-watcher.h"

/* Set the environment variable CALENDAR_SERVER_DEBUG to show debug */
static void print_debug (const gchar *str, ...);

#define BUS_NAME "org.gnome.Shell.CalendarServer"

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gnome.Shell.CalendarServer'>"
  "    <method name='SetTimeRange'>"
  "      <arg type='x' name='since' direction='in'/>"
  "      <arg type='x' name='until' direction='in'/>"
  "      <arg type='b' name='force_reload' direction='in'/>"
  "    </method>"
  "    <signal name='EventsAdded'>" /* It's for both added and modified */
  "      <arg type='a(ssbxxa{sv})' name='events' direction='out'/>"
  "    </signal>"
  "    <signal name='EventsRemoved'>"
  "      <arg type='as' name='ids' direction='out'/>"
  "    </signal>"
  "    <signal name='ClientDisappeared'>"
  "      <arg type='s' name='source_uid' direction='out'/>"
  "    </signal>"
  "    <property name='Since' type='x' access='read'/>"
  "    <property name='Until' type='x' access='read'/>"
  "    <property name='HasCalendars' type='b' access='read'/>"
  "  </interface>"
  "</node>";
static GDBusNodeInfo *introspection_data = NULL;

struct _App;
typedef struct _App App;

static gboolean      opt_replace = FALSE;
static GOptionEntry  opt_entries[] = {
  {"replace", 0, 0, G_OPTION_ARG_NONE, &opt_replace, "Replace existing daemon", NULL},
  {NULL }
};
static App *_global_app = NULL;

/* ---------------------------------------------------------------------------------------------------- */

/* While the UID is usually enough to identify an event,
 * only the triple of (source,UID,RID) is fully unambiguous;
 * neither may contain '\n', so we can safely use it to
 * create a unique ID from the triple
 */
static gchar *
create_event_id (const gchar *source_uid,
                 const gchar *comp_uid,
                 const gchar *comp_rid)
{
  return g_strconcat (
    source_uid ? source_uid : "",
    "\n",
    comp_uid ? comp_uid : "",
    "\n",
    comp_rid ? comp_rid : "",
    NULL);
}

typedef struct
{
  ECalClient *client;
  GSList **pappointments; /* CalendarAppointment * */
} CollectAppointmentsData;

typedef struct
{
  gchar  *id;
  gchar  *summary;
  time_t  start_time;
  time_t  end_time;
  guint   is_all_day : 1;
} CalendarAppointment;

static time_t
get_time_from_property (icalcomponent         *ical,
                        icalproperty_kind      prop_kind,
                        struct icaltimetype (* get_prop_func) (const icalproperty *prop),
                        icaltimezone          *default_zone)
{
  icalproperty        *prop;
  struct icaltimetype  ical_time;
  icalparameter       *param;
  icaltimezone        *timezone = NULL;

  prop = icalcomponent_get_first_property (ical, prop_kind);
  if (!prop)
    return 0;

  ical_time = get_prop_func (prop);

  param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);
  if (param)
    timezone = icaltimezone_get_builtin_timezone_from_tzid (icalparameter_get_tzid (param));
  else if (icaltime_is_utc (ical_time))
    timezone = icaltimezone_get_utc_timezone ();
  else
    timezone = default_zone;

  return icaltime_as_timet_with_zone (ical_time, timezone);
}

static inline time_t
get_ical_start_time (icalcomponent *ical,
                     icaltimezone  *default_zone)
{
  return get_time_from_property (ical,
                                 ICAL_DTSTART_PROPERTY,
                                 icalproperty_get_dtstart,
                                 default_zone);
}

static inline time_t
get_ical_end_time (icalcomponent *ical,
                   icaltimezone  *default_zone)
{
  return get_time_from_property (ical,
                                 ICAL_DTEND_PROPERTY,
                                 icalproperty_get_dtend,
                                 default_zone);
}

static gboolean
get_ical_is_all_day (icalcomponent *ical,
                     time_t         start_time,
                     icaltimezone  *default_zone)
{
  icalproperty            *prop;
  struct tm               *start_tm;
  time_t                   end_time;
  struct icaldurationtype  duration;
  struct icaltimetype      start_icaltime;

  start_icaltime = icalcomponent_get_dtstart (ical);
  if (start_icaltime.is_date)
    return TRUE;

  start_tm = gmtime (&start_time);
  if (start_tm->tm_sec  != 0 ||
      start_tm->tm_min  != 0 ||
      start_tm->tm_hour != 0)
    return FALSE;

  if ((end_time = get_ical_end_time (ical, default_zone)))
    return (end_time - start_time) % 86400 == 0;

  prop = icalcomponent_get_first_property (ical, ICAL_DURATION_PROPERTY);
  if (!prop)
    return FALSE;

  duration = icalproperty_get_duration (prop);

  return icaldurationtype_as_int (duration) % 86400 == 0;
}

static inline time_t
get_ical_due_time (icalcomponent *ical,
                   icaltimezone  *default_zone)
{
  return get_time_from_property (ical,
                                 ICAL_DUE_PROPERTY,
                                 icalproperty_get_due,
                                 default_zone);
}

static inline time_t
get_ical_completed_time (icalcomponent *ical,
                         icaltimezone  *default_zone)
{
  return get_time_from_property (ical,
                                 ICAL_COMPLETED_PROPERTY,
                                 icalproperty_get_completed,
                                 default_zone);
}

static CalendarAppointment *
calendar_appointment_new (ECalClient    *cal,
                          ECalComponent *comp)
{
  CalendarAppointment *appt;
  icaltimezone *default_zone;
  icalcomponent *ical;
  ECalComponentId *id;

  default_zone = e_cal_client_get_default_timezone (cal);
  ical = e_cal_component_get_icalcomponent (comp);
  id = e_cal_component_get_id (comp);

  appt = g_new0 (CalendarAppointment, 1);

  appt->id          = create_event_id (e_source_get_uid (e_client_get_source (E_CLIENT (cal))),
                                       id ? id->uid : NULL,
                                       id ? id->rid : NULL);
  appt->summary     = g_strdup (icalcomponent_get_summary (ical));
  appt->start_time  = get_ical_start_time (ical, default_zone);
  appt->end_time    = get_ical_end_time (ical, default_zone);
  appt->is_all_day  = get_ical_is_all_day (ical,
                                           appt->start_time,
                                           default_zone);

  if (id)
    e_cal_component_free_id (id);

  return appt;
}

static void
calendar_appointment_free (gpointer ptr)
{
  CalendarAppointment *appt = ptr;

  if (appt)
    {
      g_free (appt->id);
      g_free (appt->summary);
      g_free (appt);
    }
}

static gboolean
generate_instances_cb (ECalComponent *comp,
                       time_t         start,
                       time_t         end,
                       gpointer       user_data)
{
  CollectAppointmentsData *data = user_data;
  CalendarAppointment *appointment;

  appointment             = calendar_appointment_new (data->client, comp);
  appointment->start_time = start;
  appointment->end_time   = end;

  *(data->pappointments) = g_slist_prepend (*(data->pappointments), appointment);

  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

struct _App
{
  GDBusConnection *connection;

  time_t since;
  time_t until;

  icaltimezone *zone;

  CalendarSources *sources;
  gulong client_appeared_signal_id;
  gulong client_disappeared_signal_id;

  EReminderWatcher *reminder_watcher;

  gchar *timezone_location;

  GSList *notify_appointments; /* CalendarAppointment *, for EventsAdded */
  GSList *notify_ids; /* gchar *, for EventsRemoved */
  guint events_added_timeout_id;
  guint events_removed_timeout_id;

  GSList *live_views;
};

static void
app_update_timezone (App *app)
{
  gchar *location;

  location = e_cal_system_timezone_get_location ();
  if (g_strcmp0 (location, app->timezone_location) != 0)
    {
      if (location == NULL)
        app->zone = icaltimezone_get_utc_timezone ();
      else
        app->zone = icaltimezone_get_builtin_timezone (location);
      g_free (app->timezone_location);
      app->timezone_location = location;
      print_debug ("Using timezone %s", app->timezone_location);
    }
  else
    {
      g_free (location);
    }
}

static gboolean
on_app_schedule_events_added_cb (gpointer user_data)
{
  App *app = user_data;
  GVariantBuilder builder, extras_builder;
  GSList *events, *link;

  if (g_source_is_destroyed (g_main_current_source ()))
    return FALSE;

  events = g_slist_reverse (app->notify_appointments);
  app->notify_appointments = NULL;
  app->events_added_timeout_id = 0;

  print_debug ("Emitting EventsAdded with %d events", g_slist_length (events));

  if (!events)
    return FALSE;

  /* The a{sv} is used as an escape hatch in case we want to provide more
   * information in the future without breaking ABI
   */
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssbxxa{sv})"));
  for (link = events; link; link = g_slist_next (link))
    {
      CalendarAppointment *appt = link->data;
      time_t start_time = appt->start_time;
      time_t end_time   = appt->end_time;

      if ((start_time >= app->since &&
           start_time < app->until) ||
          (start_time <= app->since &&
          (end_time - 1) > app->since))
        {
          g_variant_builder_init (&extras_builder, G_VARIANT_TYPE ("a{sv}"));
          g_variant_builder_add (&builder,
                                 "(ssbxxa{sv})",
                                 appt->id,
                                 appt->summary != NULL ? appt->summary : "",
                                 (gboolean) appt->is_all_day,
                                 (gint64) start_time,
                                 (gint64) end_time,
                                 extras_builder);
          g_variant_builder_clear (&extras_builder);
        }
    }

  g_dbus_connection_emit_signal (app->connection,
                                 NULL, /* destination_bus_name */
                                 "/org/gnome/Shell/CalendarServer",
                                 "org.gnome.Shell.CalendarServer",
                                 "EventsAdded",
                                 g_variant_new ("(a(ssbxxa{sv}))", &builder),
                                 NULL);
  g_variant_builder_clear (&builder);

  g_slist_free_full (events, calendar_appointment_free);

  return FALSE;
}

static void
app_schedule_events_added (App *app)
{
  print_debug ("Scheduling EventsAdded");
  if (app->events_added_timeout_id == 0)
    {
      app->events_added_timeout_id = g_timeout_add_seconds (2,
                                                            on_app_schedule_events_added_cb,
                                                            app);
      g_source_set_name_by_id (app->events_added_timeout_id, "[gnome-shell] on_app_schedule_events_added_cb");
    }
}

static gboolean
on_app_schedule_events_removed_cb (gpointer user_data)
{
  App *app = user_data;
  GVariantBuilder builder;
  GSList *ids, *link;

  if (g_source_is_destroyed (g_main_current_source ()))
    return FALSE;

  ids = app->notify_ids;
  app->notify_ids = NULL;
  app->events_removed_timeout_id = 0;

  print_debug ("Emitting EventsRemoved with %d ids", g_slist_length (ids));

  if (!ids)
    return FALSE;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
  for (link = ids; link; link = g_slist_next (link))
    {
      const gchar *id = link->data;

      g_variant_builder_add (&builder, "s", id);
    }

  g_dbus_connection_emit_signal (app->connection,
                                 NULL, /* destination_bus_name */
                                 "/org/gnome/Shell/CalendarServer",
                                 "org.gnome.Shell.CalendarServer",
                                 "EventsRemoved",
                                 g_variant_new ("(as)", &builder),
                                 NULL);
  g_variant_builder_clear (&builder);

  g_slist_free_full (ids, g_free);

  return FALSE;
}

static void
app_schedule_events_removed (App *app)
{
  print_debug ("Scheduling EventsRemoved");
  if (app->events_removed_timeout_id == 0)
    {
      app->events_removed_timeout_id = g_timeout_add_seconds (2,
                                                              on_app_schedule_events_removed_cb,
                                                              app);
      g_source_set_name_by_id (app->events_removed_timeout_id, "[gnome-shell] on_app_schedule_events_removed_cb");
    }
}

static void
app_process_added_modified_objects (App *app,
                                    ECalClientView *view,
                                    GSList *objects) /* icalcomponent * */
{
  ECalClient *cal_client;
  GSList *link;
  gboolean expand_recurrences;

  cal_client = e_cal_client_view_ref_client (view);
  expand_recurrences = e_cal_client_get_source_type (cal_client) == E_CAL_CLIENT_SOURCE_TYPE_EVENTS;

  for (link = objects; link; link = g_slist_next (link))
    {
      ECalComponent *comp;
      icalcomponent *icomp = link->data;

      if (!icomp || !icalcomponent_get_uid (icomp))
        continue;

      if (expand_recurrences &&
          !e_cal_util_component_is_instance (icomp) &&
          e_cal_util_component_has_recurrences (icomp))
        {
          CollectAppointmentsData data;

          data.client = cal_client;
          data.pappointments = &app->notify_appointments;

          e_cal_client_generate_instances_for_object_sync (cal_client, icomp, app->since, app->until,
                                                           generate_instances_cb, &data);
        }
      else
        {
          comp = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (icomp));
          if (!comp)
            continue;

	  app->notify_appointments = g_slist_prepend (app->notify_appointments,
                                                      calendar_appointment_new (cal_client, comp));
          g_object_unref (comp);
        }
    }

  g_clear_object (&cal_client);

  if (app->notify_appointments)
    app_schedule_events_added (app);
}

static void
on_objects_added (ECalClientView *view,
                  GSList         *objects,
                  gpointer        user_data)
{
  App *app = user_data;
  ECalClient *client;

  client = e_cal_client_view_ref_client (view);
  print_debug ("%s (%d) for calendar '%s'", G_STRFUNC, g_slist_length (objects), e_source_get_uid (e_client_get_source (E_CLIENT (client))));
  g_clear_object (&client);

  app_process_added_modified_objects (app, view, objects);
}

static void
on_objects_modified (ECalClientView *view,
                     GSList         *objects,
                     gpointer        user_data)
{
  App *app = user_data;
  ECalClient *client;

  client = e_cal_client_view_ref_client (view);
  print_debug ("%s (%d) for calendar '%s'", G_STRFUNC, g_slist_length (objects), e_source_get_uid (e_client_get_source (E_CLIENT (client))));
  g_clear_object (&client);

  app_process_added_modified_objects (app, view, objects);
}

static void
on_objects_removed (ECalClientView *view,
                    GSList         *uids,
                    gpointer        user_data)
{
  App *app = user_data;
  ECalClient *client;
  GSList *link;
  const gchar *source_uid;

  client = e_cal_client_view_ref_client (view);
  source_uid = e_source_get_uid (e_client_get_source (E_CLIENT (client)));

  print_debug ("%s (%d) for calendar '%s'", G_STRFUNC, g_slist_length (uids), source_uid);

  for (link = uids; link; link = g_slist_next (link))
    {
      ECalComponentId *id = link->data;

      if (!id)
        continue;

      app->notify_ids = g_slist_prepend (app->notify_ids, create_event_id (source_uid, id->uid, id->rid));
    }

  g_clear_object (&client);

  if (app->notify_ids)
    app_schedule_events_removed (app);
}

static gboolean
app_has_calendars (App *app)
{
  return app->live_views != NULL;
}

static ECalClientView *
app_start_view (App *app,
                ECalClient *cal_client)
{
  gchar *since_iso8601;
  gchar *until_iso8601;
  gchar *query;
  const char *tz_location;
  ECalClientView *view = NULL;
  GError *error = NULL;

  if (app->since <= 0 || app->since >= app->until)
    return NULL;

  if (!app->since || !app->until)
    {
      print_debug ("Skipping load of events, no time interval set yet");
      return NULL;
    }
  /* timezone could have changed */
  app_update_timezone (app);

  since_iso8601 = isodate_from_time_t (app->since);
  until_iso8601 = isodate_from_time_t (app->until);
  tz_location = icaltimezone_get_location (app->zone);

  print_debug ("Loading events since %s until %s for calendar '%s'",
               since_iso8601,
               until_iso8601,
               e_source_get_uid (e_client_get_source (E_CLIENT (cal_client))));

  query = g_strdup_printf ("occur-in-time-range? (make-time \"%s\") "
                           "(make-time \"%s\") \"%s\"",
                           since_iso8601,
                           until_iso8601,
                           tz_location);

  e_cal_client_set_default_timezone (cal_client, app->zone);

  if (!e_cal_client_get_view_sync (cal_client, query, &view, NULL /* cancellable */, &error))
    {
      g_warning ("Error setting up live-query '%s' on calendar: %s\n", query, error ? error->message : "Unknown error");
      g_clear_error (&error);
      view = NULL;
    }
  else
    {
      g_signal_connect (view,
                        "objects-added",
                        G_CALLBACK (on_objects_added),
                        app);
      g_signal_connect (view,
                        "objects-modified",
                        G_CALLBACK (on_objects_modified),
                        app);
      g_signal_connect (view,
                        "objects-removed",
                        G_CALLBACK (on_objects_removed),
                        app);
      e_cal_client_view_start (view, NULL);
    }

  g_free (since_iso8601);
  g_free (until_iso8601);
  g_free (query);

  return view;
}

static void
app_stop_view (App *app,
               ECalClientView *view)
{
      e_cal_client_view_stop (view, NULL);

      g_signal_handlers_disconnect_by_func (view, on_objects_added, app);
      g_signal_handlers_disconnect_by_func (view, on_objects_modified, app);
      g_signal_handlers_disconnect_by_func (view, on_objects_removed, app);
}

static void
app_update_views (App *app)
{
  GSList *link, *clients;

  for (link = app->live_views; link; link = g_slist_next (link))
    {
      app_stop_view (app, link->data);
    }

  g_slist_free_full (app->live_views, g_object_unref);
  app->live_views = NULL;

  clients = calendar_sources_ref_clients (app->sources);

  for (link = clients; link; link = g_slist_next (link))
    {
      ECalClient *cal_client = link->data;
      ECalClientView *view;

      if (!cal_client)
        continue;

      view = app_start_view (app, cal_client);
      if (view)
        app->live_views = g_slist_prepend (app->live_views, view);
    }

  g_slist_free_full (clients, g_object_unref);
}

static void
app_notify_has_calendars (App *app)
{
  GVariantBuilder dict_builder;

  g_variant_builder_init (&dict_builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&dict_builder, "{sv}", "HasCalendars",
                         g_variant_new_boolean (app_has_calendars (app)));

  g_dbus_connection_emit_signal (g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL),
                                 NULL,
                                 "/org/gnome/Shell/CalendarServer",
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 g_variant_new ("(sa{sv}as)",
                                                "org.gnome.Shell.CalendarServer",
                                                &dict_builder,
                                                NULL),
                                 NULL);
  g_variant_builder_clear (&dict_builder);
}

static void
on_client_appeared_cb (CalendarSources *sources,
                       ECalClient *client,
                       gpointer user_data)
{
  App *app = user_data;
  ECalClientView *view;
  GSList *link;
  const gchar *source_uid;

  source_uid = e_source_get_uid (e_client_get_source (E_CLIENT (client)));

  print_debug ("Client appeared '%s'", source_uid);

  for (link = app->live_views; link; link = g_slist_next (link))
    {
      ECalClientView *view = link->data;
      ECalClient *cal_client;
      ESource *source;

      cal_client = e_cal_client_view_ref_client (view);
      source = e_client_get_source (E_CLIENT (cal_client));

      if (g_strcmp0 (source_uid, e_source_get_uid (source)) == 0)
        {
          g_clear_object (&cal_client);
          return;
        }

      g_clear_object (&cal_client);
    }

  view = app_start_view (app, client);

  if (view)
    {
      app->live_views = g_slist_prepend (app->live_views, view);

      /* It's the first view, notify that it has calendars now */
      if (!g_slist_next (app->live_views))
        app_notify_has_calendars (app);
    }
}

static void
on_client_disappeared_cb (CalendarSources *sources,
                          const gchar *source_uid,
                          gpointer user_data)
{
  App *app = user_data;
  GSList *link;

  print_debug ("Client disappeared '%s'", source_uid);

  for (link = app->live_views; link; link = g_slist_next (link))
    {
      ECalClientView *view = link->data;
      ECalClient *cal_client;
      ESource *source;

      cal_client = e_cal_client_view_ref_client (view);
      source = e_client_get_source (E_CLIENT (cal_client));

      if (g_strcmp0 (source_uid, e_source_get_uid (source)) == 0)
        {
          g_clear_object (&cal_client);
          app_stop_view (app, view);
          app->live_views = g_slist_remove (app->live_views, view);
          g_object_unref (view);

          print_debug ("Emitting ClientDisappeared for '%s'", source_uid);

          g_dbus_connection_emit_signal (app->connection,
                                         NULL, /* destination_bus_name */
                                         "/org/gnome/Shell/CalendarServer",
                                         "org.gnome.Shell.CalendarServer",
                                         "ClientDisappeared",
                                         g_variant_new ("(s)", source_uid),
                                         NULL);

          /* It was the last view, notify that it doesn't have calendars now */
          if (!app->live_views)
	    app_notify_has_calendars (app);

          break;
        }

      g_clear_object (&cal_client);
    }
}

static App *
app_new (GApplication *application,
         GDBusConnection *connection)
{
  App *app;

  app = g_new0 (App, 1);
  app->connection = g_object_ref (connection);
  app->sources = calendar_sources_get ();
  app->reminder_watcher = reminder_watcher_new (application, calendar_sources_get_registry (app->sources));
  app->client_appeared_signal_id = g_signal_connect (app->sources,
                                                     "client-appeared",
                                                     G_CALLBACK (on_client_appeared_cb),
                                                     app);
  app->client_disappeared_signal_id = g_signal_connect (app->sources,
                                                        "client-disappeared",
                                                        G_CALLBACK (on_client_disappeared_cb),
                                                        app);

  app_update_timezone (app);

  return app;
}

static void
app_free (App *app)
{
  GSList *ll;

  if (app->events_added_timeout_id != 0)
    g_source_remove (app->events_added_timeout_id);

  if (app->events_removed_timeout_id != 0)
    g_source_remove (app->events_removed_timeout_id);

  for (ll = app->live_views; ll != NULL; ll = g_slist_next (ll))
    {
      ECalClientView *view = E_CAL_CLIENT_VIEW (ll->data);

      app_stop_view (app, view);
    }

  g_signal_handler_disconnect (app->sources,
                               app->client_appeared_signal_id);
  g_signal_handler_disconnect (app->sources,
                               app->client_disappeared_signal_id);

  g_free (app->timezone_location);

  g_slist_free_full (app->live_views, g_object_unref);
  g_slist_free_full (app->notify_appointments, calendar_appointment_free);
  g_slist_free_full (app->notify_ids, g_free);

  g_object_unref (app->connection);
  g_object_unref (app->reminder_watcher);
  g_object_unref (app->sources);

  g_free (app);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  App *app = user_data;

  if (g_strcmp0 (method_name, "SetTimeRange") == 0)
    {
      gint64 since;
      gint64 until;
      gboolean force_reload = FALSE;
      gboolean window_changed = FALSE;

      g_variant_get (parameters,
                     "(xxb)",
                     &since,
                     &until,
                     &force_reload);

      if (until < since)
        {
          g_dbus_method_invocation_return_dbus_error (invocation,
                                                      "org.gnome.Shell.CalendarServer.Error.Failed",
                                                      "until cannot be before since");
          goto out;
        }

      print_debug ("Handling SetTimeRange (since=%" G_GINT64_FORMAT ", until=%" G_GINT64_FORMAT ", force_reload=%s)",
                   since,
                   until,
                   force_reload ? "true" : "false");

      if (app->until != until || app->since != since)
        {
          GVariantBuilder *builder;
          GVariantBuilder *invalidated_builder;

          app->until = until;
          app->since = since;
          window_changed = TRUE;

          builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
          invalidated_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
          g_variant_builder_add (builder, "{sv}",
                                 "Until", g_variant_new_int64 (app->until));
          g_variant_builder_add (builder, "{sv}",
                                 "Since", g_variant_new_int64 (app->since));
          g_dbus_connection_emit_signal (app->connection,
                                         NULL, /* destination_bus_name */
                                         "/org/gnome/Shell/CalendarServer",
                                         "org.freedesktop.DBus.Properties",
                                         "PropertiesChanged",
                                         g_variant_new ("(sa{sv}as)",
                                                        "org.gnome.Shell.CalendarServer",
                                                        builder,
                                                        invalidated_builder),
                                         NULL); /* GError** */

          g_variant_builder_unref (builder);
          g_variant_builder_unref (invalidated_builder);
        }

      g_dbus_method_invocation_return_value (invocation, NULL);

      if (window_changed || force_reload)
        app_update_views (app);
    }
  else
    {
      g_assert_not_reached ();
    }

 out:
  ;
}

static GVariant *
handle_get_property (GDBusConnection *connection,
                     const gchar     *sender,
                     const gchar     *object_path,
                     const gchar     *interface_name,
                     const gchar     *property_name,
                     GError         **error,
                     gpointer         user_data)
{
  App *app = user_data;
  GVariant *ret;

  ret = NULL;
  if (g_strcmp0 (property_name, "Since") == 0)
    {
      ret = g_variant_new_int64 (app->since);
    }
  else if (g_strcmp0 (property_name, "Until") == 0)
    {
      ret = g_variant_new_int64 (app->until);
    }
  else if (g_strcmp0 (property_name, "HasCalendars") == 0)
    {
      ret = g_variant_new_boolean (app_has_calendars (app));
    }
  else
    {
      g_assert_not_reached ();
    }
  return ret;
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  handle_get_property,
  NULL  /* handle_set_property */
};

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  GApplication *application = user_data;
  guint registration_id;
  GError *error = NULL;

  _global_app = app_new (application, connection);
  registration_id = g_dbus_connection_register_object (connection,
                                                       "/org/gnome/Shell/CalendarServer",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       _global_app,
                                                       NULL,  /* user_data_free_func */
                                                       &error);
  if (registration_id == 0)
    {
      g_printerr ("Error exporting object: %s (%s %d)\n",
                  error->message,
                  g_quark_to_string (error->domain),
                  error->code);
      g_error_free (error);
      g_application_quit (application);
      return;
    }

  print_debug ("Connected to the session bus");
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  GApplication *application = user_data;

  g_print ("gnome-shell-calendar-server[%d]: Lost (or failed to acquire) the name " BUS_NAME " - exiting\n",
           (gint) getpid ());
  g_application_quit (application);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  print_debug ("Acquired the name " BUS_NAME);
}

static gboolean
stdin_channel_io_func (GIOChannel *source,
                       GIOCondition condition,
                       gpointer data)
{
  GApplication *application = data;

  if (condition & G_IO_HUP)
    {
      g_debug ("gnome-shell-calendar-server[%d]: Got HUP on stdin - exiting\n",
               (gint) getpid ());
      g_application_quit (application);
    }
  else
    {
      g_warning ("Unhandled condition %d on GIOChannel for stdin", condition);
    }
  return FALSE; /* remove source */
}

int
main (int    argc,
      char **argv)
{
  GError *error;
  GOptionContext *opt_context;
  gint ret;
  guint name_owner_id;
  GIOChannel *stdin_channel;
  GApplication *application;

  ret = 1;
  opt_context = NULL;
  name_owner_id = 0;
  stdin_channel = NULL;
  error = NULL;

  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  opt_context = g_option_context_new ("gnome-shell calendar server");
  g_option_context_add_main_entries (opt_context, opt_entries, NULL);
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s\n", error->message);
      g_error_free (error);
      goto out;
    }

  application = g_application_new (BUS_NAME, G_APPLICATION_NON_UNIQUE);

  g_signal_connect (application, "activate",
                    G_CALLBACK (g_application_hold), NULL);

  stdin_channel = g_io_channel_unix_new (STDIN_FILENO);
  g_io_add_watch_full (stdin_channel,
                       G_PRIORITY_DEFAULT,
                       G_IO_HUP,
                       stdin_channel_io_func,
                       application,
                       (GDestroyNotify) NULL);

  name_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  BUS_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                   (opt_replace ? G_BUS_NAME_OWNER_FLAGS_REPLACE : 0),
                                  on_bus_acquired,
                                  on_name_acquired,
                                  on_name_lost,
                                  application,
                                  (GDestroyNotify) NULL);

  if (g_application_register (application, NULL, &error))
    {
      print_debug ("Registered application");

      ret = g_application_run (application, argc, argv);

      print_debug ("Quit application");
    }
   else
    {
       g_printerr ("Failed to register application: %s\n", error->message);
       g_clear_error (&error);
    }

 out:
  if (stdin_channel != NULL)
    g_io_channel_unref (stdin_channel);
  if (_global_app != NULL)
    app_free (_global_app);
  if (name_owner_id != 0)
    g_bus_unown_name (name_owner_id);
  if (opt_context != NULL)
    g_option_context_free (opt_context);
  g_clear_object (&application);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
print_debug (const gchar *format, ...)
{
  gchar *s;
  va_list ap;
  gchar timebuf[64];
  GTimeVal now;
  time_t now_t;
  struct tm broken_down;
  static volatile gsize once_init_value = 0;
  static gboolean show_debug = FALSE;
  static guint pid = 0;

  if (g_once_init_enter (&once_init_value))
    {
      show_debug = (g_getenv ("CALENDAR_SERVER_DEBUG") != NULL);
      pid = getpid ();
      g_once_init_leave (&once_init_value, 1);
    }

  if (!show_debug)
    goto out;

  g_get_current_time (&now);
  now_t = now.tv_sec;
  localtime_r (&now_t, &broken_down);
  strftime (timebuf, sizeof timebuf, "%H:%M:%S", &broken_down);

  va_start (ap, format);
  s = g_strdup_vprintf (format, ap);
  va_end (ap);

  g_print ("gnome-shell-calendar-server[%d]: %s.%03d: %s\n", pid, timebuf, (gint) (now.tv_usec / 1000), s);
  g_free (s);
 out:
  ;
}
