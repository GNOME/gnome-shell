/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Emmanuele Bassi <ebassi@linux.intel.com>
 *
 * Copyright (C) 2009  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CLUTTER_MASTER_CLOCK_H__
#define __CLUTTER_MASTER_CLOCK_H__

#include <clutter/clutter-timeline.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_MASTER_CLOCK       (clutter_master_clock_get_type ())
#define CLUTTER_MASTER_CLOCK(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_MASTER_CLOCK, ClutterMasterClock))
#define CLUTTER_IS_MASTER_CLOCK(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_MASTER_CLOCK))

typedef struct _ClutterMasterClock      ClutterMasterClock;

GType clutter_master_clock_get_type (void) G_GNUC_CONST;

ClutterMasterClock *_clutter_master_clock_get_default     (void);
void                _clutter_master_clock_add_timeline    (ClutterMasterClock *master_clock,
                                                           ClutterTimeline    *timeline);
void                _clutter_master_clock_remove_timeline (ClutterMasterClock *master_clock,
                                                           ClutterTimeline    *timeline);
void                _clutter_master_clock_advance         (ClutterMasterClock *master_clock);
void                _clutter_master_clock_start_running   (ClutterMasterClock *master_clock);


G_END_DECLS

#endif /* __CLUTTER_MASTER_CLOCK_H__ */
