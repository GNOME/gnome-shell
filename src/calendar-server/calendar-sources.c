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

typedef struct _CalendarSourcesPrivate CalendarSourcesPrivate;

struct _CalendarSources
{
  GObject                 parent;

  ESourceRegistryWatcher *registry_watcher;
  gulong                  filter_id;
  gulong                  appeared_id;
  gulong                  disappeared_id;

  GMutex                  clients_lock;
  GHashTable             *clients; /* ESource -> ClientData */
};

G_DEFINE_TYPE (CalendarSources, calendar_sources, G_TYPE_OBJECT)

enum
{
  CLIENT_APPEARED,
  CLIENT_DISAPPEARED,
  LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0, };

static void
calendar_sources_client_connected_cb (GObject *source_object,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
  CalendarSources *sources = CALENDAR_SOURCES (source_object);
  ESource *source = user_data;
  EClient *client;
  g_autoptr (GError) error = NULL;

  /* The calendar_sources_connect_client_sync() already stored the 'client'
   * into the sources->clients */
  client = calendar_sources_connect_client_finish (sources, result, &error);
  if (error)
    {
      g_warning ("Could not load source '%s': %s",
                 e_source_get_uid (source),
                 error->message);
    }
   else
    {
      g_signal_emit (sources, signals[CLIENT_APPEARED], 0, client, NULL);
    }

  g_clear_object (&client);
  g_clear_object (&source);
}

static gboolean
registry_watcher_filter_cb (ESourceRegistryWatcher *watcher,
                            ESource *source,
                            CalendarSources *sources)
{
  return e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR) &&
         e_source_selectable_get_selected (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));
}

static void
registry_watcher_source_appeared_cb (ESourceRegistryWatcher *watcher,
                                     ESource *source,
                                     CalendarSources *sources)
{
  ECalClientSourceType source_type;

  if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
    source_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
  else if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST))
    source_type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
  else if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
    source_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
  else
    g_return_if_reached ();

  calendar_sources_connect_client (sources, source, source_type, 30, NULL, calendar_sources_client_connected_cb, g_object_ref (source));
}

static void
registry_watcher_source_disappeared_cb (ESourceRegistryWatcher *watcher,
                                        ESource *source,
                                        CalendarSources *sources)
{
  gboolean emit;

  g_mutex_lock (&sources->clients_lock);

  emit = g_hash_table_remove (sources->clients, source);

  g_mutex_unlock (&sources->clients_lock);

  if (emit)
    g_signal_emit (sources, signals[CLIENT_DISAPPEARED], 0, e_source_get_uid (source), NULL);
}

static void
client_data_free (ClientData *data)
{
  g_signal_handler_disconnect (data->client, data->backend_died_id);
  g_object_unref (data->client);
  g_free (data);
}

static void
calendar_sources_constructed (GObject *object)
{
  CalendarSources *sources = CALENDAR_SOURCES (object);
  ESourceRegistry *registry = NULL;
  GError *error = NULL;

  G_OBJECT_CLASS (calendar_sources_parent_class)->constructed (object);

  registry = e_source_registry_new_sync (NULL, &error);
  if (error != NULL)
    {
      /* Any error is fatal, but we don't want to crash gnome-shell-calendar-server
         because of e-d-s problems. So just exit here.
      */
      g_warning ("Failed to start evolution-source-registry: %s", error->message);
      exit (EXIT_FAILURE);
    }

  g_return_if_fail (registry != NULL);

  sources->registry_watcher = e_source_registry_watcher_new (registry, NULL);

  g_clear_object (&registry);

  sources->clients = g_hash_table_new_full ((GHashFunc) e_source_hash,
                                            (GEqualFunc) e_source_equal,
                                            (GDestroyNotify) g_object_unref,
                                            (GDestroyNotify) client_data_free);
  sources->filter_id = g_signal_connect (sources->registry_watcher,
                                         "filter",
                                         G_CALLBACK (registry_watcher_filter_cb),
                                         sources);
  sources->appeared_id = g_signal_connect (sources->registry_watcher,
                                           "appeared",
                                           G_CALLBACK (registry_watcher_source_appeared_cb),
                                           sources);
  sources->disappeared_id = g_signal_connect (sources->registry_watcher,
                                              "disappeared",
                                              G_CALLBACK (registry_watcher_source_disappeared_cb),
                                              sources);

  e_source_registry_watcher_reclaim (sources->registry_watcher);
}

