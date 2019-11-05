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

/* Set the environment variable CALENDAR_SERVER_DEBUG to show debug */
static void print_debug (const gchar *str, ...);

#define BUS_NAME "org.gnome.Shell.CalendarServer"

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gnome.Shell.CalendarServer'>"
  "    <method name='GetEvents'>"
  "      <arg type='x' name='since' direction='in'/>"
  "      <arg type='x' name='until' direction='in'/>"
  "      <arg type='b' name='force_reload' direction='in'/>"
  "      <arg type='a(sssbxxa{sv})' name='events' direction='out'/>"
  "    </method>"
  "    <signal name='Changed'/>"
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

typedef struct
{
  char *rid;
  time_t start_time;
  time_t end_time;
} CalendarOccurrence;

typedef struct
{
  char   *uid;
  char   *source_id;
  char   *backend_name;
  char   *summary;
  char   *description;
  char   *color_string;
  time_t  start_time;
  time_t  end_time;
  guint   is_all_day : 1;

  /* Only used internally */
  GSList *occurrences;
} CalendarAppointment;

typedef struct
{
  ECalClient *client;
  GHashTable *appointments;
} CollectAppointmentsData;

static time_t
get_time_from_property (ECalClient            *cal,
                        ICalComponent         *icomp,
                        ICalPropertyKind       prop_kind,
                        ICalTime * (* get_prop_func) (ICalProperty *prop),
                        ICalTimezone          *default_zone)
{
  ICalProperty  *prop;
  ICalTime      *itt;
  ICalParameter *param;
  ICalTimezone  *timezone = NULL;
  time_t         retval;

  prop = i_cal_component_get_first_property (icomp, prop_kind);
  if (!prop)
    return 0;

  itt = get_prop_func (prop);

  param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);
  if (param)
    timezone = e_timezone_cache_get_timezone (E_TIMEZONE_CACHE (cal), i_cal_parameter_get_tzid (param));
  else if (i_cal_time_is_utc (itt))
    timezone = i_cal_timezone_get_utc_timezone ();
  else
    timezone = default_zone;

  i_cal_time_set_timezone (itt, timezone);

  retval = i_cal_time_as_timet_with_zone (itt, timezone);

  g_clear_object (&param);
  g_clear_object (&prop);
  g_clear_object (&itt);

  return retval;
}

static char *
get_ical_uid (ICalComponent *icomp)
{
  return g_strdup (i_cal_component_get_uid (icomp));
}

static char *
get_ical_summary (ICalComponent *icomp)
{
  ICalProperty *prop;
  char         *retval;

  prop = i_cal_component_get_first_property (icomp, I_CAL_SUMMARY_PROPERTY);
  if (!prop)
    return NULL;

  retval = g_strdup (i_cal_property_get_summary (prop));

  g_object_unref (prop);

  return retval;
}

static char *
get_ical_description (ICalComponent *icomp)
{
  ICalProperty *prop;
  char         *retval;

  prop = i_cal_component_get_first_property (icomp, I_CAL_DESCRIPTION_PROPERTY);
  if (!prop)
    return NULL;

  retval = g_strdup (i_cal_property_get_description (prop));

  g_object_unref (prop);

  return retval;
}

static inline time_t
get_ical_start_time (ECalClient    *cal,
                     ICalComponent *icomp,
                     ICalTimezone  *default_zone)
{
  return get_time_from_property (cal,
                                 icomp,
                                 I_CAL_DTSTART_PROPERTY,
                                 i_cal_property_get_dtstart,
                                 default_zone);
}

static inline time_t
get_ical_end_time (ECalClient    *cal,
                   ICalComponent *icomp,
                   ICalTimezone  *default_zone)
{
  return get_time_from_property (cal,
                                 icomp,
                                 I_CAL_DTEND_PROPERTY,
                                 i_cal_property_get_dtend,
                                 default_zone);
}

static gboolean
get_ical_is_all_day (ECalClient    *cal,
                     ICalComponent *icomp,
                     time_t         start_time,
                     ICalTimezone  *default_zone)
{
  ICalProperty *prop;
  ICalDuration *duration;
  ICalTime     *dtstart;
  struct tm    *start_tm;
  time_t        end_time;
  gboolean      retval;

  dtstart = i_cal_component_get_dtstart (icomp);
  if (dtstart && i_cal_time_is_date (dtstart))
    {
      g_clear_object (&dtstart);
      return TRUE;
    }

  g_clear_object (&dtstart);

  start_tm = gmtime (&start_time);
  if (start_tm->tm_sec  != 0 ||
      start_tm->tm_min  != 0 ||
      start_tm->tm_hour != 0)
    return FALSE;

  if ((end_time = get_ical_end_time (cal, icomp, default_zone)))
    return (end_time - start_time) % 86400 == 0;

  prop = i_cal_component_get_first_property (icomp, I_CAL_DURATION_PROPERTY);
  if (!prop)
    return FALSE;

  duration = i_cal_property_get_duration (prop);

  retval = duration && (i_cal_duration_as_int (duration) % 86400) == 0;

  g_clear_object (&duration);
  g_clear_object (&prop);

  return retval;
}

