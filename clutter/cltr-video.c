#include "cltr-video.h"
#include "cltr-private.h"

struct CltrVideo
{
  CltrWidget  widget;  

  GstElement  *play, *data_src, *video_sink, *audio_sink, *vis_element;

  GAsyncQueue *queue;

  int          video_width, video_height;
  CltrTexture *frame_texture;
};



static void
cltr_video_show(CltrWidget *widget);

static gboolean 
cltr_video_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_video_paint(CltrWidget *widget);



static gint64     length = 0; 	/* to go */

static void
cltr_video_print_tag (const GstTagList *list, 
		      const gchar      *tag, 
		      gpointer          unused)
{
  gint i, count;

  count = gst_tag_list_get_tag_size (list, tag);

  for (i = 0; i < count; i++) {
    gchar *str;

    if (gst_tag_get_type (tag) == G_TYPE_STRING) {
      if (!gst_tag_list_get_string_index (list, tag, i, &str))
        g_assert_not_reached ();
    } else {
      str =
          g_strdup_value_contents (gst_tag_list_get_value_index (list, tag, i));
    }

    if (i == 0) {
      g_print ("%15s: %s\n", gst_tag_get_nick (tag), str);
    } else {
      g_print ("               : %s\n", str);
    }

    g_free (str);
  }
}

static void
cltr_video_got_found_tag (GstPlay    *play, 
			  GstElement *source, 
			  GstTagList *tag_list,
			  CltrVideo  *video)
{
  CltrVideoSignal *signal;

  signal = g_new0 (CltrVideoSignal, 1);
  signal->signal_id                      = CLTR_VIDEO_ASYNC_FOUND_TAG;
  signal->signal_data.found_tag.source   = source;
  signal->signal_data.found_tag.tag_list = gst_tag_list_copy (tag_list);

  g_async_queue_push (video->queue, signal);

  /* gst_tag_list_foreach (tag_list, cltr_video_print_tag, NULL); */
}

static void
cltr_video_got_time_tick (GstPlay   *play, 
			  gint64     time_nanos,
			  CltrVideo *video)
{


  /*
  CltrVideoSignal *signal;

  signal = g_new0 (CltrVideoSignal, 1);
  signal->signal_id                      = CLTR_VIDEO_ASYNC_FOUND_TAG;
  signal->signal_data.found_tag.source   = source;
  signal->signal_data.found_tag.tag_list = gst_tag_list_copy (tag_list);

  g_async_queue_push (video->queue, signal);
  */

  g_print ("time tick %f\n", time_nanos / (float) GST_SECOND); 
}

static void
cltr_video_got_stream_length (GstPlay   *play, 
			      gint64     length_nanos,
			      CltrVideo *video)
{
  /*
  CltrVideoSignal *signal;

  signal = g_new0 (CltrVideoSignal, 1);

  signal->signal_id = CLTR_VIDEO_ASYNC_VIDEO_SIZE;
  signal->signal_data.video_size.width = width;
  signal->signal_data.video_size.height = height;

  g_async_queue_push (video->queue, signal);
  */
  CLTR_DBG ("got length %" G_GUINT64_FORMAT "\n", length_nanos);
  length = length_nanos;
}

static void
cltr_video_got_video_size (GstPlay   *play, 
			   gint       width, 
			   gint       height,
			   CltrVideo *video)
{
  CltrVideoSignal *signal;

  signal = g_new0 (CltrVideoSignal, 1);

  signal->signal_id = CLTR_VIDEO_ASYNC_VIDEO_SIZE;
  signal->signal_data.video_size.width = width;
  signal->signal_data.video_size.height = height;

  g_async_queue_push (video->queue, signal);
}

static void
cltr_video_got_eos (GstPlay* play, CltrVideo *video)
{
  CLTR_DBG ("End Of Stream\n");

  CltrVideoSignal *signal;

  signal = g_new0 (CltrVideoSignal, 1);

  signal->signal_id = CLTR_VIDEO_ASYNC_EOS;

  g_async_queue_push (video->queue, signal);

  gst_element_set_state (GST_ELEMENT (play), GST_STATE_READY);
}

