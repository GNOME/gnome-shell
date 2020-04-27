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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Mark McLoughlin  <mark@skynet.ie>
 *     William Jon McCann  <mccann@jhu.edu>
 *     Martin Grimme  <martin@pycage.de>
 *     Christian Kellner  <gicmo@xatom.net>
 */

#include <config.h>

#include "calendar-sources.h"

#include <libintl.h>
#include <string.h>
#define HANDLE_LIBICAL_MEMORY
#define EDS_DISABLE_DEPRECATED
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#include <libecal/libecal.h>
G_GNUC_END_IGNORE_DEPRECATIONS

#undef CALENDAR_ENABLE_DEBUG
#include "calendar-debug.h"

typedef struct _ClientData ClientData;
typedef struct _CalendarSourceData CalendarSourceData;

struct _ClientData
{
  ECalClient *client;
  gulong backend_died_id;
};

struct _CalendarSourceData
{
  ECalClientSourceType source_type;
  CalendarSources *sources;
  guint            changed_signal;

  /* ESource -> EClient */
  GHashTable      *clients;

  guint            timeout_id;

  guint            loaded : 1;
};

typedef struct _CalendarSourcesPrivate CalendarSourcesPrivate;

struct _CalendarSources
{
  GObject                 parent;
  CalendarSourcesPrivate *priv;
};

struct _CalendarSourcesPrivate
{
  ESourceRegistry    *registry;
  gulong              source_added_id;
  gulong              source_changed_id;
  gulong              source_removed_id;

  CalendarSourceData  appointment_sources;
  CalendarSourceData  task_sources;
};

G_DEFINE_TYPE_WITH_PRIVATE (CalendarSources, calendar_sources, G_TYPE_OBJECT)

static void calendar_sources_finalize   (GObject             *object);

static void backend_died_cb (EClient *client, CalendarSourceData *source_data);
static void calendar_sources_registry_source_changed_cb (ESourceRegistry *registry,
                                                         ESource         *source,
                                                         CalendarSources *sources);
static void calendar_sources_registry_source_removed_cb (ESourceRegistry *registry,
                                                         ESource         *source,
                                                         CalendarSources *sources);

enum
{
  APPOINTMENT_SOURCES_CHANGED,
  TASK_SOURCES_CHANGED,
  LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0, };

static GObjectClass    *parent_class = NULL;
static CalendarSources *calendar_sources_singleton = NULL;

static void
client_data_free (ClientData *data)
{
  g_clear_signal_handler (&data->backend_died_id, data->client);
  g_object_unref (data->client);
  g_slice_free (ClientData, data);
}