static inline time_t
get_ical_due_time (ECalClient    *cal,
                   ICalComponent *icomp,
                   ICalTimezone  *default_zone)
{
  return get_time_from_property (cal,
                                 icomp,
                                 I_CAL_DUE_PROPERTY,
                                 i_cal_property_get_due,
                                 default_zone);
}

static inline time_t
get_ical_completed_time (ECalClient    *cal,
                         ICalComponent *icomp,
                         ICalTimezone  *default_zone)
{
  return get_time_from_property (cal,
                                 icomp,
                                 I_CAL_COMPLETED_PROPERTY,
                                 i_cal_property_get_completed,
                                 default_zone);
}

static char *
get_source_color (ECalClient *esource)
{
  ESource *source;
  ECalClientSourceType source_type;
  ESourceSelectable *extension;
  const gchar *extension_name;

  g_return_val_if_fail (E_IS_CAL_CLIENT (esource), NULL);

  source = e_client_get_source (E_CLIENT (esource));
  source_type = e_cal_client_get_source_type (esource);

  switch (source_type)
    {
      case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
        extension_name = E_SOURCE_EXTENSION_CALENDAR;
        break;
      case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
        extension_name = E_SOURCE_EXTENSION_TASK_LIST;
        break;
      default:
        g_return_val_if_reached (NULL);
    }

  extension = e_source_get_extension (source, extension_name);

  return e_source_selectable_dup_color (extension);
}

static gchar *
get_source_backend_name (ECalClient *esource)
{
  ESource *source;
  ECalClientSourceType source_type;
  ESourceBackend *extension;
  const gchar *extension_name;

  g_return_val_if_fail (E_IS_CAL_CLIENT (esource), NULL);

  source = e_client_get_source (E_CLIENT (esource));
  source_type = e_cal_client_get_source_type (esource);

  switch (source_type)
    {
      case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
        extension_name = E_SOURCE_EXTENSION_CALENDAR;
        break;
      case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
        extension_name = E_SOURCE_EXTENSION_TASK_LIST;
        break;
      default:
        g_return_val_if_reached (NULL);
    }

  extension = e_source_get_extension (source, extension_name);

  return e_source_backend_dup_backend_name (extension);
}

static inline int
null_safe_strcmp (const char *a,
                  const char *b)
{
  return (!a && !b) ? 0 : (a && !b) || (!a && b) ? 1 : strcmp (a, b);
}

static inline gboolean
calendar_appointment_equal (CalendarAppointment *a,
                            CalendarAppointment *b)
{
  GSList *la, *lb;

  if (g_slist_length (a->occurrences) != g_slist_length (b->occurrences))
      return FALSE;

  for (la = a->occurrences, lb = b->occurrences; la && lb; la = la->next, lb = lb->next)
    {
      CalendarOccurrence *oa = la->data;
      CalendarOccurrence *ob = lb->data;

      if (oa->start_time != ob->start_time ||
          oa->end_time   != ob->end_time ||
          null_safe_strcmp (oa->rid, ob->rid) != 0)
        return FALSE;
    }

  return
    null_safe_strcmp (a->uid,          b->uid)          == 0 &&
    null_safe_strcmp (a->source_id,    b->source_id)    == 0 &&
    null_safe_strcmp (a->backend_name, b->backend_name) == 0 &&
    null_safe_strcmp (a->summary,      b->summary)      == 0 &&
    null_safe_strcmp (a->description,  b->description)  == 0 &&
    null_safe_strcmp (a->color_string, b->color_string) == 0 &&
    a->start_time == b->start_time                         &&
    a->end_time   == b->end_time                           &&
    a->is_all_day == b->is_all_day;
}

