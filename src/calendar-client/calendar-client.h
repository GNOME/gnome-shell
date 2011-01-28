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

#ifndef __CALENDAR_CLIENT_H__
#define __CALENDAR_CLIENT_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
  CALENDAR_EVENT_APPOINTMENT = 1 << 0,
  CALENDAR_EVENT_TASK        = 1 << 1,
  CALENDAR_EVENT_ALL         = (1 << 2) - 1
} CalendarEventType;

#define CALENDAR_TYPE_CLIENT        (calendar_client_get_type ())
#define CALENDAR_CLIENT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), CALENDAR_TYPE_CLIENT, CalendarClient))
#define CALENDAR_CLIENT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), CALENDAR_TYPE_CLIENT, CalendarClientClass))
#define CALENDAR_IS_CLIENT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), CALENDAR_TYPE_CLIENT))
#define CALENDAR_IS_CLIENT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), CALENDAR_TYPE_CLIENT))
#define CALENDAR_CLIENT_GET_CLASS(o)(G_TYPE_INSTANCE_GET_CLASS ((o), CALENDAR_TYPE_CLIENT, CalendarClientClass))

typedef struct _CalendarClient        CalendarClient;
typedef struct _CalendarClientClass   CalendarClientClass;
typedef struct _CalendarClientPrivate CalendarClientPrivate;

struct _CalendarClient
{
  GObject                parent;
  CalendarClientPrivate *priv;
};

struct _CalendarClientClass
{
  GObjectClass    parent_class;

  void         (* appointments_changed) (CalendarClient *client);
  void         (* tasks_changed)        (CalendarClient *client);
};


typedef struct
{
  time_t start_time;
  time_t end_time;
} CalendarOccurrence;

typedef struct
{
  char   *uid;
  char   *rid;
  char   *uri;
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
  char  *uid;
  char  *summary;
  char  *description;
  char  *color_string;
  char  *url;
  time_t start_time;
  time_t due_time;
  guint  percent_complete;
  time_t completed_time;
  int    priority;
} CalendarTask;

typedef struct
{
  union
  {
    CalendarAppointment appointment;
    CalendarTask        task;
  } event;
  CalendarEventType type;
} CalendarEvent;

#define CALENDAR_EVENT(e)       ((CalendarEvent *)(e))
#define CALENDAR_APPOINTMENT(e) ((CalendarAppointment *)(e))
#define CALENDAR_TASK(e)        ((CalendarTask *)(e))

typedef void (* CalendarDayIter) (CalendarClient *client,
				  guint           day,
				  gpointer        user_data);


GType           calendar_client_get_type                (void) G_GNUC_CONST;
CalendarClient *calendar_client_new                     (void);

void            calendar_client_get_date                (CalendarClient      *client,
							 guint               *year,
                                                         guint               *month,
							 guint               *day);
void            calendar_client_select_month            (CalendarClient      *client,
							 guint                month,
							 guint                year);
void            calendar_client_select_day              (CalendarClient      *client,
							 guint                day);

GSList         *calendar_client_get_events              (CalendarClient      *client,
							 CalendarEventType    event_mask);
void            calendar_client_foreach_appointment_day (CalendarClient      *client,
							 CalendarDayIter      iter_func,
							 gpointer             user_data);

void            calendar_client_set_task_completed       (CalendarClient     *client,
							  char               *task_uid,
							  gboolean            task_completed,
							  guint               percent_complete);

void calendar_event_free (CalendarEvent *event);

G_END_DECLS

#endif /* __CALENDAR_CLIENT_H__ */
