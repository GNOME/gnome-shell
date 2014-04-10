/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define GST_USE_UNSTABLE_API
#include <gst/gst.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <cogl/cogl.h>
#include <meta/screen.h>
#include <meta/meta-cursor-tracker.h>

#include "shell-recorder-src.h"
#include "shell-recorder.h"

#include <clutter/x11/clutter-x11.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

/* This is also hard-coded in mutter and GDK */
#define VIRTUAL_CORE_POINTER_ID 2

#define A11Y_APPS_SCHEMA "org.gnome.desktop.a11y.applications"
#define MAGNIFIER_ACTIVE_KEY "screen-magnifier-enabled"

typedef enum {
  RECORDER_STATE_CLOSED,
  RECORDER_STATE_RECORDING
} RecorderState;

typedef struct _RecorderPipeline RecorderPipeline;

struct _ShellRecorderClass
{
  GObjectClass parent_class;
};

struct _ShellRecorder {
  GObject parent;

  /* A "maximum" amount of memory to use for buffering. This is used
   * to alert the user that they are filling up memory rather than
   * any that actually affects recording. (In kB)
   */
  guint memory_target;
  guint memory_used; /* Current memory used. (In kB) */

  RecorderState state;

  ClutterStage *stage;
  gboolean custom_area;
  cairo_rectangle_int_t area;
  int stage_width;
  int stage_height;

  GdkScreen *gdk_screen;

  gboolean have_pointer;
  int pointer_x;
  int pointer_y;

  int xinput_opcode;

  GSettings *a11y_settings;
  gboolean draw_cursor;
  MetaCursorTracker *cursor_tracker;
  cairo_surface_t *cursor_image;
  guint8 *cursor_memory;
  int cursor_hot_x;
  int cursor_hot_y;

  int framerate;
  char *pipeline_description;
  char *file_template;

  /* We might have multiple pipelines that are finishing encoding
   * to go along with the current pipeline where we are recording.
   */
  RecorderPipeline *current_pipeline; /* current pipeline */
  GSList *pipelines; /* all pipelines */

  GstClockTime start_time; /* When we started recording */
  GstClockTime last_frame_time; /* Timestamp for the last frame */

  /* GSource IDs for different timeouts and idles */
  guint redraw_timeout;
  guint redraw_idle;
  guint update_memory_used_timeout;
  guint update_pointer_timeout;
  guint repaint_hook_id;
};

struct _RecorderPipeline
{
  ShellRecorder *recorder;
  GstElement *pipeline;
  GstElement *src;
  int outfile;
  char *filename;
};

static void recorder_set_stage    (ShellRecorder *recorder,
                                   ClutterStage  *stage);
static void recorder_set_framerate (ShellRecorder *recorder,
                                    int framerate);
static void recorder_set_pipeline (ShellRecorder *recorder,
                                   const char    *pipeline);
static void recorder_set_file_template (ShellRecorder *recorder,
                                        const char    *file_template);
static void recorder_set_draw_cursor (ShellRecorder *recorder,
                                      gboolean       draw_cursor);

static void recorder_pipeline_set_caps (RecorderPipeline *pipeline);
static void recorder_pipeline_closed   (RecorderPipeline *pipeline);

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_STAGE,
  PROP_FRAMERATE,
  PROP_PIPELINE,
  PROP_FILE_TEMPLATE,
  PROP_DRAW_CURSOR
};

G_DEFINE_TYPE(ShellRecorder, shell_recorder, G_TYPE_OBJECT);

/* The default value of the target frame rate; we'll never record more
 * than this many frames per second, though we may record less if the
 * screen isn't being redrawn. 30 is a compromise between smoothness
 * and the size of the recording.
 */
#define DEFAULT_FRAMES_PER_SECOND 30

/* The time (in milliseconds) between querying the server for the cursor
 * position.
 */
#define UPDATE_POINTER_TIME 100

/* The time we wait (in milliseconds) before redrawing when the memory used
 * changes.
 */
#define UPDATE_MEMORY_USED_DELAY 500

/* Maximum time between frames, in milliseconds. If we don't send data
 * for a long period of time, then when we send the next frame, a lot
 * of work can be created for the encoder to do, so we want to force a
 * periodic redraw when nothing happen.
 */
#define MAXIMUM_PAUSE_TIME 1000

/* The default pipeline. videorate is used to give a constant stream of
 * frames to theora even if there is a pause because nothing is moving.
 * (Theora does have some support for frames at non-uniform times, but
 * things seem to break down if there are large gaps.)
 */
#define DEFAULT_PIPELINE "vp8enc min_quantizer=13 max_quantizer=13 cpu-used=5 deadline=1000000 threads=%T ! queue ! webmmux"

/* If we can find the amount of memory on the machine, we use half
 * of that for memory_target, otherwise, we use this value, in kB.
 */
#define DEFAULT_MEMORY_TARGET (512*1024)

static guint
get_memory_target (void)
{
  FILE *f;

  /* Really simple "get amount of memory on the machine" if it
   * doesn't work, you just get the default memory target.
   */
  f = fopen("/proc/meminfo", "r");
  if (!f)
    return DEFAULT_MEMORY_TARGET;

  while (!feof(f))
    {
      gchar line_buffer[1024];
      guint mem_total;
      if (fscanf(f, "MemTotal: %u", &mem_total) == 1)
        {
          fclose(f);
          return mem_total / 2;
        }
      /* Skip to the next line and discard what we read */
      if (fgets(line_buffer, sizeof(line_buffer), f) == NULL)
        break;
    }

  fclose(f);

  return DEFAULT_MEMORY_TARGET;
}

/*
 * Used to force full stage redraws during recording to avoid artifacts
 *
 * Note: That this will cause the stage to be repainted on
 * every animation frame even if the frame wouldn't normally cause any new
 * drawing
 */
static gboolean
recorder_repaint_hook (gpointer data)
{
  ClutterActor *stage = data;
  clutter_actor_queue_redraw (stage);

  return TRUE;
}

static void
shell_recorder_init (ShellRecorder *recorder)
{
  /* Calling gst_init() is a no-op if GStreamer was previously initialized */
  gst_init (NULL, NULL);

  shell_recorder_src_register ();

  recorder->gdk_screen = gdk_screen_get_default ();

  recorder->memory_target = get_memory_target();

  recorder->a11y_settings = g_settings_new (A11Y_APPS_SCHEMA);

  recorder->state = RECORDER_STATE_CLOSED;
  recorder->framerate = DEFAULT_FRAMES_PER_SECOND;
  recorder->draw_cursor = TRUE;
}