static void
calendar_sources_class_init (CalendarSourcesClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = calendar_sources_finalize;

  signals [APPOINTMENT_SOURCES_CHANGED] =
    g_signal_new ("appointment-sources-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals [TASK_SOURCES_CHANGED] =
    g_signal_new ("task-sources-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
calendar_sources_init (CalendarSources *sources)
{
  GError *error = NULL;
  GDBusConnection *session_bus;
  GVariant *result;

  sources->priv = calendar_sources_get_instance_private (sources);

  /* WORKAROUND: the hardcoded timeout for e_source_registry_new_sync()
     (and other library calls that eventually call g_dbus_proxy_new[_sync]())
     is 25 seconds. This has been shown to be too small for
     evolution-source-registry in certain cases (slow disk, concurrent IO,
     many configured sources), so we first ensure that the service
     starts with a manual call and a higher timeout.

     HACK: every time the DBus API is bumped in e-d-s we need
     to update this!
  */
  session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (session_bus == NULL)
    {
      g_error ("Failed to connect to the session bus: %s", error->message);
    }

  result = g_dbus_connection_call_sync (session_bus, "org.freedesktop.DBus",
                                        "/", "org.freedesktop.DBus",
                                        "StartServiceByName",
                                        g_variant_new ("(su)",
                                                       "org.gnome.evolution.dataserver.Sources5",
                                                       0),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        60 * 1000,
                                        NULL, &error);
  if (result != NULL)
    {
      g_variant_unref (result);
      sources->priv->registry = e_source_registry_new_sync (NULL, &error);
    }

  if (error != NULL)
    {
      /* Any error is fatal, but we don't want to crash gnome-shell-calendar-server
         because of e-d-s problems. So just exit here.
      */
      g_warning ("Failed to start evolution-source-registry: %s", error->message);
      exit (EXIT_FAILURE);
    }

  g_object_unref (session_bus);

  sources->priv->source_added_id   = g_signal_connect (sources->priv->registry,
                                                       "source-added",
                                                       G_CALLBACK (calendar_sources_registry_source_changed_cb),
                                                       sources);
  sources->priv->source_changed_id = g_signal_connect (sources->priv->registry,
                                                       "source-changed",
                                                       G_CALLBACK (calendar_sources_registry_source_changed_cb),
                                                       sources);
  sources->priv->source_removed_id = g_signal_connect (sources->priv->registry,
                                                       "source-removed",
                                                       G_CALLBACK (calendar_sources_registry_source_removed_cb),
                                                       sources);

  sources->priv->appointment_sources.source_type    = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
  sources->priv->appointment_sources.sources        = sources;
  sources->priv->appointment_sources.changed_signal = signals [APPOINTMENT_SOURCES_CHANGED];
  sources->priv->appointment_sources.clients        = g_hash_table_new_full ((GHashFunc) e_source_hash,
                                                                             (GEqualFunc) e_source_equal,
                                                                             (GDestroyNotify) g_object_unref,
                                                                             (GDestroyNotify) client_data_free);
  sources->priv->appointment_sources.timeout_id     = 0;

  sources->priv->task_sources.source_type    = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
  sources->priv->task_sources.sources        = sources;
  sources->priv->task_sources.changed_signal = signals [TASK_SOURCES_CHANGED];
  sources->priv->task_sources.clients        = g_hash_table_new_full ((GHashFunc) e_source_hash,
                                                                      (GEqualFunc) e_source_equal,
                                                                      (GDestroyNotify) g_object_unref,
                                                                      (GDestroyNotify) client_data_free);
  sources->priv->task_sources.timeout_id     = 0;
}

static void
calendar_sources_finalize_source_data (CalendarSources    *sources,
                                       CalendarSourceData *source_data)
{
  if (source_data->loaded)
    {
      g_hash_table_destroy (source_data->clients);
      source_data->clients = NULL;

      g_clear_handle_id (&source_data->timeout_id, g_source_remove);

      source_data->loaded = FALSE;
    }
}

static void
calendar_sources_finalize (GObject *object)
{
  CalendarSources *sources = CALENDAR_SOURCES (object);

  if (sources->priv->registry)
    {
      g_clear_signal_handler (&sources->priv->source_added_id,
                              sources->priv->registry);
      g_clear_signal_handler (&sources->priv->source_changed_id,
                              sources->priv->registry);
      g_clear_signal_handler (&sources->priv->source_removed_id,
                              sources->priv->registry);
      g_object_unref (sources->priv->registry);
    }
  sources->priv->registry = NULL;

  calendar_sources_finalize_source_data (sources, &sources->priv->appointment_sources);
  calendar_sources_finalize_source_data (sources, &sources->priv->task_sources);

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

CalendarSources *
calendar_sources_get (void)
{
  gpointer singleton_location = &calendar_sources_singleton;

  if (calendar_sources_singleton)
    return g_object_ref (calendar_sources_singleton);

  calendar_sources_singleton = g_object_new (CALENDAR_TYPE_SOURCES, NULL);
  g_object_add_weak_pointer (G_OBJECT (calendar_sources_singleton),
                             singleton_location);

  return calendar_sources_singleton;
}

/* The clients are just created here but not loaded */
static void
create_client_for_source (ESource              *source,
                          ECalClientSourceType  source_type,
                          CalendarSourceData   *source_data)
{
  ClientData *data;
  EClient *client;
  GError *error = NULL;

  client = g_hash_table_lookup (source_data->clients, source);
  g_return_if_fail (client == NULL);

  client = e_cal_client_connect_sync (source, source_type, -1, NULL, &error);
  if (!client)
    {
      g_warning ("Could not load source '%s': %s",
                 e_source_get_uid (source),
                 error->message);
      g_clear_error (&error);
      return;
    }

  data = g_slice_new0 (ClientData);
  data->client = E_CAL_CLIENT (client);  /* takes ownership */
  data->backend_died_id = g_signal_connect (client,
                                            "backend-died",
                                            G_CALLBACK (backend_died_cb),
                                            source_data);

  g_hash_table_insert (source_data->clients, g_object_ref (source), data);
}

static inline void
debug_dump_ecal_list (GHashTable *clients)
{
#ifdef CALENDAR_ENABLE_DEBUG
  GList *list, *link;

  dprintf ("Loaded clients:\n");
  list = g_hash_table_get_keys (clients);
  for (link = list; link != NULL; link = g_list_next (link))
    {
      ESource *source = E_SOURCE (link->data);

      dprintf ("  %s %s\n",
               e_source_get_uid (source),
               e_source_get_display_name (source));
    }
  g_list_free (list);
#endif
}

static void
calendar_sources_load_esource_list (ESourceRegistry *registry,
                                    CalendarSourceData *source_data);

static gboolean
backend_restart (gpointer data)
{
  CalendarSourceData *source_data = data;
  ESourceRegistry *registry;

  registry = source_data->sources->priv->registry;
  calendar_sources_load_esource_list (registry, source_data);
  g_signal_emit (source_data->sources, source_data->changed_signal, 0);

  source_data->timeout_id = 0;
    
  return FALSE;
}

static void
backend_died_cb (EClient *client, CalendarSourceData *source_data)
{
  ESource *source;
  const char *display_name;

  source = e_client_get_source (client);
  display_name = e_source_get_display_name (source);
  g_warning ("The calendar backend for '%s' has crashed.", display_name);
  g_hash_table_remove (source_data->clients, source);

  g_clear_handle_id (&source_data->timeout_id, g_source_remove);

  source_data->timeout_id = g_timeout_add_seconds (2, backend_restart,
                                                   source_data);
  g_source_set_name_by_id (source_data->timeout_id, "[gnome-shell] backend_restart");
}

static void
calendar_sources_load_esource_list (ESourceRegistry *registry,
                                    CalendarSourceData *source_data)
{
  GList   *list, *link;
  const gchar *extension_name;

  switch (source_data->source_type)
    {
      case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
        extension_name = E_SOURCE_EXTENSION_CALENDAR;
        break;
      case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
        extension_name = E_SOURCE_EXTENSION_TASK_LIST;
        break;
      default:
        g_return_if_reached ();
    }

  list = e_source_registry_list_sources (registry, extension_name);

  for (link = list; link != NULL; link = g_list_next (link))
    {
      ESource *source = E_SOURCE (link->data);
      ESourceSelectable *extension;
      gboolean show_source;

      extension = e_source_get_extension (source, extension_name);
      show_source = e_source_get_enabled (source) && e_source_selectable_get_selected (extension);

      if (show_source)
        create_client_for_source (source, source_data->source_type, source_data);
    }

  debug_dump_ecal_list (source_data->clients);

  g_list_free_full (list, g_object_unref);
}

static void
calendar_sources_registry_source_changed_cb (ESourceRegistry *registry,
                                             ESource         *source,
                                             CalendarSources *sources)
{
  if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
    {
      CalendarSourceData *source_data;
      ESourceSelectable *extension;
      gboolean have_client;
      gboolean show_source;

      source_data = &sources->priv->appointment_sources;
      extension = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
      have_client = (g_hash_table_lookup (source_data->clients, source) != NULL);
      show_source = e_source_get_enabled (source) && e_source_selectable_get_selected (extension);

      if (!show_source && have_client)
        {
          g_hash_table_remove (source_data->clients, source);
          g_signal_emit (sources, source_data->changed_signal, 0);
        }
      if (show_source && !have_client)
        {
          create_client_for_source (source, source_data->source_type, source_data);
          g_signal_emit (sources, source_data->changed_signal, 0);
        }
    }

  if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
    {
      CalendarSourceData *source_data;
      ESourceSelectable *extension;
      gboolean have_client;
      gboolean show_source;

      source_data = &sources->priv->task_sources;
      extension = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);
      have_client = (g_hash_table_lookup (source_data->clients, source) != NULL);
      show_source = e_source_get_enabled (source) && e_source_selectable_get_selected (extension);

      if (!show_source && have_client)
        {
          g_hash_table_remove (source_data->clients, source);
          g_signal_emit (sources, source_data->changed_signal, 0);
        }
      if (show_source && !have_client)
        {
          create_client_for_source (source, source_data->source_type, source_data);
          g_signal_emit (sources, source_data->changed_signal, 0);
        }
    }
}

static void
calendar_sources_registry_source_removed_cb (ESourceRegistry *registry,
                                             ESource         *source,
                                             CalendarSources *sources)
{
  if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
    {
      CalendarSourceData *source_data;

      source_data = &sources->priv->appointment_sources;
      g_hash_table_remove (source_data->clients, source);
      g_signal_emit (sources, source_data->changed_signal, 0);
    }

  if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
    {
      CalendarSourceData *source_data;

      source_data = &sources->priv->task_sources;
      g_hash_table_remove (source_data->clients, source);
      g_signal_emit (sources, source_data->changed_signal, 0);
    }
}

static void
ensure_appointment_sources (CalendarSources *sources)
{
  if (!sources->priv->appointment_sources.loaded)
    {
      calendar_sources_load_esource_list (sources->priv->registry,
                                          &sources->priv->appointment_sources);
      sources->priv->appointment_sources.loaded = TRUE;
    }
}

GList *
calendar_sources_get_appointment_clients (CalendarSources *sources)
{
  GList *list, *link;

  g_return_val_if_fail (CALENDAR_IS_SOURCES (sources), NULL);

  ensure_appointment_sources (sources);

  list = g_hash_table_get_values (sources->priv->appointment_sources.clients);

  for (link = list; link != NULL; link = g_list_next (link))
    link->data = ((ClientData *) link->data)->client;

  return list;
}

static void
ensure_task_sources (CalendarSources *sources)
{
  if (!sources->priv->task_sources.loaded)
    {
      calendar_sources_load_esource_list (sources->priv->registry,
                                          &sources->priv->task_sources);
      sources->priv->task_sources.loaded = TRUE;
    }
}

GList *
calendar_sources_get_task_clients (CalendarSources *sources)
{
  GList *list, *link;

  g_return_val_if_fail (CALENDAR_IS_SOURCES (sources), NULL);

  ensure_task_sources (sources);

  list = g_hash_table_get_values (sources->priv->task_sources.clients);

  for (link = list; link != NULL; link = g_list_next (link))
    link->data = ((ClientData *) link->data)->client;

  return list;
}

gboolean
calendar_sources_has_sources (CalendarSources *sources)
{
  g_return_val_if_fail (CALENDAR_IS_SOURCES (sources), FALSE);

  ensure_appointment_sources (sources);
  ensure_task_sources (sources);

  return g_hash_table_size (sources->priv->appointment_sources.clients) > 0 ||
    g_hash_table_size (sources->priv->task_sources.clients) > 0;
}
