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

#ifndef REMINDER_WATCHER_H
#define REMINDER_WATCHER_H

#define HANDLE_LIBICAL_MEMORY
#define EDS_DISABLE_DEPRECATED
#include <libecal/libecal.h>

G_BEGIN_DECLS

/* until libecal provides it on its own */
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EReminderWatcher, g_object_unref)

#define REMINDER_TYPE_WATCHER (reminder_watcher_get_type ())
G_DECLARE_FINAL_TYPE (ReminderWatcher, reminder_watcher, REMINDER, WATCHER, EReminderWatcher)

EReminderWatcher *reminder_watcher_new          (ESourceRegistry *registry);

G_END_DECLS

#endif /* REMINDER_WATCHER_H */
