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

struct _ReminderWatcherPrivate {
  GApplication *application; /* not referenced */
  CalendarSources *sources;
  GSettings *settings;

  GMutex dismiss_lock;
  GSList *dismiss; /* EReminderData * */
  GThread *dismiss_thread; /* not referenced, only to know whether it's scheduled */
};

G_DEFINE_TYPE_WITH_PRIVATE (ReminderWatcher, reminder_watcher, E_TYPE_REMINDER_WATCHER)

static const gchar *
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
#if 0
  ICalAttach *attach = NULL;
  GSList *attachments;
  gboolean did_play = FALSE;

  g_return_val_if_fail (rw != NULL, FALSE);
  g_return_val_if_fail (rd != NULL, FALSE);
  g_return_val_if_fail (alarm != NULL, FALSE);

  attachments = e_cal_component_alarm_get_attachments (alarm);
  if (attachments && !attachments->next)
    attach = attachments->data;

  if (attach && i_cal_attach_get_is_url (attach))
    {
      const gchar *url;

      url = i_cal_attach_get_url (attach);
      if (url && *url)
        {
          gchar *filename;
          GError *error = NULL;

          filename = g_filename_from_uri (url, NULL, &error);

          if (!filename)
            ean_debug_print ("Audio notify: Failed to convert URI '%s' to filename: %s\n", url, error ? error->message : "Unknown error");
           else if (g_file_test (filename, G_FILE_TEST_EXISTS))
            {
#ifdef HAVE_CANBERRA
              gint err = ca_context_play (ca_gtk_context_get (), 0,
                                          CA_PROP_MEDIA_FILENAME, filename,
                                          NULL);

              did_play = !err;

              if (err)
                ean_debug_print ("Audio notify: Cannot play file '%s': %s\n", filename, ca_strerror (err));
#else
                ean_debug_print ("Audio notify: Cannot play file '%s': Not compiled with libcanberra\n", filename);
#endif
            }
           else
            ean_debug_print ("Audio notify: File '%s' does not exist\n", filename);

          g_clear_error (&error);
          g_free (filename);
        }
       else
        ean_debug_print ("Audio notify: Alarm has stored empty URL, fallback to default sound\n");
    }
   else if (!attach)
    ean_debug_print ("Audio notify: Alarm has no attachment, fallback to default sound\n");
   else
    ean_debug_print ("Audio notify: Alarm attachment is not a URL to sound file, fallback to default sound\n");

#ifdef HAVE_CANBERRA
  if (!did_play)
    {
      gint err = ca_context_play (ca_gtk_context_get (), 0,
                                  CA_PROP_EVENT_ID, "alarm-clock-elapsed",
                                  NULL);

      did_play = !err;

      if (err)
        ean_debug_print ("Audio notify: Cannot play event sound: %s\n", ca_strerror (err));
    }
#endif

  if (!did_play)
    {
      GdkDisplay *display;

      display = rw->priv->window ? gtk_widget_get_display (rw->priv->window) : NULL;

      if (!display)
        display = gdk_display_get_default ();

      if (display)
        gdk_display_beep (display);
      else
        ean_debug_print ("Audio notify: Cannot beep, no display found\n");
    }
#endif

  print_debug ("ReminderWatcher::Notify Audio for '%s'", reminder_watcher_get_rd_summary (rd));

  return FALSE;
}

static gchar *
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
  GNotification *notification;
#if 0
  GtkIconInfo *icon_info;
#endif
  gchar *description, *notif_id;

  g_return_val_if_fail (rw != NULL, FALSE);
  g_return_val_if_fail (rd != NULL, FALSE);
  g_return_val_if_fail (alarm != NULL, FALSE);

  notif_id = reminder_watcher_build_notif_id (rd);
  description = e_reminder_watcher_describe_data (E_REMINDER_WATCHER (rw), rd, E_REMINDER_WATCHER_DESCRIBE_FLAG_NONE);

  notification = g_notification_new (_("Reminders"));
  g_notification_set_body (notification, description);

#if 0
  icon_info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (), "appointment-soon", GTK_ICON_SIZE_DIALOG, 0);
  if (icon_info)
    {
      const gchar *filename;

      filename = gtk_icon_info_get_filename (icon_info);
      if (filename && *filename)
        {
          GFile *file;
          GIcon *icon;

          file = g_file_new_for_path (filename);
          icon = g_file_icon_new (file);

          if (icon)
            {
              g_notification_set_icon (notification, icon);
              g_object_unref (icon);
            }

          g_object_unref (file);
        }

      gtk_icon_info_free (icon_info);
    }
