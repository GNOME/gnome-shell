#include <stdbool.h>

#include <cogl/cogl.h>
#include <cogl-gst/cogl-gst.h>

typedef struct _Data
{
  CoglFramebuffer *fb;
  CoglPipeline *border_pipeline;
  CoglPipeline *video_pipeline;
  CoglGstVideoSink *sink;
  int onscreen_width;
  int onscreen_height;
  CoglGstRectangle video_output;
  bool draw_ready;
  bool frame_ready;
  GMainLoop *main_loop;
}Data;

static gboolean
_bus_watch (GstBus *bus,
            GstMessage *msg,
            void *user_data)
{
  Data *data = (Data*) user_data;
  switch (GST_MESSAGE_TYPE (msg))
    {
      case GST_MESSAGE_EOS:
        {
          g_main_loop_quit (data->main_loop);
          break;
        }
      case GST_MESSAGE_ERROR:
        {
          char *debug;
          GError *error = NULL;

          gst_message_parse_error (msg, &error, &debug);
          g_free (debug);

          if (error != NULL)
            {
              g_error ("Playback error: %s\n", error->message);
              g_error_free (error);
            }
          g_main_loop_quit (data->main_loop);
          break;
        }
      default:
        break;
    }

  return TRUE;
}

static void
_draw (Data *data)
{
  /*
    The cogl pipeline needs to be retrieved from the sink before every draw.
    This is due to the cogl-gst sink creating a new cogl pipeline for each frame
    by copying the previous one and attaching the new frame to it.
  */
  CoglPipeline* current = cogl_gst_video_sink_get_pipeline (data->sink);

  data->video_pipeline = current;

  if (data->video_output.x)
    {
      int x = data->video_output.x;

      /* Letterboxed with vertical borders */
      cogl_framebuffer_draw_rectangle (data->fb,
                                       data->border_pipeline,
                                       0, 0, x, data->onscreen_height);
      cogl_framebuffer_draw_rectangle (data->fb,
                                       data->border_pipeline,
                                       data->onscreen_width - x,
                                       0,
                                       data->onscreen_width,
                                       data->onscreen_height);
      cogl_framebuffer_draw_rectangle (data->fb, data->video_pipeline,
                                       x, 0,
                                       x + data->video_output.width,
                                       data->onscreen_height);
    }
  else if (data->video_output.y)
    {
      int y = data->video_output.y;

      /* Letterboxed with horizontal borders */
      cogl_framebuffer_draw_rectangle (data->fb,
                                       data->border_pipeline,
                                       0, 0, data->onscreen_width, y);
      cogl_framebuffer_draw_rectangle (data->fb,
                                       data->border_pipeline,
                                       0,
                                       data->onscreen_height - y,
                                       data->onscreen_width,
                                       data->onscreen_height);
      cogl_framebuffer_draw_rectangle (data->fb, data->video_pipeline,
                                       0, y,
                                       data->onscreen_width,
                                       y + data->video_output.height);

    }
  else
    {
      cogl_framebuffer_draw_rectangle (data->fb,
                                       data->video_pipeline,
                                       0, 0,
                                       data->onscreen_width,
                                       data->onscreen_height);
    }

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));
}

static void
_check_draw (Data *data)
{
  /* The frame is only drawn once we know that a new buffer is ready
   * from GStreamer and that Cogl is ready to accept some new
   * rendering */
  if (data->draw_ready && data->frame_ready)
    {
      _draw (data);
      data->draw_ready = FALSE;
      data->frame_ready = FALSE;
    }
}

static void
_frame_callback (CoglOnscreen *onscreen,
                 CoglFrameEvent event,
                 CoglFrameInfo *info,
                 void *user_data)
{
  Data *data = user_data;

  if (event == COGL_FRAME_EVENT_SYNC)
    {
      data->draw_ready = TRUE;
      _check_draw (data);
    }
}

static void
_new_frame_cb (CoglGstVideoSink *sink,
               Data *data)
{
  data->frame_ready = TRUE;
  _check_draw (data);
}

static void
_resize_callback (CoglOnscreen *onscreen,
                  int width,
                  int height,
                  void *user_data)
{
  Data *data = user_data;
  CoglGstRectangle available;

  data->onscreen_width = width;
  data->onscreen_height = height;

  cogl_framebuffer_orthographic (data->fb, 0, 0, width, height, -1, 100);

  if (!data->video_pipeline)
    return;

  available.x = 0;
  available.y = 0;
  available.width = width;
  available.height = height;
  cogl_gst_video_sink_fit_size (data->sink,
                                &available,
                                &data->video_output);
}

/*
  A callback like this should be attached to the cogl-pipeline-ready
  signal. This way requesting the cogl pipeline before its creation
  by the sink is avoided. At this point, user textures and snippets can
  be added to the cogl pipeline.
*/