static void
calendar_appointment_free (CalendarAppointment *appointment)
{
  GSList *l;

  for (l = appointment->occurrences; l; l = l->next)
    g_free (((CalendarOccurrence *)l->data)->rid);
  g_slist_free_full (appointment->occurrences, g_free);
  appointment->occurrences = NULL;

  g_free (appointment->uid);
  appointment->uid = NULL;

  g_free (appointment->source_id);
  appointment->source_id = NULL;

  g_free (appointment->backend_name);
  appointment->backend_name = NULL;

  g_free (appointment->summary);
  appointment->summary = NULL;

  g_free (appointment->description);
  appointment->description = NULL;

  g_free (appointment->color_string);
  appointment->color_string = NULL;

  appointment->start_time = 0;
  appointment->is_all_day = FALSE;
}

static void
calendar_appointment_init (CalendarAppointment  *appointment,
                           ICalComponent        *icomp,
                           ECalClient           *cal)
{
  ICalTimezone *default_zone;
  const char *source_id;

  source_id = e_source_get_uid (e_client_get_source (E_CLIENT (cal)));
  default_zone = e_cal_client_get_default_timezone (cal);

  appointment->uid          = get_ical_uid (icomp);
  appointment->source_id    = g_strdup (source_id);
  appointment->backend_name = get_source_backend_name (cal);
  appointment->summary      = get_ical_summary (icomp);
  appointment->description  = get_ical_description (icomp);
  appointment->color_string = get_source_color (cal);
  appointment->start_time   = get_ical_start_time (cal, icomp, default_zone);
  appointment->end_time     = get_ical_end_time (cal, icomp, default_zone);
  appointment->is_all_day   = get_ical_is_all_day (cal,
                                                   icomp,
                                                   appointment->start_time,
                                                   default_zone);
}

static CalendarAppointment *
calendar_appointment_new (ICalComponent        *icomp,
                          ECalClient           *cal)
{
  CalendarAppointment *appointment;

  appointment = g_new0 (CalendarAppointment, 1);

  calendar_appointment_init (appointment, icomp, cal);
  return appointment;
}

static gboolean
generate_instances_cb (ICalComponent *icomp,
                       ICalTime *instance_start,
                       ICalTime *instance_end,
                       gpointer user_data,
                       GCancellable *cancellable,
                       GError **error)
{
  ECalClient *cal = ((CollectAppointmentsData *)user_data)->client;
  GHashTable *appointments = ((CollectAppointmentsData *)user_data)->appointments;
  CalendarAppointment *appointment;
  CalendarOccurrence *occurrence;
  const gchar *uid;

  uid = i_cal_component_get_uid (icomp);
  appointment = g_hash_table_lookup (appointments, uid);

  if (appointment == NULL)
    {
      appointment = calendar_appointment_new (icomp, cal);
      g_hash_table_insert (appointments, g_strdup (uid), appointment);
    }

  occurrence             = g_new0 (CalendarOccurrence, 1);
  occurrence->start_time = i_cal_time_as_timet_with_zone (instance_start, i_cal_time_get_timezone (instance_start));
  occurrence->end_time   = i_cal_time_as_timet_with_zone (instance_end, i_cal_time_get_timezone (instance_end));
  occurrence->rid        = e_cal_util_component_get_recurid_as_string (icomp);

  appointment->occurrences = g_slist_append (appointment->occurrences, occurrence);

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
  gulong sources_signal_id;

  /* hash from uid to CalendarAppointment objects */
  GHashTable *appointments;

  gchar *timezone_location;

  guint changed_timeout_id;

  gboolean cache_invalid;

  GList *live_views;
};

