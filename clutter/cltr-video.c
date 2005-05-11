#include "cltr-video.h"
#include "cltr-private.h"

/* This is all very much based on the totem gst bacon video widget */

struct CltrVideo
{
  CltrWidget  widget;  

  GstElement  *play, *data_src, *video_sink, *audio_sink, *vis_element;

  GAsyncQueue *queue;

  gint         video_width, video_height;
  gdouble      video_fps;
  CltrTexture *frame_texture;
  
  gboolean     has_video, has_audio;

  gint64       stream_length;
  gint64       current_time_nanos;
  gint64       current_time;
  float        current_position;

  guint        update_id;
  gchar       *last_error_message;
  gchar       *mrl;
};


static void
cltr_video_show(CltrWidget *widget);

static gboolean 
cltr_video_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_video_paint(CltrWidget *widget);

static void
parse_stream_info (CltrVideo *video);

static gboolean
cb_iterate (CltrVideo *video);

static void
reset_error_msg (CltrVideo *video);

static gboolean
cltr_video_idler (CltrVideo *video);


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
got_eos (GstPlay* play, CltrVideo *video)
{
  CLTR_DBG ("End Of Stream\n");

  CltrVideoSignal *signal;

  signal = g_new0 (CltrVideoSignal, 1);

  signal->signal_id = CLTR_VIDEO_ASYNC_EOS;

  g_async_queue_push (video->queue, signal);

  gst_element_set_state (GST_ELEMENT (play), GST_STATE_READY);
}

static void
got_stream_length (GstElement *play, 
		   gint64      length_nanos,
                   CltrVideo  *video)
{
  video->stream_length = (gint64) length_nanos / GST_MSECOND;

  /* fire off some callback here ? */

  CLTR_DBG("length: %i", video->stream_length);
}

static void
got_time_tick (GstElement *play, 
	       gint64      time_nanos, 
	       CltrVideo  *video)
{
  CLTR_MARK();

  video->current_time_nanos = time_nanos;

  video->current_time = (gint64) time_nanos / GST_MSECOND;

  if (video->stream_length == 0)
    video->current_position = 0;
  else
    {
      video->current_position = (float) video->current_time / video->stream_length;
    }

  /* fire off callback here */
}


static void
got_found_tag (GstPlay    *play, 
	       GstElement *source, 
	       GstTagList *tag_list,
	       CltrVideo  *video)
{
  CltrVideoSignal *signal;

  CLTR_MARK();

  signal = g_new0 (CltrVideoSignal, 1);
  signal->signal_id                      = CLTR_VIDEO_ASYNC_FOUND_TAG;
  signal->signal_data.found_tag.source   = source;
  signal->signal_data.found_tag.tag_list = gst_tag_list_copy (tag_list);

  g_async_queue_push (video->queue, signal);

  /* gst_tag_list_foreach (tag_list, cltr_video_print_tag, NULL); */
}

static void
got_state_change (GstElement     *play, 
		  GstElementState old_state,
		  GstElementState new_state, 
		  CltrVideo      *video)
{
  if (old_state == GST_STATE_PLAYING) 
    {
      if (video->update_id != 0) 
	{
	  g_source_remove (video->update_id);
	  video->update_id = 0;
	}

      g_idle_remove_by_data (video);

    } 
  else if (new_state == GST_STATE_PLAYING) 
    {
      if (video->update_id != 0)
	g_source_remove (video->update_id);

      video->update_id = g_timeout_add (200, (GSourceFunc) cb_iterate, video);

      g_idle_add((GSourceFunc) cltr_video_idler, video);
    }

  if (old_state <= GST_STATE_READY && new_state >= GST_STATE_PAUSED) 
    {
      parse_stream_info (video);
    } 
  else if (new_state <= GST_STATE_READY && old_state >= GST_STATE_PAUSED) 
    {
      video->has_video = FALSE;
      video->has_audio = FALSE;

      /*
      if (bvw->priv->tagcache)
	{
	  gst_tag_list_free (bvw->priv->tagcache);
	  bvw->priv->tagcache = NULL;
	}
      */      

      video->video_width = 0;
      video->video_height = 0;
    }
}


static void
got_redirect (GstElement  *play, 
	      const gchar *new_location,
	      CltrVideo   *bvw)
{
  CLTR_MARK();

  /*
  bvw->priv->got_redirect = TRUE;

  signal = g_new0 (BVWSignal, 1);
  signal->signal_id = ASYNC_REDIRECT;
  signal->signal_data.redirect.new_location = g_strdup (new_location);

  g_async_queue_push (bvw->priv->queue, signal);

  g_idle_add ((GSourceFunc) bacon_video_widget_signal_idler, bvw);
  */
}


