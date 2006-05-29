/* Heavily based on totems bacon-video-widget .. */

#include "clutter-video-texture.h"
#include "clutter-main.h"
#include "clutter-private.h" 	/* for DBG */
#include "clutter-marshal.h"

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/audio/gstbaseaudiosink.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include <glib.h>

static void
got_time_tick (GstElement          *play, 
	       gint64               time_nanos, 
	       ClutterVideoTexture *video_texture);
static void
stop_play_pipeline (ClutterVideoTexture *video_texture);

G_DEFINE_TYPE (ClutterVideoTexture,   \
               clutter_video_texture, \
               CLUTTER_TYPE_TEXTURE);

enum
{
  SIGNAL_ERROR,
  SIGNAL_EOS,
  SIGNAL_REDIRECT,
  SIGNAL_TITLE_CHANGE,
  SIGNAL_CHANNELS_CHANGE,
  SIGNAL_TICK,
  SIGNAL_GOT_METADATA,
  SIGNAL_BUFFERING,
  SIGNAL_SPEED_WARNING,
  LAST_SIGNAL
};

/* Properties */
enum
{
  PROP_0,
  PROP_POSITION,
  PROP_CURRENT_TIME,
  PROP_STREAM_LENGTH,
  PROP_PLAYING,
  PROP_SEEKABLE,
};

struct ClutterVideoTexturePrivate
{
  GstElement                     *play, *video_sink, *audio_sink;
  gboolean                        has_video, has_audio;
  gint64                          stream_length;
  gint64                          current_time_nanos;
  gint64                          current_time;
  float                           current_position;
  gchar                          *mrl;

  gboolean                        got_redirect;
  gint                            eos_id;
  guint                           update_id;

  GstTagList                     *tagcache, *audiotags, *videotags;

  const GValue                   *movie_par; /* Movie pixel aspect ratio */
  gint                            video_fps_d, video_fps_n;
  gint                            video_width, video_height;
  ClutterVideoTextureAspectRatio  ratio_type;

  GstMessageType                  ignore_messages_mask;
  GstBus                         *bus;
  gulong                          sig_bus_async;
};

static int cvt_signals[LAST_SIGNAL] = { 0 };

GQuark
clutter_video_texture_error_quark (void)
{
  return g_quark_from_static_string ("clutter-video-texture-error-quark");
}

/* This is a hack to avoid doing poll_for_state_change() indirectly
 * from the bus message callback (via EOS => totem => close => wait for READY)
 * and deadlocking there. We need something like a
 * gst_bus_set_auto_flushing(bus, FALSE) ... */
static gboolean
signal_eos_delayed (gpointer user_data)
{
  ClutterVideoTexture *video_texture = (ClutterVideoTexture*)user_data;

  g_signal_emit (video_texture, cvt_signals[SIGNAL_EOS], 0, NULL);

  video_texture->priv->eos_id = 0;

  return FALSE;
}

static gboolean
query_timeout (ClutterVideoTexture *video_texture)
{
  ClutterVideoTexturePrivate *priv;
  GstFormat                   fmt = GST_FORMAT_TIME;
  gint64                      prev_len = -1, pos = -1, len = -1;

  priv = video_texture->priv;
  
  /* check length/pos of stream */
  prev_len = priv->stream_length;

  if (gst_element_query_duration (priv->play, &fmt, &len)) 
    {
      if (len != -1 && fmt == GST_FORMAT_TIME) 
	{
	  priv->stream_length = len / GST_MSECOND;
	  if (priv->stream_length != prev_len) 
	    {
	      g_signal_emit (video_texture, 
			     cvt_signals[SIGNAL_GOT_METADATA], 0, NULL);
	    }
	}
    } 
  else 
    {
      CLUTTER_DBG ("could not get duration");
    }

  if (gst_element_query_position (priv->play, &fmt, &pos)) 
    {
      if (pos != -1 && fmt == GST_FORMAT_TIME) 
	{
	  got_time_tick (GST_ELEMENT (priv->play), pos, video_texture);
	}
    } 
  else 
    CLUTTER_DBG ("could not get position");

  return TRUE;
}

static void
got_video_size (ClutterVideoTexture *video_texture)
{
  GstMessage *msg;

  g_return_if_fail (video_texture != NULL);

  /* Do we even care about this info as comes from texture sizing ? */
  CLUTTER_DBG("%ix%i", 
	      video_texture->priv->video_width,
	      video_texture->priv->video_height);

  msg = gst_message_new_application 
    (GST_OBJECT (video_texture->priv->play),
        gst_structure_new ("video-size", 
			   "width", G_TYPE_INT,
			   video_texture->priv->video_width, 
			   "height", G_TYPE_INT,
			   video_texture->priv->video_height, NULL));

  gst_element_post_message (video_texture->priv->play, msg);
}


static void
caps_set (GObject             *obj,
	  GParamSpec          *pspec, 
	  ClutterVideoTexture *video_texture)
{
  ClutterVideoTexturePrivate *priv;
  GstPad                     *pad = GST_PAD (obj);
  GstStructure               *s;
  GstCaps                    *caps;

  priv = video_texture->priv;

  if (!(caps = gst_pad_get_negotiated_caps (pad)))
    return;

  /* Get video decoder caps */
  s = gst_caps_get_structure (caps, 0);

  /* Again do we even need this - sizing info from texture signal.. */

  if (s) 
    {
      /* We need at least width/height and framerate */
      if (!(gst_structure_get_fraction (s, "framerate", 
					&priv->video_fps_n, 
					&priv->video_fps_d) 
	    &&
	    gst_structure_get_int (s, "width", &priv->video_width) &&
	    gst_structure_get_int (s, "height", &priv->video_height)))
	return;

    /* Get the movie PAR if available */
    priv->movie_par = gst_structure_get_value (s, "pixel-aspect-ratio");
    
    /* Now set for real */
    clutter_video_texture_set_aspect_ratio (video_texture, priv->ratio_type);
  }

  gst_caps_unref (caps);
}