static void
app_update_timezone (App *app)
{
  gchar *location;

  location = e_cal_system_timezone_get_location ();
  if (g_strcmp0 (location, app->timezone_location) != 0)
    {
      if (location == NULL)
        app->zone = i_cal_timezone_get_utc_timezone ();
      else
        app->zone = i_cal_timezone_get_builtin_timezone (location);
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
on_app_schedule_changed_cb (gpointer user_data)
{
  App *app = user_data;
  print_debug ("Emitting changed");
  g_dbus_connection_emit_signal (app->connection,
                                 NULL, /* destination_bus_name */
                                 "/org/gnome/Shell/CalendarServer",
                                 "org.gnome.Shell.CalendarServer",
                                 "Changed",
                                 NULL, /* no params */
                                 NULL);
  app->changed_timeout_id = 0;
  return FALSE;
}

static void
app_schedule_changed (App *app)
{
  print_debug ("Scheduling changed");
  if (app->changed_timeout_id == 0)
    {
      app->changed_timeout_id = g_timeout_add (2000,
                                               on_app_schedule_changed_cb,
                                               app);
      g_source_set_name_by_id (app->changed_timeout_id, "[gnome-shell] on_app_schedule_changed_cb");
    }
}

static void
invalidate_cache (App *app)
{
  app->cache_invalid = TRUE;
}

static void
on_objects_added (ECalClientView *view,
                  GSList         *objects,
                  gpointer        user_data)
{
  App *app = user_data;
  GSList *l;

  print_debug ("%s for calendar", G_STRFUNC);

  for (l = objects; l != NULL; l = l->next)
    {
      ICalComponent *icomp = l->data;
      const char *uid;

      uid = i_cal_component_get_uid (icomp);

      if (g_hash_table_lookup (app->appointments, uid) == NULL)
        {
          /* new appointment we don't know about => changed signal */
          invalidate_cache (app);
          app_schedule_changed (app);
        }
    }
}

static void
on_objects_modified (ECalClientView *view,
                     GSList         *objects,
                     gpointer        user_data)
{
  App *app = user_data;
  print_debug ("%s for calendar", G_STRFUNC);
  invalidate_cache (app);
  app_schedule_changed (app);
}

static void
on_objects_removed (ECalClientView *view,
                    GSList         *uids,
                    gpointer        user_data)
{
  App *app = user_data;
  print_debug ("%s for calendar", G_STRFUNC);
  invalidate_cache (app);
  app_schedule_changed (app);
}

static void
app_load_events (App *app)
{
  GList *clients;
  GList *l;
  GList *ll;
  gchar *since_iso8601;
  gchar *until_iso8601;
  gchar *query;
  const char *tz_location;

  /* out with the old */
  g_hash_table_remove_all (app->appointments);
  /* nuke existing views */
  for (ll = app->live_views; ll != NULL; ll = ll->next)
    {
      ECalClientView *view = E_CAL_CLIENT_VIEW (ll->data);
      g_signal_handlers_disconnect_by_func (view, on_objects_added, app);
      g_signal_handlers_disconnect_by_func (view, on_objects_modified, app);
      g_signal_handlers_disconnect_by_func (view, on_objects_removed, app);
      e_cal_client_view_stop (view, NULL);
      g_object_unref (view);
    }
  g_list_free (app->live_views);
  app->live_views = NULL;

  if (!app->since || !app->until)
    {
      print_debug ("Skipping load of events, no time interval set yet");
      return;
    }
  /* timezone could have changed */
  app_update_timezone (app);

  since_iso8601 = isodate_from_time_t (app->since);
  until_iso8601 = isodate_from_time_t (app->until);
  tz_location = i_cal_timezone_get_location (app->zone);

  print_debug ("Loading events since %s until %s",
               since_iso8601,
               until_iso8601);

  query = g_strdup_printf ("occur-in-time-range? (make-time \"%s\") "
                           "(make-time \"%s\") \"%s\"",
                           since_iso8601,
                           until_iso8601,
                           tz_location);

  clients = calendar_sources_get_appointment_clients (app->sources);
  for (l = clients; l != NULL; l = l->next)
    {
      ECalClient *cal = E_CAL_CLIENT (l->data);
      GError *error;
      ECalClientView *view;
      CollectAppointmentsData data;

      e_cal_client_set_default_timezone (cal, app->zone);

      data.client = cal;
      data.appointments = app->appointments;
      e_cal_client_generate_instances_sync (cal,
                                            app->since,
                                            app->until,
                                            NULL,
                                            generate_instances_cb,
                                            &data);

      error = NULL;
      if (!e_cal_client_get_view_sync (cal,
				       query,
				       &view,
				       NULL, /* cancellable */
				       &error))
        {
          g_warning ("Error setting up live-query on calendar: %s\n", error->message);
          g_error_free (error);
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
          app->live_views = g_list_prepend (app->live_views, view);
        }
    }
  g_list_free (clients);
  g_free (since_iso8601);
  g_free (until_iso8601);
  g_free (query);
  app->cache_invalid = FALSE;
}

static gboolean
app_has_calendars (App *app)
{
  return calendar_sources_has_sources (app->sources);
}

static void
on_appointment_sources_changed (CalendarSources *sources,
                                gpointer         user_data)
{
  App *app = user_data;

  print_debug ("Sources changed\n");
  app_load_events (app);

  /* Notify the HasCalendars property */
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
  }
}

static App *
app_new (GDBusConnection *connection)
{
  App *app;

  app = g_new0 (App, 1);
  app->connection = g_object_ref (connection);
  app->sources = calendar_sources_get ();
  app->sources_signal_id = g_signal_connect (app->sources,
                                             "appointment-sources-changed",
                                             G_CALLBACK (on_appointment_sources_changed),
                                             app);

  app->appointments = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             (GDestroyNotify) calendar_appointment_free);

  app_update_timezone (app);

  return app;
}

