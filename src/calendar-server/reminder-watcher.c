/*
 * Copyright (C) 2020 Red Hat (www.redhat.com)
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

#include <glib/gi18n-lib.h>

#define HANDLE_LIBICAL_MEMORY
#define EDS_DISABLE_DEPRECATED
#include <libecal/libecal.h>

#include "calendar-sources.h"
#include "reminder-watcher.h"

/* Snooze for 9 minutes */
#define SNOOZE_TIME_SECS (60 * 9)

struct _ReminderWatcher {
  EReminderWatcher parent;

  CalendarSources *sources;
  GSettings *settings;

  GMutex dismiss_lock;
  GSList *dismiss; /* EReminderData * */
  GThread *dismiss_thread; /* not referenced, only to know whether it's scheduled */
};

G_DEFINE_TYPE (ReminderWatcher, reminder_watcher, E_TYPE_REMINDER_WATCHER)

static const char *
reminder_watcher_get_rd_summary (const EReminderData *rd)
{
  if (!rd)
    return NULL;

  return i_cal_component_get_summary (e_cal_component_get_icalcomponent (e_reminder_data_get_component (rd)));
}

static gboolean
reminder_watcher_notify_audio (ReminderWatcher *rw,
                               const EReminderData *rd,
                               ECalComponentAlarm *alarm)
{
  print_debug ("ReminderWatcher::Notify Audio for '%s' not implemented", reminder_watcher_get_rd_summary (rd));

  return FALSE;
}

static char *
reminder_watcher_build_notif_id (const EReminderData *rd)
{
  GString *string;
  ECalComponentId *id;
  ECalComponentAlarmInstance *instance;

  g_return_val_if_fail (rd != NULL, NULL);

  string = g_string_sized_new (32);

  if (e_reminder_data_get_source_uid (rd))
    {
      g_string_append (string, e_reminder_data_get_source_uid (rd));
      g_string_append_c (string, '\n');
    }

  id = e_cal_component_get_id (e_reminder_data_get_component (rd));
  if (id)
    {
      if (e_cal_component_id_get_uid (id))
        {
          g_string_append (string, e_cal_component_id_get_uid (id));
          g_string_append_c (string, '\n');
        }

      if (e_cal_component_id_get_rid (id))
        {
          g_string_append (string, e_cal_component_id_get_rid (id));
          g_string_append_c (string, '\n');
        }

      e_cal_component_id_free (id);
    }

  instance = e_reminder_data_get_instance (rd);

  g_string_append_printf (string, "%" G_GINT64_FORMAT, (gint64) (instance ? e_cal_component_alarm_instance_get_time (instance) : -1));

  return g_string_free (string, FALSE);
}

static gboolean
reminder_watcher_notify_display (ReminderWatcher *rw,
                                 const EReminderData *rd,
                                 ECalComponentAlarm *alarm)
{
  g_autoptr(GNotification) notification = NULL;
  g_autoptr(GIcon) icon = NULL;
  g_autofree char *description = NULL;
  g_autofree char *notif_id = NULL;

  g_return_val_if_fail (rw != NULL, FALSE);
  g_return_val_if_fail (rd != NULL, FALSE);
  g_return_val_if_fail (alarm != NULL, FALSE);

  notif_id = reminder_watcher_build_notif_id (rd);
  description = e_reminder_watcher_describe_data (E_REMINDER_WATCHER (rw), rd, E_REMINDER_WATCHER_DESCRIBE_FLAG_NONE);
  icon = g_themed_icon_new ("appointment-soon");

  notification = g_notification_new (_("Reminders"));
  g_notification_set_body (notification, description);
  g_notification_set_icon (notification, icon);

  g_notification_set_default_action_and_target (notification, "app.open-in-app", "s", notif_id);
  g_notification_add_button_with_target (notification, _("Snooze"), "app.snooze-reminder", "s", notif_id);
  g_notification_add_button_with_target (notification, _("Dismiss"), "app.dismiss-reminder", "s", notif_id);

  g_application_send_notification (g_application_get_default (), notif_id, notification);

  print_debug ("ReminderWatcher::Notify Display for '%s'", reminder_watcher_get_rd_summary (rd));

  return TRUE;
}