static void
shell_recorder_finalize (GObject  *object)
{
  ShellRecorder *recorder = SHELL_RECORDER (object);

  if (recorder->update_memory_used_timeout)
    g_source_remove (recorder->update_memory_used_timeout);

  if (recorder->cursor_image)
    cairo_surface_destroy (recorder->cursor_image);
  if (recorder->cursor_memory)
    g_free (recorder->cursor_memory);

  recorder_set_stage (recorder, NULL);
  recorder_set_pipeline (recorder, NULL);
  recorder_set_file_template (recorder, NULL);

  g_clear_object (&recorder->a11y_settings);

  G_OBJECT_CLASS (shell_recorder_parent_class)->finalize (object);
}

static void
recorder_on_stage_destroy (ClutterActor  *actor,
                           ShellRecorder *recorder)
{
  recorder_set_stage (recorder, NULL);
}

/* Add together the memory used by all pipelines; both the
 * currently recording pipeline and pipelines finishing
 * recording asynchronously.
 */
static void
recorder_update_memory_used (ShellRecorder *recorder,
                             gboolean       repaint)
{
  guint memory_used = 0;
  GSList *l;

  for (l = recorder->pipelines; l; l = l->next)
    {
      RecorderPipeline *pipeline = l->data;
      guint pipeline_memory_used;

      g_object_get (pipeline->src,
                    "memory-used", &pipeline_memory_used,
                    NULL);
      memory_used += pipeline_memory_used;
    }

  if (memory_used != recorder->memory_used)
    recorder->memory_used = memory_used;
}

/* Timeout used to avoid not drawing for more than MAXIMUM_PAUSE_TIME
 */
static gboolean
recorder_redraw_timeout (gpointer data)
{
  ShellRecorder *recorder = data;

  recorder->redraw_timeout = 0;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (recorder->stage));

  return FALSE;
}

static void
recorder_add_redraw_timeout (ShellRecorder *recorder)
{
  if (recorder->redraw_timeout == 0)
    {
      recorder->redraw_timeout = g_timeout_add (MAXIMUM_PAUSE_TIME,
                                                recorder_redraw_timeout,
                                                recorder);
      g_source_set_name_by_id (recorder->redraw_timeout, "[gnome-shell] recorder_redraw_timeout");
    }
}

static void
recorder_remove_redraw_timeout (ShellRecorder *recorder)
{
  if (recorder->redraw_timeout != 0)
    {
      g_source_remove (recorder->redraw_timeout);
      recorder->redraw_timeout = 0;
    }
}

static void
recorder_fetch_cursor_image (ShellRecorder *recorder)
{
  CoglTexture *texture;
  int width, height;
  int stride;
  guint8 *data;

  texture = meta_cursor_tracker_get_sprite (recorder->cursor_tracker);
  width = cogl_texture_get_width (texture);
  height = cogl_texture_get_height (texture);
  stride = 4 * width;
  data = g_new (guint8, stride * height);
  cogl_texture_get_data (texture, CLUTTER_CAIRO_FORMAT_ARGB32, stride, data);

  /* FIXME: cairo-gl? */
  recorder->cursor_image = cairo_image_surface_create_for_data (data,
                                                                CAIRO_FORMAT_ARGB32,
                                                                width, height,
                                                                stride);
  recorder->cursor_memory = data;
}

/* Overlay the cursor image on the frame. We draw the cursor image
 * into the host-memory buffer after  we've captured the frame. An
 * alternate approach would be to turn off the cursor while recording
 * and draw the cursor ourselves with GL, but then we'd need to figure
 * out what the cursor looks like, or hard-code a non-system cursor.
 */
static void
recorder_draw_cursor (ShellRecorder *recorder,
                      GstBuffer     *buffer)
{
  GstMapInfo info;
  cairo_surface_t *surface;
  cairo_t *cr;

  /* We don't show a cursor unless the hot spot is in the frame; this
   * means that sometimes we aren't going to draw a cursor even when
   * there is a little bit overlapping within the stage */
  if (recorder->pointer_x < recorder->area.x ||
      recorder->pointer_y < recorder->area.y ||
      recorder->pointer_x >= recorder->area.x + recorder->area.width ||
      recorder->pointer_y >= recorder->area.y + recorder->area.height)
    return;

  if (!recorder->cursor_image)
    recorder_fetch_cursor_image (recorder);

  if (!recorder->cursor_image)
    return;

  gst_buffer_map (buffer, &info, GST_MAP_WRITE);
  surface = cairo_image_surface_create_for_data (info.data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 recorder->area.width,
                                                 recorder->area.height,
                                                 recorder->area.width * 4);

  cr = cairo_create (surface);
  cairo_set_source_surface (cr,
                            recorder->cursor_image,
                            recorder->pointer_x - recorder->cursor_hot_x - recorder->area.x,
                            recorder->pointer_y - recorder->cursor_hot_y - recorder->area.y);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  gst_buffer_unmap (buffer, &info);
}

/* We want to time-stamp each frame based on the actual time it was
 * recorded. We probably should use the pipeline clock rather than
 * gettimeofday(): that would be needed to get sync'ed audio correct.
 * I'm not immediately sure how to handle the adjustment we currently
 * do when pausing recording - is pausing the pipeline enough?
 */
static GstClockTime
get_wall_time (void)
{
  GTimeVal tv;

  g_get_current_time (&tv);

  return tv.tv_sec * 1000000000LL + tv.tv_usec * 1000LL;
}

/* Retrieve a frame and feed it into the pipeline
 */
static void
recorder_record_frame (ShellRecorder *recorder)
{
  GstBuffer *buffer;
  guint8 *data;
  guint size;
  GstClockTime now;

  g_return_if_fail (recorder->current_pipeline != NULL);

  /* If we get into the red zone, stop buffering new frames; 13/16 is
  * a bit more than the 3/4 threshold for a red indicator to keep the
  * indicator from flashing between red and yellow. */
  if (recorder->memory_used > (recorder->memory_target * 13) / 16)
    return;

  /* Drop frames to get down to something like the target frame rate; since frames
   * are generated with VBlank sync, we don't have full control anyways, so we just
   * drop frames if the interval since the last frame is less than 75% of the
   * desired inter-frame interval.
   */
  now = get_wall_time();
  if (now - recorder->last_frame_time < (3 * 1000000000LL / (4 * recorder->framerate)))
    return;

  recorder->last_frame_time = now;

  size = recorder->area.width * recorder->area.height * 4;

  data = g_malloc (recorder->area.width * 4 * recorder->area.height);
  cogl_read_pixels (recorder->area.x,
                    recorder->area.y,
                    recorder->area.width,
                    recorder->area.height,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    CLUTTER_CAIRO_FORMAT_ARGB32,
                    data);

  buffer = gst_buffer_new();
  gst_buffer_insert_memory (buffer, -1,
                            gst_memory_new_wrapped (0, data, size, 0,
                                                    size, data, g_free));

  GST_BUFFER_PTS(buffer) = now - recorder->start_time;

  if (recorder->draw_cursor &&
      !g_settings_get_boolean (recorder->a11y_settings, MAGNIFIER_ACTIVE_KEY))
    recorder_draw_cursor (recorder, buffer);

  shell_recorder_src_add_buffer (SHELL_RECORDER_SRC (recorder->current_pipeline->src), buffer);
  gst_buffer_unref (buffer);

  /* Reset the timeout that we used to avoid an overlong pause in the stream */
  recorder_remove_redraw_timeout (recorder);
  recorder_add_redraw_timeout (recorder);
}

