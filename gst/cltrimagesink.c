/* GStreamer
 * Copyright (C) <2003> Julien Moutte <julien@moutte.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DBG(x, a...) \
 g_printerr ( __FILE__ ":%d,%s() " x "\n", __LINE__, __func__, ##a)

/* Object header */
#include "cltrimagesink.h"

/* Debugging category */
#include <gst/gstinfo.h>

GST_DEBUG_CATEGORY_STATIC (gst_debug_cltrimagesink); 

#define GST_CAT_DEFAULT gst_debug_cltrimagesink

/* ElementFactory information */
static GstElementDetails gst_cltrimagesink_details =
GST_ELEMENT_DETAILS ("Video sink",
		     "Sink/Video",
		     "An Clutter based videosink",
		     "Matthew Allum <mallum@o-hand.com>");

/* Default template - initiated with class struct to allow gst-register to work
   without X running */
static GstStaticPadTemplate gst_cltrimagesink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
			 GST_PAD_SINK,
			 GST_PAD_ALWAYS,
			 GST_STATIC_CAPS ("video/x-raw-rgb, "
					  "framerate = (double) [ 1.0, 100.0 ], "
					  "width = (int) [ 1, MAX ], "
					  "height = (int) [ 1, MAX ]; "
					  "video/x-raw-yuv, "
					  "framerate = (double) [ 1.0, 100.0 ], "
					  "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
			 );

/* CltrimageSink signals and args */
enum
{
    SIGNAL_HANDOFF,
    SIGNAL_BUFALLOC,
    LAST_SIGNAL
    /* FILL ME */
 };

static guint gst_cltrimagesink_signals[LAST_SIGNAL] = { 0 };

enum
{
  ARG_0,
  ARG_QUEUE,
  ARG_SYNCHRONOUS,
  ARG_SIGNAL_HANDOFFS
      /* FILL ME */
};

static GstVideoSinkClass *parent_class = NULL;

#define GLERR()                                           \
 {                                                             \
  GLenum err = glGetError (); 	/* Roundtrip */                \
  if (err != GL_NO_ERROR)                                      \
    {                                                          \
      g_printerr (__FILE__ ": GL Error: %x [at %s:%d]\n",      \
		  err, __func__, __LINE__);                    \
    }                                                          \
 }


/* ============================================================= */
/*                                                               */
/*                       Private Methods                         */
/*                                                               */
/* ============================================================= */



/* 
=================
Element stuff 
=================
*/

#define SWAP_4(x) ( ((x) << 24) | \
         (((x) << 16) & 0x00ff0000) | \
         (((x) << 8) & 0x0000ff00) | \
         0x000000ff )





static GstCaps *
gst_cltrimagesink_fixate (GstPad        *pad, 
			  const GstCaps *caps)
{
  GstStructure *structure;
  GstCaps *newcaps;

  if (gst_caps_get_size (caps) > 1)
    return NULL;

  newcaps = gst_caps_copy (caps);
  structure = gst_caps_get_structure (newcaps, 0);

  if (gst_caps_structure_fixate_field_nearest_int (structure, "width", 320)) 
    {
      return newcaps;
    }

  if (gst_caps_structure_fixate_field_nearest_int (structure, "height", 240)) 
    {
      return newcaps;
    }

  if (gst_caps_structure_fixate_field_nearest_double (structure, 
						      "framerate",
						      30.0)) 
    {
      return newcaps;
    }

  gst_caps_free (newcaps);
  return NULL;
}

static GstCaps *
gst_cltrimagesink_getcaps (GstPad * pad)
{
  GstCltrimageSink *cltrimagesink;

  cltrimagesink = GST_CLTRIMAGESINK (gst_pad_get_parent (pad));

  if (!cltrimagesink->caps)
    cltrimagesink->caps 
      = gst_caps_new_simple (
			     "video/x-raw-rgb",
			     "bpp",        G_TYPE_INT, 24,
			     "depth",      G_TYPE_INT, 24,
			     "endianness", G_TYPE_INT, G_BIG_ENDIAN,
			     /*
			     "red_mask",   G_TYPE_INT, 0xff0000,
			     "green_mask", G_TYPE_INT, 0x00ff00,
			     "blue_mask",  G_TYPE_INT, 0x0000ff,
			     */
			     "width",      GST_TYPE_INT_RANGE, 1, G_MAXINT,
			     "height",     GST_TYPE_INT_RANGE, 1, G_MAXINT,
			     "framerate",  GST_TYPE_DOUBLE_RANGE, 1.0, 100.0, NULL);

  return gst_caps_copy (cltrimagesink->caps);

}