static gboolean
cltr_video_seek_timer (GstPlay * play)
{
  gst_play_seek_to_time (play, length / 2);
  return FALSE;
}

static void
caps_set (GObject    *obj,
	  GParamSpec *pspec, 
	  CltrVideo  *video)
{
#if 0
  GstPad *pad = GST_PAD (obj);
  GstStructure *s;

  if (!GST_PAD_CAPS (pad))
    return;



  s = gst_caps_get_structure (GST_PAD_CAPS (pad), 0);

  if (s) {


    const GValue *par;

    if (!(gst_structure_get_double (s, "framerate", &bvw->priv->video_fps) &&
          gst_structure_get_int (s, "width", &bvw->priv->video_width) &&
          gst_structure_get_int (s, "height", &bvw->priv->video_height)))
      return;
    if ((par = gst_structure_get_value (s,
                   "pixel-aspect-ratio"))) {
      gint num = gst_value_get_fraction_numerator (par),
          den = gst_value_get_fraction_denominator (par);

      if (num > den)
        bvw->priv->video_width *= (gfloat) num / den;
      else
        bvw->priv->video_height *= (gfloat) den / num;
    }

    got_video_size (bvw->priv->play, bvw->priv->video_width,
		    bvw->priv->video_height, bvw);

  }
#endif
  
  /* and disable ourselves */
  //g_signal_handlers_disconnect_by_func (pad, caps_set, bvw);
}