static void
parse_stream_info (ClutterVideoTexture *video_texture)
{
  ClutterVideoTexturePrivate *priv;
  GList                      *streaminfo = NULL;
  GstPad                     *videopad = NULL;

  priv = video_texture->priv;

  g_object_get (priv->play, "stream-info", &streaminfo, NULL);

  streaminfo = g_list_copy (streaminfo);

  g_list_foreach (streaminfo, (GFunc) g_object_ref, NULL);

  for ( ; streaminfo != NULL; streaminfo = streaminfo->next) 
    {
      GObject    *info = streaminfo->data;
      gint        type;
      GParamSpec *pspec;
      GEnumValue *val;

      if (!info)
	continue;

      g_object_get (info, "type", &type, NULL);

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (info), "type");
      val = g_enum_get_value (G_PARAM_SPEC_ENUM (pspec)->enum_class, type);

      if (!g_strcasecmp (val->value_nick, "audio")) 
	{
	  priv->has_audio = TRUE;
	} 
      else if (!g_strcasecmp (val->value_nick, "video")) 
	{
	  priv->has_video = TRUE;

	  if (!videopad) 
	    g_object_get (info, "object", &videopad, NULL);
	}
    }

  if (videopad) 
    {
      GstCaps *caps;

      if ((caps = gst_pad_get_negotiated_caps (videopad))) 
	{
	  caps_set (G_OBJECT (videopad), NULL, video_texture);
	  gst_caps_unref (caps);
	}

      g_signal_connect (videopad, "notify::caps",
			G_CALLBACK (caps_set), video_texture);
    } 

  g_list_foreach (streaminfo, (GFunc) g_object_unref, NULL);
  g_list_free (streaminfo);
}

static void
handle_element_message (ClutterVideoTexture *video_texture, GstMessage *msg)
{
  const gchar *type_name = NULL;
  gchar       *src_name;

  CLUTTER_MARK();

  src_name = gst_object_get_name (msg->src);

  if (msg->structure)
    type_name = gst_structure_get_name (msg->structure);

  if (type_name == NULL)
    goto unhandled;

  if (strcmp (type_name, "redirect") == 0) 
    {
      const gchar *new_location;

      new_location = gst_structure_get_string (msg->structure, "new-location");

      CLUTTER_DBG ("Got redirect to '%s'", GST_STR_NULL (new_location));

      if (new_location && *new_location) 
	{
	  g_signal_emit (video_texture, 
			 cvt_signals[SIGNAL_REDIRECT], 
			 0, 
			 new_location);
	  goto done;
	}
    }

 unhandled:
  CLUTTER_DBG ("Unhandled element message '%s' from element '%s'",
	       GST_STR_NULL (type_name), src_name);
 done:
  g_free (src_name);
}

static void
handle_application_message (ClutterVideoTexture *video_texture, 
			    GstMessage          *msg)
{
  const gchar *msg_name;

  msg_name = gst_structure_get_name (msg->structure);

  g_return_if_fail (msg_name != NULL);

  CLUTTER_DBG ("Handling application message");

  if (strcmp (msg_name, "notify-streaminfo") == 0) 
    {
      g_signal_emit (video_texture, cvt_signals[SIGNAL_GOT_METADATA], 0, NULL);
      g_signal_emit (video_texture, cvt_signals[SIGNAL_CHANNELS_CHANGE], 0);
    }
  else if (strcmp (msg_name, "video-size") == 0)
    {
      g_signal_emit (video_texture, cvt_signals[SIGNAL_GOT_METADATA], 0, NULL);

      CLUTTER_DBG("Got video size");
    }
  else
    CLUTTER_DBG ("Unhandled application message %s", msg_name);
}

static void
bus_message_cb (GstBus *bus, GstMessage *message, gpointer data)
{
  ClutterVideoTexturePrivate *priv;
  ClutterVideoTexture        *video_texture = (ClutterVideoTexture*)data;
  GstMessageType              msg_type;

  g_return_if_fail (video_texture != NULL);
  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE(video_texture));

  priv = video_texture->priv;

  msg_type = GST_MESSAGE_TYPE (message);

  /* somebody else is handling the message, 
     probably in poll_for_state_change */
  if (priv->ignore_messages_mask & msg_type) 
    {
      gchar *src_name = gst_object_get_name (message->src);
      CLUTTER_DBG ("Ignoring %s message from element %s as requested",
		   gst_message_type_get_name (msg_type), src_name);
      g_free (src_name);
      return;
    }

  if (msg_type != GST_MESSAGE_STATE_CHANGED && clutter_want_debug()) 
    {
      gchar *src_name = gst_object_get_name (message->src);
      CLUTTER_DBG ("Handling %s message from element %s",
		   gst_message_type_get_name (msg_type), src_name);
      g_free (src_name);
    }

  switch (msg_type) 
    {
    case GST_MESSAGE_ERROR: 
      {
	GError *error = NULL;
	gchar  *debug = NULL;

	gst_message_parse_error (message, &error, &debug);

	CLUTTER_DBG ("Error message: %s [%s]", 
		     GST_STR_NULL (error->message),
		     GST_STR_NULL (debug));

	g_signal_emit (video_texture, 
		       cvt_signals[SIGNAL_ERROR], 
		       0,
		       error->message, 
		       TRUE, 
		       FALSE);

	g_error_free (error);

	if (priv->play)
	  gst_element_set_state (priv->play, GST_STATE_NULL);

	g_free (debug);
	break;
      }
    case GST_MESSAGE_WARNING: 
      {
	GError *error = NULL;
	gchar  *debug = NULL;

	gst_message_parse_warning (message, &error, &debug);

	g_warning ("%s [%s]", 
		   GST_STR_NULL (error->message), 
		   GST_STR_NULL (debug));

	g_error_free (error);
	g_free (debug);
	break;
      }
    case GST_MESSAGE_TAG: 
      {
	GstTagList *tag_list, *result;
	GstElementFactory *f;

	gst_message_parse_tag (message, &tag_list);

	CLUTTER_DBG ("Tags: %p", tag_list);

	/* all tags */
	result = gst_tag_list_merge (priv->tagcache, 
				     tag_list, 
				     GST_TAG_MERGE_KEEP);

	if (priv->tagcache)
	  gst_tag_list_free (priv->tagcache);
	priv->tagcache = result;
	
	/* media-type-specific tags */
	if (GST_IS_ELEMENT (message->src) &&
	    (f = gst_element_get_factory (GST_ELEMENT (message->src)))) 
	  {
	    const gchar *klass = gst_element_factory_get_klass (f);
	    GstTagList **cache = NULL;
	  
	  if (g_strrstr (klass, "Video")) 
	    {
	      cache = &priv->videotags;
	    } 
	  else if (g_strrstr (klass, "Audio")) 
	    {
	      cache = &priv->audiotags;
	    }
	  
	  if (cache) 
	    {
	      result = gst_tag_list_merge (*cache, tag_list, 
					   GST_TAG_MERGE_KEEP);
	      if (*cache)
		gst_tag_list_free (*cache);
	      *cache = result;
	    }
	  }

	gst_tag_list_free (tag_list);
	g_signal_emit (video_texture, cvt_signals[SIGNAL_GOT_METADATA], 0);
	break;
    }
    case GST_MESSAGE_EOS:
      CLUTTER_DBG ("GST_MESSAGE_EOS");

      if (priv->eos_id == 0)
        priv->eos_id = g_idle_add (signal_eos_delayed, video_texture);
      break;
    case GST_MESSAGE_BUFFERING: 
      {
	gint percent = 0;
	gst_structure_get_int (message->structure, "buffer-percent", &percent);

	CLUTTER_DBG ("Buffering message (%u%%)", percent);

	g_signal_emit (video_texture, 
		       cvt_signals[SIGNAL_BUFFERING], 
		       0, 
		       percent);
	break;
      }

    case GST_MESSAGE_APPLICATION: 
      handle_application_message (video_texture, message);
      break;

    case GST_MESSAGE_STATE_CHANGED: 
      {
	GstState old_state, new_state;
	gchar   *src_name;

	CLUTTER_DBG ("GST_MESSAGE_STATE_CHANGED");

	gst_message_parse_state_changed (message, 
					 &old_state, &new_state, NULL);

	if (old_state == new_state)
	  break;

	/* we only care about playbin (pipeline) state changes */
	if (GST_MESSAGE_SRC (message) != GST_OBJECT (priv->play))
	  break;

	src_name = gst_object_get_name (message->src);

	CLUTTER_DBG ("%s changed state from %s to %s", src_name,
		     gst_element_state_get_name (old_state),
		     gst_element_state_get_name (new_state));

	g_free (src_name);

      if (new_state <= GST_STATE_PAUSED) 
	{
	  if (priv->update_id != 0) 
	    {
	      CLUTTER_DBG ("removing tick timeout");
	      g_source_remove (priv->update_id);
	      priv->update_id = 0;
	    }
	} 
      else if (new_state > GST_STATE_PAUSED) 
	{
	  if (priv->update_id == 0) 
	    {
	      CLUTTER_DBG ("starting tick timeout");

	      priv->update_id = g_timeout_add (200, 
					       (GSourceFunc) query_timeout, 
					       video_texture);
	    }
	}

      if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) 
	{
	  parse_stream_info (video_texture);
	} 
      else if (old_state == GST_STATE_PAUSED && new_state == GST_STATE_READY) 
	{
	  priv->has_video = FALSE;
	  priv->has_audio = FALSE;

	  /* clean metadata cache */
	  if (priv->tagcache) 
	    {
	      gst_tag_list_free (priv->tagcache);
	      priv->tagcache = NULL;
	    }

	  if (priv->audiotags) 
	    {
	      gst_tag_list_free (priv->audiotags);
	      priv->audiotags = NULL;
	    }

	  if (priv->videotags) 
	    {
	      gst_tag_list_free (priv->videotags);
	      priv->videotags = NULL;
	    }
	  
	  priv->video_width = 0;
	  priv->video_height = 0;
	}
      break;
    }

    case GST_MESSAGE_ELEMENT:
      handle_element_message (video_texture, message);
      break;

    case GST_MESSAGE_DURATION: 
      {
	CLUTTER_DBG ("GST_MESSAGE_DURATION");
	/* force _get_stream_length() to do new duration query */
	priv->stream_length = 0;
	if (clutter_video_texture_get_stream_length (video_texture) == 0)
	  CLUTTER_DBG ("Failed to query duration after DURATION message?!");
	break;
      }

    case GST_MESSAGE_CLOCK_PROVIDE:
    case GST_MESSAGE_CLOCK_LOST:
    case GST_MESSAGE_NEW_CLOCK:
    case GST_MESSAGE_STATE_DIRTY:
      break;
    default:
      CLUTTER_DBG ("Unhandled message of type '%s' (0x%x)", 
		   gst_message_type_get_name (msg_type), msg_type);
      break;
    }
}

