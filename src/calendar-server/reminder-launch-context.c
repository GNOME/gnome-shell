/*
 * Copyright (C) 2026 Sebastian Keller
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

#include "reminder-launch-context.h"

#include <gio/gio.h>

struct _ReminderLaunchContext {
  GAppLaunchContext parent;

  char *startup_notify_id;
};

G_DEFINE_FINAL_TYPE (ReminderLaunchContext, reminder_launch_context, G_TYPE_APP_LAUNCH_CONTEXT)

static void
reminder_launch_context_finalize (GObject *object)
{
  ReminderLaunchContext *context = REMINDER_LAUNCH_CONTEXT (object);

  g_clear_pointer (&context->startup_notify_id, g_free);

  G_OBJECT_CLASS (reminder_launch_context_parent_class)->finalize (object);
}

static char *
reminder_launch_context_get_startup_notify_id (GAppLaunchContext *context,
                                               GAppInfo          *info,
                                               GList             *files)
{
  ReminderLaunchContext *reminder_context = REMINDER_LAUNCH_CONTEXT (context);

  return g_strdup (reminder_context->startup_notify_id);
}

static void
reminder_launch_context_class_init (ReminderLaunchContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GAppLaunchContextClass *context_class = G_APP_LAUNCH_CONTEXT_CLASS (klass);

  gobject_class->finalize = reminder_launch_context_finalize;

  context_class->get_startup_notify_id = reminder_launch_context_get_startup_notify_id;
}

static void
reminder_launch_context_init (ReminderLaunchContext *context)
{
}

ReminderLaunchContext *
reminder_launch_context_new (void)
{
  return g_object_new (REMINDER_TYPE_LAUNCH_CONTEXT, NULL);
}

void
reminder_launch_context_set_startup_notify_id (ReminderLaunchContext *context,
                                               const char            *startup_notify_id)
{
  g_set_str (&context->startup_notify_id, startup_notify_id);
}