static gboolean
reminder_watcher_notify_email (ReminderWatcher *rw,
                               const EReminderData *rd,
                               ECalComponentAlarm *alarm)
{
  print_debug ("ReminderWatcher::Notify Email for '%s'", reminder_watcher_get_rd_summary (rd));

  /* Nothing to do here */
  return FALSE;
}

static gboolean
reminder_watcher_is_blessed_program (GSettings *settings,
                                     const char *url)
{
  char **list;
  int ii;
  gboolean found = FALSE;

  g_return_val_if_fail (G_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (url != NULL, FALSE);

  list = g_settings_get_strv (settings, "notify-programs");

  for (ii = 0; list && list[ii] && !found; ii++)
    {
      found = g_strcmp0 (list[ii], url) == 0;
    }

  g_strfreev (list);

  return found;
}

static gboolean
reminder_watcher_can_procedure (ReminderWatcher *rw,
                                const char *cmd,
                                const char *url)
{
  return reminder_watcher_is_blessed_program (rw->settings, url);
}

static gboolean
reminder_watcher_notify_procedure (ReminderWatcher *rw,
                                   const EReminderData *rd,
                                   ECalComponentAlarm *alarm)
{
  ECalComponentText *description;
  ICalAttach *attach = NULL;
  GSList *attachments;
  const char *url;
  char *cmd;
  gboolean result = FALSE;

  g_return_val_if_fail (rw != NULL, FALSE);
  g_return_val_if_fail (rd != NULL, FALSE);
  g_return_val_if_fail (alarm != NULL, FALSE);

  print_debug ("ReminderWatcher::Notify Procedure for '%s'", reminder_watcher_get_rd_summary (rd));

  attachments = e_cal_component_alarm_get_attachments (alarm);

  if (attachments && !attachments->next)
    attach = attachments->data;

  description = e_cal_component_alarm_get_description (alarm);

  /* If the alarm has no attachment, simply display a notification dialog. */
  if (!attach)
    goto fallback;

  if (!i_cal_attach_get_is_url (attach))
    goto fallback;

  url = i_cal_attach_get_url (attach);
  if (!url)
    goto fallback;

  /* Ask for confirmation before executing the stuff */
  if (description && e_cal_component_text_get_value (description))
    cmd = g_strconcat (url, " ", e_cal_component_text_get_value (description), NULL);
  else
    cmd = (char *) url;

  if (reminder_watcher_can_procedure (rw, cmd, url))
    result = g_spawn_command_line_async (cmd, NULL);

  if (cmd != (char *) url)
    g_free (cmd);

  /* Fall back to display notification if we got an error */
  if (!result)
    goto fallback;

  return FALSE;

 fallback:

  return reminder_watcher_notify_display (rw, rd, alarm);
}

/* Returns %TRUE to keep it, %FALSE to dismiss it */
static gboolean
reminders_process_one (ReminderWatcher *rw,
                       const EReminderData *rd,
                       gboolean snoozed)
{
  ECalComponentAlarm *alarm;
  ECalComponentAlarmInstance *instance;
  ECalComponentAlarmAction action;
  gboolean keep_in_reminders = FALSE;

  g_return_val_if_fail (rw != NULL, FALSE);
  g_return_val_if_fail (rd != NULL, FALSE);

  if (e_cal_component_get_vtype (e_reminder_data_get_component (rd)) == E_CAL_COMPONENT_TODO)
    {
      ICalPropertyStatus status;

      status = e_cal_component_get_status (e_reminder_data_get_component (rd));

      if (status == I_CAL_STATUS_COMPLETED &&
          !g_settings_get_boolean (rw->settings, "notify-completed-tasks"))
        return FALSE;
    }

  instance = e_reminder_data_get_instance (rd);

  alarm = instance ? e_cal_component_get_alarm (e_reminder_data_get_component (rd), e_cal_component_alarm_instance_get_uid (instance)) : NULL;
  if (!alarm)
    return FALSE;

  if (!snoozed && !g_settings_get_boolean (rw->settings, "notify-past-events"))
    {
      ECalComponentAlarmTrigger *trigger;
      time_t offset = 0, event_relative, orig_trigger_day, today;

      trigger = e_cal_component_alarm_get_trigger (alarm);

      switch (trigger ? e_cal_component_alarm_trigger_get_kind (trigger) : E_CAL_COMPONENT_ALARM_TRIGGER_NONE)
        {
          case E_CAL_COMPONENT_ALARM_TRIGGER_NONE:
          case E_CAL_COMPONENT_ALARM_TRIGGER_ABSOLUTE:
            break;

          case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START:
          case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END:
            offset = i_cal_duration_as_int (e_cal_component_alarm_trigger_get_duration (trigger));
            break;

          default:
            break;
        }

        today = time (NULL);
        event_relative = e_cal_component_alarm_instance_get_occur_start (instance) - offset;

        #define CLAMP_TO_DAY(x) ((x) - ((x) % (60 * 60 * 24)))

        event_relative = CLAMP_TO_DAY (event_relative);
        orig_trigger_day = CLAMP_TO_DAY (e_cal_component_alarm_instance_get_time (instance));
        today = CLAMP_TO_DAY (today);

        #undef CLAMP_TO_DAY

        if (event_relative < today && orig_trigger_day < today)
          {
            e_cal_component_alarm_free (alarm);
            return FALSE;
          }
    }

  action = e_cal_component_alarm_get_action (alarm);

  switch (action)
    {
      case E_CAL_COMPONENT_ALARM_AUDIO:
        keep_in_reminders = reminder_watcher_notify_audio (rw, rd, alarm);
        break;

      case E_CAL_COMPONENT_ALARM_DISPLAY:
        keep_in_reminders = reminder_watcher_notify_display (rw, rd, alarm);
        break;

      case E_CAL_COMPONENT_ALARM_EMAIL:
        keep_in_reminders = reminder_watcher_notify_email (rw, rd, alarm);
        break;

      case E_CAL_COMPONENT_ALARM_PROCEDURE:
        keep_in_reminders = reminder_watcher_notify_procedure (rw, rd, alarm);
        break;

      case E_CAL_COMPONENT_ALARM_NONE:
      case E_CAL_COMPONENT_ALARM_UNKNOWN:
        break;
    }

  e_cal_component_alarm_free (alarm);

  return keep_in_reminders;
}

static gpointer
reminders_dismiss_thread (gpointer user_data)
{
  ReminderWatcher *rw = user_data;
  EReminderWatcher *watcher;
  GSList *dismiss, *link;

  g_return_val_if_fail (REMINDER_IS_WATCHER (rw), NULL);

  g_mutex_lock (&rw->dismiss_lock);
  dismiss = rw->dismiss;
  rw->dismiss = NULL;
  rw->dismiss_thread = NULL;
  g_mutex_unlock (&rw->dismiss_lock);

  watcher = E_REMINDER_WATCHER (rw);
  if (watcher)
    {
      for (link = dismiss; link; link = g_slist_next (link))
        {
          EReminderData *rd = link->data;

          if (rd)
            {
              /* Silently ignore any errors here */
              e_reminder_watcher_dismiss_sync (watcher, rd, NULL, NULL);
            }
        }
    }

  g_slist_free_full (dismiss, e_reminder_data_free);
  g_clear_object (&rw);

  return NULL;
}

static void
reminders_triggered_cb (EReminderWatcher *watcher,
                        const GSList *reminders, /* EReminderData * */
                        gboolean snoozed,
                        gpointer user_data)
{
  ReminderWatcher *rw = REMINDER_WATCHER (watcher);
  GSList *link;

  g_return_if_fail (REMINDER_IS_WATCHER (rw));

  g_mutex_lock (&rw->dismiss_lock);

  for (link = (GSList *) reminders; link; link = g_slist_next (link))
    {
      const EReminderData *rd = link->data;

      if (rd && !reminders_process_one (rw, rd, snoozed))
        {
         rw->dismiss = g_slist_prepend (rw->dismiss, e_reminder_data_copy (rd));
        }
    }

  if (rw->dismiss && !rw->dismiss_thread)
    {
       rw->dismiss_thread = g_thread_new (NULL, reminders_dismiss_thread, g_object_ref (rw));
       /* do not set the 'dismiss_thread' to NULL, it's used as a guard
	  of "the thread is starting", where it's still time to add new
	  reminders to be dismissed */
       g_thread_unref (rw->dismiss_thread);
    }

  g_mutex_unlock (&rw->dismiss_lock);
}

static EClient *
reminder_watcher_cal_client_connect_sync (EReminderWatcher *watcher,
                                          ESource *source,
                                          ECalClientSourceType source_type,
                                          unsigned int wait_for_connected_seconds,
                                          GCancellable *cancellable,
                                          GError **error)
{
  ReminderWatcher *reminder_watcher = REMINDER_WATCHER (watcher);

  return calendar_sources_connect_client_sync (reminder_watcher->sources, FALSE, source, source_type,
                                               wait_for_connected_seconds, cancellable, error);
}

static void
reminder_watcher_cal_client_connect (EReminderWatcher *watcher,
                                     ESource *source,
                                     ECalClientSourceType source_type,
                                     unsigned int wait_for_connected_seconds,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
  ReminderWatcher *reminder_watcher = REMINDER_WATCHER (watcher);

  calendar_sources_connect_client (reminder_watcher->sources, FALSE, source, source_type,
                                   wait_for_connected_seconds, cancellable, callback, user_data);
}

static EClient *
reminder_watcher_cal_client_connect_finish (EReminderWatcher *watcher,
                                            GAsyncResult *result,
                                            GError **error)
{
  ReminderWatcher *reminder_watcher = REMINDER_WATCHER (watcher);

  return calendar_sources_connect_client_finish (reminder_watcher->sources, result, error);
}

static void
reminder_watcher_finalize (GObject *object)
{
  ReminderWatcher *rw = REMINDER_WATCHER (object);

  g_clear_object (&rw->sources);
  g_clear_object (&rw->settings);
  g_mutex_clear (&rw->dismiss_lock);

  G_OBJECT_CLASS (reminder_watcher_parent_class)->finalize (object);
}

static void
reminder_watcher_class_init (ReminderWatcherClass *klass)
{
  GObjectClass *object_class;
  EReminderWatcherClass *watcher_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = reminder_watcher_finalize;

  watcher_class = E_REMINDER_WATCHER_CLASS (klass);
  watcher_class->cal_client_connect_sync = reminder_watcher_cal_client_connect_sync;
  watcher_class->cal_client_connect = reminder_watcher_cal_client_connect;
  watcher_class->cal_client_connect_finish = reminder_watcher_cal_client_connect_finish;
}

static void
reminder_watcher_init (ReminderWatcher *rw)
{
  g_mutex_init (&rw->dismiss_lock);

  rw->settings = g_settings_new ("org.gnome.evolution-data-server.calendar");
  rw->sources = calendar_sources_get ();

  g_signal_connect (rw, "triggered",
                    G_CALLBACK (reminders_triggered_cb), NULL);
}

EReminderWatcher *
reminder_watcher_new (ESourceRegistry *registry)
{
  ReminderWatcher *rw;

  g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

  rw = g_object_new (REMINDER_TYPE_WATCHER,
                     "registry", registry,
                     NULL);

  return E_REMINDER_WATCHER (rw);
}

static void
reminder_watcher_dismiss_done_cb (GObject *source_object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
  GError *error = NULL;

  if (!e_reminder_watcher_dismiss_finish (E_REMINDER_WATCHER (source_object), result, &error))
    {
      if (!g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_NOT_SUPPORTED))
        print_debug ("Dismiss: Failed with error: %s", error ? error->message : "Unknown error");

      g_clear_error (&error);
    }
}