/* We hook in by recording each frame right after the stage is painted
 * by clutter before glSwapBuffers() makes it visible to the user.
 */
static void
recorder_on_stage_paint (ClutterActor  *actor,
                         ShellRecorder *recorder)
{
  if (recorder->state == RECORDER_STATE_RECORDING)
    recorder_record_frame (recorder);
}

static void
recorder_update_size (ShellRecorder *recorder)
{
  ClutterActorBox allocation;

  clutter_actor_get_allocation_box (CLUTTER_ACTOR (recorder->stage), &allocation);
  recorder->stage_width = (int)(0.5 + allocation.x2 - allocation.x1);
  recorder->stage_height = (int)(0.5 + allocation.y2 - allocation.y1);

  if (!recorder->custom_area)
    {
      recorder->area.x = 0;
      recorder->area.y = 0;
      recorder->area.width = recorder->stage_width;
      recorder->area.height = recorder->stage_height;
    }
}

static void
recorder_on_stage_notify_size (GObject          *object,
                               GParamSpec       *pspec,
                               ShellRecorder    *recorder)
{
  recorder_update_size (recorder);

  /* This breaks the recording but tweaking the GStreamer pipeline a bit
   * might make it work, at least if the codec can handle a stream where
   * the frame size changes in the middle.
   */
  if (recorder->current_pipeline)
    recorder_pipeline_set_caps (recorder->current_pipeline);
}

static gboolean
recorder_idle_redraw (gpointer data)
{
  ShellRecorder *recorder = data;

  recorder->redraw_idle = 0;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (recorder->stage));

  return FALSE;
}

static void
recorder_queue_redraw (ShellRecorder *recorder)
{
  /* If we just queue a redraw on every mouse motion (for example), we
   * starve Clutter, which operates at a very low priority. So
   * we need to queue a "low priority redraw" after timeline updates
   */
  if (recorder->state == RECORDER_STATE_RECORDING && recorder->redraw_idle == 0)
    {
      recorder->redraw_idle = g_idle_add_full (CLUTTER_PRIORITY_REDRAW + 1,
                                               recorder_idle_redraw, recorder, NULL);
      g_source_set_name_by_id (recorder->redraw_idle, "[gnome-shell] recorder_idle_redraw");
    }
}

static void
on_cursor_changed (MetaCursorTracker *tracker,
                   ShellRecorder     *recorder)
{
  if (recorder->cursor_image)
    {
      cairo_surface_destroy (recorder->cursor_image);
      recorder->cursor_image = NULL;
    }
  if (recorder->cursor_memory)
    {
      g_free (recorder->cursor_memory);
      recorder->cursor_memory = NULL;
    }

  recorder_queue_redraw (recorder);
}

/* We use an event filter on the stage to get the XFixesCursorNotifyEvent
 * and also to track cursor position (when the cursor is over the stage's
 * input area); tracking cursor position here rather than with ClutterEvent
 * allows us to avoid worrying about event propagation and competing
 * signal handlers.
 */
static ClutterX11FilterReturn
recorder_event_filter (XEvent        *xev,
                       ClutterEvent  *cev,
                       gpointer       data)
{
  ShellRecorder *recorder = data;
  XIEvent *input_event = NULL;

  if (xev->xany.window != clutter_x11_get_stage_window (recorder->stage))
    return CLUTTER_X11_FILTER_CONTINUE;

  if (xev->xany.type == GenericEvent &&
      xev->xcookie.extension == recorder->xinput_opcode)
    input_event = (XIEvent *) xev->xcookie.data;

  if (input_event != NULL &&
      input_event->evtype == XI_Motion)
    {
      XIDeviceEvent *device_event = (XIDeviceEvent *) input_event;
      if (device_event->deviceid == VIRTUAL_CORE_POINTER_ID)
        {
          recorder->pointer_x = device_event->event_x;
          recorder->pointer_y = device_event->event_y;

          recorder_queue_redraw (recorder);
        }
    }
  /* We want to track whether the pointer is over the stage
   * window itself, and not in a child window. A "virtual"
   * crossing is one that goes directly from ancestor to child.
   */
  else if (input_event != NULL &&
           input_event->evtype == XI_Enter)
    {
      XIEnterEvent *enter_event = (XIEnterEvent *) input_event;

      if (enter_event->deviceid == VIRTUAL_CORE_POINTER_ID &&
          (enter_event->detail != XINotifyVirtual &&
           enter_event->detail != XINotifyNonlinearVirtual))
        {
          recorder->have_pointer = TRUE;
          recorder->pointer_x = enter_event->event_x;
          recorder->pointer_y = enter_event->event_y;

          recorder_queue_redraw (recorder);
        }
    }
  else if (input_event != NULL &&
           input_event->evtype == XI_Leave)
    {
      XILeaveEvent *leave_event = (XILeaveEvent *) input_event;

      if (leave_event->deviceid == VIRTUAL_CORE_POINTER_ID &&
          (leave_event->detail != XINotifyVirtual &&
           leave_event->detail != XINotifyNonlinearVirtual))
        {
          recorder->have_pointer = FALSE;
          recorder->pointer_x = leave_event->event_x;
          recorder->pointer_y = leave_event->event_y;

          recorder_queue_redraw (recorder);
        }
    }

  return CLUTTER_X11_FILTER_CONTINUE;
}

/* We optimize out querying the server for the pointer position if the
 * pointer is in the input area of the ClutterStage. We track changes to
 * that with Enter/Leave events, but we need to 100% accurate about the
 * initial condition, which is a little involved.
 */