static void
calendar_sources_finalize (GObject *object)
{
  CalendarSources *sources = CALENDAR_SOURCES (object);

  g_clear_pointer (&sources->clients, g_hash_table_destroy);

  if (sources->registry_watcher)
    {
      g_signal_handler_disconnect (sources->registry_watcher,
                                   sources->filter_id);
      g_signal_handler_disconnect (sources->registry_watcher,
                                   sources->appeared_id);
      g_signal_handler_disconnect (sources->registry_watcher,
                                   sources->disappeared_id);
      g_clear_object (&sources->registry_watcher);
    }

  g_mutex_clear (&sources->clients_lock);

  G_OBJECT_CLASS (calendar_sources_parent_class)->finalize (object);
}

static void
calendar_sources_class_init (CalendarSourcesClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = calendar_sources_constructed;
  gobject_class->finalize = calendar_sources_finalize;

  signals [CLIENT_APPEARED] =
    g_signal_new ("client-appeared",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  E_TYPE_CAL_CLIENT);

  signals [CLIENT_DISAPPEARED] =
    g_signal_new ("client-disappeared",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING); /* ESource::uid of the disappeared client */
}

static void
calendar_sources_init (CalendarSources *sources)
{
  g_mutex_init (&sources->clients_lock);
}

CalendarSources *
calendar_sources_get (void)
{
  static CalendarSources *calendar_sources_singleton = NULL;
  gpointer singleton_location = &calendar_sources_singleton;

  if (calendar_sources_singleton)
    return g_object_ref (calendar_sources_singleton);

  calendar_sources_singleton = g_object_new (CALENDAR_TYPE_SOURCES, NULL);
  g_object_add_weak_pointer (G_OBJECT (calendar_sources_singleton),
                             singleton_location);

  return calendar_sources_singleton;
}

ESourceRegistry *
calendar_sources_get_registry (CalendarSources *sources)
{
  return e_source_registry_watcher_get_registry (sources->registry_watcher);
}

static void
gather_event_clients_cb (gpointer key,
                         gpointer value,
                         gpointer user_data)
{
  GSList **plist = user_data;
  ClientData *cd = value;

  if (cd)
    *plist = g_slist_prepend (*plist, g_object_ref (cd->client));
}

GSList *
calendar_sources_ref_clients (CalendarSources *sources)
{
  GSList *list = NULL;

  g_return_val_if_fail (CALENDAR_IS_SOURCES (sources), NULL);

  g_mutex_lock (&sources->clients_lock);
  g_hash_table_foreach (sources->clients, gather_event_clients_cb, &list);
  g_mutex_unlock (&sources->clients_lock);

  return list;
}

gboolean
calendar_sources_has_clients (CalendarSources *sources)
{
  GHashTableIter iter;
  gpointer value;
  gboolean has = FALSE;

  g_return_val_if_fail (CALENDAR_IS_SOURCES (sources), FALSE);

  g_mutex_lock (&sources->clients_lock);

  g_hash_table_iter_init (&iter, sources->clients);
  while (!has && g_hash_table_iter_next (&iter, NULL, &value))
   {
     ClientData *cd = value;

     has = cd != NULL;
   }

  g_mutex_unlock (&sources->clients_lock);

  return has;
}

static void
backend_died_cb (EClient *client,
                 CalendarSources *sources)
{
  ESource *source;
  const char *display_name;

  source = e_client_get_source (client);
  display_name = e_source_get_display_name (source);
  g_warning ("The calendar backend for '%s' has crashed.", display_name);
  g_mutex_lock (&sources->clients_lock);
  g_hash_table_remove (sources->clients, source);
  g_mutex_unlock (&sources->clients_lock);
}