static void
_set_up_pipeline (gpointer instance,
                  gpointer user_data)
{
  Data* data = (Data*) user_data;

  /*
    The cogl-gst sink, depending on the video format, can use up to 3 texture
    layers to render a frame. To avoid overwriting frame data, the first
    free layer in the cogl pipeline needs to be queried before adding any
    additional textures.
  */

  int free_layer = cogl_gst_video_sink_get_free_layer (data->sink);
  data->video_pipeline = cogl_gst_video_sink_get_pipeline (data->sink);

  while (free_layer > 0)
    {
      free_layer--;
      cogl_pipeline_set_layer_filters (data->video_pipeline, free_layer,
                                       COGL_PIPELINE_FILTER_LINEAR_MIPMAP_LINEAR,
                                       COGL_PIPELINE_FILTER_LINEAR);
    }

  /* disable blending... */
  cogl_pipeline_set_blend (data->video_pipeline,
                           "RGBA = ADD (SRC_COLOR, 0)", NULL);

  /* Now that we know the video size we can perform letterboxing */
  _resize_callback (COGL_ONSCREEN (data->fb),
                    data->onscreen_width,
                    data->onscreen_height,
                    data);

  cogl_onscreen_add_frame_callback (COGL_ONSCREEN (data->fb), _frame_callback,
                                    data, NULL);

  /*
     The cogl-gst-new-frame signal is emitted when the cogl-gst sink has
     retrieved a new frame and attached it to the cogl pipeline. This can be
     used to make sure cogl doesn't do any unnecessary drawing i.e. keeps to the
     frame-rate of the video.
  */

  g_signal_connect (data->sink, "new-frame", G_CALLBACK (_new_frame_cb), data);
}

int
main (int argc,
      char **argv)
{
  Data data;
  CoglContext *ctx;
  CoglOnscreen *onscreen;
  GstElement *pipeline;
  GstElement *bin;
  GSource *cogl_source;
  GstBus *bus;
  char *uri;

  memset (&data, 0, sizeof (Data));

  /* Set the necessary cogl elements */

  ctx = cogl_context_new (NULL, NULL);

  onscreen = cogl_onscreen_new (ctx, 640, 480);
  cogl_onscreen_set_resizable (onscreen, TRUE);
  cogl_onscreen_add_resize_callback (onscreen, _resize_callback, &data, NULL);
  cogl_onscreen_show (onscreen);

  data.fb = COGL_FRAMEBUFFER (onscreen);
  cogl_framebuffer_orthographic (data.fb, 0, 0, 640, 480, -1, 100);

  data.border_pipeline = cogl_pipeline_new (ctx);
  cogl_pipeline_set_color4f (data.border_pipeline, 0, 0, 0, 1);
  /* disable blending */
  cogl_pipeline_set_blend (data.border_pipeline,
                           "RGBA = ADD (SRC_COLOR, 0)", NULL);

  /* Intialize GStreamer */

  gst_init (&argc, &argv);

  /*
     Create the cogl-gst video sink by calling the cogl_gst_video_sink_new
     function and passing it a CoglContext (this is used to create the
     CoglPipeline and the texures for each frame). Alternatively you can use
     gst_element_factory_make ("coglsink", "some_name") and then set the
     context with cogl_gst_video_sink_set_context.
  */

  data.sink = cogl_gst_video_sink_new (ctx);

  pipeline = gst_pipeline_new ("gst-player");
  bin = gst_element_factory_make ("playbin", "bin");

  if (argc < 2)
    uri = "http://docs.gstreamer.com/media/sintel_trailer-480p.webm";
  else
    uri = argv[1];

  g_object_set (G_OBJECT (bin), "video-sink", GST_ELEMENT (data.sink), NULL);


  gst_bin_add (GST_BIN (pipeline), bin);

  g_object_set (G_OBJECT (bin), "uri", uri, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, _bus_watch, &data);

  data.main_loop = g_main_loop_new (NULL, FALSE);

  cogl_source = cogl_glib_source_new (ctx, G_PRIORITY_DEFAULT);
  g_source_attach (cogl_source, NULL);

  /*
    The cogl-pipeline-ready signal tells you when the cogl pipeline is
    initialized i.e. when cogl-gst has figured out the video format and
    is prepared to retrieve and attach the first frame of the video.
  */

  g_signal_connect (data.sink, "pipeline-ready",
                    G_CALLBACK (_set_up_pipeline), &data);

  data.draw_ready = TRUE;
  data.frame_ready = FALSE;

  g_main_loop_run (data.main_loop);

  g_source_destroy (cogl_source);
  g_source_unref (cogl_source);

  g_main_loop_unref (data.main_loop);

  return 0;
}