static void
got_time_tick (GstElement          *play, 
	       gint64               time_nanos, 
	       ClutterVideoTexture *video_texture)
{
  gboolean                    seekable;
  ClutterVideoTexturePrivate *priv;

  g_return_if_fail (video_texture != NULL);
  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE(video_texture));

  priv = video_texture->priv;

  priv->current_time_nanos = time_nanos;
  priv->current_time       = (gint64) time_nanos / GST_MSECOND;

  if (priv->stream_length == 0) 
    {
      priv->current_position = 0;
      seekable = clutter_video_texture_is_seekable (video_texture);
    }
  else
    {
      priv->current_position =
	(gfloat) priv->current_time / priv->stream_length;
      seekable = TRUE;
    }

  g_signal_emit (video_texture, 
		 cvt_signals[SIGNAL_TICK], 
		 0,
                 priv->current_time, 
		 priv->stream_length,
                 priv->current_position,
                 seekable);
}

static void
playbin_got_source (GObject             *play,
		    GParamSpec          *pspec,
		    ClutterVideoTexture *video_texture)
{
  /* Called via notify::source on playbin */

  ClutterVideoTexturePrivate *priv;
  GObject                    *source = NULL;

  priv = video_texture->priv;

  if (priv->tagcache) 
    {
      gst_tag_list_free (priv->tagcache);
      priv->tagcache = NULL;
    }

  if (priv->audiotags) 
    {
      gst_tag_list_free (priv->audiotags);
      priv->audiotags = NULL;
    }

  if (priv->videotags) 
    {
      gst_tag_list_free (priv->videotags);
      priv->videotags = NULL;
    }

  g_object_get (play, "source", &source, NULL);

  if (!source)
    return;

  g_object_unref (source);
}

static void
playbin_stream_info_set (GObject             *obj, 
			 GParamSpec          *pspec, 
			 ClutterVideoTexture *video_texture)
{
  ClutterVideoTexturePrivate *priv;
  GstMessage                 *msg;

  priv = video_texture->priv;  

  parse_stream_info (video_texture);

  msg = gst_message_new_application (GST_OBJECT (priv->play),
				     gst_structure_new ("notify-streaminfo", 
							NULL));
  gst_element_post_message (priv->play, msg);
}