static EReminderData *
reminder_watcher_find_by_id (EReminderWatcher *reminder_watcher,
			     const char *id)
{
  EReminderData *res = NULL;
  GSList *past, *link;

  past = e_reminder_watcher_dup_past (reminder_watcher);

  for (link = past; link; link = g_slist_next (link))
    {
      EReminderData *rd = link->data;
      g_autofree char *rd_id = NULL;

      rd_id = reminder_watcher_build_notif_id (rd);

      if (g_strcmp0 (rd_id, id) == 0)
        {
          res = g_steal_pointer (&link->data);
          break;
        }
    }

  g_slist_free_full (past, e_reminder_data_free);

  return res;
}

void
reminder_watcher_dismiss_by_id (EReminderWatcher *reminder_watcher,
                                const char *id)
{
  EReminderData *rd;

  g_return_if_fail (REMINDER_IS_WATCHER (reminder_watcher));
  g_return_if_fail (id && *id);

  rd = reminder_watcher_find_by_id (reminder_watcher, id);

  if (rd != NULL)
    {
      print_debug ("Dismiss: Going to dismiss '%s'", reminder_watcher_get_rd_summary (rd));

      g_application_withdraw_notification (g_application_get_default (), id);

      e_reminder_watcher_dismiss (reminder_watcher, rd, NULL,
                                  reminder_watcher_dismiss_done_cb, NULL);
      e_reminder_data_free (rd);
    }
   else
    {
      print_debug ("Dismiss: Cannot find reminder '%s'", id);
    }
}

