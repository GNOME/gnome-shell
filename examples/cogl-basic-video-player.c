#include <cogl/cogl.h>
#include <cogl-gst/cogl-gst.h>

typedef struct _Data
{
  CoglFramebuffer *fb;
  CoglPipeline *pln;
  CoglGstVideoSink *sink;
  CoglBool draw_ready;
  CoglBool frame_ready;
  GMainLoop *main_loop;
}Data;

static CoglBool
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

  cogl_framebuffer_clear4f (data->fb,
                            COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH, 0,
                            0, 0, 1);
  data->pln = current;

  cogl_framebuffer_push_matrix (data->fb);
  cogl_framebuffer_translate (data->fb, 640 / 2, 480 / 2, 0);
  cogl_framebuffer_draw_textured_rectangle (data->fb, data->pln, -320, -240,
                                            320, 240, 0, 0, 1, 1);
  cogl_framebuffer_pop_matrix (data->fb);

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
  data->pln = cogl_gst_video_sink_get_pipeline (data->sink);

  while (free_layer > 0)
    {
      free_layer--;
      cogl_pipeline_set_layer_filters (data->pln, free_layer,
                                       COGL_PIPELINE_FILTER_LINEAR_MIPMAP_LINEAR,
                                       COGL_PIPELINE_FILTER_LINEAR);
    }

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
  CoglMatrix view;
  float fovy, aspect, z_near, z_2d, z_far;
  GstElement *pipeline;
  GstElement *bin;
  GSource *cogl_source;
  GstBus *bus;
  char *uri;

  /* Set the necessary cogl elements */

  ctx = cogl_context_new (NULL, NULL);
  onscreen = cogl_onscreen_new (ctx, 640, 480);
  data.fb = COGL_FRAMEBUFFER (onscreen);
  cogl_onscreen_show (onscreen);

  cogl_framebuffer_set_viewport (data.fb, 0, 0, 640, 480);
  fovy = 60;
  aspect = 640 / 480;
  z_near = 0.1;
  z_2d = 1000;
  z_far = 2000;

  cogl_framebuffer_perspective (data.fb, fovy, aspect, z_near, z_far);
  cogl_matrix_init_identity (&view);
  cogl_matrix_view_2d_in_perspective (&view, fovy, aspect, z_near, z_2d,
                                      640, 480);
  cogl_framebuffer_set_modelview_matrix (data.fb, &view);

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