static void
recorder_get_initial_cursor_position (ShellRecorder *recorder)
{
  Display *xdisplay = clutter_x11_get_default_display ();
  Window xwindow = clutter_x11_get_stage_window (recorder->stage);
  XWindowAttributes xwa;
  Window root, child, parent;
  Window *children;
  guint n_children;
  int root_x,root_y;
  int window_x, window_y;
  guint mask;

  XGrabServer(xdisplay);

  XGetWindowAttributes (xdisplay, xwindow, &xwa);
  XQueryTree (xdisplay, xwindow, &root, &parent, &children, &n_children);
  XFree (children);

  if (xwa.map_state == IsViewable &&
      XQueryPointer (xdisplay, parent,
                     &root, &child, &root_x, &root_y, &window_x, &window_y, &mask) &&
      child == xwindow)
    {
      /* The point of this call is not actually to translate the coordinates -
       * we could do that ourselves using xwa.{x,y} -  but rather to see if
       * the pointer is in a child of the window, which we count as "not in
       * window", because we aren't guaranteed to get pointer events.
       */
      XTranslateCoordinates(xdisplay, parent, xwindow,
                            window_x, window_y,
                            &window_x, &window_y, &child);
      if (child == None)
        {
          recorder->have_pointer = TRUE;
          recorder->pointer_x = window_x;
          recorder->pointer_y = window_y;
        }
    }
  else
    recorder->have_pointer = FALSE;

  XUngrabServer(xdisplay);
  XFlush(xdisplay);
}

/* When the cursor is not over the stage's input area, we query for the
 * pointer position in a timeout.
 */
static void
recorder_update_pointer (ShellRecorder *recorder)
{
  Display *xdisplay = clutter_x11_get_default_display ();
  Window xwindow = clutter_x11_get_stage_window (recorder->stage);
  Window root, child;
  int root_x,root_y;
  int window_x, window_y;
  guint mask;

  if (recorder->have_pointer)
    return;

  if (XQueryPointer (xdisplay, xwindow,
                     &root, &child, &root_x, &root_y, &window_x, &window_y, &mask))
    {
      if (window_x != recorder->pointer_x || window_y != recorder->pointer_y)
        {
          recorder->pointer_x = window_x;
          recorder->pointer_y = window_y;

          recorder_queue_redraw (recorder);
        }
    }
}

static gboolean
recorder_update_pointer_timeout (gpointer data)
{
  recorder_update_pointer (data);

  return TRUE;
}

static void
recorder_add_update_pointer_timeout (ShellRecorder *recorder)
{
  if (!recorder->update_pointer_timeout)
    {
      recorder->update_pointer_timeout = g_timeout_add (UPDATE_POINTER_TIME,
                                                        recorder_update_pointer_timeout,
                                                        recorder);
      g_source_set_name_by_id (recorder->update_pointer_timeout, "[gnome-shell] recorder_update_pointer_timeout");
    }
}

static void
recorder_remove_update_pointer_timeout (ShellRecorder *recorder)
{
  if (recorder->update_pointer_timeout)
    {
      g_source_remove (recorder->update_pointer_timeout);
      recorder->update_pointer_timeout = 0;
    }
}

static void
recorder_connect_stage_callbacks (ShellRecorder *recorder)
{
  g_signal_connect (recorder->stage, "destroy",
                    G_CALLBACK (recorder_on_stage_destroy), recorder);
  g_signal_connect_after (recorder->stage, "paint",
                          G_CALLBACK (recorder_on_stage_paint), recorder);
  g_signal_connect (recorder->stage, "notify::width",
                    G_CALLBACK (recorder_on_stage_notify_size), recorder);
  g_signal_connect (recorder->stage, "notify::width",
                    G_CALLBACK (recorder_on_stage_notify_size), recorder);

  clutter_x11_add_filter (recorder_event_filter, recorder);
}

static void
recorder_disconnect_stage_callbacks (ShellRecorder *recorder)
{
  g_signal_handlers_disconnect_by_func (recorder->stage,
                                        (void *)recorder_on_stage_destroy,
                                        recorder);
  g_signal_handlers_disconnect_by_func (recorder->stage,
                                        (void *)recorder_on_stage_paint,
                                        recorder);
  g_signal_handlers_disconnect_by_func (recorder->stage,
                                        (void *)recorder_on_stage_notify_size,
                                        recorder);

  clutter_x11_remove_filter (recorder_event_filter, recorder);

  /* We don't don't deselect for cursor changes in case someone else just
   * happened to be selecting for cursor events on the same window; sending
   * us the events is close to free in any case.
   */

  if (recorder->redraw_idle)
    {
      g_source_remove (recorder->redraw_idle);
      recorder->redraw_idle = 0;
    }
}

static void
recorder_set_stage (ShellRecorder *recorder,
                    ClutterStage  *stage)
{
  if (recorder->stage == stage)
    return;

  if (recorder->current_pipeline)
    shell_recorder_close (recorder);

  if (recorder->stage)
   recorder_disconnect_stage_callbacks (recorder);

  recorder->stage = stage;

  if (recorder->stage)
    {
      int error_base, event_base;
      int major = 2, minor = 3;

      recorder->stage = stage;

      recorder_update_size (recorder);

      if (XQueryExtension (clutter_x11_get_default_display (),
                           "XInputExtension",
                           &recorder->xinput_opcode,
                           &error_base,
                           &event_base))
        {
          if (XIQueryVersion (clutter_x11_get_default_display (), &major, &minor) == Success)
            {
              int version = (major * 10) + minor;
              if (version < 22)
                g_warning("ShellRecorder: xinput version %d.%d is too old", major, minor);
            }
          else
            {
              g_warning("ShellRecorder: xinput version could not be queried");
            }
        }
      else
        {
          g_warning("ShellRecorder: xinput extension unavailable");
        }

      clutter_stage_ensure_current (stage);

      recorder_get_initial_cursor_position (recorder);
    }
}

static void
recorder_set_screen (ShellRecorder *recorder,
                     MetaScreen    *screen)
{
  MetaCursorTracker *tracker;

  tracker = meta_cursor_tracker_get_for_screen (screen);

  if (tracker == recorder->cursor_tracker)
    return;

  recorder->cursor_tracker = tracker;
  g_signal_connect_object (tracker, "cursor-changed",
                           G_CALLBACK (on_cursor_changed), recorder, 0);
}

static void
recorder_set_framerate (ShellRecorder *recorder,
                        int framerate)
{
  if (framerate == recorder->framerate)
    return;

  if (recorder->current_pipeline)
    shell_recorder_close (recorder);

  recorder->framerate = framerate;

  g_object_notify (G_OBJECT (recorder), "framerate");
}