static gboolean
poll_for_state_change_full (ClutterVideoTexture *video_texture,
			    GstElement          *element,
			    GstState             state, 
			    GError             **error, 
			    gint64               timeout)
{
  GstBus                     *bus;
  GstMessageType              events, saved_events;
  ClutterVideoTexturePrivate *priv;

  priv         = video_texture->priv;
  bus          = gst_element_get_bus (element);
  saved_events = priv->ignore_messages_mask;

  events  = GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS;

  if (element != NULL && element == priv->play) 
    {
      /* we do want the main handler to process state changed messages for
       * playbin as well, otherwise it won't hook up the timeout etc. */
      priv->ignore_messages_mask |= (events ^ GST_MESSAGE_STATE_CHANGED);
    } 
  else 
    priv->ignore_messages_mask |= events;

  while (TRUE) 
    {
      GstMessage *message;
      GstElement *src;

      message = gst_bus_poll (bus, events, timeout);
    
      if (!message)
	goto timed_out;
    
      src = (GstElement*)GST_MESSAGE_SRC (message);

      switch (GST_MESSAGE_TYPE (message)) 
	{
	case GST_MESSAGE_STATE_CHANGED:
	  {
	    GstState old, new, pending;

	    if (src == element) 
	      {
		gst_message_parse_state_changed (message, 
						 &old, &new, &pending);
		if (new == state) 
		  {
		    gst_message_unref (message);
		    goto success;
		  }
	      }
	  }
	  break;
	case GST_MESSAGE_ERROR:
	  {
	    gchar  *debug = NULL;
	    GError *gsterror = NULL;

	    gst_message_parse_error (message, &gsterror, &debug);

	    g_warning ("Error: %s (%s)", gsterror->message, debug);

	    gst_message_unref (message);
	    g_error_free (gsterror);
	    g_free (debug);
	    goto error;
	  }
	  break;
	case GST_MESSAGE_EOS:
	  g_set_error (error, CLUTTER_VIDEO_TEXTURE_ERROR,
		       CLUTTER_VIDEO_TEXTURE_ERROR_FILE_GENERIC,
		       "Media file could not be played.");
	  gst_message_unref (message);
	  goto error;
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}
      gst_message_unref (message);
    }
  
  g_assert_not_reached ();

success:
  /* state change succeeded */
  CLUTTER_DBG ("state change to %s succeeded", 
	       gst_element_state_get_name (state));

  priv->ignore_messages_mask = saved_events;
  return TRUE;

timed_out:
  /* it's taking a long time to open -- just tell totem it was ok, this allows
   * the user to stop the loading process with the normal stop button */
  CLUTTER_DBG ("state change to %s timed out, returning success and handling "
	       "errors asynchroneously", gst_element_state_get_name (state));
  priv->ignore_messages_mask = saved_events;
  return TRUE;

error:
  CLUTTER_DBG ("error while waiting for state change to %s: %s",
	       gst_element_state_get_name (state),
	       (error && *error) ? (*error)->message : "unknown");
  priv->ignore_messages_mask = saved_events;
  return FALSE;
}

static gboolean
poll_for_state_change (ClutterVideoTexture *video_texture, 
		       GstElement          *element,
		       GstState             state, 
		       GError             **error)
{
  return poll_for_state_change_full (video_texture, 
				     element, 
				     state, 
				     error, 
				     GST_SECOND/4 );
}

static void
fakesink_handoff_cb (GstElement *fakesrc, 
		     GstBuffer  *buffer,
		     GstPad     *pad, 
		     gpointer    user_data)
{

  GstStructure  *structure;
  int            width, height;
  GdkPixbuf     *pixb;

  structure = gst_caps_get_structure(GST_CAPS(buffer->caps), 0);
  gst_structure_get_int(structure, "width", &width);
  gst_structure_get_int(structure, "height", &height);

  /* FIXME: We really dont want to do this every time as gobject creation
   *        really need a clutter_texture_set_from_data call ?
  */
  pixb = gdk_pixbuf_new_from_data (GST_BUFFER_DATA (buffer),
				   GDK_COLORSPACE_RGB, 
				   FALSE, 
				   8, 
				   width, 
				   height,
				   (3 * width + 3) &~ 3,
				   NULL,
				   NULL);

  if (pixb)
    {
      clutter_texture_set_pixbuf (CLUTTER_TEXTURE(user_data), pixb);
      g_object_unref(G_OBJECT(pixb));
    }
}

static void 
clutter_video_texture_finalize (GObject *object)
{
  ClutterVideoTexture        *self;
  ClutterVideoTexturePrivate *priv; 

  self = CLUTTER_VIDEO_TEXTURE(object); 
  priv = self->priv;

  if (priv->bus) 
    {
      /* make bus drop all messages to make sure none of our callbacks is ever
       * called again (main loop might be run again to display error dialog) */
      gst_bus_set_flushing (priv->bus, TRUE);

      if (priv->sig_bus_async)
	g_signal_handler_disconnect (priv->bus, priv->sig_bus_async);
      
      gst_object_unref (priv->bus);
      priv->bus = NULL;
    }

  if (priv->mrl)
    g_free (priv->mrl);
  priv->mrl = NULL;
  
  if (priv->play != NULL && GST_IS_ELEMENT (priv->play)) 
    {
      gst_element_set_state (priv->play, GST_STATE_NULL);
      gst_object_unref (priv->play);
      priv->play = NULL;
    }

  if (priv->update_id) 
    {
      g_source_remove (priv->update_id);
      priv->update_id = 0;
    }

  if (priv->tagcache) 
    {
      gst_tag_list_free (priv->tagcache);
      priv->tagcache = NULL;
    }

  if (priv->audiotags) 
    {
      gst_tag_list_free (priv->audiotags);
      priv->audiotags = NULL;
    }

  if (priv->videotags) 
    {
      gst_tag_list_free (priv->videotags);
      priv->videotags = NULL;
    }

  if (priv->eos_id != 0)
    g_source_remove (priv->eos_id);

  g_free (priv);

  self->priv = NULL;

  G_OBJECT_CLASS (clutter_video_texture_parent_class)->finalize (object);
}

