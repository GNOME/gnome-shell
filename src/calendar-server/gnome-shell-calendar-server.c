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
#define EDS_DISABLE_DEPRECATED
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#include <libecal/libecal.h>
G_GNUC_END_IGNORE_DEPRECATIONS

#include "calendar-sources.h"

#define BUS_NAME "org.gnome.Shell.CalendarServer"

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gnome.Shell.CalendarServer'>"
  "    <method name='SetTimeRange'>"
  "      <arg type='x' name='since' direction='in'/>"
  "      <arg type='x' name='until' direction='in'/>"
  "      <arg type='b' name='force_reload' direction='in'/>"
  "    </method>"
  "    <signal name='EventsAddedOrUpdated'>"
  "      <arg type='a(ssxxa{sv})' name='events' direction='out'/>"
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
} CalendarAppointment;

static gboolean
get_time_from_property (ECalClient            *cal,
                        ICalComponent         *icomp,
                        ICalPropertyKind       prop_kind,
                        ICalTime * (* get_prop_func) (ICalProperty *prop),
                        ICalTimezone          *default_zone,
                        ICalTime              **out_itt,
                        ICalTimezone          **out_timezone)
{
  ICalProperty  *prop;
  ICalTime      *itt;
  ICalTimezone  *timezone = NULL;

  prop = i_cal_component_get_first_property (icomp, prop_kind);
  if (!prop)
    return FALSE;

  itt = get_prop_func (prop);

  if (i_cal_time_is_utc (itt))
    timezone = i_cal_timezone_get_utc_timezone ();
  else
   {
      ICalParameter *param;

      param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);
      if (param && !e_cal_client_get_timezone_sync (cal, i_cal_parameter_get_tzid (param), &timezone, NULL, NULL))
        print_debug ("Failed to get timezone '%s'\n", i_cal_parameter_get_tzid (param));

      g_clear_object (&param);
   }

  if (timezone == NULL)
    timezone = default_zone;

  i_cal_time_set_timezone (itt, timezone);

  g_clear_object (&prop);

  *out_itt = itt;
  *out_timezone = timezone;

  return TRUE;
}

static inline time_t
get_ical_start_time (ECalClient    *cal,
                     ICalComponent *icomp,
                     ICalTimezone  *default_zone)
{
  ICalTime     *itt;
  ICalTimezone *timezone;
  time_t        retval;

  if (!get_time_from_property (cal,
                               icomp,
                               I_CAL_DTSTART_PROPERTY,
                               i_cal_property_get_dtstart,
                               default_zone,
                               &itt,
                               &timezone))
    {
      return 0;
    }

  retval = i_cal_time_as_timet_with_zone (itt, timezone);

  g_clear_object (&itt);

  return retval;
}

static inline time_t
get_ical_end_time (ECalClient    *cal,
                   ICalComponent *icomp,
                   ICalTimezone  *default_zone)
{
  ICalTime     *itt;
  ICalTimezone *timezone;
  time_t        retval;

  if (!get_time_from_property (cal,
                               icomp,
                               I_CAL_DTEND_PROPERTY,
                               i_cal_property_get_dtend,
                               default_zone,
                               &itt,
                               &timezone))
    {
      if (!get_time_from_property (cal,
                                   icomp,
                                   I_CAL_DTSTART_PROPERTY,
                                   i_cal_property_get_dtstart,
                                   default_zone,
                                   &itt,
                                   &timezone))
        {
          return 0;
        }

      if (i_cal_time_is_date (itt))
        i_cal_time_adjust (itt, 1, 0, 0, 0);
    }

  retval = i_cal_time_as_timet_with_zone (itt, timezone);

  g_clear_object (&itt);

  return retval;
}

static CalendarAppointment *
calendar_appointment_new (ECalClient    *cal,
                          ECalComponent *comp)
{
  CalendarAppointment *appt;
  ICalTimezone *default_zone;
  ICalComponent *ical;
  ECalComponentId *id;

  default_zone = e_cal_client_get_default_timezone (cal);
  ical = e_cal_component_get_icalcomponent (comp);
  id = e_cal_component_get_id (comp);

  appt = g_new0 (CalendarAppointment, 1);

  appt->id          = create_event_id (e_source_get_uid (e_client_get_source (E_CLIENT (cal))),
                                       id ? e_cal_component_id_get_uid (id) : NULL,
                                       id ? e_cal_component_id_get_rid (id) : NULL);
  appt->summary     = g_strdup (i_cal_component_get_summary (ical));
  appt->start_time  = get_ical_start_time (cal, ical, default_zone);
  appt->end_time    = get_ical_end_time (cal, ical, default_zone);

  e_cal_component_id_free (id);

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

static time_t
timet_from_ical_time (ICalTime     *time,
                      ICalTimezone *default_zone)
{
  ICalTimezone *timezone = NULL;

  timezone = i_cal_time_get_timezone (time);
  if (timezone == NULL)
    timezone = default_zone;
  return i_cal_time_as_timet_with_zone (time, timezone);
}

static gboolean
generate_instances_cb (ICalComponent *icomp,
                       ICalTime *instance_start,
                       ICalTime *instance_end,
                       gpointer user_data,
                       GCancellable *cancellable,
                       GError **error)
{
  CollectAppointmentsData *data = user_data;
  CalendarAppointment *appointment;
  ECalComponent *comp;
  ICalTimezone *default_zone;

  default_zone = e_cal_client_get_default_timezone (data->client);
  comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));

  appointment             = calendar_appointment_new (data->client, comp);
  appointment->start_time = timet_from_ical_time (instance_start, default_zone);
  appointment->end_time   = timet_from_ical_time (instance_end, default_zone);

  *(data->pappointments) = g_slist_prepend (*(data->pappointments), appointment);

  g_clear_object (&comp);

  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

