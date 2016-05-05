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

/*
 * SECTION:clutter-master-clock
 * @short_description: The master clock for all animations
 *
 * The #ClutterMasterClock class is responsible for advancing all
 * #ClutterTimelines when a stage is being redrawn. The master clock
 * makes sure that the scenegraph is always integrally updated before
 * painting it.
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-master-clock.h"
#include "clutter-master-clock-default.h"
#include "clutter-private.h"

#define clutter_master_clock_get_type   _clutter_master_clock_get_type

typedef ClutterMasterClockIface ClutterMasterClockInterface;

G_DEFINE_INTERFACE (ClutterMasterClock, clutter_master_clock, G_TYPE_OBJECT)

static void
clutter_master_clock_default_init (ClutterMasterClockInterface *iface)
{
}

ClutterMasterClock *
_clutter_master_clock_get_default (void)
{
  ClutterMainContext *context = _clutter_context_get_default ();

  if (G_UNLIKELY (context->master_clock == NULL))
    context->master_clock = g_object_new (CLUTTER_TYPE_MASTER_CLOCK_DEFAULT, NULL);

  return context->master_clock;

}

/*
 * _clutter_master_clock_add_timeline:
 * @master_clock: a #ClutterMasterClock
 * @timeline: a #ClutterTimeline
 *
 * Adds @timeline to the list of playing timelines held by the master
 * clock.
 */
void
_clutter_master_clock_add_timeline (ClutterMasterClock *master_clock,
                                    ClutterTimeline    *timeline)
{
  g_return_if_fail (CLUTTER_IS_MASTER_CLOCK (master_clock));

  CLUTTER_MASTER_CLOCK_GET_IFACE (master_clock)->add_timeline (master_clock,
                                                               timeline);
}

/*
 * _clutter_master_clock_remove_timeline:
 * @master_clock: a #ClutterMasterClock
 * @timeline: a #ClutterTimeline
 *
 * Removes @timeline from the list of playing timelines held by the
 * master clock.
 */
void
_clutter_master_clock_remove_timeline (ClutterMasterClock *master_clock,
                                       ClutterTimeline    *timeline)
{
  g_return_if_fail (CLUTTER_IS_MASTER_CLOCK (master_clock));

  CLUTTER_MASTER_CLOCK_GET_IFACE (master_clock)->remove_timeline (master_clock,
                                                                  timeline);
}

/*
 * _clutter_master_clock_start_running:
 * @master_clock: a #ClutterMasterClock
 *
 * Called when we have events or redraws to process; if the clock
 * is stopped, does the processing necessary to wake it up again.
 */
void
_clutter_master_clock_start_running (ClutterMasterClock *master_clock)
{
  g_return_if_fail (CLUTTER_IS_MASTER_CLOCK (master_clock));

  CLUTTER_MASTER_CLOCK_GET_IFACE (master_clock)->start_running (master_clock);
}

/**
 * _clutter_master_clock_ensure_next_iteration:
 * @master_clock: a #ClutterMasterClock
 *
 * Ensures that the master clock will run at least one iteration
 */
void
_clutter_master_clock_ensure_next_iteration (ClutterMasterClock *master_clock)
{
  g_return_if_fail (CLUTTER_IS_MASTER_CLOCK (master_clock));

  CLUTTER_MASTER_CLOCK_GET_IFACE (master_clock)->ensure_next_iteration (master_clock);
}

void
_clutter_master_clock_set_paused (ClutterMasterClock *master_clock,
                                  gboolean            paused)
{
  g_return_if_fail (CLUTTER_IS_MASTER_CLOCK (master_clock));

  CLUTTER_MASTER_CLOCK_GET_IFACE (master_clock)->set_paused (master_clock,
                                                             !!paused);
}
