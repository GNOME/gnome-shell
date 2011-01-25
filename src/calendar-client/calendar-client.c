/*
 * Copyright (C) 2004 Free Software Foundation, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *     Mark McLoughlin  <mark@skynet.ie>
 *     William Jon McCann  <mccann@jhu.edu>
 *     Martin Grimme  <martin@pycage.de>
 *     Christian Kellner  <gicmo@xatom.net>
 */

#include <config.h>

#include "calendar-client.h"

#include <libintl.h>
#include <string.h>
#define HANDLE_LIBICAL_MEMORY
#include <libecal/e-cal.h>
#include <libecal/e-cal-time-util.h>
#include <libecal/e-cal-recur.h>

#include "calendar-sources.h"

#undef CALENDAR_ENABLE_DEBUG
#include "calendar-debug.h"

#define CALENDAR_CONFIG_PREFIX   "/apps/evolution/calendar"
#define CALENDAR_CONFIG_TIMEZONE CALENDAR_CONFIG_PREFIX "/display/timezone"

#ifndef _
#define _(x) gettext(x)
#endif

#ifndef N_
#define N_(x) x
#endif

#define CALENDAR_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CALENDAR_TYPE_CLIENT, CalendarClientPrivate))

typedef struct _CalendarClientQuery  CalendarClientQuery;
typedef struct _CalendarClientSource CalendarClientSource;

struct _CalendarClientQuery
{
  ECalView   *view;
  GHashTable *events;
};

struct _CalendarClientSource
{
  CalendarClient      *client;
  ECal                *source;

  CalendarClientQuery  completed_query;
  CalendarClientQuery  in_progress_query;

  guint                changed_signal_id;

  guint                query_completed : 1;
  guint                query_in_progress : 1;
};

struct _CalendarClientPrivate
{
  CalendarSources     *calendar_sources;

  GSList              *appointment_sources;
  GSList              *task_sources;

  icaltimezone        *zone;

  guint                zone_listener;
  GConfClient         *gconf_client;

  guint                day;
  guint                month;
  guint                year;
};

static void calendar_client_class_init   (CalendarClientClass *klass);
static void calendar_client_init         (CalendarClient      *client);
static void calendar_client_finalize     (GObject             *object);
static void calendar_client_set_property (GObject             *object,
					  guint                prop_id,
					  const GValue        *value,
					  GParamSpec          *pspec);
static void calendar_client_get_property (GObject             *object,
					  guint                prop_id,
					  GValue              *value,
					  GParamSpec          *pspec);

static GSList *calendar_client_update_sources_list         (CalendarClient       *client,
							    GSList               *sources,
							    GSList               *esources,
							    guint                 changed_signal_id);
static void    calendar_client_appointment_sources_changed (CalendarClient       *client);
static void    calendar_client_task_sources_changed        (CalendarClient       *client);

static void calendar_client_stop_query  (CalendarClient       *client,
					 CalendarClientSource *source,
					 CalendarClientQuery  *query);
static void calendar_client_start_query (CalendarClient       *client,
					 CalendarClientSource *source,
					 const char           *query);

static void calendar_client_source_finalize (CalendarClientSource *source);
static void calendar_client_query_finalize  (CalendarClientQuery  *query);

static void
calendar_client_update_appointments (CalendarClient *client);
static void
calendar_client_update_tasks (CalendarClient *client);

enum
{
  PROP_O,
  PROP_DAY,
  PROP_MONTH,
  PROP_YEAR
};

