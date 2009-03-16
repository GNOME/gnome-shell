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

#include <glib-object.h>
#include <clutter/clutter-fixed.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TIMELINE                   (clutter_timeline_get_type ())
#define CLUTTER_TIMELINE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TIMELINE, ClutterTimeline))
#define CLUTTER_TIMELINE_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TIMELINE, ClutterTimelineClass))
#define CLUTTER_IS_TIMELINE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TIMELINE))
#define CLUTTER_IS_TIMELINE_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TIMELINE))
#define CLUTTER_TIMELINE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TIMELINE, ClutterTimelineClass))

/**
 * ClutterTimelineDirection:
 * @CLUTTER_TIMELINE_FORWARD: forward direction for a timeline
 * @CLUTTER_TIMELINE_BACKWARD: backward direction for a timeline
 *
 * The direction of a #ClutterTimeline
 *
 * Since: 0.6
 */
typedef enum {
  CLUTTER_TIMELINE_FORWARD,
  CLUTTER_TIMELINE_BACKWARD
} ClutterTimelineDirection;

typedef struct _ClutterTimeline        ClutterTimeline;
typedef struct _ClutterTimelineClass   ClutterTimelineClass; 
typedef struct _ClutterTimelinePrivate ClutterTimelinePrivate;

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
  GObject parent;
  ClutterTimelinePrivate *priv;
};

/**
 * ClutterTimelineClass:
 * @started: handler for the #ClutterTimeline::started signal
 * @completed: handler for the #ClutterTimeline::completed signal
 * @paused: handler for the #ClutterTimeline::paused signal
 * @new_frame: handler for the #ClutterTimeline::new-frame signal
 * @marker_reached: handler for the #ClutterTimeline::marker-reached signal
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
		          gint             frame_num);

  void (*marker_reached) (ClutterTimeline *timeline,
                          const gchar     *marker_name,
                          gint             frame_num);

  /*< private >*/
  void (*_clutter_timeline_1) (void);
  void (*_clutter_timeline_2) (void);
  void (*_clutter_timeline_3) (void);
  void (*_clutter_timeline_4) (void);
  void (*_clutter_timeline_5) (void);
};

GType clutter_timeline_get_type (void) G_GNUC_CONST;

ClutterTimeline *clutter_timeline_new                   (guint            n_frames,
                                                         guint            fps);
ClutterTimeline *clutter_timeline_new_for_duration      (guint            msecs);
ClutterTimeline *clutter_timeline_clone                 (ClutterTimeline *timeline);

guint            clutter_timeline_get_duration          (ClutterTimeline *timeline);
void             clutter_timeline_set_duration          (ClutterTimeline *timeline,
                                                         guint            msecs);
guint            clutter_timeline_get_speed             (ClutterTimeline *timeline);
void             clutter_timeline_set_speed             (ClutterTimeline *timeline,
                                                         guint            fps);
ClutterTimelineDirection clutter_timeline_get_direction (ClutterTimeline *timeline);
void             clutter_timeline_set_direction         (ClutterTimeline *timeline,
                                                         ClutterTimelineDirection direction);
void             clutter_timeline_start                 (ClutterTimeline *timeline);
void             clutter_timeline_pause                 (ClutterTimeline *timeline);
void             clutter_timeline_stop                  (ClutterTimeline *timeline);
void             clutter_timeline_set_loop              (ClutterTimeline *timeline,
                                                         gboolean         loop);
gboolean         clutter_timeline_get_loop              (ClutterTimeline *timeline);
void             clutter_timeline_rewind                (ClutterTimeline *timeline);
void             clutter_timeline_skip                  (ClutterTimeline *timeline,
                                                         guint            n_frames);
void             clutter_timeline_advance               (ClutterTimeline *timeline,
                                                         guint            frame_num);
gint             clutter_timeline_get_current_frame     (ClutterTimeline *timeline);
gdouble          clutter_timeline_get_progress          (ClutterTimeline *timeline);
CoglFixed        clutter_timeline_get_progressx         (ClutterTimeline *timeline);
void             clutter_timeline_set_n_frames          (ClutterTimeline *timeline,
                                                         guint            n_frames);
guint            clutter_timeline_get_n_frames          (ClutterTimeline *timeline);
gboolean         clutter_timeline_is_playing            (ClutterTimeline *timeline);
void             clutter_timeline_set_delay             (ClutterTimeline *timeline,
                                                         guint            msecs);
guint            clutter_timeline_get_delay             (ClutterTimeline *timeline);
guint            clutter_timeline_get_delta             (ClutterTimeline *timeline,
                                                         guint           *msecs);

void             clutter_timeline_add_marker_at_frame   (ClutterTimeline *timeline,
                                                         const gchar     *marker_name,
                                                         guint            frame_num);
void             clutter_timeline_add_marker_at_time    (ClutterTimeline *timeline,
                                                         const gchar     *marker_name,
                                                         guint            msecs);
void             clutter_timeline_remove_marker         (ClutterTimeline *timeline,
                                                         const gchar     *marker_name);
gchar **         clutter_timeline_list_markers          (ClutterTimeline *timeline,
                                                         gint             frame_num,
                                                         gsize           *n_markers) G_GNUC_MALLOC;
gboolean         clutter_timeline_has_marker            (ClutterTimeline *timeline,
                                                         const gchar     *marker_name);
void             clutter_timeline_advance_to_marker     (ClutterTimeline *timeline,
                                                         const gchar     *marker_name);

G_END_DECLS

#endif /* _CLUTTER_TIMELINE_H__ */