static void
clutter_video_texture_set_property (GObject      *object, 
				    guint         property_id,
				    const GValue *value, 
				    GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_video_texture_get_property (GObject    *object, 
				    guint       property_id,
				    GValue     *value, 
				    GParamSpec *pspec)
{
  ClutterVideoTexture *video_texture;

  video_texture = CLUTTER_VIDEO_TEXTURE (object);

  switch (property_id)
    {
      case PROP_POSITION:
	g_value_set_int64 (value, 
			   clutter_video_texture_get_position (video_texture));
	break;
      case PROP_STREAM_LENGTH:
	g_value_set_int64 (value,
	    clutter_video_texture_get_stream_length (video_texture));
	break;
      case PROP_PLAYING:
	g_value_set_boolean (value,
	    clutter_video_texture_is_playing (video_texture));
	break;
      case PROP_SEEKABLE:
	g_value_set_boolean (value,
	    clutter_video_texture_is_seekable (video_texture));
	break;
      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


static void
clutter_video_texture_class_init (ClutterVideoTextureClass *klass)
{
  GObjectClass        *object_class;
  ClutterElementClass *element_class;

  object_class = (GObjectClass*)klass;
  element_class = (ClutterElementClass*)klass;

  object_class->finalize = clutter_video_texture_finalize;
  object_class->set_property = clutter_video_texture_set_property;
  object_class->get_property = clutter_video_texture_get_property;

  /* Properties */
  g_object_class_install_property (object_class, PROP_POSITION,
				   g_param_spec_int ("position", NULL, NULL,
						     0, G_MAXINT, 0,
						     G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_STREAM_LENGTH,
				   g_param_spec_int64 ("stream_length", NULL,
						     NULL, 0, G_MAXINT64, 0,
						     G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_PLAYING,
				   g_param_spec_boolean ("playing", NULL,
							 NULL, FALSE,
							 G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_SEEKABLE,
				   g_param_spec_boolean ("seekable", NULL,
							 NULL, FALSE,
							 G_PARAM_READABLE));

  /* Signals */
  cvt_signals[SIGNAL_ERROR] =
    g_signal_new ("error",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterVideoTextureClass, error),
		  NULL, NULL,
		  clutter_marshal_VOID__STRING_BOOLEAN_BOOLEAN,
		  G_TYPE_NONE, 3, G_TYPE_STRING,
		  G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

  cvt_signals[SIGNAL_EOS] =
    g_signal_new ("eos",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterVideoTextureClass, eos),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  cvt_signals[SIGNAL_GOT_METADATA] =
    g_signal_new ("got-metadata",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterVideoTextureClass, got_metadata),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  cvt_signals[SIGNAL_REDIRECT] =
    g_signal_new ("got-redirect",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterVideoTextureClass, got_redirect),
		  NULL, NULL, g_cclosure_marshal_VOID__STRING,
		  G_TYPE_NONE, 1, G_TYPE_STRING);

  cvt_signals[SIGNAL_TITLE_CHANGE] =
    g_signal_new ("title-change",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterVideoTextureClass, title_change),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__STRING,
		  G_TYPE_NONE, 1, G_TYPE_STRING);

  cvt_signals[SIGNAL_CHANNELS_CHANGE] =
    g_signal_new ("channels-change",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterVideoTextureClass, channels_change),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  cvt_signals[SIGNAL_TICK] =
    g_signal_new ("tick",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterVideoTextureClass, tick),
		  NULL, NULL,
		  clutter_marshal_VOID__INT64_INT64_FLOAT_BOOLEAN,
		  G_TYPE_NONE, 4, G_TYPE_INT64, G_TYPE_INT64, G_TYPE_FLOAT,
                  G_TYPE_BOOLEAN);

  cvt_signals[SIGNAL_BUFFERING] =
    g_signal_new ("buffering",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterVideoTextureClass, buffering),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  cvt_signals[SIGNAL_SPEED_WARNING] =
    g_signal_new ("speed-warning",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterVideoTextureClass, speed_warning),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

}


static void
clutter_video_texture_init (ClutterVideoTexture *video_texture)
{
  ClutterVideoTexturePrivate *priv;
  GstElement                 *audio_sink, *video_sink, *bin, *capsfilter;
  GstCaps                    *filtercaps;
  GstPad                     *ghost_pad;

  priv                 = g_new0 (ClutterVideoTexturePrivate, 1);
  video_texture->priv  = priv;

  priv->ratio_type     = CLUTTER_VIDEO_TEXTURE_AUTO;

  priv->play = gst_element_factory_make ("playbin", "play");
  
  if (!priv->play) 
    {
      g_warning ("Could not create element 'playbin'");
      return;
    }

  priv->bus = gst_element_get_bus (priv->play);
  gst_bus_add_signal_watch (priv->bus);
  priv->sig_bus_async = g_signal_connect (priv->bus, 
					  "message", 
					  G_CALLBACK (bus_message_cb),
					  video_texture);

  audio_sink = gst_element_factory_make ("gconfaudiosink", "audio-sink");

  if (audio_sink == NULL) 
    {
      g_warning ("Could not create element 'gconfaudiosink' trying autosink");
      audio_sink = gst_element_factory_make ("autoaudiosink", "audio-sink");

      if (audio_sink == NULL)
	{
	  g_warning ("Could not create element 'autoaudiosink' "
		     "trying fakesink");
	  audio_sink = gst_element_factory_make ("fakesink",  
						 "audio-fake-sink");
	  if (audio_sink == NULL)
	    {
	      g_warning ("Could not create element 'fakesink' for audio, giving up. ");
	    }
	}
    }
  
  priv->audio_sink = audio_sink;

  video_sink = gst_element_factory_make ("fakesink", "fakesink");

  if (video_sink == NULL) 
    {
      g_warning ("Could not create element 'fakesink' for video playback");
      return;
    }

  bin = gst_bin_new  ("video-bin");

  capsfilter = gst_element_factory_make ("capsfilter", "vfilter");

  filtercaps 
    = gst_caps_new_simple("video/x-raw-rgb",
			  "bpp", G_TYPE_INT, 24,
			  "depth", G_TYPE_INT, 24,
			  "endianness", G_TYPE_INT, G_BIG_ENDIAN, 
			  "red_mask", G_TYPE_INT, 0xff0000 /* >> 8 for 24bpp */, 
			  "green_mask", G_TYPE_INT, 0xff00,
			  "blue_mask", G_TYPE_INT,  0xff,
			  "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
			  "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
			  "framerate", GST_TYPE_FRACTION_RANGE, 
			  0, 1, G_MAXINT, 1, 
			  NULL);

  g_object_set(G_OBJECT(capsfilter), "caps", filtercaps, NULL);

  gst_bin_add(GST_BIN(bin), capsfilter);
  gst_bin_add(GST_BIN(bin), video_sink);

  gst_element_link (capsfilter, video_sink);

  ghost_pad = gst_ghost_pad_new ("sink", 
				 gst_element_get_pad (capsfilter, "sink"));

  gst_element_add_pad (bin, ghost_pad);

  g_object_set (G_OBJECT(video_sink), 
		"signal-handoffs", TRUE, 
		"sync", TRUE,
		NULL);

  g_signal_connect(G_OBJECT (video_sink), "handoff",
		   G_CALLBACK(fakesink_handoff_cb), video_texture);

  priv->video_sink = bin;

  if (priv->video_sink)
    g_object_set (priv->play, "video-sink", bin, NULL);

  if (priv->audio_sink)
    g_object_set (priv->play, "audio-sink", audio_sink, NULL);
  
  g_signal_connect (priv->play, "notify::source",
		    G_CALLBACK (playbin_got_source), video_texture);
  g_signal_connect (priv->play, "notify::stream-info",
		    G_CALLBACK (playbin_stream_info_set), video_texture);
  return;
}

ClutterElement*
clutter_video_texture_new (void)
{
  ClutterVideoTexture        *video_texture;

  video_texture = g_object_new (CLUTTER_TYPE_VIDEO_TEXTURE, 
				"tiled", FALSE, 
				"pixel-format", GL_RGB,
				NULL);

  return CLUTTER_ELEMENT(video_texture);
}

gboolean
clutter_video_texture_open (ClutterVideoTexture *video_texture,
			    const gchar         *mrl, 
			    const gchar         *subtitle_uri, 
			    GError             **error)
{
  ClutterVideoTexturePrivate *priv;
  gboolean                    ret;

  priv = video_texture->priv;

  g_return_val_if_fail (video_texture != NULL, FALSE);
  g_return_val_if_fail (mrl != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE(video_texture), FALSE);
  g_return_val_if_fail (priv->play != NULL, FALSE);
  
  if (priv->mrl && strcmp (priv->mrl, mrl) == 0) 
    return TRUE;

  /* this allows non-URI type of files in the thumbnailer and so on */
  if (priv->mrl)
    g_free (priv->mrl);

  if (mrl[0] == '/') 
    {
      priv->mrl = g_strdup_printf ("file://%s", mrl);
    }
  else 
    {
      if (strchr (mrl, ':')) 
	{
	  priv->mrl = g_strdup (mrl);
	} 
      else 
	{
	  gchar *cur_dir = g_get_current_dir ();
	  if (!cur_dir) 
	    {
	      g_set_error (error, CLUTTER_VIDEO_TEXTURE_ERROR,
			   CLUTTER_VIDEO_TEXTURE_ERROR_GENERIC,
			   "Failed to retrieve working directory");
	      return FALSE;
	    }
	  priv->mrl = g_strdup_printf ("file://%s/%s", cur_dir, mrl);
	  g_free (cur_dir);
	}
    }

  priv->got_redirect = FALSE;
  priv->has_video    = FALSE;
  priv->has_audio    = FALSE;
  priv->stream_length = 0;
  
  if (g_strrstr (priv->mrl, "#subtitle:")) 
    {
      gchar **uris;
      gchar *subtitle_uri;

      uris = g_strsplit (priv->mrl, "#subtitle:", 2);
      /* Try to fix subtitle uri if needed */
      if (uris[1][0] == '/') 
	{
	  subtitle_uri = g_strdup_printf ("file://%s", uris[1]);
	}
      else 
	{
	  if (strchr (uris[1], ':')) 
	    {
	      subtitle_uri = g_strdup (uris[1]);
	    } 
	  else 
	    {
	      gchar *cur_dir = g_get_current_dir ();
	      if (!cur_dir) 
		{
		  g_set_error (error, CLUTTER_VIDEO_TEXTURE_ERROR,
			       CLUTTER_VIDEO_TEXTURE_ERROR_GENERIC,
			       "Failed to retrieve working directory");
		  return FALSE;
		}

	      subtitle_uri = g_strdup_printf ("file://%s/%s", 
					      cur_dir, uris[1]);
	      g_free (cur_dir);
	    }
	}

      g_object_set (priv->play, 
		    "uri", priv->mrl,
		    "suburi", subtitle_uri, 
		    NULL);
      g_free (subtitle_uri);
      g_strfreev (uris);
    } 
  else 
    {
      g_object_set (priv->play, 
		    "uri", priv->mrl,
		    "suburi", subtitle_uri, 
		    NULL);
    }
  
  gst_element_set_state (priv->play, GST_STATE_PAUSED);

  ret = poll_for_state_change (video_texture, 
			       priv->play, 
			       GST_STATE_PAUSED, 
			       NULL);

  if (!ret) 
    {
      priv->ignore_messages_mask |= GST_MESSAGE_ERROR;
      stop_play_pipeline (video_texture);
      g_free (priv->mrl);
      priv->mrl = NULL;
    }
  else
    g_signal_emit (video_texture, cvt_signals[SIGNAL_CHANNELS_CHANGE], 0);

  return ret;
}

gboolean
clutter_video_texture_play (ClutterVideoTexture *video_texture, 
			    GError             ** error)
{
  ClutterVideoTexturePrivate *priv;
  gboolean                    ret;

  priv = video_texture->priv;

  g_return_val_if_fail (video_texture != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE(video_texture), FALSE);
  g_return_val_if_fail (priv->play != NULL, FALSE);

  gst_element_set_state (priv->play, GST_STATE_PLAYING); 

  ret = poll_for_state_change (video_texture, 
			       priv->play, 
			       GST_STATE_PLAYING, 
			       error);
  return ret;
}

void
clutter_video_texture_pause (ClutterVideoTexture *video_texture)
{
  g_return_if_fail (video_texture != NULL);
  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture));
  g_return_if_fail (GST_IS_ELEMENT (video_texture->priv->play));

  CLUTTER_DBG ("Pausing");

  gst_element_set_state (GST_ELEMENT (video_texture->priv->play), 
			 GST_STATE_PAUSED);
}


gboolean
clutter_video_texture_can_direct_seek (ClutterVideoTexture *video_texture)
{
  g_return_val_if_fail (video_texture != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (video_texture->priv->play), FALSE);

  if (!video_texture->priv->mrl)
    return FALSE;

  /* (instant seeking only make sense with video, hence no cdda:// here) */
  if (g_str_has_prefix (video_texture->priv->mrl, "file://") ||
      g_str_has_prefix (video_texture->priv->mrl, "dvd://") ||
      g_str_has_prefix (video_texture->priv->mrl, "vcd://"))
    return TRUE;

  return FALSE;
}

gboolean
clutter_video_texture_seek_time (ClutterVideoTexture *video_texture, 
				 gint64               time, 
				 GError             **gerror)
{
  g_return_val_if_fail (video_texture != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (video_texture->priv->play), FALSE);

  got_time_tick (video_texture->priv->play, time * GST_MSECOND, video_texture);
  
  gst_element_seek (video_texture->priv->play, 
		    1.0,
		    GST_FORMAT_TIME, 
		    GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
		    GST_SEEK_TYPE_SET, 
		    time * GST_MSECOND,
		    GST_SEEK_TYPE_NONE, 
		    GST_CLOCK_TIME_NONE);

  gst_element_get_state (video_texture->priv->play, 
			 NULL, NULL, 100 * GST_MSECOND);
  return TRUE;
}

gboolean
clutter_video_texture_seek (ClutterVideoTexture *video_texture, 
			    float                position, 
			    GError             **error)
{
  gint64 seek_time, length_nanos;

  g_return_val_if_fail (video_texture != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (video_texture->priv->play), FALSE);

  length_nanos = (gint64) (video_texture->priv->stream_length * GST_MSECOND);
  seek_time    = (gint64) (length_nanos * position);

  return clutter_video_texture_seek_time (video_texture, 
					  seek_time / GST_MSECOND, error);
}

static void
stop_play_pipeline (ClutterVideoTexture *video_texture)
{
  GstElement *playbin = video_texture->priv->play;
  GstState    current_state;

  /* first go to ready, that way our state change handler gets to see
   * our state change messages (before the bus goes to flushing) and
   * cleans up */
  gst_element_get_state (playbin, &current_state, NULL, 0);

  if (current_state > GST_STATE_READY) 
    {
      GError *err = NULL;

      gst_element_set_state (playbin, GST_STATE_READY);
      poll_for_state_change_full (video_texture, 
				  playbin, 
				  GST_STATE_READY, &err, -1);
      if (err)
	g_error_free (err);
    }

  /* now finally go to null state */
  gst_element_set_state (playbin, GST_STATE_NULL);
  gst_element_get_state (playbin, NULL, NULL, -1);
}

void
clutter_video_texture_stop (ClutterVideoTexture *video_texture)
{
  g_return_if_fail (video_texture != NULL);
  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture));
  g_return_if_fail (GST_IS_ELEMENT (video_texture->priv->play));

  stop_play_pipeline (video_texture);
  
  /* Reset position to 0 when stopping */
  got_time_tick (GST_ELEMENT (video_texture->priv->play), 0, video_texture);
}