static void
app_free (App *app)
{
  GList *ll;
  for (ll = app->live_views; ll != NULL; ll = ll->next)
    {
      ECalClientView *view = E_CAL_CLIENT_VIEW (ll->data);
      g_signal_handlers_disconnect_by_func (view, on_objects_added, app);
      g_signal_handlers_disconnect_by_func (view, on_objects_modified, app);
      g_signal_handlers_disconnect_by_func (view, on_objects_removed, app);
      e_cal_client_view_stop (view, NULL);
      g_object_unref (view);
    }
  g_list_free (app->live_views);

  g_free (app->timezone_location);

  g_hash_table_unref (app->appointments);

  g_object_unref (app->connection);
  g_signal_handler_disconnect (app->sources,
                               app->sources_signal_id);
  g_object_unref (app->sources);

  if (app->changed_timeout_id != 0)
    g_source_remove (app->changed_timeout_id);

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

  if (g_strcmp0 (method_name, "GetEvents") == 0)
    {
      GVariantBuilder builder;
      GHashTableIter hash_iter;
      CalendarAppointment *a;
      gint64 since;
      gint64 until;
      gboolean force_reload;
      gboolean window_changed;

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

      print_debug ("Handling GetEvents (since=%" G_GINT64_FORMAT ", until=%" G_GINT64_FORMAT ", force_reload=%s)",
                   since,
                   until,
                   force_reload ? "true" : "false");

      window_changed = FALSE;
      if (!(app->until == until && app->since == since))
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
        }

      /* reload events if necessary */
      if (window_changed || force_reload || app->cache_invalid)
        {
          app_load_events (app);
        }

      /* The a{sv} is used as an escape hatch in case we want to provide more
       * information in the future without breaking ABI
       */
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sssbxxa{sv})"));
      g_hash_table_iter_init (&hash_iter, app->appointments);
      while (g_hash_table_iter_next (&hash_iter, NULL, (gpointer) &a))
        {
          GVariantBuilder extras_builder;
          GSList *l;

          for (l = a->occurrences; l; l = l->next)
            {
              CalendarOccurrence *o = l->data;
              time_t start_time = o->start_time;
              time_t end_time   = o->end_time;

              if ((start_time >= app->since &&
                   start_time < app->until) ||
                  (start_time <= app->since &&
                  (end_time - 1) > app->since))
                {
                  /* While the UID is usually enough to identify an event,
                   * only the triple of (source,UID,RID) is fully unambiguous;
                   * neither may contain '\n', so we can safely use it to
                   * create a unique ID from the triple
                   */
                  char *id = g_strdup_printf ("%s\n%s\n%s",
                                              a->source_id,
                                              a->uid,
                                              o->rid ? o->rid : "");

                  g_variant_builder_init (&extras_builder, G_VARIANT_TYPE ("a{sv}"));
                  g_variant_builder_add (&builder,
                                         "(sssbxxa{sv})",
                                         id,
                                         a->summary != NULL ? a->summary : "",
                                         a->description != NULL ? a->description : "",
                                         (gboolean) a->is_all_day,
                                         (gint64) start_time,
                                         (gint64) end_time,
                                         extras_builder);
                  g_free (id);
                }
            }
        }
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(a(sssbxxa{sv}))", &builder));
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
  GError *error;
  guint registration_id;

  _global_app = app_new (connection);

  error = NULL;
  registration_id = g_dbus_connection_register_object (connection,
                                                       "/org/gnome/Shell/CalendarServer",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       _global_app,
                                                       NULL,  /* user_data_free_func */
                                                       &error);
  if (registration_id == 0)
    {
      g_printerr ("Error exporting object: %s (%s %d)",
                  error->message,
                  g_quark_to_string (error->domain),
                  error->code);
      g_error_free (error);
      _exit (1);
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
  GError *error;
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
  error = NULL;
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s", error->message);
      g_error_free (error);
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

/* ---------------------------------------------------------------------------------------------------- */

static void __attribute__((format(printf, 1, 0)))
print_debug (const gchar *format, ...)
{
  g_autofree char *s = NULL;
  g_autofree char *timestamp = NULL;
  va_list ap;
  g_autoptr (GDateTime) now = NULL;
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

  now = g_date_time_new_now_local ();
  timestamp = g_date_time_format (now, "%H:%M:%S");

  va_start (ap, format);
  s = g_strdup_vprintf (format, ap);
  va_end (ap);

  g_print ("gnome-shell-calendar-server[%d]: %s.%03d: %s\n",
           pid, timestamp, g_date_time_get_microsecond (now), s);
 out:
  ;
}