static void
stream_info_set (GObject    *obj, 
		 GParamSpec *pspec, 
		 CltrVideo  *video)
{

  parse_stream_info (video);

  /*
  signal = g_new0 (BVWSignal, 1);
  signal->signal_id = ASYNC_NOTIFY_STREAMINFO;

  g_async_queue_push (bvw->priv->queue, signal);

  g_idle_add ((GSourceFunc) bacon_video_widget_signal_idler, bvw);
  */
}

static void
got_source (GObject    *play,
	    GParamSpec *pspec,
	    CltrVideo  *video)
{
  GObject      *source = NULL;
  GObjectClass *klass;

  CLTR_MARK();

  /*
  if (bvw->priv->tagcache) {
    gst_tag_list_free (bvw->priv->tagcache);
    bvw->priv->tagcache = NULL;
  }

  if (!bvw->priv->media_device)
    return;

  g_object_get (play, "source", &source, NULL);
  if (!source)
    return;

  klass = G_OBJECT_GET_CLASS (source);
  if (!g_object_class_find_property (klass, "device"))
    return;

  g_object_set (source, "device", bvw->priv->media_device, NULL);
  */
}


static void
got_buffering (GstElement *play, 
	       gint        percentage,
	       CltrVideo  *video)
{
  CLTR_DBG("Buffering with %i", percentage);

#if 0
  BVWSignal *signal;

  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  signal = g_new0 (BVWSignal, 1);
  signal->signal_id = ASYNC_BUFFERING;
  signal->signal_data.buffering.percent = percentage;

  g_async_queue_push (bvw->priv->queue, signal);

  g_idle_add ((GSourceFunc) bacon_video_widget_signal_idler, bvw);
#endif
}

static void
reset_error_msg (CltrVideo *video)
{
  if (video->last_error_message)
    {
      g_free (video->last_error_message);
      video->last_error_message = NULL;
    }
}


static void
got_error (GstElement *play, 
	   GstElement *orig, 
	   GError     *error,
           gchar      *debug, 
	   CltrVideo  *video)
{

  /* 
     XXX TODO cpy the error message to asyc queueu

  */

  CLTR_MARK();

#if 0
  /* since we're opening, we will never enter the mainloop
   * until we return, so setting an idle handler doesn't
   * help... Anyway, let's prepare a message. */
  if (GST_STATE (play) != GST_STATE_PLAYING) {
    g_free (bvw->priv->last_error_message);
    bvw->priv->last_error_message = g_strdup (error->message);
    return;
  }
  
  signal = g_new0 (BVWSignal, 1);
  signal->signal_id = ASYNC_ERROR;
  signal->signal_data.error.element = orig;
  signal->signal_data.error.error = g_error_copy (error);
  if (debug)
    signal->signal_data.error.debug_message = g_strdup (debug);

  g_async_queue_push (bvw->priv->queue, signal);

  g_idle_add ((GSourceFunc) bacon_video_widget_signal_idler, bvw);
#endif
}


static void
caps_set (GObject         *obj,
	  GParamSpec      *pspec, 
	  CltrVideo       *video)
{
  GstPad *pad = GST_PAD (obj);
  GstStructure *s;

  if (!GST_PAD_CAPS (pad))
    return;

  s = gst_caps_get_structure (GST_PAD_CAPS (pad), 0);

  if (s) 
    {
      /* const GValue *par; */

      if (!(gst_structure_get_double (s, "framerate", &video->video_fps) &&
	    gst_structure_get_int (s, "width", &video->video_width) &&
	    gst_structure_get_int (s, "height", &video->video_height)))
	return;

      /*
      if ((par = gst_structure_get_value (s, "pixel-aspect-ratio"))) 
	{
	  gint num = gst_value_get_fraction_numerator (par),
	    den = gst_value_get_fraction_denominator (par);

	  if (num > den)
	    bvw->priv->video_width *= (gfloat) num / den;
	  else
	    bvw->priv->video_height *= (gfloat) den / num;
	}

	got_video_size (bvw->priv->play, bvw->priv->video_width,
	bvw->priv->video_height, bvw);

      */
  }
}