void
reminder_watcher_snooze_by_id (EReminderWatcher *reminder_watcher,
                               const char *id)
{
  EReminderData *rd;

  g_return_if_fail (REMINDER_IS_WATCHER (reminder_watcher));
  g_return_if_fail (id && *id);

  rd = reminder_watcher_find_by_id (reminder_watcher, id);

  if (rd != NULL)
    {
      print_debug ("Snooze: Going to snooze '%s'", reminder_watcher_get_rd_summary (rd));

      g_application_withdraw_notification (g_application_get_default (), id);

      e_reminder_watcher_snooze (reminder_watcher, rd, (g_get_real_time () / G_USEC_PER_SEC) + SNOOZE_TIME_SECS);

      e_reminder_data_free (rd);
    }
   else
    {
      print_debug ("Snooze: Cannot find reminder '%s'", id);
    }
}

void
reminder_watcher_open_in_app_by_id (EReminderWatcher *reminder_watcher,
                                    const char *id)
{
  g_autoptr(GAppInfo) app_info = NULL;
  g_autoptr(GError) local_error = NULL;

  app_info = g_app_info_get_default_for_type ("text/calendar", FALSE);
  if (app_info == NULL)
    {
      GList *recommended;

      print_debug ("OpenInApp: No default application for 'text/calendar' found");

      recommended = g_app_info_get_recommended_for_type ("text/calendar");
      if (recommended)
        {
          /* pick the last used, when there's no default app */
          app_info = g_object_ref (recommended->data);

          g_list_free_full (recommended, g_object_unref);
        }
       else
        {
          print_debug ("OpenInApp: No recommended application for 'text/calendar' found");
          return;
        }
    }

  if (g_app_info_launch_uris (app_info, NULL, NULL, &local_error))
    print_debug ("OpenInApp: Launched '%s'", g_app_info_get_id (app_info));
   else
    print_debug ("OpenInApp: Failed to launch '%s': %s", g_app_info_get_id (app_info), local_error ? local_error->message : "Unknown error");
}