static void
recorder_set_pipeline (ShellRecorder *recorder,
                       const char    *pipeline)
{
  if (pipeline == recorder->pipeline_description ||
      (pipeline && recorder->pipeline_description && strcmp (recorder->pipeline_description, pipeline) == 0))
    return;

  if (recorder->current_pipeline)
    shell_recorder_close (recorder);

  if (recorder->pipeline_description)
    g_free (recorder->pipeline_description);

  recorder->pipeline_description = g_strdup (pipeline);

  g_object_notify (G_OBJECT (recorder), "pipeline");
}

static void
recorder_set_file_template (ShellRecorder *recorder,
                            const char    *file_template)
{
  if (file_template == recorder->file_template ||
      (file_template && recorder->file_template && strcmp (recorder->file_template, file_template) == 0))
    return;

  if (recorder->current_pipeline)
    shell_recorder_close (recorder);

  if (recorder->file_template)
    g_free (recorder->file_template);

  recorder->file_template = g_strdup (file_template);

  g_object_notify (G_OBJECT (recorder), "file-template");
}

static void
recorder_set_draw_cursor (ShellRecorder *recorder,
                          gboolean       draw_cursor)
{
  if (draw_cursor == recorder->draw_cursor)
    return;

  recorder->draw_cursor = draw_cursor;

  g_object_notify (G_OBJECT (recorder), "draw-cursor");
}