static void
parse_stream_info (CltrVideo *video)
{
  GList  *streaminfo = NULL;
  GstPad *videopad = NULL;

  g_object_get (G_OBJECT (video->play), "stream-info", &streaminfo, NULL);

  streaminfo = g_list_copy (streaminfo);

  g_list_foreach (streaminfo, (GFunc) g_object_ref, NULL);

  for ( ; streaminfo != NULL; streaminfo = streaminfo->next) 
    {
      GObject *info = streaminfo->data;
      gint type;
      GParamSpec *pspec;
      GEnumValue *val;

      if (!info)
	continue;

      g_object_get (info, "type", &type, NULL);

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (info), "type");

      val = g_enum_get_value (G_PARAM_SPEC_ENUM (pspec)->enum_class, type);

    if (strstr (val->value_name, "AUDIO")) 
      {
	if (!video->has_audio) {
	  video->has_audio = TRUE;
	  /*if (!bvw->priv->media_has_video &&
            bvw->priv->show_vfx && bvw->priv->vis_element) {
	    videopad = gst_element_get_pad (bvw->priv->vis_element, "src");
	    }*/
	}
      } 
    else if (strstr (val->value_name, "VIDEO")) 
      {
	video->has_video = TRUE;
	if (!videopad)
	  g_object_get (info, "object", &videopad, NULL);

      }
    }

  if (videopad) 
    {
      GstPad *real = (GstPad *) GST_PAD_REALIZE (videopad);

      /* handle explicit caps as well - they're set later */
      if (((GstRealPad *) real)->link != NULL && GST_PAD_CAPS (real))
	caps_set (G_OBJECT (real), NULL, video);

      g_signal_connect (real, "notify::caps", G_CALLBACK (caps_set), video);

    } 
  /*
  else if (bvw->priv->show_vfx && bvw->priv->vis_element) 
    {
      fixate_visualization (NULL, NULL, bvw);
    }
  */

  g_list_foreach (streaminfo, (GFunc) g_object_unref, NULL);
  g_list_free (streaminfo);
}

static gboolean
cb_iterate (CltrVideo *video)
{
  GstFormat fmt = GST_FORMAT_TIME;
  gint64          value;

  /* check length/pos of stream */
  if (gst_element_query (GST_ELEMENT (video->play),
			 GST_QUERY_TOTAL, &fmt, &value) 
      && GST_CLOCK_TIME_IS_VALID (value) 
      && value / GST_MSECOND != video->stream_length) 
    {
      got_stream_length (GST_ELEMENT (video->play), value, video);
    }

  if (gst_element_query (GST_ELEMENT (video->play),
			 GST_QUERY_POSITION, &fmt, &value)) 
    {
      got_time_tick (GST_ELEMENT (video->play), value, video);
    }

  return TRUE;
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
		     G_CALLBACK (got_eos), (gpointer) video);

  g_signal_connect (G_OBJECT (video->play), "state-change",
		    G_CALLBACK (got_state_change), (gpointer) video);

  g_signal_connect (G_OBJECT (video->play), "found_tag",
		    G_CALLBACK (got_found_tag), (gpointer) video);

  g_signal_connect (G_OBJECT (video->play), "error",
		    G_CALLBACK (got_error), (gpointer) video);

  g_signal_connect (G_OBJECT (video->play), "buffering",
		    G_CALLBACK (got_buffering), (gpointer) video);

  g_signal_connect (G_OBJECT (video->play), "notify::source",
		    G_CALLBACK (got_source), (gpointer) video);

  g_signal_connect (G_OBJECT (video->play), "notify::stream-info",
		    G_CALLBACK (stream_info_set), (gpointer) video);

  /* what does this do ?
  g_signal_connect (G_OBJECT (video->play), "group-switch",
		    G_CALLBACK (group_switch), (gpointer) video);
  */

  g_signal_connect (G_OBJECT (video->play), "got-redirect",
		    G_CALLBACK (got_redirect), (gpointer) video);


  video->queue = g_async_queue_new ();

  gst_element_set(video->video_sink, "queue", video->queue, NULL);


  return CLTR_WIDGET(video);
}

gboolean
cltr_video_play ( CltrVideo *video, GError ** error)
{
  gboolean ret;

  reset_error_msg (video);

  ret = (gst_element_set_state (GST_ELEMENT (video->play),
				GST_STATE_PLAYING) == GST_STATE_SUCCESS);
  if (!ret)
    {
      g_set_error (error, 0, 0, "%s", video->last_error_message ?
          video->last_error_message : "Failed to play; reason unknown");
    }

  return ret;
}

gboolean
cltr_video_seek (CltrVideo *video, float position, GError **gerror)
{
  gint64 seek_time, length_nanos;

  /* Resetting last_error_message to NULL */
  if (video->last_error_message)
    {
      g_free (video->last_error_message);
      video->last_error_message = NULL;
    }

  length_nanos = (gint64) (video->stream_length * GST_MSECOND);
  seek_time    = (gint64) (length_nanos * position);

  gst_element_seek (video->play, GST_SEEK_METHOD_SET |
		    GST_SEEK_FLAG_FLUSH | GST_FORMAT_TIME,
		    seek_time);

  return TRUE;
}