struct _App
{
  GDBusConnection *connection;

  time_t since;
  time_t until;

  ICalTimezone *zone;

  CalendarSources *sources;
  gulong client_appeared_signal_id;
  gulong client_disappeared_signal_id;

  gchar *timezone_location;

  GSList *notify_appointments; /* CalendarAppointment *, for EventsAdded */
  GSList *notify_ids; /* gchar *, for EventsRemoved */

  GSList *live_views;
};

static void
app_update_timezone (App *app)
{
  g_autofree char *location = NULL;

  location = e_cal_system_timezone_get_location ();
  if (g_strcmp0 (location, app->timezone_location) != 0)
    {
      if (location == NULL)
        app->zone = i_cal_timezone_get_utc_timezone ();
      else
        app->zone = i_cal_timezone_get_builtin_timezone (location);
      g_free (app->timezone_location);
      app->timezone_location = g_steal_pointer (&location);
      print_debug ("Using timezone %s", app->timezone_location);
    }
}

static void
app_notify_events_added (App *app)
{
  GVariantBuilder builder, extras_builder;
  GSList *events, *link;

  events = g_slist_reverse (app->notify_appointments);
  app->notify_appointments = NULL;

  print_debug ("Emitting EventsAddedOrUpdated with %d events", g_slist_length (events));

  if (!events)
    return;

  /* The a{sv} is used as an escape hatch in case we want to provide more
   * information in the future without breaking ABI
   */
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssxxa{sv})"));
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
                                 "(ssxxa{sv})",
                                 appt->id,
                                 appt->summary != NULL ? appt->summary : "",
                                 (gint64) start_time,
                                 (gint64) end_time,
                                 &extras_builder);
        }
    }

  g_dbus_connection_emit_signal (app->connection,
                                 NULL, /* destination_bus_name */
                                 "/org/gnome/Shell/CalendarServer",
                                 "org.gnome.Shell.CalendarServer",
                                 "EventsAddedOrUpdated",
                                 g_variant_new ("(a(ssxxa{sv}))", &builder),
                                 NULL);

  g_variant_builder_clear (&builder);

  g_slist_free_full (events, calendar_appointment_free);
}

static void
app_notify_events_removed (App *app)
{
  GVariantBuilder builder;
  GSList *ids, *link;

  ids = app->notify_ids;
  app->notify_ids = NULL;

  print_debug ("Emitting EventsRemoved with %d ids", g_slist_length (ids));

  if (!ids)
    return;

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

  return;
}