static GstPadLinkReturn
gst_cltrimagesink_sink_link (GstPad * pad, const GstCaps * caps)
{
  GstCltrimageSink *cltrimagesink;
  gboolean ret;
  GstStructure *structure;
  Pixbuf *pixb = NULL;

  cltrimagesink = GST_CLTRIMAGESINK (gst_pad_get_parent (pad));

  /*
  if (!cltrimagesink->texture)
    return GST_PAD_LINK_DELAYED;
  */

  structure = gst_caps_get_structure (caps, 0);

  ret = gst_structure_get_int (structure, "width",
			       &(GST_VIDEOSINK_WIDTH (cltrimagesink)));

  ret &= gst_structure_get_int (structure, "height",
				&(GST_VIDEOSINK_HEIGHT (cltrimagesink)));

  ret &= gst_structure_get_double (structure,
				   "framerate", &cltrimagesink->framerate);
  if (!ret)
    {
      DBG("!ret returning GST_PAD_LINK_REFUSED");
      return GST_PAD_LINK_REFUSED;
    }

  cltrimagesink->pixel_width = 1;

  gst_structure_get_int (structure, "pixel_width", 
			 &cltrimagesink->pixel_width);

  cltrimagesink->pixel_height = 1;

  gst_structure_get_int (structure, "pixel_height", 
			 &cltrimagesink->pixel_height);

  DBG("returning GST_PAD_LINK_OK, with %ix%i or %ix%i", 
      cltrimagesink->pixel_width,
      cltrimagesink->pixel_height,
      GST_VIDEOSINK_WIDTH (cltrimagesink),
      GST_VIDEOSINK_HEIGHT (cltrimagesink));

  pixb = pixbuf_new(GST_VIDEOSINK_WIDTH (cltrimagesink), 
		    GST_VIDEOSINK_HEIGHT (cltrimagesink));

  DBG("pixbuf new at %ix%i", GST_VIDEOSINK_WIDTH (cltrimagesink), 
      GST_VIDEOSINK_HEIGHT (cltrimagesink));

  /* Is this the right place ? */
  cltrimagesink->texture = cltr_texture_no_tile_new(pixb);

  pixbuf_unref(pixb);

  return GST_PAD_LINK_OK;
}

static GstElementStateReturn
gst_cltrimagesink_change_state (GstElement * element)
{
  GstCltrimageSink *cltrimagesink;

  DBG("mark");

  cltrimagesink = GST_CLTRIMAGESINK (element);

  switch (GST_STATE_TRANSITION (element)) 
    {
    case GST_STATE_NULL_TO_READY:
      /* Initializing the Context */
      /*
      if (!cltrimagesink->texture) 
	{
	  DBG("setting state to failure");
	  return GST_STATE_FAILURE;
	}
      */
      break;
    case GST_STATE_READY_TO_PAUSED:
      cltrimagesink->time = 0;
      break;
    case GST_STATE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_PAUSED_TO_READY:
      cltrimagesink->framerate = 0;
      GST_VIDEOSINK_WIDTH (cltrimagesink) = 0;
      GST_VIDEOSINK_HEIGHT (cltrimagesink) = 0;
      break;
    case GST_STATE_READY_TO_NULL:
      if (cltrimagesink->texture)
	cltr_texture_unref(cltrimagesink->texture);

      break;
    }

  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);

  return GST_STATE_SUCCESS;
}