enum
{
  APPOINTMENTS_CHANGED,
  TASKS_CHANGED,
  LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
static guint         signals [LAST_SIGNAL] = { 0, };

GType
calendar_client_get_type (void)
{
  static GType client_type = 0;
  
  if (!client_type)
    {
      static const GTypeInfo client_info =
      {
	sizeof (CalendarClientClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) calendar_client_class_init,
	NULL,           /* class_finalize */
	NULL,		/* class_data */
	sizeof (CalendarClient),
	0,		/* n_preallocs */
	(GInstanceInitFunc) calendar_client_init,
      };
      
      client_type = g_type_register_static (G_TYPE_OBJECT,
					    "CalendarClient",
					    &client_info, 0);
    }
  
  return client_type;
}

static void
calendar_client_class_init (CalendarClientClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize     = calendar_client_finalize;
  gobject_class->set_property = calendar_client_set_property;
  gobject_class->get_property = calendar_client_get_property;

  g_type_class_add_private (klass, sizeof (CalendarClientPrivate));

  g_object_class_install_property (gobject_class,
				   PROP_DAY,
				   g_param_spec_uint ("day",
						      "Day",
						      "The currently monitored day between 1 and 31 (0 denotes unset)",
						      0, G_MAXUINT, 0,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_MONTH,
				   g_param_spec_uint ("month",
						      "Month",
						      "The currently monitored month between 0 and 11",
						      0, G_MAXUINT, 0,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_YEAR,
				   g_param_spec_uint ("year",
						      "Year",
						      "The currently monitored year",
						      0, G_MAXUINT, 0,
						      G_PARAM_READWRITE));

  signals [APPOINTMENTS_CHANGED] =
    g_signal_new ("appointments-changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (CalendarClientClass, tasks_changed),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);

  signals [TASKS_CHANGED] =
    g_signal_new ("tasks-changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (CalendarClientClass, tasks_changed),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);
}

/* Timezone code adapted from evolution/calendar/gui/calendar-config.c */
/* The current timezone, e.g. "Europe/London". It may be NULL, in which case
   you should assume UTC. */
static gchar *
calendar_client_config_get_timezone (GConfClient *gconf_client)
{
  char *location;

  location = gconf_client_get_string (gconf_client,
                                      CALENDAR_CONFIG_TIMEZONE,
                                      NULL);

  return location;
}

static icaltimezone *
calendar_client_config_get_icaltimezone (GConfClient *gconf_client)
{
  char         *location;
  icaltimezone *zone = NULL;
	
  location = calendar_client_config_get_timezone (gconf_client);
  if (!location)
    return icaltimezone_get_utc_timezone ();

  zone = icaltimezone_get_builtin_timezone (location);
  g_free (location);
	
  return zone;
}

static void
calendar_client_set_timezone (CalendarClient *client) 
{
  GSList *l;
  GSList *esources;

  client->priv->zone = calendar_client_config_get_icaltimezone (client->priv->gconf_client);

  esources = calendar_sources_get_appointment_sources (client->priv->calendar_sources);
  for (l = esources; l; l = l->next) {
    ECal *source = l->data;
			
    e_cal_set_default_timezone (source, client->priv->zone, NULL);
  }
}

static void
calendar_client_timezone_changed_cb (GConfClient    *gconf_client,
                                     guint           id,
                                     GConfEntry     *entry,
                                     CalendarClient *client)
{
  calendar_client_set_timezone (client);
}

static void
cal_opened_cb (ECal                 *ecal,
               ECalendarStatus       status,
               CalendarClientSource *cl_source)
{
  ECalSourceType  s_type;
  CalendarClient *client = cl_source->client;

  s_type = e_cal_get_source_type (ecal);

  if (status == E_CALENDAR_STATUS_BUSY &&
      e_cal_get_load_state (ecal) == E_CAL_LOAD_NOT_LOADED)
    {
      e_cal_open_async (ecal, FALSE);
      return;
    }
  
  g_signal_handlers_disconnect_by_func (ecal, cal_opened_cb, cl_source);

  if (status != E_CALENDAR_STATUS_OK)
    {
      if (s_type == E_CAL_SOURCE_TYPE_EVENT)
        client->priv->appointment_sources = g_slist_remove (client->priv->appointment_sources,
                                                            cl_source);
      else
        client->priv->task_sources = g_slist_remove (client->priv->task_sources,
                                                     cl_source);

      calendar_client_source_finalize (cl_source);
      g_free (cl_source);

      return;
    }

  if (s_type == E_CAL_SOURCE_TYPE_EVENT)
    calendar_client_update_appointments (client);
  else
    calendar_client_update_tasks (client);
}

static void
load_calendars (CalendarClient    *client,
                CalendarEventType  type) 
{
  GSList *l, *clients;

  switch (type)
    {
      case CALENDAR_EVENT_APPOINTMENT:
        clients = client->priv->appointment_sources;
        break;
      case CALENDAR_EVENT_TASK:
        clients = client->priv->task_sources;
        break;
      default:
        g_assert_not_reached ();
    }

  for (l = clients; l != NULL; l = l->next)
    {
      ECal *ecal;	
      CalendarClientSource *cl_source = l->data;

      ecal = cl_source->source;

      if (e_cal_get_load_state (ecal) == E_CAL_LOAD_LOADED)
        continue;

      g_signal_connect (G_OBJECT (ecal), "cal_opened",
                        G_CALLBACK (cal_opened_cb), cl_source);
      e_cal_open_async (ecal, TRUE);
    }
}

static void
calendar_client_init (CalendarClient *client)
{
  GSList *esources;

  client->priv = CALENDAR_CLIENT_GET_PRIVATE (client);

  client->priv->calendar_sources = calendar_sources_get ();
  client->priv->gconf_client = gconf_client_get_default ();

  esources = calendar_sources_get_appointment_sources (client->priv->calendar_sources);
  client->priv->appointment_sources =
    calendar_client_update_sources_list (client, NULL, esources, signals [APPOINTMENTS_CHANGED]);

  esources = calendar_sources_get_task_sources (client->priv->calendar_sources);
  client->priv->task_sources =
    calendar_client_update_sources_list (client, NULL, esources, signals [TASKS_CHANGED]);
 
  /* set the timezone before loading the clients */ 
  calendar_client_set_timezone (client);
  load_calendars (client, CALENDAR_EVENT_APPOINTMENT);
  load_calendars (client, CALENDAR_EVENT_TASK);

  g_signal_connect_swapped (client->priv->calendar_sources,
			    "appointment-sources-changed",
			    G_CALLBACK (calendar_client_appointment_sources_changed),
			    client);
  g_signal_connect_swapped (client->priv->calendar_sources,
			    "task-sources-changed",
			    G_CALLBACK (calendar_client_task_sources_changed),
			    client);

  gconf_client_add_dir (client->priv->gconf_client,
			CALENDAR_CONFIG_PREFIX,
			GCONF_CLIENT_PRELOAD_NONE,
			NULL);

  client->priv->zone_listener = gconf_client_notify_add (client->priv->gconf_client,
                                                         CALENDAR_CONFIG_TIMEZONE,
                                                         (GConfClientNotifyFunc) calendar_client_timezone_changed_cb,
                                                         client, NULL, NULL);

  client->priv->day   = -1;
  client->priv->month = -1;
  client->priv->year  = -1;
}

static void
calendar_client_finalize (GObject *object)
{
  CalendarClient *client = CALENDAR_CLIENT (object);
  GSList         *l;

  if (client->priv->zone_listener)
    {
      gconf_client_notify_remove (client->priv->gconf_client,
                                  client->priv->zone_listener);
      client->priv->zone_listener = 0;
    }

  gconf_client_remove_dir (client->priv->gconf_client,
                           CALENDAR_CONFIG_PREFIX,
                           NULL);

  if (client->priv->gconf_client)
    g_object_unref (client->priv->gconf_client);
  client->priv->gconf_client = NULL;

  for (l = client->priv->appointment_sources; l; l = l->next)
    {
      calendar_client_source_finalize (l->data);
      g_free (l->data);
    }
  g_slist_free (client->priv->appointment_sources);
  client->priv->appointment_sources = NULL;

  for (l = client->priv->task_sources; l; l = l->next)
    {
      calendar_client_source_finalize (l->data);
      g_free (l->data);
    }
  g_slist_free (client->priv->task_sources);
  client->priv->task_sources = NULL;

  if (client->priv->calendar_sources)
    g_object_unref (client->priv->calendar_sources);
  client->priv->calendar_sources = NULL;

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
calendar_client_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  CalendarClient *client = CALENDAR_CLIENT (object);

  switch (prop_id)
    {
    case PROP_DAY:
      calendar_client_select_day (client, g_value_get_uint (value));
      break;
    case PROP_MONTH:
      calendar_client_select_month (client,
				    g_value_get_uint (value),
				    client->priv->year);
      break;
    case PROP_YEAR:
      calendar_client_select_month (client,
				    client->priv->month,
				    g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
calendar_client_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  CalendarClient *client = CALENDAR_CLIENT (object);

  switch (prop_id)
    {
    case PROP_DAY:
      g_value_set_uint (value, client->priv->day);
      break;
    case PROP_MONTH:
      g_value_set_uint (value, client->priv->month);
      break;
    case PROP_YEAR:
      g_value_set_uint (value, client->priv->year);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

CalendarClient *
calendar_client_new (void)
{
  return g_object_new (CALENDAR_TYPE_CLIENT, NULL);
}

/* @day and @month can happily be out of range as
 * mktime() will normalize them correctly. From mktime(3):
 *
 * "If structure members are outside their legal interval,
 *  they will be normalized (so that, e.g., 40 October is
 *  changed into 9 November)."
 *
 * "What?", you say, "Something useful in libc?"
 */
static inline time_t
make_time_for_day_begin (int day,
			 int month,
			 int year)
{
  struct tm localtime_tm = { 0, };

  localtime_tm.tm_mday  = day;
  localtime_tm.tm_mon   = month;
  localtime_tm.tm_year  = year - 1900;
  localtime_tm.tm_isdst = -1;

  return mktime (&localtime_tm);
}

static inline char *
make_isodate_for_day_begin (int day,
			    int month,
			    int year)
{
  time_t utctime;

  utctime = make_time_for_day_begin (day, month, year);

  return utctime != -1 ? isodate_from_time_t (utctime) : NULL;
}

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

static char *
get_ical_uid (icalcomponent *ical)
{
  return g_strdup (icalcomponent_get_uid (ical));
}

static char *
get_ical_rid (icalcomponent *ical)
{
  icalproperty        *prop;
  struct icaltimetype  ical_time;
  
  prop = icalcomponent_get_first_property (ical, ICAL_RECURRENCEID_PROPERTY);
  if (!prop)
    return NULL;

  ical_time = icalproperty_get_recurrenceid (prop);

  return icaltime_is_valid_time (ical_time) && !icaltime_is_null_time (ical_time) ? 
    g_strdup (icaltime_as_ical_string (ical_time)) : NULL;
}

static char *
get_ical_summary (icalcomponent *ical)
{
  icalproperty *prop;

  prop = icalcomponent_get_first_property (ical, ICAL_SUMMARY_PROPERTY);
  if (!prop)
    return NULL;

  return g_strdup (icalproperty_get_summary (prop));
}

static char *
get_ical_description (icalcomponent *ical)
{
  icalproperty *prop;

  prop = icalcomponent_get_first_property (ical, ICAL_DESCRIPTION_PROPERTY);
  if (!prop)
    return NULL;

  return g_strdup (icalproperty_get_description (prop));
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

static guint
get_ical_percent_complete (icalcomponent *ical)
{
  icalproperty *prop;
  icalproperty_status status;
  int           percent_complete;

  status = icalcomponent_get_status (ical);
  if (status == ICAL_STATUS_COMPLETED)
    return 100;

  prop = icalcomponent_get_first_property (ical, ICAL_COMPLETED_PROPERTY);
  if (prop)
    return 100;

  prop = icalcomponent_get_first_property (ical, ICAL_PERCENTCOMPLETE_PROPERTY);
  if (!prop)
    return 0;

  percent_complete = icalproperty_get_percentcomplete (prop);

  return CLAMP (percent_complete, 0, 100);
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

static int
get_ical_priority (icalcomponent *ical)
{
  icalproperty *prop;

  prop = icalcomponent_get_first_property (ical, ICAL_PRIORITY_PROPERTY);
  if (!prop)
    return -1;

  return icalproperty_get_priority (prop);
}

static char *
get_source_color (ECal *esource)
{
  ESource *source;

  g_return_val_if_fail (E_IS_CAL (esource), NULL);

  source = e_cal_get_source (esource);

  return g_strdup (e_source_peek_color_spec (source));
}

static gchar *
get_source_uri (ECal *esource)
{
    ESource *source;
    gchar   *string;
    gchar  **list;

    g_return_val_if_fail (E_IS_CAL (esource), NULL);

    source = e_cal_get_source (esource);
    string = g_strdup (e_source_get_uri (source));
    if (string) {
        list = g_strsplit (string, ":", 2);
        g_free (string);

        if (list[0]) {
            string = g_strdup (list[0]);
            g_strfreev (list);
            return string;
        }
	g_strfreev (list);
    }
    return NULL;
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
	  oa->end_time   != ob->end_time)
	return FALSE;
    }

  return
    null_safe_strcmp (a->uid,          b->uid)          == 0 &&
    null_safe_strcmp (a->uri,          b->uri)          == 0 &&
    null_safe_strcmp (a->summary,      b->summary)      == 0 &&
    null_safe_strcmp (a->description,  b->description)  == 0 &&
    null_safe_strcmp (a->color_string, b->color_string) == 0 &&
    a->start_time == b->start_time                         &&
    a->end_time   == b->end_time                           &&
    a->is_all_day == b->is_all_day;
}

static void
calendar_appointment_copy (CalendarAppointment *appointment,
			   CalendarAppointment *appointment_copy)
{
  GSList *l;

  g_assert (appointment != NULL);
  g_assert (appointment_copy != NULL);

  appointment_copy->occurrences = g_slist_copy (appointment->occurrences);
  for (l = appointment_copy->occurrences; l; l = l->next)
    {
      CalendarOccurrence *occurrence = l->data;
      CalendarOccurrence *occurrence_copy;

      occurrence_copy             = g_new0 (CalendarOccurrence, 1);
      occurrence_copy->start_time = occurrence->start_time;
      occurrence_copy->end_time   = occurrence->end_time;

      l->data = occurrence_copy;
    }

  appointment_copy->uid          = g_strdup (appointment->uid);
  appointment_copy->uri          = g_strdup (appointment->uri);
  appointment_copy->summary      = g_strdup (appointment->summary);
  appointment_copy->description  = g_strdup (appointment->description);
  appointment_copy->color_string = g_strdup (appointment->color_string);
  appointment_copy->start_time   = appointment->start_time;
  appointment_copy->end_time     = appointment->end_time;
  appointment_copy->is_all_day   = appointment->is_all_day;
}

static void
calendar_appointment_finalize (CalendarAppointment *appointment)
{
  GSList *l;

  for (l = appointment->occurrences; l; l = l->next)
    g_free (l->data);
  g_slist_free (appointment->occurrences);
  appointment->occurrences = NULL;

  g_free (appointment->uid);
  appointment->uid = NULL;

  g_free (appointment->rid);
  appointment->rid = NULL;

  g_free (appointment->uri);
  appointment->uri = NULL;

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
			   icalcomponent        *ical,
                           CalendarClientSource *source,
                           icaltimezone         *default_zone)
{
  appointment->uid          = get_ical_uid (ical);
  appointment->rid          = get_ical_rid (ical);
  appointment->uri          = get_source_uri (source->source);
  appointment->summary      = get_ical_summary (ical);
  appointment->description  = get_ical_description (ical);
  appointment->color_string = get_source_color (source->source);
  appointment->start_time   = get_ical_start_time (ical, default_zone);
  appointment->end_time     = get_ical_end_time (ical, default_zone);
  appointment->is_all_day   = get_ical_is_all_day (ical,
                                                   appointment->start_time,
                                                   default_zone);
}

static icaltimezone *
resolve_timezone_id (const char *tzid,
		     ECal       *source)
{
  icaltimezone *retval;

  retval = icaltimezone_get_builtin_timezone_from_tzid (tzid);
  if (!retval)
    {
      e_cal_get_timezone (source, tzid, &retval, NULL);
    }

  return retval;
}

static gboolean
calendar_appointment_collect_occurrence (ECalComponent  *component,
					 time_t          occurrence_start,
					 time_t          occurrence_end,
					 gpointer        data)
{
  CalendarOccurrence *occurrence;
  GSList **collect_loc = data;

  occurrence             = g_new0 (CalendarOccurrence, 1);
  occurrence->start_time = occurrence_start;
  occurrence->end_time   = occurrence_end;

  *collect_loc = g_slist_prepend (*collect_loc, occurrence);

  return TRUE;
}

static void
calendar_appointment_generate_ocurrences (CalendarAppointment *appointment,
					  icalcomponent       *ical,
					  ECal                *source,
					  time_t               start,
					  time_t               end,
                                          icaltimezone        *default_zone)
{
  ECalComponent *ecal;

  g_assert (appointment->occurrences == NULL);

  ecal = e_cal_component_new ();
  e_cal_component_set_icalcomponent (ecal,
				     icalcomponent_new_clone (ical));

  e_cal_recur_generate_instances (ecal,
				  start,
				  end,
				  calendar_appointment_collect_occurrence,
				  &appointment->occurrences,
				  (ECalRecurResolveTimezoneFn) resolve_timezone_id,
				  source,
				  default_zone);

  g_object_unref (ecal);

  appointment->occurrences = g_slist_reverse (appointment->occurrences);
}

static inline gboolean
calendar_task_equal (CalendarTask *a,
		     CalendarTask *b)
{
  return
    null_safe_strcmp (a->uid,          b->uid)          == 0 &&
    null_safe_strcmp (a->summary,      b->summary)      == 0 &&
    null_safe_strcmp (a->description,  b->description)  == 0 &&
    null_safe_strcmp (a->color_string, b->color_string) == 0 &&
    a->start_time       == b->start_time                   &&
    a->due_time         == b->due_time                     &&
    a->percent_complete == b->percent_complete             &&
    a->completed_time   == b->completed_time               &&
    a->priority         == b->priority;
}

static void
calendar_task_copy (CalendarTask *task,
		    CalendarTask *task_copy)
{
  g_assert (task != NULL);
  g_assert (task_copy != NULL);

  task_copy->uid              = g_strdup (task->uid);
  task_copy->summary          = g_strdup (task->summary);
  task_copy->description      = g_strdup (task->description);
  task_copy->color_string     = g_strdup (task->color_string);
  task_copy->start_time       = task->start_time;
  task_copy->due_time         = task->due_time;
  task_copy->percent_complete = task->percent_complete;
  task_copy->completed_time   = task->completed_time;
  task_copy->priority         = task->priority;
}

static void
calendar_task_finalize (CalendarTask *task)
{
  g_free (task->uid);
  task->uid = NULL;

  g_free (task->summary);
  task->summary = NULL;

  g_free (task->description);
  task->description = NULL;

  g_free (task->color_string);
  task->color_string = NULL;

  task->percent_complete = 0;
}

static void
calendar_task_init (CalendarTask         *task,
		    icalcomponent        *ical,
                    CalendarClientSource *source,
                    icaltimezone         *default_zone)
{
  task->uid              = get_ical_uid (ical);
  task->summary          = get_ical_summary (ical);
  task->description      = get_ical_description (ical);
  task->color_string     = get_source_color (source->source);
  task->start_time       = get_ical_start_time (ical, default_zone);
  task->due_time         = get_ical_due_time (ical, default_zone);
  task->percent_complete = get_ical_percent_complete (ical);
  task->completed_time   = get_ical_completed_time (ical, default_zone);
  task->priority         = get_ical_priority (ical);
}

void
calendar_event_free (CalendarEvent *event)
{
  switch (event->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      calendar_appointment_finalize (CALENDAR_APPOINTMENT (event));
      break;
    case CALENDAR_EVENT_TASK:
      calendar_task_finalize (CALENDAR_TASK (event));
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  g_free (event);
}

static CalendarEvent *
calendar_event_new (icalcomponent        *ical,
                    CalendarClientSource *source,
                    icaltimezone         *default_zone)
{
  CalendarEvent *event;

  event = g_new0 (CalendarEvent, 1);

  switch (icalcomponent_isa (ical))
    {
    case ICAL_VEVENT_COMPONENT:
      event->type = CALENDAR_EVENT_APPOINTMENT;
      calendar_appointment_init (CALENDAR_APPOINTMENT (event),
                                 ical,
                                 source,
                                 default_zone);
      break;
    case ICAL_VTODO_COMPONENT:
      event->type = CALENDAR_EVENT_TASK;
      calendar_task_init (CALENDAR_TASK (event),
                          ical,
                          source,
                          default_zone);
      break;
    default:
      g_warning ("Unknown calendar component type: %d\n",
                 icalcomponent_isa (ical));
      g_free (event);
      return NULL;
    }

  return event;
}

static CalendarEvent *
calendar_event_copy (CalendarEvent *event)
{
  CalendarEvent *retval;

  if (!event)
    return NULL;

  retval = g_new0 (CalendarEvent, 1);

  retval->type = event->type;

  switch (event->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      calendar_appointment_copy (CALENDAR_APPOINTMENT (event),
				 CALENDAR_APPOINTMENT (retval));
      break;
    case CALENDAR_EVENT_TASK:
      calendar_task_copy (CALENDAR_TASK (event),
			  CALENDAR_TASK (retval));
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  return retval;
}

static char *
calendar_event_get_uid (CalendarEvent *event)
{
  switch (event->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      return g_strdup_printf ("%s%s", CALENDAR_APPOINTMENT (event)->uid, CALENDAR_APPOINTMENT (event)->rid ? CALENDAR_APPOINTMENT (event)->rid : ""); 
      break;
    case CALENDAR_EVENT_TASK:
      return g_strdup (CALENDAR_TASK (event)->uid);
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  return NULL;
}

static gboolean
calendar_event_equal (CalendarEvent *a,
		      CalendarEvent *b)
{
  if (!a && !b)
    return TRUE;

  if ((a && !b) || (!a && b))
    return FALSE;

  if (a->type != b->type)
    return FALSE;

  switch (a->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      return calendar_appointment_equal (CALENDAR_APPOINTMENT (a),
					 CALENDAR_APPOINTMENT (b));
    case CALENDAR_EVENT_TASK:
      return calendar_task_equal (CALENDAR_TASK (a),
				  CALENDAR_TASK (b));
    default:
      break;
    }
 
  g_assert_not_reached ();

  return FALSE;
}

static void
calendar_event_generate_ocurrences (CalendarEvent *event,
				    icalcomponent *ical,
				    ECal          *source,
				    time_t         start,
				    time_t         end,
                                    icaltimezone  *default_zone)
{
  if (event->type != CALENDAR_EVENT_APPOINTMENT)
    return;

  calendar_appointment_generate_ocurrences (CALENDAR_APPOINTMENT (event),
					    ical,
					    source,
					    start,
					    end,
                                            default_zone);
}

static inline void
calendar_event_debug_dump (CalendarEvent *event)
{
#ifdef CALENDAR_ENABLE_DEBUG
  switch (event->type)
    {
    case CALENDAR_EVENT_APPOINTMENT:
      {
	char   *start_str;
	char   *end_str;
	GSList *l;

	start_str = CALENDAR_APPOINTMENT (event)->start_time ?
	                    isodate_from_time_t (CALENDAR_APPOINTMENT (event)->start_time) :
	                    g_strdup ("(undefined)");
	end_str = CALENDAR_APPOINTMENT (event)->end_time ?
	                    isodate_from_time_t (CALENDAR_APPOINTMENT (event)->end_time) :
	                    g_strdup ("(undefined)");
	  
	dprintf ("Appointment: uid '%s', summary '%s', description '%s', "
		 "start_time '%s', end_time '%s', is_all_day %s\n",
		 CALENDAR_APPOINTMENT (event)->uid,
		 CALENDAR_APPOINTMENT (event)->summary,
		 CALENDAR_APPOINTMENT (event)->description,
		 start_str,
		 end_str,
		 CALENDAR_APPOINTMENT (event)->is_all_day ? "(true)" : "(false)");

	g_free (start_str);
	g_free (end_str);

	dprintf ("  Occurrences:\n");
	for (l = CALENDAR_APPOINTMENT (event)->occurrences; l; l = l->next)
	  {
	    CalendarOccurrence *occurrence = l->data;

	    start_str = occurrence->start_time ?
	      isodate_from_time_t (occurrence->start_time) :
	      g_strdup ("(undefined)");
	    
	    end_str = occurrence->end_time ?
	      isodate_from_time_t (occurrence->end_time) :
	      g_strdup ("(undefined)");

	    dprintf ("    start_time '%s', end_time '%s'\n",
		     start_str, end_str);

	    g_free (start_str);
	    g_free (end_str);
	  }
      }
      break;
    case CALENDAR_EVENT_TASK:
      {
	char *start_str;
	char *due_str;
	char *completed_str;

	start_str = CALENDAR_TASK (event)->start_time ?
	                    isodate_from_time_t (CALENDAR_TASK (event)->start_time) :
	                    g_strdup ("(undefined)");
	due_str = CALENDAR_TASK (event)->due_time ?
	                    isodate_from_time_t (CALENDAR_TASK (event)->due_time) :
	                    g_strdup ("(undefined)");
	completed_str = CALENDAR_TASK (event)->completed_time ?
	                    isodate_from_time_t (CALENDAR_TASK (event)->completed_time) :
	                    g_strdup ("(undefined)");

	dprintf ("Task: uid '%s', summary '%s', description '%s', "
		 "start_time '%s', due_time '%s', percent_complete %d, completed_time '%s'\n",
		 CALENDAR_TASK (event)->uid,
		 CALENDAR_TASK (event)->summary,
		 CALENDAR_TASK (event)->description,
		 start_str,
		 due_str,
		 CALENDAR_TASK (event)->percent_complete,
		 completed_str);

	g_free (completed_str);
      }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
#endif
}

static inline CalendarClientQuery *
goddamn_this_is_crack (CalendarClientSource *source,
		       ECalView             *view,
		       gboolean             *emit_signal)
{
  g_assert (view != NULL);

  if (source->completed_query.view == view)
    {
      if (emit_signal)
	*emit_signal = TRUE;
      return &source->completed_query;
    }
  else if (source->in_progress_query.view == view)
    {
      if (emit_signal)
	*emit_signal = FALSE;
      return &source->in_progress_query;
    }

  g_assert_not_reached ();

  return NULL;
}

static void
calendar_client_handle_query_completed (CalendarClientSource *source,
					ECalendarStatus       status,
					ECalView             *view)
{
  CalendarClientQuery *query;

  query = goddamn_this_is_crack (source, view, NULL);
  
  dprintf ("Query %p completed: %s\n", query, e_cal_get_error_message (status));

  if (status != E_CALENDAR_STATUS_OK)
    {
      g_warning ("Calendar query failed: %s\n",
		 e_cal_get_error_message (status));
      calendar_client_stop_query (source->client, source, query);
      return;
    }

  g_assert (source->query_in_progress != FALSE);
  g_assert (query == &source->in_progress_query);

  calendar_client_query_finalize (&source->completed_query);

  source->completed_query = source->in_progress_query;
  source->query_completed = TRUE;

  source->query_in_progress        = FALSE;
  source->in_progress_query.view   = NULL;
  source->in_progress_query.events = NULL;

  g_signal_emit (source->client, source->changed_signal_id, 0);
}

static void
calendar_client_handle_query_result (CalendarClientSource *source,
				     GList                *objects,
				     ECalView             *view)
{
  CalendarClientQuery *query;
  CalendarClient      *client;
  gboolean             emit_signal;
  gboolean             events_changed;
  GList               *l;
  time_t               month_begin;
  time_t               month_end;

  client = source->client;

  query = goddamn_this_is_crack (source, view, &emit_signal);

  dprintf ("Query %p result: %d objects:\n",
	   query, g_list_length (objects));

  month_begin = make_time_for_day_begin (1,
					 client->priv->month,
					 client->priv->year);

  month_end = make_time_for_day_begin (1,
				       client->priv->month + 1,
				       client->priv->year);

  events_changed = FALSE;
  for (l = objects; l; l = l->next)
    {
      CalendarEvent *event;
      CalendarEvent *old_event;
      icalcomponent *ical = l->data;
      char          *uid;
      
      event = calendar_event_new (ical, source, client->priv->zone);
      if (!event)
	      continue;

      calendar_event_generate_ocurrences (event,
					  ical,
					  source->source,
					  month_begin,
					  month_end,
                                          client->priv->zone);

      uid = calendar_event_get_uid (event);
      
      old_event = g_hash_table_lookup (query->events, uid);

      if (!calendar_event_equal (event, old_event))
	{
 	  dprintf ("Event %s: ", old_event ? "modified" : "added");

	  calendar_event_debug_dump (event);

	  g_hash_table_replace (query->events, uid, event);

	  events_changed = TRUE;
	}
      else
	{
	  g_free (uid);
	}		
    }

  if (emit_signal && events_changed)
    {
      g_signal_emit (source->client, source->changed_signal_id, 0);
    }
}

static gboolean
check_object_remove (gpointer key,
                     gpointer value,
                     gpointer data)
{
  char             *uid = data;
  ssize_t           len;

  len = strlen (uid);
  
  if (len <= strlen (key) && strncmp (uid, key, len) == 0)
    {
      dprintf ("Event removed: ");

      calendar_event_debug_dump (value);

      return TRUE;
    }

  return FALSE;
}

static void
calendar_client_handle_objects_removed (CalendarClientSource *source,
					GList                *ids,
					ECalView             *view)
{
  CalendarClientQuery *query;
  gboolean             emit_signal;
  gboolean             events_changed;
  GList               *l;

  query = goddamn_this_is_crack (source, view, &emit_signal);

  events_changed = FALSE;
  for (l = ids; l; l = l->next)
    {
      CalendarEvent   *event;
      ECalComponentId *id = l->data;
      char            *uid = g_strdup_printf ("%s%s", id->uid, id->rid ? id->rid : "");

      if (!id->rid || !(*id->rid))
	{
	  int size = g_hash_table_size (query->events);

	  g_hash_table_foreach_remove (query->events, check_object_remove, id->uid);

		if (size != g_hash_table_size (query->events))
			events_changed = TRUE;		
	}
      else if ((event = g_hash_table_lookup (query->events, uid)))
	{
	  dprintf ("Event removed: ");

	  calendar_event_debug_dump (event);

	  g_assert (g_hash_table_remove (query->events, uid));

	  events_changed = TRUE;
	}
      g_free (uid);
    }

  if (emit_signal && events_changed)
    {
      g_signal_emit (source->client, source->changed_signal_id, 0);
    }
}

static void
calendar_client_query_finalize (CalendarClientQuery *query)
{
  if (query->view)
    g_object_unref (query->view);
  query->view = NULL;

  if (query->events)
    g_hash_table_destroy (query->events);
  query->events = NULL;
}

static void
calendar_client_stop_query (CalendarClient       *client,
			    CalendarClientSource *source,
			    CalendarClientQuery  *query)
{
  if (query == &source->in_progress_query)
    {
      dprintf ("Stopping in progress query %p\n", query);

      g_assert (source->query_in_progress != FALSE);

      source->query_in_progress = FALSE;
    }
  else if (query == &source->completed_query)
    {
      dprintf ("Stopping completed query %p\n", query);

      g_assert (source->query_completed != FALSE);

      source->query_completed = FALSE;
    }
  else
    g_assert_not_reached ();
  
  calendar_client_query_finalize (query);
}

static void
calendar_client_start_query (CalendarClient       *client,
			     CalendarClientSource *source,
			     const char           *query)
{
  ECalView *view = NULL;
  GError   *error = NULL;

  if (!e_cal_get_query (source->source, query, &view, &error))
    {
      g_warning ("Error preparing the query: '%s': %s\n",
		 query, error->message);
      g_error_free (error);
      return;
    }

  g_assert (view != NULL);

  if (source->query_in_progress)
    calendar_client_stop_query (client, source, &source->in_progress_query);
  
  dprintf ("Starting query %p: '%s'\n", &source->in_progress_query, query);

  source->query_in_progress        = TRUE;
  source->in_progress_query.view   = view;
  source->in_progress_query.events =
    g_hash_table_new_full (g_str_hash,
			   g_str_equal,
			   g_free,
			   (GDestroyNotify) calendar_event_free);

  g_signal_connect_swapped (view, "objects-added",
			    G_CALLBACK (calendar_client_handle_query_result),
			    source);
  g_signal_connect_swapped (view, "objects-modified",
			    G_CALLBACK (calendar_client_handle_query_result),
			    source);
  g_signal_connect_swapped (view, "objects-removed",
			    G_CALLBACK (calendar_client_handle_objects_removed),
			    source);
  g_signal_connect_swapped (view, "view-done",
			    G_CALLBACK (calendar_client_handle_query_completed),
			    source);

  e_cal_view_start (view);
}

static void
calendar_client_update_appointments (CalendarClient *client)
{
  GSList *l;
  char   *query;
  char   *month_begin;
  char   *month_end;

  if (client->priv->month == -1 ||
      client->priv->year  == -1)
    return;

  month_begin = make_isodate_for_day_begin (1,
					    client->priv->month,
					    client->priv->year);

  month_end = make_isodate_for_day_begin (1,
					  client->priv->month + 1,
					  client->priv->year);

  query = g_strdup_printf ("occur-in-time-range? (make-time \"%s\") "
			                        "(make-time \"%s\")",
			   month_begin, month_end);

  for (l = client->priv->appointment_sources; l; l = l->next)
    {
      CalendarClientSource *cs = l->data;
                  
      if (e_cal_get_load_state (cs->source) != E_CAL_LOAD_LOADED)  
        continue;

      calendar_client_start_query (client, cs, query);
    }

  g_free (month_begin);
  g_free (month_end);
  g_free (query);
}

/* FIXME:
 * perhaps we should use evo's "hide_completed_tasks" pref?
 */
static void
calendar_client_update_tasks (CalendarClient *client)
{
  GSList *l;
  char   *query;

#ifdef FIX_BROKEN_TASKS_QUERY
  /* FIXME: this doesn't work for tasks without a start or
   *        due date
   *        Look at filter_task() to see the behaviour we
   *        want.
   */
  
  char   *day_begin;
  char   *day_end;

  if (client->priv->day   == -1 ||
      client->priv->month == -1 ||
      client->priv->year  == -1)
    return;

  day_begin = make_isodate_for_day_begin (client->priv->day,
					  client->priv->month,
					  client->priv->year);

  day_end = make_isodate_for_day_begin (client->priv->day + 1,
					client->priv->month,
					client->priv->year);
  if (!day_begin || !day_end)
    {
      g_warning ("Cannot run query with invalid date: %dd %dy %dm\n",
		 client->priv->day,
		 client->priv->month,
		 client->priv->year);
      g_free (day_begin);
      g_free (day_end);
      return;
    }
  
  query = g_strdup_printf ("(and (occur-in-time-range? (make-time \"%s\") "
                                                      "(make-time \"%s\")) "
                             "(or (not is-completed?) "
                               "(and (is-completed?) "
                                    "(not (completed-before? (make-time \"%s\"))))))",
			   day_begin, day_end, day_begin);
#else
  query = g_strdup ("#t");
#endif /* FIX_BROKEN_TASKS_QUERY */

  for (l = client->priv->task_sources; l; l = l->next)
    {
      CalendarClientSource *cs = l->data;

      if (e_cal_get_load_state (cs->source) != E_CAL_LOAD_LOADED)  
        continue;

      calendar_client_start_query (client, cs, query);
    }

#ifdef FIX_BROKEN_TASKS_QUERY
  g_free (day_begin);
  g_free (day_end);
#endif
  g_free (query);
}

static void
calendar_client_source_finalize (CalendarClientSource *source)
{
  source->client = NULL;

  if (source->source) {
    g_signal_handlers_disconnect_by_func (source->source,
                                          cal_opened_cb, source);
    g_object_unref (source->source);
  }
  source->source = NULL;

  calendar_client_query_finalize (&source->completed_query);
  calendar_client_query_finalize (&source->in_progress_query);
  
  source->query_completed   = FALSE;
  source->query_in_progress = FALSE;
}

static int
compare_calendar_sources (CalendarClientSource *s1,
			  CalendarClientSource *s2)
{
  return (s1->source == s2->source) ? 0 : 1;
}

static GSList *
calendar_client_update_sources_list (CalendarClient *client,
				     GSList         *sources,
				     GSList         *esources,
				     guint           changed_signal_id)
{
  GSList *retval, *l;

  retval = NULL;

  for (l = esources; l; l = l->next)
    {
      CalendarClientSource  dummy_source;
      CalendarClientSource *new_source;
      GSList               *s;
      ECal                 *esource = l->data;

      dummy_source.source = esource;

      dprintf ("update_sources_list: adding client %s: ",
	       e_source_peek_uid (e_cal_get_source (esource)));

      if ((s = g_slist_find_custom (sources,
				    &dummy_source,
				    (GCompareFunc) compare_calendar_sources)))
	{
	  dprintf ("already on list\n");
	  new_source = s->data;
	  sources = g_slist_delete_link (sources, s);
	}
      else
	{
	  dprintf ("added\n");
	  new_source                    = g_new0 (CalendarClientSource, 1);
	  new_source->client            = client;
	  new_source->source            = g_object_ref (esource);
	  new_source->changed_signal_id = changed_signal_id;
	}

      retval = g_slist_prepend (retval, new_source);
    }

  for (l = sources; l; l = l->next)
    {
      CalendarClientSource *source = l->data;

      dprintf ("Removing client %s from list\n",
	       e_source_peek_uid (e_cal_get_source (source->source)));

      calendar_client_source_finalize (source);
      g_free (source);
    }
  g_slist_free (sources);

  return retval;
}

static void
calendar_client_appointment_sources_changed (CalendarClient  *client)
{
  GSList *esources;

  dprintf ("appointment_sources_changed: updating ...\n");

  esources = calendar_sources_get_appointment_sources (client->priv->calendar_sources);

  client->priv->appointment_sources = 
    calendar_client_update_sources_list (client,
					 client->priv->appointment_sources,
					 esources,
					 signals [APPOINTMENTS_CHANGED]);

  load_calendars (client, CALENDAR_EVENT_APPOINTMENT);
  calendar_client_update_appointments (client);
}

static void
calendar_client_task_sources_changed (CalendarClient  *client)
{
  GSList *esources;

  dprintf ("task_sources_changed: updating ...\n");

  esources = calendar_sources_get_task_sources (client->priv->calendar_sources);

  client->priv->task_sources = 
    calendar_client_update_sources_list (client,
					 client->priv->task_sources,
					 esources,
					 signals [TASKS_CHANGED]);

  load_calendars (client, CALENDAR_EVENT_TASK);
  calendar_client_update_tasks (client);
}

void
calendar_client_get_date (CalendarClient *client,
                          guint          *year,
                          guint          *month,
                          guint          *day)
{
  g_return_if_fail (CALENDAR_IS_CLIENT (client));

  if (year)
    *year = client->priv->year;

  if (month)
    *month = client->priv->month;

  if (day)
    *day = client->priv->day;
}

void
calendar_client_select_month (CalendarClient *client,
			      guint           month,
			      guint           year)
{
  g_return_if_fail (CALENDAR_IS_CLIENT (client));
  g_return_if_fail (month <= 11);

  if (client->priv->year != year || client->priv->month != month)
    {
      client->priv->month = month;
      client->priv->year  = year;

      calendar_client_update_appointments (client);
      calendar_client_update_tasks (client);

      g_object_freeze_notify (G_OBJECT (client));
      g_object_notify (G_OBJECT (client), "month");
      g_object_notify (G_OBJECT (client), "year");
      g_object_thaw_notify (G_OBJECT (client));
    }
}

void
calendar_client_select_day (CalendarClient *client,
			    guint           day)
{
  g_return_if_fail (CALENDAR_IS_CLIENT (client));
  g_return_if_fail (day <= 31);

  if (client->priv->day != day)
    {
      client->priv->day = day;

      /* don't need to update appointments unless
       * the selected month changes
       */
#ifdef FIX_BROKEN_TASKS_QUERY
      calendar_client_update_tasks (client);
#endif

      g_object_notify (G_OBJECT (client), "day");
    }
}

typedef struct
{
  CalendarClient *client;
  GSList         *events;
  time_t          start_time;
  time_t          end_time;
} FilterData;

typedef void (* CalendarEventFilterFunc) (const char    *uid,
					  CalendarEvent *event,
					  FilterData    *filter_data);

static void
filter_appointment (const char    *uid,
		    CalendarEvent *event,
		    FilterData    *filter_data)
{
  GSList *occurrences, *l;

  if (event->type != CALENDAR_EVENT_APPOINTMENT)
    return;

  occurrences = CALENDAR_APPOINTMENT (event)->occurrences;
  CALENDAR_APPOINTMENT (event)->occurrences = NULL;

  for (l = occurrences; l; l = l->next)
    {
      CalendarOccurrence *occurrence = l->data;
      time_t start_time = occurrence->start_time;
      time_t end_time   = occurrence->end_time;

      if ((start_time >= filter_data->start_time &&
           start_time < filter_data->end_time) ||
          (start_time <= filter_data->start_time &&
           (end_time - 1) > filter_data->start_time))
	{
	  CalendarEvent *new_event;

	  new_event = calendar_event_copy (event);
	      
	  CALENDAR_APPOINTMENT (new_event)->start_time = occurrence->start_time;
	  CALENDAR_APPOINTMENT (new_event)->end_time   = occurrence->end_time;
	      
	  filter_data->events = g_slist_prepend (filter_data->events, new_event);
	}
    }

  CALENDAR_APPOINTMENT (event)->occurrences = occurrences;
}

static void
filter_task (const char    *uid,
	     CalendarEvent *event,
	     FilterData    *filter_data)
{
#ifdef FIX_BROKEN_TASKS_QUERY
  CalendarTask *task;
#endif

  if (event->type != CALENDAR_EVENT_TASK)
    return;

#ifdef FIX_BROKEN_TASKS_QUERY
  task = CALENDAR_TASK (event);

  if (task->start_time && task->start_time > filter_data->start_time)
    return;

  if (task->completed_time && 
      (task->completed_time < filter_data->start_time ||
       task->completed_time > filter_data->end_time))
    return;
#endif /* FIX_BROKEN_TASKS_QUERY */

  filter_data->events = g_slist_prepend (filter_data->events,
					 calendar_event_copy (event));
}

static GSList *
calendar_client_filter_events (CalendarClient          *client,
			       GSList                  *sources,
			       CalendarEventFilterFunc  filter_func,
			       time_t                   start_time,
			       time_t                   end_time)
{
  FilterData  filter_data;
  GSList     *l;
  GSList     *retval;

  if (!sources)
    return NULL;

  filter_data.client     = client;
  filter_data.events     = NULL;
  filter_data.start_time = start_time;
  filter_data.end_time   = end_time;

  retval = NULL;
  for (l = sources; l; l = l->next)
    {
      CalendarClientSource *source = l->data;

      if (source->query_completed)
	{
	  filter_data.events = NULL;
	  g_hash_table_foreach (source->completed_query.events,
				(GHFunc) filter_func,
				&filter_data);

	  filter_data.events = g_slist_reverse (filter_data.events);

	  retval = g_slist_concat (retval, filter_data.events);
	}
    }

  return retval;
}

GSList *
calendar_client_get_events (CalendarClient    *client,
			    CalendarEventType  event_mask)
{
  GSList *appointments;
  GSList *tasks;
  time_t  day_begin;
  time_t  day_end;

  g_return_val_if_fail (CALENDAR_IS_CLIENT (client), NULL);
  g_return_val_if_fail (client->priv->day   != -1 &&
			client->priv->month != -1 &&
			client->priv->year  != -1, NULL);

  day_begin = make_time_for_day_begin (client->priv->day,
				       client->priv->month,
				       client->priv->year);
  day_end   = make_time_for_day_begin (client->priv->day + 1,
				       client->priv->month,
				       client->priv->year);

  appointments = NULL;
  if (event_mask & CALENDAR_EVENT_APPOINTMENT)
    {
      appointments = calendar_client_filter_events (client,
						    client->priv->appointment_sources,
						    filter_appointment,
						    day_begin,
						    day_end);
    }

  tasks = NULL;
  if (event_mask & CALENDAR_EVENT_TASK)
    {
      tasks = calendar_client_filter_events (client,
					     client->priv->task_sources,
					     filter_task,
					     day_begin,
					     day_end);
    }

  return g_slist_concat (appointments, tasks);
}

static inline int
day_from_time_t (time_t t)
{
  struct tm *tm = localtime (&t);

  g_assert (tm == NULL || (tm->tm_mday >=1 && tm->tm_mday <= 31));

  return tm ? tm->tm_mday : 0;
}

void
calendar_client_foreach_appointment_day (CalendarClient  *client,
					 CalendarDayIter  iter_func,
					 gpointer         user_data)
{
  GSList   *appointments, *l;
  gboolean  marked_days [32] = { FALSE, };
  time_t    month_begin;
  time_t    month_end;
  int       i;

  g_return_if_fail (CALENDAR_IS_CLIENT (client));
  g_return_if_fail (iter_func != NULL);
  g_return_if_fail (client->priv->month != -1 &&
		    client->priv->year  != -1);

  month_begin = make_time_for_day_begin (1,
					 client->priv->month,
					 client->priv->year);
  month_end   = make_time_for_day_begin (1,
					 client->priv->month + 1,
					 client->priv->year);
  
  appointments = calendar_client_filter_events (client,
						client->priv->appointment_sources,
						filter_appointment,
						month_begin,
						month_end);
  for (l = appointments; l; l = l->next)
    {
      CalendarAppointment *appointment = l->data;

      if (appointment->start_time)
        {
          time_t day_time = appointment->start_time;

          if (day_time >= month_begin)
            marked_days [day_from_time_t (day_time)] = TRUE;
      
          if (appointment->end_time)
            {
              int day_offset;
              int duration = appointment->end_time - appointment->start_time;
	      /* mark the days for the appointment, no need to add an extra one when duration is a multiple of 86400 */
              for (day_offset = 1; day_offset <= duration / 86400 && duration != day_offset * 86400; day_offset++)
                {
                  time_t day_tm = appointment->start_time + day_offset * 86400;

                  if (day_tm > month_end)
                    break;
                  if (day_tm >= month_begin)
                    marked_days [day_from_time_t (day_tm)] = TRUE;
                }
            }
        }
      calendar_event_free (CALENDAR_EVENT (appointment));
    }

  g_slist_free (appointments);

  for (i = 1; i < 32; i++)
    {
      if (marked_days [i])
	iter_func (client, i, user_data);
    }
}

void
calendar_client_set_task_completed (CalendarClient *client,
				    char           *task_uid,
				    gboolean        task_completed,
				    guint           percent_complete)
{
  GSList              *l;
  ECal                *esource;
  icalcomponent       *ical;
  icalproperty        *prop;
  icalproperty_status  status;

  g_return_if_fail (CALENDAR_IS_CLIENT (client));
  g_return_if_fail (task_uid != NULL);
  g_return_if_fail (task_completed == FALSE || percent_complete == 100);

  ical = NULL;
  esource = NULL;
  for (l = client->priv->task_sources; l; l = l->next)
    {
      CalendarClientSource *source = l->data;

      esource = source->source;
      e_cal_get_object (esource, task_uid, NULL, &ical, NULL);
      if (ical)
	break;
    }

  if (!ical)
    {
      g_warning ("Cannot locate task with uid = '%s'\n", task_uid);
      return;
    }

  g_assert (esource != NULL);

  /* Completed time */
  prop = icalcomponent_get_first_property (ical,
					   ICAL_COMPLETED_PROPERTY);
  if (task_completed)
    {
      struct icaltimetype  completed_time;

      completed_time = icaltime_current_time_with_zone (client->priv->zone);
      if (!prop)
	{
	  icalcomponent_add_property (ical,
				      icalproperty_new_completed (completed_time));
	}
      else
	{
	  icalproperty_set_completed (prop, completed_time);
	}
    }
  else if (prop)
    {
      icalcomponent_remove_property (ical, prop);
    }

  /* Percent complete */
  prop = icalcomponent_get_first_property (ical,
					   ICAL_PERCENTCOMPLETE_PROPERTY);
  if (!prop)
    {
      icalcomponent_add_property (ical,
				  icalproperty_new_percentcomplete (percent_complete));
    }
  else
    {
      icalproperty_set_percentcomplete (prop, percent_complete);
    }

  /* Status */
  status = task_completed ? ICAL_STATUS_COMPLETED : ICAL_STATUS_NEEDSACTION;
  prop = icalcomponent_get_first_property (ical, ICAL_STATUS_PROPERTY);
  if (prop)
    {
      icalproperty_set_status (prop, status);
    }
  else
    {
      icalcomponent_add_property (ical,
				  icalproperty_new_status (status));
    }

  e_cal_modify_object (esource, ical, CALOBJ_MOD_ALL, NULL);
}