gboolean
clutter_video_texture_can_set_volume (ClutterVideoTexture *video_texture)
{
  g_return_val_if_fail (video_texture != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (video_texture->priv->play), FALSE);
  
  return TRUE;
}

void
clutter_video_texture_set_volume (ClutterVideoTexture *video_texture, 
				  int                  volume)
{
  g_return_if_fail (video_texture != NULL);
  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture));
  g_return_if_fail (GST_IS_ELEMENT (video_texture->priv->play));

  if (clutter_video_texture_can_set_volume (video_texture) != FALSE)
  {
    volume = CLAMP (volume, 0, 100);
    g_object_set (video_texture->priv->play, "volume",
		  (gdouble) (1. * volume / 100), NULL);
  }
}

int
clutter_video_texture_get_volume (ClutterVideoTexture *video_texture)
{
  gdouble vol;

  g_return_val_if_fail (video_texture != NULL, -1);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), -1);
  g_return_val_if_fail (GST_IS_ELEMENT (video_texture->priv->play), -1);

  g_object_get (G_OBJECT (video_texture->priv->play), "volume", &vol, NULL);

  return (gint) (vol * 100 + 0.5);
}

gint64
clutter_video_texture_get_current_time (ClutterVideoTexture *video_texture)
{
  g_return_val_if_fail (video_texture != NULL, -1);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), -1);

  return video_texture->priv->current_time;
}