static void
gst_cltrimagesink_chain (GstPad * pad, GstData * data)
{
  GstBuffer *buf = GST_BUFFER (data);
  GstCltrimageSink *cltrimagesink;

  g_return_if_fail (GST_IS_PAD (pad));
  g_return_if_fail (buf != NULL);

  cltrimagesink = GST_CLTRIMAGESINK (gst_pad_get_parent (pad));
 
 if (GST_IS_EVENT (data)) 
   {
     gst_pad_event_default (pad, GST_EVENT (data));
     DBG("GST_IS_EVENT, returning");
     return;
   }

 buf = GST_BUFFER (data);
 
 /* update time */
 if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) 
   cltrimagesink->time = GST_BUFFER_TIMESTAMP (buf);

 /* If this buffer has been allocated using our buffer management we 
  * simply put the ximage which is in the PRIVATE pointer */

#if 0
 if (GST_BUFFER_FREE_DATA_FUNC (buf) == gst_cltrimagesink_buffer_free)
   {
	/*
	  gst_cltrimagesink_ximage_put (cltrimagesink, GST_BUFFER_PRIVATE (buf));
	*/
   }
 else
#endif 
   {   /* Else we have to copy the data into our private image, */
     /* if we have one... */


     if (cltrimagesink->texture) 
       {
	 /* need to copy the data into out pixbuf here */
	 Pixbuf *pixb = NULL;

	 cltr_texture_lock(cltrimagesink->texture);

	 pixb = cltr_texture_get_pixbuf(cltrimagesink->texture);

	 if (pixb)
	   {
	     int      i = 0;
	     guint8 *d = GST_BUFFER_DATA (buf);
	     for (i = 0; i < pixb->height * pixb->width; i++)
	       {
		 int r,g,b, a;
		 r = *d++;  g = *d++; b = *d++; a = 0xff;
		 pixb->data[i] = ((r << 24) | 
				  (g << 16) | 
				  (b << 8) |
				  a );
	       }

	     /*
	     memcpy (pixb->data,
		     GST_BUFFER_DATA (buf),
		     MIN (GST_BUFFER_SIZE (buf), 
			  pixb->bytes_per_line * pixb->width));
	     */

	     /* Below faster but threading issues causing DRI to bomb out */

	     /*
	     if (GST_BUFFER_SIZE (buf) >= pixb->width * pixb->height * 3)
	       cltr_texture_force_rgb_data(cltrimagesink->texture,
					   pixb->width,
					   pixb->height,
					   GST_BUFFER_DATA (buf));
	     */
	   }

	 cltr_texture_unlock(cltrimagesink->texture);

	 if (cltrimagesink->queue)
	   {
	     CltrVideoSignal *signal;

	     signal = g_new0 (CltrVideoSignal, 1);
	     signal->signal_id = CLTR_VIDEO_ASYNC_TEXTURE;
	     signal->signal_data.texture.ref = cltrimagesink->texture;
	     
	     g_async_queue_push(cltrimagesink->queue, 
				(gpointer)signal);
	   }
       } 
     else 
       {                  
	    /* No image available. Something went wrong during capsnego ! */

	 gst_buffer_unref (buf);
	 GST_ELEMENT_ERROR (cltrimagesink, CORE, NEGOTIATION, (NULL),
			    ("no format defined before chain function"));
	 return;
       }
   }
  
  /* swap buffer here ? */

  GST_DEBUG ("clock wait: %" GST_TIME_FORMAT, 
	     GST_TIME_ARGS (cltrimagesink->time));

  /* ah, BTW, I think the gst_element_wait should happen _before_ 
     the ximage is shown */

  if (GST_VIDEOSINK_CLOCK (cltrimagesink))
    gst_element_wait (GST_ELEMENT (cltrimagesink), cltrimagesink->time);

  /* set correct time for next buffer */
  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buf) && cltrimagesink->framerate > 0)
    cltrimagesink->time += GST_SECOND / cltrimagesink->framerate;

  gst_buffer_unref (buf);

  /*
  if (!cltrimagesink->signal_handoffs)
    gst_cltrimagesink_handle_xevents (cltrimagesink, pad);
  */
}


