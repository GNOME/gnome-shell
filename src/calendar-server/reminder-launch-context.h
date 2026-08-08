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

#pragma once

#include <gio/gio.h>

#define REMINDER_TYPE_LAUNCH_CONTEXT (reminder_launch_context_get_type ())
G_DECLARE_FINAL_TYPE (ReminderLaunchContext, reminder_launch_context,
                      REMINDER, LAUNCH_CONTEXT, GAppLaunchContext)


G_BEGIN_DECLS

ReminderLaunchContext *reminder_launch_context_new (void);

void reminder_launch_context_set_startup_notify_id (ReminderLaunchContext *context,
                                                    const char            *id);

G_END_DECLS
