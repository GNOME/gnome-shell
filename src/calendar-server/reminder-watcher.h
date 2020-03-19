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

/* Standard GObject macros */
#define TYPE_REMINDER_WATCHER \
        (reminder_watcher_get_type ())
#define REMINDER_WATCHER(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST \
        ((obj), TYPE_REMINDER_WATCHER, ReminderWatcher))
#define REMINDER_WATCHER_CLASS(cls) \
        (G_TYPE_CHECK_CLASS_CAST \
        ((cls), TYPE_REMINDER_WATCHER, ReminderWatcherClass))
#define IS_REMINDER_WATCHER(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE \
        ((obj), TYPE_REMINDER_WATCHER))
#define IS_REMINDER_WATCHER_CLASS(cls) \
        (G_TYPE_CHECK_CLASS_TYPE \
        ((cls), TYPE_REMINDER_WATCHER))
#define REMINDER_WATCHER_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS \
        ((obj), TYPE_REMINDER_WATCHER, ReminderWatcherClass))

G_BEGIN_DECLS

typedef struct _ReminderWatcher ReminderWatcher;
typedef struct _ReminderWatcherClass ReminderWatcherClass;
typedef struct _ReminderWatcherPrivate ReminderWatcherPrivate;

struct _ReminderWatcher {
  EReminderWatcher parent;
  ReminderWatcherPrivate *priv;
};

struct _ReminderWatcherClass {
  EReminderWatcherClass parent_class;
};

GType             reminder_watcher_get_type     (void) G_GNUC_CONST;
EReminderWatcher *reminder_watcher_new          (GApplication *application,
                                                 ESourceRegistry *registry);
void              reminder_watcher_dismiss_by_id(EReminderWatcher *reminder_watcher,
                                                 const gchar *id);

G_END_DECLS

#endif /* REMINDER_WATCHER_H */
