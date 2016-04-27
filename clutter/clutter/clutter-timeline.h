/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_TIMELINE_H__
#define __CLUTTER_TIMELINE_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TIMELINE                   (clutter_timeline_get_type ())
#define CLUTTER_TIMELINE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TIMELINE, ClutterTimeline))
#define CLUTTER_TIMELINE_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TIMELINE, ClutterTimelineClass))
#define CLUTTER_IS_TIMELINE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TIMELINE))
#define CLUTTER_IS_TIMELINE_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TIMELINE))
#define CLUTTER_TIMELINE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TIMELINE, ClutterTimelineClass))

typedef struct _ClutterTimelineClass   ClutterTimelineClass; 
typedef struct _ClutterTimelinePrivate ClutterTimelinePrivate;

/**
 * ClutterTimelineProgressFunc:
 * @timeline: a #ClutterTimeline
 * @elapsed: the elapsed time, in milliseconds
 * @total: the total duration of the timeline, in milliseconds,
 * @user_data: data passed to the function
 *
 * A function for defining a custom progress.
 *
 * Return value: the progress, as a floating point value between -1.0 and 2.0.
 *
 * Since: 1.10
 */
typedef gdouble (* ClutterTimelineProgressFunc) (ClutterTimeline *timeline,
                                                 gdouble          elapsed,
                                                 gdouble          total,
                                                 gpointer         user_data);

/**
 * ClutterTimeline:
 *
 * The #ClutterTimeline structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 0.2
 */
struct _ClutterTimeline
{
  /*< private >*/
  GObject parent_instance;

  ClutterTimelinePrivate *priv;
};

/**
 * ClutterTimelineClass:
 * @started: class handler for the #ClutterTimeline::started signal
 * @completed: class handler for the #ClutterTimeline::completed signal
 * @paused: class handler for the #ClutterTimeline::paused signal
 * @new_frame: class handler for the #ClutterTimeline::new-frame signal
 * @marker_reached: class handler for the #ClutterTimeline::marker-reached signal
 * @stopped: class handler for the #ClutterTimeline::stopped signal
 *
 * The #ClutterTimelineClass structure contains only private data
 *
 * Since: 0.2
 */
struct _ClutterTimelineClass
{
  /*< private >*/
  GObjectClass parent_class;
  
  /*< public >*/
  void (*started)        (ClutterTimeline *timeline);
  void (*completed)      (ClutterTimeline *timeline);
  void (*paused)         (ClutterTimeline *timeline);
  
  void (*new_frame)      (ClutterTimeline *timeline,
		          gint             msecs);

  void (*marker_reached) (ClutterTimeline *timeline,
                          const gchar     *marker_name,
                          gint             msecs);
  void (*stopped)        (ClutterTimeline *timeline,
                          gboolean         is_finished);

  /*< private >*/
  void (*_clutter_timeline_1) (void);
  void (*_clutter_timeline_2) (void);
  void (*_clutter_timeline_3) (void);
  void (*_clutter_timeline_4) (void);
};

CLUTTER_AVAILABLE_IN_ALL
GType clutter_timeline_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_ALL
ClutterTimeline *               clutter_timeline_new                            (guint                     msecs);

CLUTTER_AVAILABLE_IN_ALL
guint                           clutter_timeline_get_duration                   (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_set_duration                   (ClutterTimeline          *timeline,
                                                                                 guint                     msecs);
CLUTTER_AVAILABLE_IN_ALL
ClutterTimelineDirection        clutter_timeline_get_direction                  (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_set_direction                  (ClutterTimeline          *timeline,
                                                                                 ClutterTimelineDirection  direction);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_start                          (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_pause                          (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_stop                           (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_1_6
void                            clutter_timeline_set_auto_reverse               (ClutterTimeline          *timeline,
                                                                                 gboolean                  reverse);
CLUTTER_AVAILABLE_IN_1_6
gboolean                        clutter_timeline_get_auto_reverse               (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_timeline_set_repeat_count               (ClutterTimeline          *timeline,
                                                                                 gint                      count);
CLUTTER_AVAILABLE_IN_1_10
gint                            clutter_timeline_get_repeat_count               (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_rewind                         (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_skip                           (ClutterTimeline          *timeline,
                                                                                 guint                     msecs);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_advance                        (ClutterTimeline          *timeline,
                                                                                 guint                     msecs);
CLUTTER_AVAILABLE_IN_ALL
guint                           clutter_timeline_get_elapsed_time               (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
gdouble                         clutter_timeline_get_progress                   (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
gboolean                        clutter_timeline_is_playing                     (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_set_delay                      (ClutterTimeline          *timeline,
                                                                                 guint                     msecs);
CLUTTER_AVAILABLE_IN_ALL
guint                           clutter_timeline_get_delay                      (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_ALL
guint                           clutter_timeline_get_delta                      (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_1_14
void                            clutter_timeline_add_marker                     (ClutterTimeline          *timeline,
                                                                                 const gchar              *marker_name,
                                                                                 gdouble                   progress);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_add_marker_at_time             (ClutterTimeline          *timeline,
                                                                                 const gchar              *marker_name,
                                                                                 guint                     msecs);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_remove_marker                  (ClutterTimeline          *timeline,
                                                                                 const gchar              *marker_name);
CLUTTER_AVAILABLE_IN_ALL
gchar **                        clutter_timeline_list_markers                   (ClutterTimeline          *timeline,
                                                                                 gint                      msecs,
                                                                                 gsize                    *n_markers) G_GNUC_MALLOC;
CLUTTER_AVAILABLE_IN_ALL
gboolean                        clutter_timeline_has_marker                     (ClutterTimeline          *timeline,
                                                                                 const gchar              *marker_name);
CLUTTER_AVAILABLE_IN_ALL
void                            clutter_timeline_advance_to_marker              (ClutterTimeline          *timeline,
                                                                                 const gchar              *marker_name);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_timeline_set_progress_func              (ClutterTimeline          *timeline,
                                                                                 ClutterTimelineProgressFunc func,
                                                                                 gpointer                  data,
                                                                                 GDestroyNotify            notify);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_timeline_set_progress_mode              (ClutterTimeline          *timeline,
                                                                                 ClutterAnimationMode      mode);
CLUTTER_AVAILABLE_IN_1_10
ClutterAnimationMode            clutter_timeline_get_progress_mode              (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_timeline_set_step_progress              (ClutterTimeline          *timeline,
                                                                                 gint                      n_steps,
                                                                                 ClutterStepMode           step_mode);
CLUTTER_AVAILABLE_IN_1_12
gboolean                        clutter_timeline_get_step_progress              (ClutterTimeline          *timeline,
                                                                                 gint                     *n_steps,
                                                                                 ClutterStepMode          *step_mode);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_timeline_set_cubic_bezier_progress      (ClutterTimeline          *timeline,
                                                                                 const ClutterPoint       *c_1,
                                                                                 const ClutterPoint       *c_2);
CLUTTER_AVAILABLE_IN_1_12
gboolean                        clutter_timeline_get_cubic_bezier_progress      (ClutterTimeline          *timeline,
                                                                                 ClutterPoint             *c_1,
                                                                                 ClutterPoint             *c_2);

CLUTTER_AVAILABLE_IN_1_10
gint64                          clutter_timeline_get_duration_hint              (ClutterTimeline          *timeline);
CLUTTER_AVAILABLE_IN_1_10
gint                            clutter_timeline_get_current_repeat             (ClutterTimeline          *timeline);

G_END_DECLS

#endif /* _CLUTTER_TIMELINE_H__ */