static void
shell_recorder_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ShellRecorder *recorder = SHELL_RECORDER (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      recorder_set_screen (recorder, g_value_get_object (value));
      break;
    case PROP_STAGE:
      recorder_set_stage (recorder, g_value_get_object (value));
      break;
    case PROP_FRAMERATE:
      recorder_set_framerate (recorder, g_value_get_int (value));
      break;
    case PROP_PIPELINE:
      recorder_set_pipeline (recorder, g_value_get_string (value));
      break;
    case PROP_FILE_TEMPLATE:
      recorder_set_file_template (recorder, g_value_get_string (value));
      break;
    case PROP_DRAW_CURSOR:
      recorder_set_draw_cursor (recorder, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_recorder_get_property (GObject         *object,
                             guint            prop_id,
                             GValue          *value,
                             GParamSpec      *pspec)
{
  ShellRecorder *recorder = SHELL_RECORDER (object);

  switch (prop_id)
    {
    case PROP_STAGE:
      g_value_set_object (value, G_OBJECT (recorder->stage));
      break;
    case PROP_FRAMERATE:
      g_value_set_int (value, recorder->framerate);
      break;
    case PROP_PIPELINE:
      g_value_set_string (value, recorder->pipeline_description);
      break;
    case PROP_FILE_TEMPLATE:
      g_value_set_string (value, recorder->file_template);
      break;
    case PROP_DRAW_CURSOR:
      g_value_set_boolean (value, recorder->draw_cursor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_recorder_class_init (ShellRecorderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shell_recorder_finalize;
  gobject_class->get_property = shell_recorder_get_property;
  gobject_class->set_property = shell_recorder_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_SCREEN,
                                   g_param_spec_object ("screen",
                                                        "Screen",
                                                        "Screen to record",
                                                        META_TYPE_SCREEN,
                                                        G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_STAGE,
                                   g_param_spec_object ("stage",
                                                        "Stage",
                                                        "Stage to record",
                                                        CLUTTER_TYPE_STAGE,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_FRAMERATE,
                                   g_param_spec_int ("framerate",
                                                     "Framerate",
                                                     "Framerate used for resulting video in frames-per-second",
                                                      0,
                                                      G_MAXINT,
                                                      DEFAULT_FRAMES_PER_SECOND,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PIPELINE,
                                   g_param_spec_string ("pipeline",
                                                        "Pipeline",
                                                        "GStreamer pipeline description to encode recordings",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_FILE_TEMPLATE,
                                   g_param_spec_string ("file-template",
                                                        "File Template",
                                                        "The filename template to use for output files",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_CURSOR,
                                   g_param_spec_boolean ("draw-cursor",
                                                         "Draw Cursor",
                                                         "Whether to record the cursor",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
}

/* Sets the GstCaps (video format, in this case) on the stream
 */
static void
recorder_pipeline_set_caps (RecorderPipeline *pipeline)
{
  GstCaps *caps;

  /* The data is always native-endian xRGB; videoconvert
   * doesn't support little-endian xRGB, but does support
   * big-endian BGRx.
   */
  caps = gst_caps_new_simple ("video/x-raw",
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                              "format", G_TYPE_STRING, "BGRx",
#else
                              "format", G_TYPE_STRING, "xRGB",
#endif
                              "bpp", G_TYPE_INT, 32,
                              "depth", G_TYPE_INT, 24,
                              "framerate", GST_TYPE_FRACTION, pipeline->recorder->framerate, 1,
                              "width", G_TYPE_INT, pipeline->recorder->area.width,
                              "height", G_TYPE_INT, pipeline->recorder->area.height,
                              NULL);
  g_object_set (pipeline->src, "caps", caps, NULL);
  gst_caps_unref (caps);
}

/* Augments the supplied pipeline with the source elements: the actual
 * ShellRecorderSrc element where we inject frames then additional elements
 * to convert the output into something palatable.
 */
static gboolean
recorder_pipeline_add_source (RecorderPipeline *pipeline)
{
  GstPad *sink_pad = NULL, *src_pad = NULL;
  gboolean result = FALSE;
  GstElement *videoconvert;

  sink_pad = gst_bin_find_unlinked_pad (GST_BIN (pipeline->pipeline), GST_PAD_SINK);
  if (sink_pad == NULL)
    {
      g_warning("ShellRecorder: pipeline has no unlinked sink pad");
      goto out;
    }

  pipeline->src = gst_element_factory_make ("shellrecordersrc", NULL);
  if (pipeline->src == NULL)
    {
      g_warning ("Can't create recorder source element");
      goto out;
    }
  gst_bin_add (GST_BIN (pipeline->pipeline), pipeline->src);

  recorder_pipeline_set_caps (pipeline);

  /* The videoconvert element is a generic converter; it will convert
   * our supplied fixed format data into whatever the encoder wants
   */
  videoconvert = gst_element_factory_make ("videoconvert", NULL);
  if (!videoconvert)
    {
      g_warning("Can't create videoconvert element");
      goto out;
    }
  gst_bin_add (GST_BIN (pipeline->pipeline), videoconvert);

  gst_element_link_many (pipeline->src, videoconvert, NULL);
  src_pad = gst_element_get_static_pad (videoconvert, "src");

  if (!src_pad)
    {
      g_warning("ShellRecorder: can't get src pad to link into pipeline");
      goto out;
    }

  if (gst_pad_link (src_pad, sink_pad) != GST_PAD_LINK_OK)
    {
      g_warning("ShellRecorder: can't link to sink pad");
      goto out;
    }

  result = TRUE;

 out:
  if (sink_pad)
    gst_object_unref (sink_pad);
  if (src_pad)
    gst_object_unref (src_pad);

  return result;
}

static char *
get_absolute_path (char *maybe_relative)
{
  char *path;

  if (g_path_is_absolute (maybe_relative))
    path = g_strdup (maybe_relative);
  else
    {
      const char *video_dir = g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS);
      path = g_build_filename (video_dir, maybe_relative, NULL);
    }

  return path;
}

/* Open a file for writing. Opening the file ourselves and using fdsink has
 * the advantage over filesink of being able to use O_EXCL when we want to
 * avoid overwriting* an existing file. Returns -1 if the file couldn't
 * be opened.
 */
static int
recorder_open_outfile (ShellRecorder  *recorder,
                       char          **outfilename)
{
  const char *pattern;
  int flags;
  int outfile = -1;

  pattern = recorder->file_template;
  if (!pattern)
    return -1;

  while (TRUE)
    {
      GString *filename = g_string_new (NULL);
      const char *p;
      char *path;

      for (p = pattern; *p; p++)
        {
          if (*p == '%')
            {
              switch (*(p + 1))
                {
                case '%':
                case '\0':
                  g_string_append_c (filename, '%');
                  break;
                case 'd':
                  {
                    /* Appends date according to locale */
                    GDateTime *datetime = g_date_time_new_now_local ();
                    char *date_str = g_date_time_format (datetime, "%0x");
                    char *s;

                    for (s = date_str; *s; s++)
                      if (G_IS_DIR_SEPARATOR (*s))
                          *s = '-';

                    g_string_append (filename, date_str);
                    g_free (date_str);
                    g_date_time_unref (datetime);
                  }
                  break;
                case 't':
                  {
                    /* Appends time according to locale */
                    GDateTime *datetime = g_date_time_new_now_local ();
                    char *time_str = g_date_time_format (datetime, "%0X");
                    char *s;

                    for (s = time_str; *s; s++)
                      if (G_IS_DIR_SEPARATOR (*s))
                          *s = ':';

                    g_string_append (filename, time_str);
                    g_free (time_str);
                    g_date_time_unref (datetime);
                  }
                  break;
                default:
                  g_warning ("Unknown escape %%%c in filename", *(p + 1));
                  goto out;
                }

              p++;
            }
          else
            g_string_append_c (filename, *p);
        }

      /* If a filename is explicitly specified without %u then we assume the user
       * is fine with over-writing the old contents; putting %u in the default
       * should avoid problems with malicious symlinks.
       */
      flags = O_WRONLY | O_CREAT | O_TRUNC;

      path = get_absolute_path (filename->str);
      outfile = open (path, flags, 0666);
      if (outfile != -1)
        {
          g_printerr ("Recording to %s\n", path);

          if (outfilename != NULL)
            *outfilename = path;
          else
            g_free (path);
          g_string_free (filename, TRUE);

          goto out;
        }

      if (outfile == -1 && errno != EEXIST)
        {
          g_warning ("Cannot open output file '%s': %s", filename->str, g_strerror (errno));
          g_string_free (filename, TRUE);
          g_free (path);
          goto out;
        }

      g_string_free (filename, TRUE);
      g_free (path);
    }

 out:

  return outfile;
}

/* Augments the supplied pipeline with a sink element to write to the output
 * file, if necessary.
 */
static gboolean
recorder_pipeline_add_sink (RecorderPipeline *pipeline)
{
  GstPad *sink_pad = NULL, *src_pad = NULL;
  GstElement *fdsink;
  gboolean result = FALSE;

  src_pad = gst_bin_find_unlinked_pad (GST_BIN (pipeline->pipeline), GST_PAD_SRC);
  if (src_pad == NULL)
    {
      /* Nothing to do - assume that we were given a complete pipeline */
      return TRUE;
    }

  pipeline->outfile = recorder_open_outfile (pipeline->recorder,
                                             &pipeline->filename);
  if (pipeline->outfile == -1)
    goto out;

  fdsink = gst_element_factory_make ("fdsink", NULL);
  if (fdsink == NULL)
    {
      g_warning("Can't create fdsink element");
      goto out;
    }
  gst_bin_add (GST_BIN (pipeline->pipeline), fdsink);
  g_object_set (fdsink, "fd", pipeline->outfile, NULL);

  sink_pad = gst_element_get_static_pad (fdsink, "sink");
  if (!sink_pad)
    {
      g_warning("ShellRecorder: can't get sink pad to link pipeline output");
      goto out;
    }

  if (gst_pad_link (src_pad, sink_pad) != GST_PAD_LINK_OK)
    {
      g_warning("ShellRecorder: can't link to sink pad");
      goto out;
    }

  result = TRUE;

 out:
  if (src_pad)
    gst_object_unref (src_pad);
  if (sink_pad)
    gst_object_unref (sink_pad);

  return result;
}

static gboolean
recorder_update_memory_used_timeout (gpointer data)
{
  ShellRecorder *recorder = data;
  recorder->update_memory_used_timeout = 0;

  recorder_update_memory_used (recorder, TRUE);

  return FALSE;
}

/* We throttle down the frequency which we recompute memory usage
 * and draw the buffer indicator to avoid cutting into performance.
 */
static void
recorder_pipeline_on_memory_used_changed (ShellRecorderSrc *src,
                                          GParamSpec       *spec,
                                          RecorderPipeline *pipeline)
{
  ShellRecorder *recorder = pipeline->recorder;
  if (!recorder)
    return;

  if (recorder->update_memory_used_timeout == 0)
    {
      recorder->update_memory_used_timeout = g_timeout_add (UPDATE_MEMORY_USED_DELAY,
                                                            recorder_update_memory_used_timeout,
                                                            recorder);
      g_source_set_name_by_id (recorder->update_memory_used_timeout, "[gnome-shell] recorder_update_memory_used_timeout");
    }
}

static void
recorder_pipeline_free (RecorderPipeline *pipeline)
{
  if (pipeline->pipeline != NULL)
    gst_object_unref (pipeline->pipeline);

  if (pipeline->outfile != -1)
    close (pipeline->outfile);

  g_free (pipeline->filename);

  g_clear_object (&pipeline->recorder);

  g_free (pipeline);
}

/* Function gets called on pipeline-global events; we use it to
 * know when the pipeline is finished.
 */
static gboolean
recorder_pipeline_bus_watch (GstBus     *bus,
                             GstMessage *message,
                             gpointer    data)
{
  RecorderPipeline *pipeline = data;

  switch (message->type)
    {
    case GST_MESSAGE_EOS:
      recorder_pipeline_closed (pipeline);
      return FALSE; /* remove watch */
    case GST_MESSAGE_ERROR:
      {
        GError *error;

        gst_message_parse_error (message, &error, NULL);
        g_warning ("Error in recording pipeline: %s\n", error->message);
        g_error_free (error);
        recorder_pipeline_closed (pipeline);
        return FALSE; /* remove watch */
      }
    default:
      break;
    }

  /* Leave the watch in place */
  return TRUE;
}

/* Clean up when the pipeline is finished
 */
static void
recorder_pipeline_closed (RecorderPipeline *pipeline)
{
  g_signal_handlers_disconnect_by_func (pipeline->src,
                                        (gpointer) recorder_pipeline_on_memory_used_changed,
                                        pipeline);

  recorder_disconnect_stage_callbacks (pipeline->recorder);

  gst_element_set_state (pipeline->pipeline, GST_STATE_NULL);

  if (pipeline->recorder)
    {
      GtkRecentManager *recent_manager;
      GFile *file;
      char *uri;

      ShellRecorder *recorder = pipeline->recorder;
      if (pipeline == recorder->current_pipeline)
        {
          /* Error case; force a close */
          recorder->current_pipeline = NULL;
          shell_recorder_close (recorder);
        }

      recent_manager = gtk_recent_manager_get_default ();

      file = g_file_new_for_path (pipeline->filename);
      uri = g_file_get_uri (file);
      gtk_recent_manager_add_item (recent_manager,
                                   uri);
      g_free (uri);
      g_object_unref (file);

      recorder->pipelines = g_slist_remove (recorder->pipelines, pipeline);
    }

  recorder_pipeline_free (pipeline);
}

/*
 * Replaces '%T' in the passed pipeline with the thread count,
 * the maximum possible value is 64 (limit of what vp8enc supports)
 *
 * It is assumes that %T occurs only once.
 */
static char*
substitute_thread_count (const char *pipeline)
{
  char *tmp;
  int n_threads;
  GString *result;

  tmp = strstr (pipeline, "%T");

  if (!tmp)
    return g_strdup (pipeline);

#ifdef _SC_NPROCESSORS_ONLN
    {
      int n_processors = sysconf (_SC_NPROCESSORS_ONLN); /* includes hyper-threading */
      n_threads = MIN (MAX (1, n_processors - 1), 64);
    }
#else
    n_threads = 3;
#endif

  result = g_string_new (NULL);
  g_string_append_len (result, pipeline, tmp - pipeline);
  g_string_append_printf (result, "%d", n_threads);
  g_string_append (result, tmp + 2);

  return g_string_free (result, FALSE);;
}

static gboolean
recorder_open_pipeline (ShellRecorder *recorder)
{
  RecorderPipeline *pipeline;
  const char *pipeline_description;
  char *parsed_pipeline;
  GError *error = NULL;
  GstBus *bus;

  pipeline = g_new0(RecorderPipeline, 1);
  pipeline->recorder = g_object_ref (recorder);
  pipeline->outfile = - 1;

  pipeline_description = recorder->pipeline_description;
  if (!pipeline_description)
    pipeline_description = DEFAULT_PIPELINE;

  parsed_pipeline = substitute_thread_count (pipeline_description);

  pipeline->pipeline = gst_parse_launch_full (parsed_pipeline, NULL,
                                              GST_PARSE_FLAG_FATAL_ERRORS,
                                              &error);
  g_free (parsed_pipeline);

  if (pipeline->pipeline == NULL)
    {
      g_warning ("ShellRecorder: failed to parse pipeline: %s", error->message);
      g_error_free (error);
      goto error;
    }

  if (!recorder_pipeline_add_source (pipeline))
    goto error;

  if (!recorder_pipeline_add_sink (pipeline))
    goto error;

  gst_element_set_state (pipeline->pipeline, GST_STATE_PLAYING);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline->pipeline));
  gst_bus_add_watch (bus, recorder_pipeline_bus_watch, pipeline);
  gst_object_unref (bus);

  g_signal_connect (pipeline->src, "notify::memory-used",
                    G_CALLBACK (recorder_pipeline_on_memory_used_changed), pipeline);

  recorder->current_pipeline = pipeline;
  recorder->pipelines = g_slist_prepend (recorder->pipelines, pipeline);

  return TRUE;

 error:
  recorder_pipeline_free (pipeline);

  return FALSE;
}

static void
recorder_close_pipeline (ShellRecorder *recorder)
{
  if (recorder->current_pipeline != NULL)
    {
      /* This will send an EOS (end-of-stream) message after the last frame
       * is written. The bus watch for the pipeline will get it and do
       * final cleanup
       */
      shell_recorder_src_close (SHELL_RECORDER_SRC (recorder->current_pipeline->src));

      recorder->current_pipeline = NULL;
    }
}

/**
 * shell_recorder_new:
 * @stage: The #ClutterStage
 *
 * Create a new #ShellRecorder to record movies of a #ClutterStage
 *
 * Return value: The newly created #ShellRecorder object
 */
ShellRecorder     *
shell_recorder_new (ClutterStage  *stage)
{
  return g_object_new (SHELL_TYPE_RECORDER,
                       "stage",    stage,
                       NULL);
}

/**
 * shell_recorder_set_framerate:
 * @recorder: the #ShellRecorder
 * @framerate: Framerate used for resulting video in frames-per-second.
 *
 * Sets the number of frames per second we try to record. Less frames
 * will be recorded when the screen doesn't need to be redrawn this
 * quickly. (This value will also be set as the framerate for the
 * GStreamer pipeline; whether that has an effect on the resulting
 * video will depend on the details of the pipeline and the codec. The
 * default encoding to webm format doesn't pay attention to the pipeline
 * framerate.)
 *
 * The default value is 30.
 */
void
shell_recorder_set_framerate (ShellRecorder *recorder,
                             int framerate)
{
  g_return_if_fail (SHELL_IS_RECORDER (recorder));

  recorder_set_framerate (recorder, framerate);
}

/**
 * shell_recorder_set_file_template:
 * @recorder: the #ShellRecorder
 * @file_template: the filename template to use for output files,
 *                 or %NULL for the defalt value.
 *
 * Sets the filename that will be used when creating output
 * files. This is only used if the configured pipeline has an
 * unconnected source pad (as the default pipeline does). If
 * the pipeline is complete, then the filename is unused. The
 * provided string is used as a template.It can contain
 * the following escapes:
 *
 * %d: The current date as YYYYYMMDD
 * %%: A literal percent
 *
 * The default value is 'shell-%d%u-%c.ogg'.
 */
void
shell_recorder_set_file_template (ShellRecorder *recorder,
                                  const char    *file_template)
{
  g_return_if_fail (SHELL_IS_RECORDER (recorder));

  recorder_set_file_template (recorder, file_template);

}

void
shell_recorder_set_draw_cursor (ShellRecorder *recorder,
                                gboolean       draw_cursor)
{
  g_return_if_fail (SHELL_IS_RECORDER (recorder));

  recorder_set_draw_cursor (recorder, draw_cursor);
}

/**
 * shell_recorder_set_pipeline:
 * @recorder: the #ShellRecorder
 * @pipeline: (allow-none): the GStreamer pipeline used to encode recordings
 *            or %NULL for the default value.
 *
 * Sets the GStreamer pipeline used to encode recordings.
 * It follows the syntax used for gst-launch. The pipeline
 * should have an unconnected sink pad where the recorded
 * video is recorded. It will normally have a unconnected
 * source pad; output from that pad will be written into the
 * output file. (See shell_recorder_set_file_template().) However
 * the pipeline can also take care of its own output - this
 * might be used to send the output to an icecast server
 * via shout2send or similar.
 *
 * The default value is 'vp8enc min_quantizer=13 max_quantizer=13 cpu-used=5 deadline=1000000 threads=%T ! queue ! webmmux'
 */
void
shell_recorder_set_pipeline (ShellRecorder *recorder,
                             const char    *pipeline)
{
  g_return_if_fail (SHELL_IS_RECORDER (recorder));

  recorder_set_pipeline (recorder, pipeline);
}

void
shell_recorder_set_area (ShellRecorder *recorder,
                         int            x,
                         int            y,
                         int            width,
                         int            height)
{
  g_return_if_fail (SHELL_IS_RECORDER (recorder));

  recorder->custom_area = TRUE;
  recorder->area.x = CLAMP (x, 0, recorder->stage_width);
  recorder->area.y = CLAMP (y, 0, recorder->stage_height);
  recorder->area.width = CLAMP (width,
                                0, recorder->stage_width - recorder->area.x);
  recorder->area.height = CLAMP (height,
                                 0, recorder->stage_height - recorder->area.y);

  /* This breaks the recording but tweaking the GStreamer pipeline a bit
   * might make it work, at least if the codec can handle a stream where
   * the frame size changes in the middle.
   */
  if (recorder->current_pipeline)
    recorder_pipeline_set_caps (recorder->current_pipeline);
}

/**
 * shell_recorder_record:
 * @recorder: the #ShellRecorder
 * @filename_used: (out) (allow-none): actual filename used for recording
 *
 * Starts recording, Starting the recording may fail if the output file
 * cannot be opened, or if the output stream cannot be created
 * for other reasons. In that case a warning is printed to
 * stderr. There is no way currently to get details on how
 * recording failed to start.
 *
 * An extra reference count is added to the recorder if recording
 * is succesfully started; the recording object will not be freed
 * until recording is stopped even if the creator no longer holds
 * a reference. Recording is automatically stopped if the stage
 * is destroyed.
 *
 * Return value: %TRUE if recording was succesfully started
 */
gboolean
shell_recorder_record (ShellRecorder  *recorder,
                       char          **filename_used)
{
  g_return_val_if_fail (SHELL_IS_RECORDER (recorder), FALSE);
  g_return_val_if_fail (recorder->stage != NULL, FALSE);
  g_return_val_if_fail (recorder->state != RECORDER_STATE_RECORDING, FALSE);

  if (!recorder_open_pipeline (recorder))
    return FALSE;

  if (filename_used)
    *filename_used = g_strdup (recorder->current_pipeline->filename);

  recorder_connect_stage_callbacks (recorder);

  recorder->start_time = get_wall_time();
  recorder->last_frame_time = 0;

  recorder->state = RECORDER_STATE_RECORDING;
  recorder_add_update_pointer_timeout (recorder);

  /* Set up repaint hook */
  recorder->repaint_hook_id = clutter_threads_add_repaint_func(recorder_repaint_hook, recorder->stage, NULL);

  /* Record an initial frame and also redraw with the indicator */
  clutter_actor_queue_redraw (CLUTTER_ACTOR (recorder->stage));

  /* We keep a ref while recording to let a caller start a recording then
   * drop their reference to the recorder
   */
  g_object_ref (recorder);

  return TRUE;
}

/**
 * shell_recorder_close:
 * @recorder: the #ShellRecorder
 *
 * Stops recording. It's possible to call shell_recorder_record()
 * again to reopen a new recording stream, but unless change the
 * recording filename, this may result in the old recording being
 * overwritten.
 */
void
shell_recorder_close (ShellRecorder *recorder)
{
  g_return_if_fail (SHELL_IS_RECORDER (recorder));
  g_return_if_fail (recorder->state != RECORDER_STATE_CLOSED);

  /* We want to record one more frame since some time may have
   * elapsed since the last frame
   */
  clutter_actor_paint (CLUTTER_ACTOR (recorder->stage));

  recorder_remove_update_pointer_timeout (recorder);
  recorder_close_pipeline (recorder);

  /* Queue a redraw to remove the recording indicator */
  clutter_actor_queue_redraw (CLUTTER_ACTOR (recorder->stage));

  if (recorder->repaint_hook_id != 0)
    {
      clutter_threads_remove_repaint_func (recorder->repaint_hook_id);
      recorder->repaint_hook_id = 0;
    }

  recorder->state = RECORDER_STATE_CLOSED;

  /* Release the refcount we took when we started recording */
  g_object_unref (recorder);
}

/**
 * shell_recorder_is_recording:
 *
 * Determine if recording is currently in progress. (The recorder
 * is not paused or closed.)
 *
 * Return value: %TRUE if the recorder is currently recording.
 */
gboolean
shell_recorder_is_recording (ShellRecorder *recorder)
{
  g_return_val_if_fail (SHELL_IS_RECORDER (recorder), FALSE);

  return recorder->state == RECORDER_STATE_RECORDING;
}