#endif

  g_application_send_notification (rw->priv->application, notif_id, notification);

  g_object_unref (notification);
  g_free (description);
  g_free (notif_id);

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
                                     const gchar *url)
{
  gchar **list;
  gint ii;
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

#if 0
static void
reminder_watcher_save_blessed_program (GSettings *settings,
                                       const gchar *url)
{
  gchar **list;
  gint ii;
  GPtrArray *array;

  g_return_if_fail (G_IS_SETTINGS (settings));
  g_return_if_fail (url != NULL);

  array = g_ptr_array_new ();

  list = g_settings_get_strv (settings, "notify-programs");

  for (ii = 0; list && list[ii]; ii++)
    {
      if (g_strcmp0 (url, list[ii]) != 0)
        g_ptr_array_add (array, list[ii]);
    }

  g_ptr_array_add (array, (gpointer) url);
  g_ptr_array_add (array, NULL);

  g_settings_set_strv (settings, "notify-programs", (const gchar * const *) array->pdata);

  g_ptr_array_free (array, TRUE);
  g_strfreev (list);
}
#endif

static gboolean
reminder_watcher_can_procedure (ReminderWatcher *rw,
                                const gchar *cmd,
                                const gchar *url)
{
#if 0
  GtkWidget *container;
  GtkWidget *dialog;
  GtkWidget *label;
  GtkWidget *checkbox;
  gchar *str;
  gint response;
#endif

  if (reminder_watcher_is_blessed_program (rw->priv->settings, url))
    return TRUE;

#if 0
  dialog = gtk_dialog_new_with_buttons (
    _("Warning"), GTK_WINDOW (rw->priv->window), 0,
    _("_No"), GTK_RESPONSE_CANCEL,
    _("_Yes"), GTK_RESPONSE_OK,
    NULL);

  str = g_strdup_printf (
    _("A calendar reminder is about to trigger. "
      "This reminder is configured to run the following program:\n\n"
      "        %s\n\n"
      "Are you sure you want to run this program?"),
    cmd);
  label = gtk_label_new (str);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_widget_show (label);

  container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_pack_start (GTK_BOX (container), label, TRUE, TRUE, 4);
  g_free (str);

  checkbox = gtk_check_button_new_with_label (_("Do not ask me about this program again"));
  gtk_widget_show (checkbox);
  gtk_box_pack_start (GTK_BOX (container), checkbox, TRUE, TRUE, 4);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK &&
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox)))
    {
      reminder_watcher_save_blessed_program (rw->priv->settings, url);
    }

  gtk_widget_destroy (dialog);

  return response == GTK_RESPONSE_OK;
#endif

  return FALSE;
}

static gboolean
reminder_watcher_notify_procedure (ReminderWatcher *rw,
                                   const EReminderData *rd,
                                   ECalComponentAlarm *alarm)
{
  ECalComponentText *description;
  ICalAttach *attach = NULL;
  GSList *attachments;
  const gchar *url;
  gchar *cmd;
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
  g_return_val_if_fail (url != NULL, FALSE);

  /* Ask for confirmation before executing the stuff */
  if (description && e_cal_component_text_get_value (description))
    cmd = g_strconcat (url, " ", e_cal_component_text_get_value (description), NULL);
  else
    cmd = (gchar *) url;

  if (reminder_watcher_can_procedure (rw, cmd, url))
    result = g_spawn_command_line_async (cmd, NULL);

  if (cmd != (gchar *) url)
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
          !g_settings_get_boolean (rw->priv->settings, "notify-completed-tasks"))
        return FALSE;
    }

  instance = e_reminder_data_get_instance (rd);

  alarm = instance ? e_cal_component_get_alarm (e_reminder_data_get_component (rd), e_cal_component_alarm_instance_get_uid (instance)) : NULL;
  if (!alarm)
    return FALSE;

  if (!snoozed && !g_settings_get_boolean (rw->priv->settings, "notify-past-events"))
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

  g_return_val_if_fail (IS_REMINDER_WATCHER (rw), NULL);

  g_mutex_lock (&rw->priv->dismiss_lock);
  dismiss = rw->priv->dismiss;
  rw->priv->dismiss = NULL;
  rw->priv->dismiss_thread = NULL;
  g_mutex_unlock (&rw->priv->dismiss_lock);

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

  g_return_if_fail (IS_REMINDER_WATCHER (rw));

  g_mutex_lock (&rw->priv->dismiss_lock);

  for (link = (GSList *) reminders; link; link = g_slist_next (link))
    {
      const EReminderData *rd = link->data;

      if (rd && !reminders_process_one (rw, rd, snoozed))
        {
         rw->priv->dismiss = g_slist_prepend (rw->priv->dismiss, e_reminder_data_copy (rd));
        }
    }

  if (rw->priv->dismiss && !rw->priv->dismiss_thread)
    {
       rw->priv->dismiss_thread = g_thread_new (NULL, reminders_dismiss_thread, g_object_ref (rw));
       g_warn_if_fail (rw->priv->dismiss_thread != NULL);
       if (rw->priv->dismiss_thread)
          g_thread_unref (rw->priv->dismiss_thread);
    }

  g_mutex_unlock (&rw->priv->dismiss_lock);
}