static void
app_process_added_modified_objects (App *app,
                                    ECalClientView *view,
                                    GSList *objects) /* ICalComponent * */
{
  ECalClient *cal_client;
  g_autoptr(GHashTable) covered_uids = NULL;
  GSList *link;
  gboolean expand_recurrences;

  cal_client = e_cal_client_view_ref_client (view);
  covered_uids = g_hash_table_new (g_str_hash, g_str_equal);
  expand_recurrences = e_cal_client_get_source_type (cal_client) == E_CAL_CLIENT_SOURCE_TYPE_EVENTS;

  for (link = objects; link; link = g_slist_next (link))
    {
      ECalComponent *comp;
      ICalComponent *icomp = link->data;
      const gchar *uid;
      gboolean fallback = FALSE;

      if (!icomp)
        continue;

      uid = i_cal_component_get_uid (icomp);
      if (!uid || g_hash_table_contains (covered_uids, uid))
        continue;

      g_hash_table_add (covered_uids, (gpointer) uid);

      if (expand_recurrences &&
          !e_cal_util_component_is_instance (icomp) &&
          e_cal_util_component_has_recurrences (icomp))
        {
          CollectAppointmentsData data;

          data.client = cal_client;
          data.pappointments = &app->notify_appointments;

          e_cal_client_generate_instances_for_object_sync (cal_client, icomp, app->since, app->until, NULL,
                                                           generate_instances_cb, &data);
        }
      else if (expand_recurrences &&
               e_cal_util_component_is_instance (icomp))
        {
          ICalComponent *main_comp = NULL;

          /* Always pass whole series of the recurring events, because
           * the calendar removes events with the same UID first. */
          if (e_cal_client_get_object_sync (cal_client, uid, NULL, &main_comp, NULL, NULL))
            {
              CollectAppointmentsData data;

              data.client = cal_client;
              data.pappointments = &app->notify_appointments;

              e_cal_client_generate_instances_for_object_sync (cal_client, main_comp, app->since, app->until, NULL,
                                                               generate_instances_cb, &data);

              g_clear_object (&main_comp);
            }
          else
            {
               fallback = TRUE;
            }
        }
      else
        {
	  fallback = TRUE;
        }

      if (fallback)
        {
          comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
          if (!comp)
            continue;

          app->notify_appointments = g_slist_prepend (app->notify_appointments,
                                                      calendar_appointment_new (cal_client, comp));
          g_object_unref (comp);
        }
    }

  g_clear_object (&cal_client);

  if (app->notify_appointments)
    app_notify_events_added (app);
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

      app->notify_ids = g_slist_prepend (app->notify_ids,
                                         create_event_id (source_uid,
                                         e_cal_component_id_get_uid (id),
                                         e_cal_component_id_get_rid (id)));
    }

  g_clear_object (&client);

  if (app->notify_ids)
    app_notify_events_removed (app);
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
  g_autofree char *since_iso8601 = NULL;
  g_autofree char *until_iso8601 = NULL;
  g_autofree char *query = NULL;
  const gchar *tz_location;
  ECalClientView *view = NULL;
  g_autoptr (GError) error = NULL;

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
  tz_location = i_cal_timezone_get_location (app->zone);

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
app_notify_has_calendars (App *app)
{
  GVariantBuilder dict_builder;

  g_variant_builder_init (&dict_builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&dict_builder, "{sv}", "HasCalendars",
                         g_variant_new_boolean (app_has_calendars (app)));

  g_dbus_connection_emit_signal (app->connection,
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
app_update_views (App *app)
{
  GSList *link, *clients;
  gboolean had_views, has_views;

  had_views = app->live_views != NULL;

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

  has_views = app->live_views != NULL;

  if (has_views != had_views)
    app_notify_has_calendars (app);

  g_slist_free_full (clients, g_object_unref);
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
app_new (GDBusConnection *connection)
{
  App *app;

  app = g_new0 (App, 1);
  app->connection = g_object_ref (connection);
  app->sources = calendar_sources_get ();
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
  GMainLoop *main_loop = user_data;
  guint registration_id;
  g_autoptr (GError) error = NULL;

  _global_app = app_new (connection);

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
      g_main_loop_quit (main_loop);
      return;
    }

  print_debug ("Connected to the session bus");
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  GMainLoop *main_loop = user_data;

  g_print ("gnome-shell-calendar-server[%d]: Lost (or failed to acquire) the name " BUS_NAME " - exiting\n",
           (gint) getpid ());
  g_main_loop_quit (main_loop);
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
  GMainLoop *main_loop = data;

  if (condition & G_IO_HUP)
    {
      g_debug ("gnome-shell-calendar-server[%d]: Got HUP on stdin - exiting\n",
               (gint) getpid ());
      g_main_loop_quit (main_loop);
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
  g_autoptr (GError) error = NULL;
  GOptionContext *opt_context;
  GMainLoop *main_loop;
  gint ret;
  guint name_owner_id;
  GIOChannel *stdin_channel;

  ret = 1;
  opt_context = NULL;
  name_owner_id = 0;
  stdin_channel = NULL;

  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  opt_context = g_option_context_new ("gnome-shell calendar server");
  g_option_context_add_main_entries (opt_context, opt_entries, NULL);
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s\n", error->message);
      goto out;
    }

  main_loop = g_main_loop_new (NULL, FALSE);

  stdin_channel = g_io_channel_unix_new (STDIN_FILENO);
  g_io_add_watch_full (stdin_channel,
                       G_PRIORITY_DEFAULT,
                       G_IO_HUP,
                       stdin_channel_io_func,
                       g_main_loop_ref (main_loop),
                       (GDestroyNotify) g_main_loop_unref);

  name_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  BUS_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                   (opt_replace ? G_BUS_NAME_OWNER_FLAGS_REPLACE : 0),
                                  on_bus_acquired,
                                  on_name_acquired,
                                  on_name_lost,
                                  g_main_loop_ref (main_loop),
                                  (GDestroyNotify) g_main_loop_unref);

  g_main_loop_run (main_loop);

  g_main_loop_unref (main_loop);

  ret = 0;

 out:
  if (stdin_channel != NULL)
    g_io_channel_unref (stdin_channel);
  if (_global_app != NULL)
    app_free (_global_app);
  if (name_owner_id != 0)
    g_bus_unown_name (name_owner_id);
  if (opt_context != NULL)
    g_option_context_free (opt_context);

  return ret;
}