static EClient *
calendar_sources_connect_client_sync (CalendarSources *sources,
                                      ESource *source,
                                      ECalClientSourceType source_type,
                                      guint32 wait_for_connected_seconds,
                                      GCancellable *cancellable,
                                      GError **error)
{
  EClient *client = NULL;
  ClientData *client_data;

  g_mutex_lock (&sources->clients_lock);
  client_data = g_hash_table_lookup (sources->clients, source);
  if (client_data)
     client = E_CLIENT (g_object_ref (client_data->client));
  g_mutex_unlock (&sources->clients_lock);

  if (client)
    return client;

  client = e_cal_client_connect_sync (source, source_type, wait_for_connected_seconds, cancellable, error);
  if (!client)
    return NULL;

  g_mutex_lock (&sources->clients_lock);
  client_data = g_hash_table_lookup (sources->clients, source);
  if (client_data)
    {
      g_clear_object (&client);
      client = E_CLIENT (g_object_ref (client_data->client));
    }
   else
    {
      client_data = g_new0 (ClientData, 1);
      client_data->client = E_CAL_CLIENT (g_object_ref (client));
      client_data->backend_died_id = g_signal_connect (client,
                                                       "backend-died",
                                                       G_CALLBACK (backend_died_cb),
                                                       sources);

      g_hash_table_insert (sources->clients, g_object_ref (source), client_data);
    }
  g_mutex_unlock (&sources->clients_lock);

  return client;
}

typedef struct _AsyncContext {
  ESource *source;
  ECalClientSourceType source_type;
  guint32 wait_for_connected_seconds;
} AsyncContext;

static void
async_context_free (gpointer ptr)
{
  AsyncContext *ctx = ptr;

  if (ctx)
    {
      g_clear_object (&ctx->source);
      g_free (ctx);
    }
}

static void
calendar_sources_connect_client_thread (GTask *task,
                                        gpointer source_object,
                                        gpointer task_data,
                                        GCancellable *cancellable)
{
  CalendarSources *sources = source_object;
  AsyncContext *ctx = task_data;
  EClient *client;
  GError *local_error = NULL;

  client = calendar_sources_connect_client_sync (sources, ctx->source, ctx->source_type,
                                                 ctx->wait_for_connected_seconds, cancellable, &local_error);
  if (!client)
    {
      if (local_error)
        g_task_return_error (task, local_error);
      else
        g_task_return_pointer (task, NULL, NULL);
    } else {
      g_task_return_pointer (task, client, g_object_unref);
    }
}

void
calendar_sources_connect_client (CalendarSources *sources,
                                 ESource *source,
                                 ECalClientSourceType source_type,
                                 guint32 wait_for_connected_seconds,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
  AsyncContext *ctx;
  g_autoptr (GTask) task = NULL;

  ctx = g_new0 (AsyncContext, 1);
  ctx->source = g_object_ref (source);
  ctx->source_type = source_type;
  ctx->wait_for_connected_seconds = wait_for_connected_seconds;

  task = g_task_new (sources, cancellable, callback, user_data);
  g_task_set_source_tag (task, calendar_sources_connect_client);
  g_task_set_task_data (task, ctx, async_context_free);

  g_task_run_in_thread (task, calendar_sources_connect_client_thread);
}

EClient *
calendar_sources_connect_client_finish (CalendarSources *sources,
                                        GAsyncResult *result,
                                        GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, sources), NULL);
  g_return_val_if_fail (g_async_result_is_tagged (result, calendar_sources_connect_client), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}


void
print_debug (const gchar *format,
             ...)
{
  g_autofree char *s = NULL;
  g_autofree char *timestamp = NULL;
  va_list ap;
  g_autoptr (GDateTime) now = NULL;
  static size_t once_init_value = 0;
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