static EClient *
reminder_watcher_cal_client_connect_sync (EReminderWatcher *watcher,
                                          ESource *source,
                                          ECalClientSourceType source_type,
                                          guint32 wait_for_connected_seconds,
                                          GCancellable *cancellable,
                                          GError **error)
{
  ReminderWatcher *reminder_watcher = REMINDER_WATCHER (watcher);

  return calendar_sources_connect_client_sync (reminder_watcher->priv->sources, FALSE, source, source_type,
                                               wait_for_connected_seconds, cancellable, error);
}

static void
reminder_watcher_cal_client_connect (EReminderWatcher *watcher,
                                     ESource *source,
                                     ECalClientSourceType source_type,
                                     guint32 wait_for_connected_seconds,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
  ReminderWatcher *reminder_watcher = REMINDER_WATCHER (watcher);

  calendar_sources_connect_client (reminder_watcher->priv->sources, FALSE, source, source_type,
                                   wait_for_connected_seconds, cancellable, callback, user_data);
}

static EClient *
reminder_watcher_cal_client_connect_finish (EReminderWatcher *watcher,
                                            GAsyncResult *result,
                                            GError **error)
{
  ReminderWatcher *reminder_watcher = REMINDER_WATCHER (watcher);

  return calendar_sources_connect_client_finish (reminder_watcher->priv->sources, result, error);
}

static void
reminder_watcher_constructed (GObject *object)
{
  ReminderWatcher *rw = REMINDER_WATCHER (object);

  G_OBJECT_CLASS (reminder_watcher_parent_class)->constructed (object);

  rw->priv->sources = calendar_sources_get ();

  g_signal_connect (rw, "triggered",
                    G_CALLBACK (reminders_triggered_cb), NULL);
}

static void
reminder_watcher_finalize (GObject *object)
{
  ReminderWatcher *rw = REMINDER_WATCHER (object);

  g_clear_object (&rw->priv->sources);
  g_clear_object (&rw->priv->settings);
  g_mutex_clear (&rw->priv->dismiss_lock);

  G_OBJECT_CLASS (reminder_watcher_parent_class)->finalize (object);
}

static void
reminder_watcher_class_init (ReminderWatcherClass *klass)
{
  GObjectClass *object_class;
  EReminderWatcherClass *watcher_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = reminder_watcher_constructed;
  object_class->finalize = reminder_watcher_finalize;

  watcher_class = E_REMINDER_WATCHER_CLASS (klass);
  watcher_class->cal_client_connect_sync = reminder_watcher_cal_client_connect_sync;
  watcher_class->cal_client_connect = reminder_watcher_cal_client_connect;
  watcher_class->cal_client_connect_finish = reminder_watcher_cal_client_connect_finish;
}

static void
reminder_watcher_init (ReminderWatcher *rw)
{
  rw->priv = reminder_watcher_get_instance_private (rw);
  rw->priv->settings = g_settings_new ("org.gnome.evolution-data-server.calendar");

  g_mutex_init (&rw->priv->dismiss_lock);
}

EReminderWatcher *
reminder_watcher_new (GApplication *application,
                      ESourceRegistry *registry)
{
  ReminderWatcher *rw;

  g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

  rw = g_object_new (TYPE_REMINDER_WATCHER,
                     "registry", registry,
                     NULL);

  rw->priv->application = application;

  return E_REMINDER_WATCHER (rw);
}