gint64
clutter_video_texture_get_stream_length (ClutterVideoTexture *video_texture)
{
  ClutterVideoTexturePrivate *priv;

  g_return_val_if_fail (video_texture != NULL, -1);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), -1);

  priv = video_texture->priv;

  if (priv->stream_length == 0  && priv->play != NULL) 
    {
      GstFormat fmt = GST_FORMAT_TIME;
      gint64    len = -1;

      if (gst_element_query_duration (priv->play, &fmt, &len) && len != -1)
	priv->stream_length = len / GST_MSECOND;
    }

  return priv->stream_length;
}

gboolean
clutter_video_texture_is_playing (ClutterVideoTexture *video_texture)
{
  GstState cur, pending;

  g_return_val_if_fail (video_texture != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (video_texture->priv->play), FALSE);

  gst_element_get_state (video_texture->priv->play, &cur, &pending, 0);

  if (cur == GST_STATE_PLAYING || pending == GST_STATE_PLAYING)
    return TRUE;

  return FALSE;
}

gboolean
clutter_video_texture_is_seekable (ClutterVideoTexture *video_texture)
{
  gboolean res;

  g_return_val_if_fail (video_texture != NULL, FALSE);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (video_texture->priv->play), FALSE);

  if (video_texture->priv->stream_length == 0) 
    res = (clutter_video_texture_get_stream_length (video_texture) > 0);
  else 
    res = (video_texture->priv->stream_length > 0);

  return res;
}

float
clutter_video_texture_get_position (ClutterVideoTexture  *video_texture)
{
  g_return_val_if_fail (video_texture != NULL, -1);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), -1);

  return video_texture->priv->current_position;
}

void
clutter_video_texture_set_aspect_ratio (ClutterVideoTexture  *video_texture,
					ClutterVideoTextureAspectRatio ratio)
{
  g_return_if_fail (video_texture != NULL);
  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture));

  video_texture->priv->ratio_type = ratio;
  got_video_size (video_texture);
}

ClutterVideoTextureAspectRatio
clutter_video_texture_get_aspect_ratio (ClutterVideoTexture  *video_texture)
{
  g_return_val_if_fail (video_texture != NULL, 0);
  g_return_val_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture), 0);

  return video_texture->priv->ratio_type;
}

/* Metadata */

static const struct _metadata_map_info 
{
  ClutterVideoTextureMetadataType type;
  const gchar *str;
} metadata_str_map[] = {
  { CLUTTER_INFO_TITLE,        "title" },
  { CLUTTER_INFO_ARTIST,       "artist" },
  { CLUTTER_INFO_YEAR,         "year" },
  { CLUTTER_INFO_ALBUM,        "album" },
  { CLUTTER_INFO_DURATION,     "duration" },
  { CLUTTER_INFO_TRACK_NUMBER, "track-number" },
  { CLUTTER_INFO_HAS_VIDEO,    "has-video" },
  { CLUTTER_INFO_DIMENSION_X,  "dimension-x" },
  { CLUTTER_INFO_DIMENSION_Y,  "dimension-y" },
  { CLUTTER_INFO_VIDEO_BITRATE,"video-bitrate" },
  { CLUTTER_INFO_VIDEO_CODEC,  "video-codec" },
  { CLUTTER_INFO_FPS,          "fps" },
  { CLUTTER_INFO_HAS_AUDIO,    "has-audio" },
  { CLUTTER_INFO_AUDIO_BITRATE,"audio-bitrate" },
  { CLUTTER_INFO_AUDIO_CODEC,  "audio-codec" }
};

static const gchar*
get_metadata_type_name (ClutterVideoTextureMetadataType type)
{
  guint i;
  for (i = 0; i < G_N_ELEMENTS (metadata_str_map); ++i) 
    {
      if (metadata_str_map[i].type == type)
	return metadata_str_map[i].str;
    }
  return "unknown";
}