gboolean
cltr_video_seek_time (CltrVideo *video, gint64 time, GError **gerror)
{
  if (video->last_error_message)
    {
      g_free (video->last_error_message);
      video->last_error_message = NULL;
    }

  gst_element_seek (video->play, GST_SEEK_METHOD_SET |
		    GST_SEEK_FLAG_FLUSH | GST_FORMAT_TIME,
		    time * GST_MSECOND);

  return TRUE;
}

void
cltr_video_stop ( CltrVideo *video)
{
  gst_element_set_state (GST_ELEMENT (video->play), GST_STATE_READY);
}

void
cltr_video_close ( CltrVideo *video)
{
  gst_element_set_state (GST_ELEMENT (video->play), GST_STATE_READY);
  
  /* XX close callback here */
}

void
cltr_video_pause ( CltrVideo *video)
{
  gst_element_set_state (GST_ELEMENT (video->play), GST_STATE_PAUSED);
}


gboolean
cltr_video_can_set_volume ( CltrVideo *video )
{
  return TRUE;
}

void
cltr_video_set_volume ( CltrVideo *video, int volume)
{
  if (cltr_video_can_set_volume (video) != FALSE)
  {
    volume = CLAMP (volume, 0, 100);
    g_object_set (G_OBJECT (video->play), "volume",
	(gdouble) (1. * volume / 100), NULL);
  }
}

int
cltr_video_get_volume ( CltrVideo *video)
{
  gdouble vol;

  g_object_get (G_OBJECT (video->play), "volume", &vol, NULL);

  return (gint) (vol * 100 + 0.5);
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
      {
	Pixbuf *pixb = NULL;

	video->frame_texture = signal->signal_data.texture.ref;

	cltr_texture_lock(video->frame_texture);

	pixb = cltr_texture_get_pixbuf(video->frame_texture);

	if (pixb)
	  cltr_texture_force_rgb_data(video->frame_texture,
				      pixb->width,
				      pixb->height,
				      pixb->data);

	cltr_texture_unlock(video->frame_texture);

	cltr_widget_queue_paint(CLTR_WIDGET(video));
      }
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
cltr_video_set_source(CltrVideo *video, char *mrl)
{
  gboolean ret;

  if (video->mrl && !strcmp (video->mrl, mrl))
    return TRUE;

  if (video->mrl)
    g_free (video->mrl);

  video->mrl = g_strdup (mrl);

  gst_element_set_state (GST_ELEMENT (video->play), GST_STATE_READY);

  reset_error_msg (video);

  /* video->got_redirect  = FALSE; */
  video->has_video     = FALSE;
  video->stream_length = 0;

  /* Dont handle subtitles as yet
  if (g_strrstr (video->mrl, "#subtitle:")) 
    {
      gchar **uris;

      uris = g_strsplit (video->mrl, "#subtitle:", 2);
      g_object_set (G_OBJECT (video->play), "uri",
		    uris[0], "suburi", uris[1], NULL);
      g_strfreev (uris);
    } 
  else 
    {
      g_object_set (G_OBJECT (video->play), "uri",
		    video->mrl, "suburi", subtitle_uri, NULL);
  }
  */

  g_object_set (G_OBJECT (video->play), "uri",
		video->mrl, "suburi", NULL, NULL);


  ret = (gst_element_set_state (video->play, 
				GST_STATE_PAUSED) == GST_STATE_SUCCESS);


  if (!ret /* && !video->got_redirect */)
    {

      /*
      g_set_error (error, 0, 0, "%s", video->last_error_message ?
          video->last_error_message : "Failed to open; reason unknown");
      */

      g_free (video->mrl);
      video->mrl = NULL;
      
      return FALSE;
    }

  /*
  if (ret)
    g_signal_emit (bvw, bvw_table_signals[SIGNAL_CHANNELS_CHANGE], 0);
  */

  return ret;

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
      && video->video_height
      && video->video_width)
    {
      int dis_x = 0, dis_y = 0, dis_height = 0, dis_width = 0;

      if (video->video_width > video->video_height)
	{
	  dis_width  = widget->width;
	  dis_height = ( video->video_height * widget->width )
	                          / video->video_width;
	  dis_y = (widget->height - dis_height)/2;
	  dis_x = 0;
	}


      glEnable(GL_BLEND); 

      glColor4f(1.0, 1.0, 1.0, 1.0);

      glEnable(GL_TEXTURE_2D);

      glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

      cltr_texture_lock(video->frame_texture);

      cltr_texture_render_to_gl_quad(video->frame_texture, 
				     dis_x, 
				     dis_y,
				     dis_x + dis_width,
				     dis_y + dis_height);

      cltr_texture_unlock(video->frame_texture);

      glDisable(GL_TEXTURE_2D); 

      glColor4f(1.0, 1.0, 1.0, 0.5);

      // glRecti(100, 100, 600, 600);
    }

  glPopMatrix();
}