/* =========================================== */
/*                                             */
/*              Init & Class init              */
/*                                             */
/* =========================================== */

static void
gst_cltrimagesink_set_property (GObject      *object, 
				guint         prop_id,
				const GValue *value, 
				GParamSpec   *pspec)
{
  GstCltrimageSink *cltrimagesink;

  g_return_if_fail (GST_IS_CLTRIMAGESINK (object));

  cltrimagesink = GST_CLTRIMAGESINK (object);

  switch (prop_id) 
    {
    case ARG_QUEUE:
      cltrimagesink->queue = g_value_get_pointer (value);
      break;
      /*
    case ARG_SIGNAL_HANDOFFS:
      cltrimagesink->signal_handoffs = g_value_get_boolean (value);
      */
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cltrimagesink_get_property (GObject    *object, 
				guint       prop_id,
				GValue     *value, 
				GParamSpec *pspec)
{
  GstCltrimageSink *cltrimagesink;

  g_return_if_fail (GST_IS_CLTRIMAGESINK (object));

  cltrimagesink = GST_CLTRIMAGESINK (object);

  switch (prop_id) 
    {
    case ARG_QUEUE:
      g_value_set_pointer (value, cltrimagesink->queue);
      break;
      /*
    case ARG_SIGNAL_HANDOFFS:
      g_value_set_boolean (value, cltrimagesink->signal_handoffs);
      break;
      */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}

static void
gst_cltrimagesink_finalize (GObject *object)
{
  GstCltrimageSink *cltrimagesink;

  cltrimagesink = GST_CLTRIMAGESINK (object);

  /*
  if (cltrimagesink->display_name) 
    {
      g_free (cltrimagesink->display_name);
      cltrimagesink->display_name = NULL;
    }
  */

  g_mutex_free (cltrimagesink->x_lock);
  g_mutex_free (cltrimagesink->pool_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_cltrimagesink_init (GstCltrimageSink * cltrimagesink)
{
  GST_VIDEOSINK_PAD (cltrimagesink) 
    = gst_pad_new_from_template ( gst_static_pad_template_get(&gst_cltrimagesink_sink_template_factory), "sink");

  gst_element_add_pad (GST_ELEMENT (cltrimagesink),
		       GST_VIDEOSINK_PAD (cltrimagesink));
  
  gst_pad_set_chain_function (GST_VIDEOSINK_PAD (cltrimagesink),
			      gst_cltrimagesink_chain);

  gst_pad_set_link_function (GST_VIDEOSINK_PAD (cltrimagesink),
			     gst_cltrimagesink_sink_link);

  gst_pad_set_getcaps_function (GST_VIDEOSINK_PAD (cltrimagesink),
				gst_cltrimagesink_getcaps);

  gst_pad_set_fixate_function (GST_VIDEOSINK_PAD (cltrimagesink),
			       gst_cltrimagesink_fixate);

  /*
  gst_pad_set_bufferalloc_function (GST_VIDEOSINK_PAD (cltrimagesink),
				    gst_cltrimagesink_buffer_alloc);
  */

  cltrimagesink->framerate    = 0;

  cltrimagesink->x_lock       = g_mutex_new ();

  cltrimagesink->pixel_width  = cltrimagesink->pixel_height = 1;

  cltrimagesink->image_pool   = NULL;
  cltrimagesink->pool_lock    = g_mutex_new ();

  cltrimagesink->texture = NULL;

  GST_FLAG_SET (cltrimagesink, GST_ELEMENT_THREAD_SUGGESTED);
  GST_FLAG_SET (cltrimagesink, GST_ELEMENT_EVENT_AWARE);
}

static void
gst_cltrimagesink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &gst_cltrimagesink_details);

  gst_element_class_add_pad_template (element_class,
				      gst_static_pad_template_get (&gst_cltrimagesink_sink_template_factory));
}

static void
gst_cltrimagesink_class_init (GstCltrimageSinkClass * klass)
{
  GObjectClass    *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class    = (GObjectClass*) klass;
  gstelement_class = (GstElementClass*) klass;

  parent_class = g_type_class_ref (GST_TYPE_VIDEOSINK);

  /* TOGO */
  g_object_class_install_property (gobject_class, 
				   ARG_QUEUE,
				   g_param_spec_pointer ("queue", 
							"Queue", 
							"Async Signal Queue",
							 G_PARAM_READWRITE));

  /* TOGO */
  g_object_class_install_property (gobject_class, 
				   ARG_SYNCHRONOUS,
				   g_param_spec_boolean ("synchronous", 
							 "Synchronous", 
							 "When enabled, runs "
							 "the X display in synchronous mode. (used only for debugging)", 
							 FALSE,
							 G_PARAM_READWRITE));
  /* TOGO */
  g_object_class_install_property (gobject_class, 
				   ARG_SIGNAL_HANDOFFS,
				   g_param_spec_boolean ("signal-handoffs", 
							 "Signal handoffs",
							 "Send a signal before unreffing the buffer, forces YUV, no GL output",
							 FALSE, 
							 G_PARAM_READWRITE));

  gst_cltrimagesink_signals[SIGNAL_HANDOFF] 
    = g_signal_new ("handoff", 
		    G_TYPE_FROM_CLASS (klass), 
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GstCltrimageSinkClass, handoff), 
		    NULL, 
		    NULL,
		    gst_marshal_VOID__POINTER_OBJECT, 
		    G_TYPE_NONE, 
		    2,
		    GST_TYPE_BUFFER, 
		    GST_TYPE_PAD);

  gst_cltrimagesink_signals[SIGNAL_BUFALLOC] =
      g_signal_new ("bufferalloc", 
		    G_TYPE_FROM_CLASS (klass), 
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GstCltrimageSinkClass, bufferalloc), 
		    NULL, 
		    NULL,
		    gst_marshal_VOID__POINTER_OBJECT, 
		    G_TYPE_NONE, 2,
		    GST_TYPE_BUFFER, GST_TYPE_PAD);

  gobject_class->finalize     = gst_cltrimagesink_finalize;
  gobject_class->set_property = gst_cltrimagesink_set_property;
  gobject_class->get_property = gst_cltrimagesink_get_property;

  gstelement_class->change_state = gst_cltrimagesink_change_state;
}

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

/* =========================================== */
/*                                             */
/*          Object typing & Creation           */
/*                                             */
/* =========================================== */

GType
gst_cltrimagesink_get_type (void)
{
  static GType cltrimagesink_type = 0;

  if (!cltrimagesink_type) 
    {
      static const GTypeInfo cltrimagesink_info = 
	{
	  sizeof (GstCltrimageSinkClass),
	  gst_cltrimagesink_base_init,
	  NULL,
	  (GClassInitFunc) gst_cltrimagesink_class_init,
	  NULL,
	  NULL,
	  sizeof (GstCltrimageSink),
	  0,
	  (GInstanceInitFunc) gst_cltrimagesink_init,
	};


    cltrimagesink_type 
      = g_type_register_static (GST_TYPE_VIDEOSINK,
				"GstCltrimageSink", 
				&cltrimagesink_info, 
				0);
    }
  
  return cltrimagesink_type;
}

static gboolean
plugin_init (GstPlugin *plugin)
{
  /* Loading the library containing GstVideoSink, our parent object */
  if (!gst_library_load ("gstvideo"))
    return FALSE;

  if (!gst_element_register (plugin, 
			     "cltrimagesink",
			     GST_RANK_SECONDARY, 
			     GST_TYPE_CLTRIMAGESINK))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_debug_cltrimagesink, 
			   "cltrimagesink", 
			   0,
			   "cltrimagesink element");
  return TRUE;
}

#define GST_LICENSE "LGPL"
#define GST_PACKAGE "GStreamer"
#define GST_ORIGIN  "http://o-hand.com"

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		   GST_VERSION_MINOR,
		   "cltrimagesink",
		   "Clutter video output plugin based on OpenGL 1.2 calls",
		   plugin_init, 
		   VERSION, 
		   GST_LICENSE, 
		   GST_PACKAGE, 
		   GST_ORIGIN)