static void
get_metadata_string (ClutterVideoTexture            *video_texture,
		     ClutterVideoTextureMetadataType type,
		     GValue                         *value)
{
  ClutterVideoTexturePrivate *priv;
  char                       *string = NULL;
  gboolean                    res = FALSE;

  priv = video_texture->priv;

  g_value_init (value, G_TYPE_STRING);

  if (priv->play == NULL || priv->tagcache == NULL)
    {
      g_value_set_string (value, NULL);
      return;
    }

  switch (type)
    {
    case CLUTTER_INFO_TITLE:
      res = gst_tag_list_get_string_index (priv->tagcache,
					   GST_TAG_TITLE, 0, &string);
      break;
    case CLUTTER_INFO_ARTIST:
      res = gst_tag_list_get_string_index (priv->tagcache,
					   GST_TAG_ARTIST, 0, &string);
      break;
    case CLUTTER_INFO_YEAR: 
      {
	GDate *date;
	if ((res = gst_tag_list_get_date (priv->tagcache,
					  GST_TAG_DATE, &date))) 
	  {
	    string = g_strdup_printf ("%d", g_date_get_year (date));
	    g_date_free (date);
	  }
	break;
      }
    case CLUTTER_INFO_ALBUM:
      res = gst_tag_list_get_string_index (priv->tagcache,
					   GST_TAG_ALBUM, 0, &string);
      break;
    case CLUTTER_INFO_VIDEO_CODEC:
      res = gst_tag_list_get_string (priv->tagcache,
				     GST_TAG_VIDEO_CODEC, &string);
      break;
    case CLUTTER_INFO_AUDIO_CODEC:
      res = gst_tag_list_get_string (priv->tagcache,
				     GST_TAG_AUDIO_CODEC, &string);
      break;
    default:
      g_assert_not_reached ();
    }

  if (res) 
    {
      g_value_take_string (value, string);
      CLUTTER_DBG ("%s = '%s'", get_metadata_type_name (type), string);
    } 
  else
    g_value_set_string (value, NULL);

  return;
}

static void
get_metadata_int (ClutterVideoTexture            *video_texture,
		  ClutterVideoTextureMetadataType type,
		  GValue                         *value)
{
  ClutterVideoTexturePrivate *priv;
  int                         integer = 0;

  priv = video_texture->priv;

  g_value_init (value, G_TYPE_INT);

  if (priv->play == NULL)
    {
      g_value_set_int (value, 0);
      return;
    }

  switch (type)
    {
    case CLUTTER_INFO_DURATION:
      integer = clutter_video_texture_get_stream_length (video_texture) / 1000;
      break;
    case CLUTTER_INFO_TRACK_NUMBER:
      if (!gst_tag_list_get_uint (priv->tagcache,
				  GST_TAG_TRACK_NUMBER, (guint *) &integer))
        integer = 0;
      break;
    case CLUTTER_INFO_DIMENSION_X:
      integer = priv->video_width;
      break;
    case CLUTTER_INFO_DIMENSION_Y:
      integer = priv->video_height;
      break;
    case CLUTTER_INFO_FPS:
      if (priv->video_fps_d > 0) 
	{
	  /* Round up/down to the nearest integer framerate */
	  integer = (priv->video_fps_n + priv->video_fps_d/2) /
	    priv->video_fps_d;
	}
      else
        integer = 0;
      break;
    case CLUTTER_INFO_AUDIO_BITRATE:
      if (priv->audiotags == NULL)
        break;
      if (gst_tag_list_get_uint (priv->audiotags, GST_TAG_BITRATE,
          (guint *)&integer) ||
          gst_tag_list_get_uint (priv->audiotags, GST_TAG_NOMINAL_BITRATE,
          (guint *)&integer)) {
        integer /= 1000;
      }
      break;
    case CLUTTER_INFO_VIDEO_BITRATE:
      if (priv->videotags == NULL)
        break;
      if (gst_tag_list_get_uint (priv->videotags, GST_TAG_BITRATE,
				 (guint *)&integer) ||
          gst_tag_list_get_uint (priv->videotags, GST_TAG_NOMINAL_BITRATE,
				 (guint *)&integer)) {
        integer /= 1000;
      }
      break;
    default:
      g_assert_not_reached ();
    }

  g_value_set_int (value, integer);
  CLUTTER_DBG ("%s = %d", get_metadata_type_name (type), integer);

  return;
}

static void
get_metadata_bool (ClutterVideoTexture            *video_texture,
		   ClutterVideoTextureMetadataType type,
		   GValue                         *value)
{
  ClutterVideoTexturePrivate *priv;
  gboolean                    boolean = FALSE;

  priv = video_texture->priv;

  g_value_init (value, G_TYPE_BOOLEAN);

  if (priv->play == NULL) 
    {
      g_value_set_boolean (value, FALSE);
      return;
    }

  switch (type)
    {
    case CLUTTER_INFO_HAS_VIDEO:
      boolean = priv->has_video;
      /* if properties dialog, show the metadata we
       * have even if we cannot decode the stream */
      if (!boolean
	  && priv->tagcache != NULL 
	  && gst_structure_has_field ((GstStructure *) priv->tagcache,
				      GST_TAG_VIDEO_CODEC)) 
        boolean = TRUE;
      break;
    case CLUTTER_INFO_HAS_AUDIO:
      boolean = priv->has_audio;
      /* if properties dialog, show the metadata we
       * have even if we cannot decode the stream */
      if (!boolean
	  && priv->tagcache != NULL 
	  && gst_structure_has_field ((GstStructure *) priv->tagcache,
				      GST_TAG_AUDIO_CODEC)) 
        boolean = TRUE;
      break;
    default:
      g_assert_not_reached ();
  }

  g_value_set_boolean (value, boolean);
  CLUTTER_DBG ("%s = %s", get_metadata_type_name (type), 
	                                 (boolean) ? "yes" : "no");
  return;
}

void
clutter_video_texture_get_metadata (ClutterVideoTexture *video_texture,
				    ClutterVideoTextureMetadataType type,
				    GValue                         *value)
{
  g_return_if_fail (video_texture != NULL);
  g_return_if_fail (CLUTTER_IS_VIDEO_TEXTURE (video_texture));
  g_return_if_fail (GST_IS_ELEMENT (video_texture->priv->play));

  switch (type)
    {
    case CLUTTER_INFO_TITLE:
    case CLUTTER_INFO_ARTIST:
    case CLUTTER_INFO_YEAR:
    case CLUTTER_INFO_ALBUM:
    case CLUTTER_INFO_VIDEO_CODEC:
    case CLUTTER_INFO_AUDIO_CODEC:
      get_metadata_string (video_texture, type, value);
      break;
    case CLUTTER_INFO_DURATION:
    case CLUTTER_INFO_DIMENSION_X:
    case CLUTTER_INFO_DIMENSION_Y:
    case CLUTTER_INFO_FPS:
    case CLUTTER_INFO_AUDIO_BITRATE:
    case CLUTTER_INFO_VIDEO_BITRATE:
    case CLUTTER_INFO_TRACK_NUMBER:
      get_metadata_int (video_texture, type, value);
      break;
    case CLUTTER_INFO_HAS_VIDEO:
    case CLUTTER_INFO_HAS_AUDIO:
      get_metadata_bool (video_texture, type, value);
      break;
    default:
      g_return_if_reached ();
    }

  return;
}