CltrWidget*
cltr_video_new(int width, int height)
{
  CltrVideo *video;
  GError    *error = NULL;

  video = g_malloc0(sizeof(CltrVideo));
  
  video->widget.width          = width;
  video->widget.height         = height;
  
  video->widget.show           = cltr_video_show;
  video->widget.paint          = cltr_video_paint;
  
  video->widget.xevent_handler = cltr_video_handle_xevent;

  /* Creating the GstPlay object */

  video->play = gst_element_factory_make ("playbin", "play");
  if (!video->play) {
    g_error ("Could not make playbin element");
    /* XXX Error */
    return NULL;
  }

  video->audio_sink = gst_gconf_get_default_audio_sink ();

  if (!GST_IS_ELEMENT (video->audio_sink))
    g_error ("Could not get default audio sink from GConf");
  
  video->video_sink = gst_element_factory_make ("cltrimagesink", "cltr-output");

  if (!GST_IS_ELEMENT (video->video_sink))
    g_error ("Could not get clutter video sink");

#if 0
  sig1 = g_signal_connect (video_sink, "error", G_CALLBACK (out_error), err);
  sig2 = g_signal_connect (audio_sink, "error", G_CALLBACK (out_error), err);
  if (gst_element_set_state (video_sink,
			     GST_STATE_READY) != GST_STATE_SUCCESS ||
      gst_element_set_state (audio_sink,
			     GST_STATE_READY) != GST_STATE_SUCCESS) {
    if (err && !*err) {
      g_set_error (err, 0, 0,
		   "Failed to intialize %s output; check your configuration",
		   GST_STATE (video_sink) == GST_STATE_NULL ?
		   "video" : "audio");
    }
    gst_object_unref (GST_OBJECT (video_sink));
    gst_object_unref (GST_OBJECT (audio_sink));
    g_object_unref (G_OBJECT (bvw));
    return NULL;
  }
  /* somehow, alsa hangs? */
  gst_element_set_state (video->audio_sink, GST_STATE_NULL);
  g_signal_handler_disconnect (video->video_sink, sig1);
  g_signal_handler_disconnect (video->audio_sink, sig2);
#endif
  g_object_set (G_OBJECT (video->play), "video-sink",
		video->video_sink, NULL);
  g_object_set (G_OBJECT (video->play), "audio-sink",
		video->audio_sink, NULL);

  /* Needed ? */
#if 0
  g_signal_connect (GST_PAD_REALIZE (gst_element_get_pad (audio_sink, "sink")),
		    "fixate", G_CALLBACK (cb_audio_fixate), (gpointer) bvw);
#endif

  g_signal_connect (G_OBJECT (video->play), "eos",
		     G_CALLBACK (cltr_video_got_eos), (gpointer) video);
  /*
  g_signal_connect (G_OBJECT (video->play), "state-change",
		    G_CALLBACK (state_change), (gpointer) video);
  */
  g_signal_connect (G_OBJECT (video->play), "found_tag",
		    G_CALLBACK (cltr_video_got_found_tag), (gpointer) video);

  /*
  g_signal_connect (G_OBJECT (video->play), "error",
		    G_CALLBACK (got_error), (gpointer) video);

  g_signal_connect (G_OBJECT (video->play), "buffering",
		    G_CALLBACK (got_buffering), (gpointer) video);

  g_signal_connect (G_OBJECT (video->play), "notify::source",
		    G_CALLBACK (got_source), (gpointer) video);
  g_signal_connect (G_OBJECT (video->play), "notify::stream-info",
		    G_CALLBACK (stream_info_set), (gpointer) video);
  g_signal_connect (G_OBJECT (video->play), "group-switch",
		    G_CALLBACK (group_switch), (gpointer) video);
  g_signal_connect (G_OBJECT (video->play), "got-redirect",
		    G_CALLBACK (got_redirect), (gpointer) video);
  */

  video->queue = g_async_queue_new ();

  gst_element_set(video->video_sink, "queue", video->queue, NULL);


  return CLTR_WIDGET(video);

#if 0
  video->play = gst_play_new (&error);

  if (error) 
    {
      g_print ("Error: could not create play object:\n%s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  /* Getting default audio and video plugins from GConf */
  video->vis_element = gst_element_factory_make ("goom", "vis_element");
  video->data_src    = gst_element_factory_make ("gnomevfssrc", "source");

  video->audio_sink = gst_gconf_get_default_audio_sink ();

  if (!GST_IS_ELEMENT (video->audio_sink))
    g_error ("Could not get default audio sink from GConf");

  video->video_sink = gst_element_factory_make ("cltrimagesink", "cltr-output");

  if (!GST_IS_ELEMENT (video->video_sink))
    g_error ("Could not get clutter video sink");

  video->queue = g_async_queue_new ();

  gst_element_set(video->video_sink, "queue", video->queue, NULL);

  /* Let's send them to GstPlay object */

  if (!gst_play_set_audio_sink (video->play, video->audio_sink))
    g_warning ("Could not set audio sink");
  if (!gst_play_set_video_sink (video->play, video->video_sink))
    g_warning ("Could not set video sink");
  if (!gst_play_set_data_src (video->play, video->data_src))
    g_warning ("Could not set data src");
  if (!gst_play_set_visualization (video->play, video->vis_element))
    g_warning ("Could not set visualisation");

  /* Setting location we want to play */

  /* Uncomment that line to get an XML dump of the pipeline */
  /* gst_xml_write_file (GST_ELEMENT (play), stdout);  */

  g_signal_connect (G_OBJECT (video->play), "time_tick",
      G_CALLBACK (cltr_video_got_time_tick), video);
  g_signal_connect (G_OBJECT (video->play), "stream_length",
      G_CALLBACK (cltr_video_got_stream_length), video);
  g_signal_connect (G_OBJECT (video->play), "have_video_size",
      G_CALLBACK (cltr_video_got_video_size), video);
  g_signal_connect (G_OBJECT (video->play), "found_tag",
      G_CALLBACK (cltr_video_got_found_tag), video);
  g_signal_connect (G_OBJECT (video->play), "error",
      G_CALLBACK (gst_element_default_error), NULL);
  g_signal_connect (G_OBJECT (video->play), 
		    "eos", G_CALLBACK (cltr_video_got_eos), video);

  g_signal_connect (G_OBJECT (video->video_sink), "notify::caps",
		    G_CALLBACK (caps_set), video);

#endif
  /*
  g_object_set (G_OBJECT (video->play), "volume",
		(gdouble) (1. *  0 / 100), NULL);
  */

  /* gst_element_set_state (GST_ELEMENT (play), GST_STATE_READY); */

  return CLTR_WIDGET(video);
}


static gboolean
cltr_video_idler (CltrVideo *video)
{
  gint  queue_length;
  CltrVideoSignal *signal;

  signal = g_async_queue_try_pop (video->queue);

  if (!signal)
    return TRUE;

  switch (signal->signal_id)
    {
    case CLTR_VIDEO_ASYNC_TEXTURE:
      video->frame_texture = signal->signal_data.texture.ref;

      /* 
       * we can actually grab the width and height from 
       * the textures pixbuf.
      */

      cltr_widget_queue_paint(CLTR_WIDGET(video));
      break;
    case CLTR_VIDEO_ASYNC_VIDEO_SIZE:
      video->video_width  = signal->signal_data.video_size.width;
      video->video_height = signal->signal_data.video_size.height;
      break;
    case CLTR_VIDEO_ASYNC_ERROR:
      break;
    case CLTR_VIDEO_ASYNC_FOUND_TAG:
      break;
    case CLTR_VIDEO_ASYNC_NOTIFY_STREAMINFO:
      break;
    case CLTR_VIDEO_ASYNC_EOS:
      break; 
    case CLTR_VIDEO_ASYNC_BUFFERING:
      break;
    case CLTR_VIDEO_ASYNC_REDIRECT:
      break; 
    }

  g_free (signal);

  return TRUE;
}

gboolean
cltr_video_set_source(CltrVideo *video, char *location)
{
  /* if (!gst_play_set_location (video->play, location)) */

  g_object_set (G_OBJECT (video->play), "uri", location, NULL);

  return TRUE;
}

void
cltr_video_play(CltrVideo *video)
{
  /* Change state to PLAYING */
  if (gst_element_set_state (GST_ELEMENT (video->play),
          GST_STATE_PLAYING) == GST_STATE_FAILURE)
    g_error ("Could not set state to PLAYING");

  g_timeout_add(FPS_TO_TIMEOUT(20), (GSourceFunc) cltr_video_idler, video);
}

void
cltr_video_pause(CltrVideo *video)
{
  if (gst_element_set_state (GST_ELEMENT (video->play),
			     GST_STATE_PAUSED) == GST_STATE_FAILURE)
    g_error ("Could not set state to PAUSED");
}


static void
cltr_video_show(CltrWidget *widget)
{
  return;
}

static void
cltr_video_hide(CltrWidget *widget)
{
  return;
}

static gboolean 
cltr_video_handle_xevent (CltrWidget *widget, XEvent *xev) 
{
  CLTR_DBG("X Event");

  return False;
}

static void
cltr_video_paint(CltrWidget *widget)
{
  CltrVideo *video = CLTR_VIDEO(widget);

  glPushMatrix();

  if (video->frame_texture
      /*
      && video->video_height
      && video->video_width 
      */)
    {
      int dis_x, dis_y, dis_height, dis_width;

      /* Hack */

      if (!video->video_height || !video->video_width )
	{
	  Pixbuf *pixb = cltr_texture_get_pixbuf(video->frame_texture);

	  video->video_height = pixb->height;
	  video->video_width = pixb->width;
	}


      if (video->video_width > video->video_height)
	{
	  dis_width  = widget->width;
	  dis_height = ( video->video_height * widget->width )
	                          / video->video_width;
	  dis_y = (widget->height - dis_height)/2;
	  dis_x = 0;
	}

      glColor4f(1.0, 1.0, 1.0, 1.0);

      glEnable(GL_TEXTURE_2D);

      glDisable(GL_BLEND); 

      glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

      cltr_texture_lock(video->frame_texture);

      cltr_texture_render_to_gl_quad(video->frame_texture, 
				     dis_x, 
				     dis_y,
				     dis_x + dis_width,
				     dis_y + dis_height);

      cltr_texture_unlock(video->frame_texture);

      glDisable(GL_TEXTURE_2D); 
    }

  glPopMatrix();
}
