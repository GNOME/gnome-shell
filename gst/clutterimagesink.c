/*
 * Clutter GST VideoSink 
 *
 * Heavily based on code XImageSink with following copyright;
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
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

/* Our interfaces */
#include <gst/interfaces/navigation.h>

/* Object header */
#include "clutterimagesink.h"

/* Debugging category */
#include <gst/gstinfo.h>

/* Clutter */
#include <clutter/clutter.h>

GST_DEBUG_CATEGORY_EXTERN (gst_debug_clutterimagesink);
#define GST_CAT_DEFAULT gst_debug_clutterimagesink

#define DBG(x, a...) \
 g_printerr ( __FILE__ ":%d,%s() " x "\n", __LINE__, __func__, ##a)

static void 
gst_clutterimagesink_clutterimage_destroy (GstClutterImageSink *clutterimagesink,
					   GstClutterImageBuffer *clutterimage);

/* ElementFactory information */
static GstElementDetails gst_clutterimagesink_details =
GST_ELEMENT_DETAILS ("Video sink",
		     "Sink/Video",
		     "Clutter videosink",
		     "Matthew Allum <mallum@o-hand.com>");

static GstStaticPadTemplate gst_clutterimagesink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
	    GST_PAD_SINK,
	    GST_PAD_ALWAYS,
	    GST_STATIC_CAPS ("video/x-raw-rgb, "
			     "framerate = (fraction) [ 0, MAX ], "
			     "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
			 );
enum
{
  PROP_0,
  PROP_VIDEO_TEXTURE,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_FORCE_ASPECT_RATIO
};

static GstVideoSinkClass *parent_class = NULL;


#define GST_TYPE_CLUTTERIMAGE_BUFFER (gst_clutterimage_buffer_get_type())

#define GST_IS_CLUTTERIMAGE_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLUTTERIMAGE_BUFFER))
#define GST_CLUTTERIMAGE_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLUTTERIMAGE_BUFFER, GstClutterImageBuffer))
#define GST_CLUTTERIMAGE_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLUTTERIMAGE_BUFFER, GstClutterImageBufferClass))

static void
gst_clutterimage_buffer_finalize (GstClutterImageBuffer *clutterimage)
{
  GstClutterImageSink *clutterimagesink = NULL;
  gboolean                     recycled = FALSE;

  g_return_if_fail (clutterimage != NULL);

  clutterimagesink = clutterimage->clutterimagesink;

  if (!clutterimagesink) 
    {
      GST_WARNING_OBJECT (clutterimagesink, "no sink found");
      goto beach;
    }

  /* If our geometry changed we can't reuse that image. */
  if ((clutterimage->width != GST_VIDEO_SINK_WIDTH (clutterimagesink)) 
      || (clutterimage->height != GST_VIDEO_SINK_HEIGHT (clutterimagesink))) 
    {
      gst_clutterimagesink_clutterimage_destroy (clutterimagesink, 
						 clutterimage);
    } 
  else 
    {
      /* In that case we can reuse the image and add it to our image pool. */
      GST_LOG_OBJECT (clutterimagesink, "recycling image %p in pool", 
		      clutterimage);

      /* need to increment the refcount again to recycle */
      gst_buffer_ref (GST_BUFFER (clutterimage));
      g_mutex_lock (clutterimagesink->pool_lock);
      clutterimagesink->buffer_pool 
	= g_slist_prepend (clutterimagesink->buffer_pool, clutterimage);
      g_mutex_unlock (clutterimagesink->pool_lock);
      recycled = TRUE;
    }
  
 beach:
  return;
}

static void
gst_clutterimage_buffer_free (GstClutterImageBuffer *clutterimage)
{
  /* make sure it is not recycled */
  clutterimage->width  = -1;
  clutterimage->height = -1;
  gst_buffer_unref (GST_BUFFER (clutterimage));
}

static void
gst_clutterimage_buffer_init (GstClutterImageBuffer *clutterimage_buffer, 
			      gpointer               g_class)
{
  ;
}

static void
gst_clutterimage_buffer_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      gst_clutterimage_buffer_finalize;
}

GType
gst_clutterimage_buffer_get_type (void)
{
  static GType _gst_clutterimage_buffer_type;

  if (G_UNLIKELY (_gst_clutterimage_buffer_type == 0)) 
    {
      static const GTypeInfo clutterimage_buffer_info = 
	{
	  sizeof (GstBufferClass),
	  NULL,
	  NULL,
	  gst_clutterimage_buffer_class_init,
	  NULL,
	  NULL,
	  sizeof (GstClutterImageBuffer),
	  0,
	  (GInstanceInitFunc) gst_clutterimage_buffer_init,
	  NULL
	};
      
      _gst_clutterimage_buffer_type 
	= g_type_register_static (GST_TYPE_BUFFER,
				  "GstClutterImageBuffer", 
				  &clutterimage_buffer_info, 0);
    }
  return _gst_clutterimage_buffer_type;
}

static GstClutterImageBuffer *
gst_clutterimagesink_clutterimage_new (GstClutterImageSink *clutterimagesink, 
				       GstCaps             *caps)
{
  GstClutterImageBuffer *clutterimage = NULL;
  GstStructure          *structure = NULL;
  gboolean               succeeded = FALSE;

  g_return_val_if_fail (GST_IS_CLUTTERIMAGESINK (clutterimagesink), NULL);

  clutterimage = (GstClutterImageBuffer*)gst_mini_object_new (GST_TYPE_CLUTTERIMAGE_BUFFER);

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "width", &clutterimage->width) ||
      !gst_structure_get_int (structure, "height", &clutterimage->height)) 
    GST_WARNING ("failed getting geometry from caps %" GST_PTR_FORMAT, caps);

  GST_DEBUG_OBJECT (clutterimagesink, 
		    "creating image %p (%dx%d)", 
		    clutterimage,
		    clutterimage->width, clutterimage->height);

  g_mutex_lock (clutterimagesink->x_lock);

  clutterimage->clutterimage = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
					       TRUE, 
					       8,
					       clutterimage->width,
					       clutterimage->height);

  if (!clutterimage->clutterimage) 
    {
      GST_ELEMENT_ERROR (clutterimagesink, RESOURCE, WRITE, (NULL),
			 ("could not XCreateImage a %dx%d image"));
      goto beach;
    }

  succeeded = TRUE;

  GST_BUFFER_DATA (clutterimage) = (guchar *) gdk_pixbuf_get_pixels (clutterimage->clutterimage);
  GST_BUFFER_SIZE (clutterimage) = gdk_pixbuf_get_rowstride (clutterimage->clutterimage) * clutterimage->height;
  
  /* Keep a ref to our sink */
  clutterimage->clutterimagesink = gst_object_ref (clutterimagesink);

beach:
  g_mutex_unlock (clutterimagesink->x_lock);
  
  if (!succeeded) {
    gst_clutterimage_buffer_free (clutterimage);
    clutterimage = NULL;
  }

  return clutterimage;
}

static void
gst_clutterimagesink_clutterimage_destroy (GstClutterImageSink   *clutterimagesink,
					   GstClutterImageBuffer *clutterimage)
{
  g_return_if_fail (clutterimage != NULL);
  g_return_if_fail (GST_IS_CLUTTERIMAGESINK (clutterimagesink));

  /* If the destroyed image is the current one we destroy our reference too */
  if (clutterimagesink->cur_image == clutterimage) 
    clutterimagesink->cur_image = NULL;

  /* We might have some buffers destroyed after changing state to NULL */
  if (!clutterimagesink->context)
    goto beach;


  g_mutex_lock (clutterimagesink->x_lock);

  if (clutterimage->clutterimage) 
    g_object_unref (clutterimage->clutterimage);

  g_mutex_unlock (clutterimagesink->x_lock);

beach:
  if (clutterimage->clutterimagesink) 
    {
      /* Release the ref to our sink */
      clutterimage->clutterimagesink = NULL;
      gst_object_unref (clutterimagesink);
    }

  return;
}

static void
gst_clutterimagesink_clutterimage_put (GstClutterImageSink   *clutterimagesink,
				       GstClutterImageBuffer *clutterimage)
{
  GstVideoRectangle src, dst, result;

  g_return_if_fail (GST_IS_CLUTTERIMAGESINK (clutterimagesink));

  /* We take the flow_lock. If expose is in there we don't want to run
     concurrently from the data flow thread */
  g_mutex_lock (clutterimagesink->flow_lock);

  /* Store a reference to the last image we put, lose the previous one */
  if (clutterimage && clutterimagesink->cur_image != clutterimage) 
    {
      if (clutterimagesink->cur_image) 
	{
	  GST_LOG_OBJECT (clutterimagesink, 
			  "unreffing %p", 
			  clutterimagesink->cur_image);
	  gst_buffer_unref (clutterimagesink->cur_image);
	}
      
      GST_LOG_OBJECT (clutterimagesink, 
		      "reffing %p as our current image", 
		      clutterimage);
      
      clutterimagesink->cur_image 
	= GST_CLUTTERIMAGE_BUFFER (gst_buffer_ref (clutterimage));
    }

  /* Expose sends a NULL image, we take the latest frame */
  if (!clutterimage) 
    {
      if (clutterimagesink->cur_image) 
	{
	  clutterimage = clutterimagesink->cur_image;
	} 
      else 
	{
	  g_mutex_unlock (clutterimagesink->flow_lock);
	  return;
	}
    }

  /* FIXME: figure this out */
  src.w = clutterimage->width;
  src.h = clutterimage->height;
  dst.w = clutterimage->width;
  dst.h = clutterimage->height;

  gst_video_sink_center_rect (src, dst, &result, FALSE);
  
  g_mutex_lock (clutterimagesink->x_lock);
  
  if (clutterimagesink->video_texture)
    {
      guchar *pixels;
      guint   x,y, off, stride, total;

      /* gstreamer does not seem to want to give us data in 
       * LITTLE_ENDIAN which GL textures really need so we
       * have to byteswap.
       *
       * Setting pixel_format to GL_BGR does not seem to help 
       * either - maybe something pixbufs are doing something
       * funky also.
       *
       * FIXME:
       * Ultimatly the whole thing can be optimised. ( avoid
       * pixbufs completely with just int data? ). need to just 
       * figure out best way.
      */
      
      pixels = gdk_pixbuf_get_pixels (clutterimage->clutterimage);
      stride = gdk_pixbuf_get_rowstride (clutterimage->clutterimage);

      /* Swap endianess - FIXME: count down + faster, safer */
      for (y=0; y < gdk_pixbuf_get_height(clutterimage->clutterimage); y++)
	{
	  off = (y * stride);
	  for (x=0; x < stride; x += 4)
	    { 
	      pixels[off+x+2] ^= pixels[off+x];
	      pixels[off+x]   ^= pixels[off+x+2];
	      pixels[off+x+2] ^= pixels[off+x];

	      pixels[off+x+4] ^= 0; /* double fix alpha */
	    }
	}

      

      clutter_texture_set_pixbuf (CLUTTER_TEXTURE(clutterimagesink->video_texture),
				  clutterimage->clutterimage);
    }

  g_mutex_unlock (clutterimagesink->x_lock);

  g_mutex_unlock (clutterimagesink->flow_lock);
}

/* This function calculates the pixel aspect ratio based on the properties
 * in the context structure and stores it there. */
static void
gst_clutterimagesink_calculate_pixel_aspect_ratio (GstClutterContext *context)
{
  gint par[][2] = {
    {1, 1},                     /* regular screen */
    {16, 15},                   /* PAL TV */
    {11, 10},                   /* 525 line Rec.601 video */
    {54, 59},                   /* 625 line Rec.601 video */
    {64, 45},                   /* 1280x1024 on 16:9 display */
    {5, 3},                     /* 1280x1024 on 4:3 display */
    {4, 3}                      /*  800x600 on 16:9 display */
  };

  gint    i;
  gint    index;
  gdouble ratio;
  gdouble delta;

#define DELTA(idx) (ABS (ratio - ((gdouble) par[idx][0] / par[idx][1])))

  /* first calculate the "real" ratio based on the X values;
   * which is the "physical" w/h divided by the w/h in pixels of the display */
  ratio = (gdouble) (context->widthmm * context->height)
                     / (context->heightmm * context->width);

  GST_DEBUG ("calculated pixel aspect ratio: %f", ratio);

  /* now find the one from par[][2] with the lowest delta to the real one */
  delta = DELTA (0);
  index = 0;
  
  for (i = 1; i < sizeof (par) / (sizeof (gint) * 2); ++i) 
    {
      gdouble this_delta = DELTA (i);
      
      if (this_delta < delta) {
	index = i;
	delta = this_delta;
      }
    }

  GST_DEBUG ("Decided on index %d (%d/%d)", 
	     index,  par[index][0], par[index][1]);

  g_free (context->par);

  context->par = g_new0 (GValue, 1);

  g_value_init (context->par, GST_TYPE_FRACTION);

  gst_value_set_fraction (context->par, par[index][0], par[index][1]);

  GST_DEBUG ("set context PAR to %d/%d",
	     gst_value_get_fraction_numerator (context->par),
	     gst_value_get_fraction_denominator (context->par));
}

static GstClutterContext *
gst_clutterimagesink_context_get (GstClutterImageSink * clutterimagesink)
{
  GstClutterContext    *context = NULL;
  XPixmapFormatValues *px_formats = NULL;
  gint                 nb_formats = 0, i;

  g_return_val_if_fail (GST_IS_CLUTTERIMAGESINK (clutterimagesink), NULL);

  context = g_new0 (GstClutterContext, 1);

  g_mutex_lock (clutterimagesink->x_lock);

  /* FIXME: pull from clutter context */
  context->disp = XOpenDisplay (clutterimagesink->display_name);

  if (!context->disp) 
    {
      g_mutex_unlock (clutterimagesink->x_lock);
      g_free (context);
      GST_ELEMENT_ERROR (clutterimagesink, RESOURCE, WRITE, (NULL),
			 ("Could not open display"));
      return NULL;
    }

  context->screen     = DefaultScreenOfDisplay (context->disp);
  context->screen_num = DefaultScreen (context->disp);
  context->width    = DisplayWidth (context->disp, context->screen_num);
  context->height   = DisplayHeight (context->disp, context->screen_num);
  context->widthmm  = DisplayWidthMM (context->disp, context->screen_num);
  context->heightmm = DisplayHeightMM (context->disp, context->screen_num);

  DBG("X reports %dx%d pixels and %d mm x %d mm",
      context->width, context->height, 
      context->widthmm, context->heightmm);

  gst_clutterimagesink_calculate_pixel_aspect_ratio (context);

  XCloseDisplay (context->disp);

  /* update object's par with calculated one if not set yet */
  if (!clutterimagesink->par) 
    {
      clutterimagesink->par = g_new0 (GValue, 1);
      gst_value_init_and_copy (clutterimagesink->par, context->par);
      GST_DEBUG_OBJECT (clutterimagesink, "set calculated PAR on object's PAR");
    }

  context->caps = gst_caps_new_simple ("video/x-raw-rgb",
      "bpp", G_TYPE_INT, 32,
      "depth", G_TYPE_INT, 24,
      "endianness", G_TYPE_INT, G_BIG_ENDIAN, 
       "red_mask", G_TYPE_INT,   0xff00 /* >> 8 for 24bpp */, 
      "green_mask", G_TYPE_INT, 0xff0000,
      "blue_mask", G_TYPE_INT,  0xff000000,
      "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

  if (clutterimagesink->par) 
    {
      int nom, den;
      
      nom = gst_value_get_fraction_numerator (clutterimagesink->par);
      den = gst_value_get_fraction_denominator (clutterimagesink->par);
      gst_caps_set_simple (context->caps, 
			   "pixel-aspect-ratio",
			   GST_TYPE_FRACTION, nom, den, NULL);
    }

  g_mutex_unlock (clutterimagesink->x_lock);

  return context;
}

static void
gst_clutterimagesink_context_clear (GstClutterImageSink *clutterimagesink)
{
  g_return_if_fail (GST_IS_CLUTTERIMAGESINK (clutterimagesink));
  g_return_if_fail (clutterimagesink->context != NULL);

  gst_caps_unref (clutterimagesink->context->caps);
  g_free (clutterimagesink->context->par);
  g_free (clutterimagesink->par);
  clutterimagesink->par = NULL;

  g_mutex_lock (clutterimagesink->x_lock);

  XCloseDisplay (clutterimagesink->context->disp);

  g_mutex_unlock (clutterimagesink->x_lock);

  g_free (clutterimagesink->context);

  clutterimagesink->context = NULL;
}

static void
gst_clutterimagesink_bufferpool_clear (GstClutterImageSink *clutterimagesink)
{

  g_mutex_lock (clutterimagesink->pool_lock);

  while (clutterimagesink->buffer_pool) 
    {
      GstClutterImageBuffer *clutterimage;

      clutterimage = clutterimagesink->buffer_pool->data;

      clutterimagesink->buffer_pool 
	= g_slist_delete_link (clutterimagesink->buffer_pool,
			       clutterimagesink->buffer_pool);

      gst_clutterimage_buffer_free (clutterimage);
    }

  g_mutex_unlock (clutterimagesink->pool_lock);
}

/* Element stuff */

static GstCaps *
gst_clutterimagesink_getcaps (GstBaseSink * bsink)
{
  GstClutterImageSink *clutterimagesink;
  GstCaps             *caps;
  int                  i;

  clutterimagesink = GST_CLUTTERIMAGESINK (bsink);

  if (clutterimagesink->context)
    return gst_caps_ref (clutterimagesink->context->caps);

  /* get a template copy and add the pixel aspect ratio */
  caps =
    gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SINK (clutterimagesink)->sinkpad));

  for (i = 0; i < gst_caps_get_size (caps); ++i) 
    {
      GstStructure *structure = gst_caps_get_structure (caps, i);

      if (clutterimagesink->par) 
	{
	  int nom, den;

	  nom = gst_value_get_fraction_numerator (clutterimagesink->par);
	  den = gst_value_get_fraction_denominator (clutterimagesink->par);
	  gst_structure_set (structure, "pixel-aspect-ratio",
			     GST_TYPE_FRACTION, nom, den, NULL);
	}
    }
  
  return caps;
}

static gboolean
gst_clutterimagesink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstClutterImageSink *clutterimagesink;
  gboolean             ret = TRUE;
  GstStructure        *structure;
  GstCaps             *intersection;
  const GValue        *par;
  gint                 new_width, new_height;
  const GValue        *fps;

  clutterimagesink = GST_CLUTTERIMAGESINK (bsink);

  if (!clutterimagesink->context)
    return FALSE;

  GST_DEBUG_OBJECT (clutterimagesink,
		    "sinkconnect possible caps %" GST_PTR_FORMAT " with given caps %"
		    GST_PTR_FORMAT, clutterimagesink->context->caps, caps);

  /* We intersect those caps with our template to make sure they are correct */
  intersection = gst_caps_intersect (clutterimagesink->context->caps, caps);

  GST_DEBUG_OBJECT (clutterimagesink, "intersection returned %" GST_PTR_FORMAT,
		    intersection);

  if (gst_caps_is_empty (intersection)) 
    return FALSE;

  gst_caps_unref (intersection);

  structure = gst_caps_get_structure (caps, 0);

  ret &= gst_structure_get_int (structure, "width", &new_width);
  ret &= gst_structure_get_int (structure, "height", &new_height);
  fps = gst_structure_get_value (structure, "framerate");
  ret &= (fps != NULL);

  if (!ret) return FALSE;

  /* if the caps contain pixel-aspect-ratio, they have to match ours,
   * otherwise linking should fail */
  par = gst_structure_get_value (structure, "pixel-aspect-ratio");

  if (par) 
    {
      if (clutterimagesink->par) 
	{
	  if (gst_value_compare (par, 
				 clutterimagesink->par) != GST_VALUE_EQUAL) 
	    {
	      goto wrong_aspect;
	    }
	} 
      else if (clutterimagesink->context->par) 
	{
	  if (gst_value_compare (par, 
				 clutterimagesink->context->par) != GST_VALUE_EQUAL) 
	    {
	      goto wrong_aspect;
	    }
	}
    }

  GST_VIDEO_SINK_WIDTH (clutterimagesink)  = new_width;
  GST_VIDEO_SINK_HEIGHT (clutterimagesink) = new_height;

  clutterimagesink->fps_n = gst_value_get_fraction_numerator (fps);
  clutterimagesink->fps_d = gst_value_get_fraction_denominator (fps);

  /* Creating our window and our image */
  g_assert (GST_VIDEO_SINK_WIDTH (clutterimagesink) > 0);
  g_assert (GST_VIDEO_SINK_HEIGHT (clutterimagesink) > 0);

  /* If our clutterimage has changed we destroy it, next chain 
     iteration will create a new one */
  if ((clutterimagesink->clutterimage) &&
      ((GST_VIDEO_SINK_WIDTH (clutterimagesink) != clutterimagesink->clutterimage->width) ||
       (GST_VIDEO_SINK_HEIGHT (clutterimagesink) != clutterimagesink->clutterimage->height))) 
    {
      GST_DEBUG_OBJECT (clutterimagesink, 
			"our image is not usable anymore, unref %p",
			clutterimagesink->clutterimage);
      gst_buffer_unref (GST_BUFFER (clutterimagesink->clutterimage));
      clutterimagesink->clutterimage = NULL;
    }

  return TRUE;

wrong_aspect:
  {
    GST_INFO_OBJECT (clutterimagesink, "pixel aspect ratio does not match");
    return FALSE;
  }
}

static GstStateChangeReturn
gst_clutterimagesink_change_state (GstElement    *element, 
				   GstStateChange transition)
{
  GstClutterImageSink *clutterimagesink;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  clutterimagesink = GST_CLUTTERIMAGESINK (element);

  switch (transition) 
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
      clutterimagesink->running = TRUE;
      /* Initializing the Context */
      if (!clutterimagesink->context)
        clutterimagesink->context 
	  = gst_clutterimagesink_context_get (clutterimagesink);
      if (!clutterimagesink->context) 
	{
	  ret = GST_STATE_CHANGE_FAILURE;
	  goto beach;
	}
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      clutterimagesink->fps_n = 0;
      clutterimagesink->fps_d = 1;
      GST_VIDEO_SINK_WIDTH (clutterimagesink)  = 0;
      GST_VIDEO_SINK_HEIGHT (clutterimagesink) = 0;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      clutterimagesink->running = FALSE;
      if (clutterimagesink->clutterimage) 
	{
	  gst_buffer_unref (clutterimagesink->clutterimage);
	  clutterimagesink->clutterimage = NULL;
	}
      if (clutterimagesink->cur_image) 
	{
	  gst_buffer_unref (clutterimagesink->cur_image);
	  clutterimagesink->cur_image = NULL;
	}
      if (clutterimagesink->buffer_pool)
        gst_clutterimagesink_bufferpool_clear (clutterimagesink);

      if (clutterimagesink->context) 
	{
	  gst_clutterimagesink_context_clear (clutterimagesink);
	  clutterimagesink->context = NULL;
	}
      break;
    default:
      break;
  }
beach:
  return ret;
}

static void
gst_clutterimagesink_get_times (GstBaseSink  *bsink, 
				GstBuffer    *buf,
				GstClockTime *start, 
				GstClockTime *end)
{
  GstClutterImageSink *clutterimagesink;

  clutterimagesink = GST_CLUTTERIMAGESINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) 
    {
      *start = GST_BUFFER_TIMESTAMP (buf);
      if (GST_BUFFER_DURATION_IS_VALID (buf)) 
	{
	  *end = *start + GST_BUFFER_DURATION (buf);
	} 
      else 
	{
	  if (clutterimagesink->fps_n > 0) {
	    *end = *start + (GST_SECOND * clutterimagesink->fps_d) 
	                        / clutterimagesink->fps_n;
	  }
	}
    }
}

static GstFlowReturn
gst_clutterimagesink_show_frame (GstBaseSink *bsink, GstBuffer *buf)
{
  GstClutterImageSink *clutterimagesink;

  g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);

  clutterimagesink = GST_CLUTTERIMAGESINK (bsink);

  /* If this buffer has been allocated using our buffer management we simply
     put the clutterimage which is in the PRIVATE pointer */
  if (GST_IS_CLUTTERIMAGE_BUFFER (buf)) 
    {
      GST_LOG_OBJECT (clutterimagesink, 
		      "buffer from our pool, writing directly");
      
      gst_clutterimagesink_clutterimage_put (clutterimagesink, 
					     GST_CLUTTERIMAGE_BUFFER (buf));
    } 
  else 
    {
      /* Else we have to copy the data into our private image, */
      /* if we have one... */
      GST_LOG_OBJECT (clutterimagesink, "normal buffer, copying from it");

      if (!clutterimagesink->clutterimage) 
	{
	  GST_DEBUG_OBJECT (clutterimagesink, "creating our clutterimage");

	  clutterimagesink->clutterimage 
	    = gst_clutterimagesink_clutterimage_new (clutterimagesink,
						     GST_BUFFER_CAPS (buf));
	  if (!clutterimagesink->clutterimage)
	    goto no_clutterimage;
	}
      
      memcpy (GST_BUFFER_DATA (clutterimagesink->clutterimage), 
	      GST_BUFFER_DATA (buf),
	      MIN (GST_BUFFER_SIZE (buf), 
		   clutterimagesink->clutterimage->size));
      
      gst_clutterimagesink_clutterimage_put (clutterimagesink, 
					     clutterimagesink->clutterimage);
    }

  return GST_FLOW_OK;
  
  /* ERRORS */
 no_clutterimage:
  {
    /* No image available. That's very bad ! */
    GST_DEBUG ("could not create image");

    GST_ELEMENT_ERROR (clutterimagesink, CORE, NEGOTIATION, (NULL),
     ("Failed creating an ClutterImage in clutterimagesink chain function."));
    return GST_FLOW_ERROR;
  }
}

/* Buffer management */

static GstFlowReturn
gst_clutterimagesink_buffer_alloc (GstBaseSink *bsink, 
				   guint64      offset, 
				   guint        size,
				   GstCaps     *caps, 
				   GstBuffer  **buf)
{
  GstClutterImageSink   *clutterimagesink;
  GstClutterImageBuffer *clutterimage = NULL;
  GstStructure          *structure = NULL;
  GstCaps               *desired_caps = NULL;
  GstFlowReturn          ret = GST_FLOW_OK;
  gboolean               rev_nego = FALSE;
  gint                   width, height;

  clutterimagesink = GST_CLUTTERIMAGESINK (bsink);

  GST_LOG_OBJECT (clutterimagesink,
      "a buffer of %d bytes was requested with caps %" GST_PTR_FORMAT
      " and offset %" G_GUINT64_FORMAT, size, caps, offset);

  desired_caps = gst_caps_copy (caps);

  structure = gst_caps_get_structure (desired_caps, 0);

  if (gst_structure_get_int (structure, "width", &width) &&
      gst_structure_get_int (structure, "height", &height)) 
    {
      GstVideoRectangle dst, src, result;
      
      src.w = width;
      src.h = height;
      
      /* We take the flow_lock because the window might go away */
      g_mutex_lock (clutterimagesink->flow_lock);

      /* What is our geometry */
#if 0
    gst_clutterimagesink_xwindow_update_geometry (clutterimagesink, clutterimagesink->xwindow);
    dst.w = clutterimagesink->xwindow->width;
    dst.h = clutterimagesink->xwindow->height;
#endif

    g_mutex_unlock (clutterimagesink->flow_lock);
    
    if (clutterimagesink->keep_aspect) 
      {
	GST_LOG_OBJECT (clutterimagesink, 
			"enforcing aspect ratio in reverse caps "
			"negotiation");
	gst_video_sink_center_rect (src, dst, &result, TRUE);
      } 
    else 
      {
	GST_LOG_OBJECT (clutterimagesink, 
			"trying to resize to window geometry "
			"ignoring aspect ratio");
	result.x = result.y = 0;
	result.w = dst.w;
	result.h = dst.h;
      }

    /* We would like another geometry */
    if (width != result.w || height != result.h) 
      {
	int     nom, den;
	GstPad *peer;

	peer = gst_pad_get_peer (GST_VIDEO_SINK_PAD (clutterimagesink));

      if (!GST_IS_PAD (peer)) 
        goto alloc;

      GST_DEBUG ("we would love to receive a %dx%d video", result.w, result.h);

      gst_structure_set (structure, "width", G_TYPE_INT, result.w, NULL);
      gst_structure_set (structure, "height", G_TYPE_INT, result.h, NULL);

      /* PAR property overrides the X calculated one */
      if (clutterimagesink->par) 
	{
	  nom = gst_value_get_fraction_numerator (clutterimagesink->par);
	  den = gst_value_get_fraction_denominator (clutterimagesink->par);
	  gst_structure_set (structure, "pixel-aspect-ratio",
			     GST_TYPE_FRACTION, nom, den, NULL);
	} 
      else if (clutterimagesink->context->par) 
	{
	  nom = gst_value_get_fraction_numerator (clutterimagesink->context->par);
	  den = gst_value_get_fraction_denominator (clutterimagesink->context->par);
	  gst_structure_set (structure, "pixel-aspect-ratio",
			     GST_TYPE_FRACTION, nom, den, NULL);
	}

      if (gst_pad_accept_caps (peer, desired_caps)) 
	{
	  gint bpp;

	  bpp      = size / height / width;
	  rev_nego = TRUE;
	  width    = result.w;
	  height   = result.h;
	  size     = bpp * width * height;

	  GST_DEBUG ("peed pad accepts our desired caps %" GST_PTR_FORMAT
		     " buffer size is now %d bytes", desired_caps, size);
	} 
      else 
	{
	  GST_DEBUG ("peer pad does not accept our desired caps %" GST_PTR_FORMAT,
		     desired_caps);
	  rev_nego = FALSE;
	  width  = GST_VIDEO_SINK_WIDTH (clutterimagesink);
	  height = GST_VIDEO_SINK_HEIGHT (clutterimagesink);
      }
      gst_object_unref (peer);
      }
    }
  
 alloc:
  /* Inspect our buffer pool */
  g_mutex_lock (clutterimagesink->pool_lock);
  while (clutterimagesink->buffer_pool) 
    {
      clutterimage = (GstClutterImageBuffer *) clutterimagesink->buffer_pool->data;

      if (clutterimage) 
	{
	  /* Removing from the pool */
	  clutterimagesink->buffer_pool 
	    = g_slist_delete_link (clutterimagesink->buffer_pool,
				   clutterimagesink->buffer_pool);

	  /* If the clutterimage is invalid for our need, destroy */
	  if ((clutterimage->width != width) 
	      || (clutterimage->height != height)) 
	    {
	      gst_clutterimage_buffer_free (clutterimage);
	      clutterimage = NULL;
	    } 
	  else 
	    {
	      /* We found a suitable clutterimage */
	      break;
	    }
	}
    }
  g_mutex_unlock (clutterimagesink->pool_lock);

  /* We haven't found anything, creating a new one */
  if (!clutterimage) 
    {
      if (rev_nego)
	clutterimage = gst_clutterimagesink_clutterimage_new (clutterimagesink,
							      desired_caps);
      else 
	clutterimage = gst_clutterimagesink_clutterimage_new (clutterimagesink,
							      caps);
 
    }

  /* Now we should have a clutterimage, set appropriate caps on it */
  if (clutterimage) 
    {
      if (rev_nego) 
	gst_buffer_set_caps (GST_BUFFER (clutterimage), desired_caps);
      else 
	gst_buffer_set_caps (GST_BUFFER (clutterimage), caps);
    }

  gst_caps_unref (desired_caps);
  
  *buf = GST_BUFFER (clutterimage);
  
  return ret;
}

/* Interfaces stuff - probably non working clutter */

static gboolean
gst_clutterimagesink_interface_supported (GstImplementsInterface *iface, 
					  GType                   type)
{
  g_assert (type == GST_TYPE_NAVIGATION);
  return TRUE;
}

static void
gst_clutterimagesink_interface_init (GstImplementsInterfaceClass *klass)
{
  klass->supported = gst_clutterimagesink_interface_supported;
}

static void
gst_clutterimagesink_navigation_send_event (GstNavigation *navigation,
					    GstStructure  *structure)
{
  GstClutterImageSink *clutterimagesink = GST_CLUTTERIMAGESINK (navigation);
  GstEvent            *event;
  gint                 x_offset, y_offset;
  gdouble              x, y;
  GstPad              *pad = NULL;

  event = gst_event_new_navigation (structure);

  /* We are not converting the pointer coordinates as there's no hardware
     scaling done here. The only possible scaling is done by videoscale and
     videoscale will have to catch those events and tranform the coordinates
     to match the applied scaling. So here we just add the offset if the image
     is centered in the window.  */

  /* We take the flow_lock while we look at the window */
  g_mutex_lock (clutterimagesink->flow_lock);

  x_offset = 0;
  y_offset = 0;

  g_mutex_unlock (clutterimagesink->flow_lock);

  if (gst_structure_get_double (structure, "pointer_x", &x)) 
    {
      x -= x_offset / 2;
      gst_structure_set (structure, "pointer_x", G_TYPE_DOUBLE, x, NULL);
    }

  if (gst_structure_get_double (structure, "pointer_y", &y)) 
    {
      y -= y_offset / 2;
      gst_structure_set (structure, "pointer_y", G_TYPE_DOUBLE, y, NULL);
    }

  pad = gst_pad_get_peer (GST_VIDEO_SINK_PAD (clutterimagesink));

  if (GST_IS_PAD (pad) && GST_IS_EVENT (event)) {
    gst_pad_send_event (pad, event);
    
    gst_object_unref (pad);
  }
}

static void
gst_clutterimagesink_navigation_init (GstNavigationInterface *iface)
{
  iface->send_event = gst_clutterimagesink_navigation_send_event;
}


/* =========================================== */
/*                                             */
/*              Init & Class init              */
/*                                             */
/* =========================================== */

static void
gst_clutterimagesink_set_property (GObject      *object, 
				   guint         prop_id,
				   const GValue *value, 
				   GParamSpec   *pspec)
{
  GstClutterImageSink *clutterimagesink;

  g_return_if_fail (GST_IS_CLUTTERIMAGESINK (object));

  clutterimagesink = GST_CLUTTERIMAGESINK (object);

  switch (prop_id) 
    {
    case PROP_VIDEO_TEXTURE:
      clutterimagesink->video_texture = g_value_get_pointer (value);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      clutterimagesink->keep_aspect = g_value_get_boolean (value);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      {
	GValue *tmp;
	
	tmp = g_new0 (GValue, 1);
	g_value_init (tmp, GST_TYPE_FRACTION);
	
	if (!g_value_transform (value, tmp)) 
	  {
	    GST_WARNING_OBJECT (clutterimagesink,
				"Could not transform string to aspect ratio");
	    g_free (tmp);
	  } 
	else 
	  {
	    GST_DEBUG_OBJECT (clutterimagesink, "set PAR to %d/%d",
			      gst_value_get_fraction_numerator (tmp),
			      gst_value_get_fraction_denominator (tmp));
	    g_free (clutterimagesink->par);
	    clutterimagesink->par = tmp;
	  }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gst_clutterimagesink_get_property (GObject    *object, 
				   guint       prop_id,
				   GValue     *value, 
				   GParamSpec *pspec)
{
  GstClutterImageSink *clutterimagesink;

  g_return_if_fail (GST_IS_CLUTTERIMAGESINK (object));

  clutterimagesink = GST_CLUTTERIMAGESINK (object);

  switch (prop_id) 
    {
    case PROP_VIDEO_TEXTURE:
      g_value_set_pointer (value, clutterimagesink->video_texture);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, clutterimagesink->keep_aspect);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      if (clutterimagesink->par)
        g_value_transform (clutterimagesink->par, value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gst_clutterimagesink_finalize (GObject * object)
{
  GstClutterImageSink *clutterimagesink;

  clutterimagesink = GST_CLUTTERIMAGESINK (object);

  if (clutterimagesink->display_name) 
    {
      g_free (clutterimagesink->display_name);
      clutterimagesink->display_name = NULL;
    }

  if (clutterimagesink->par) 
    {
      g_free (clutterimagesink->par);
      clutterimagesink->par = NULL;
    }

  if (clutterimagesink->x_lock) 
    {
      g_mutex_free (clutterimagesink->x_lock);
      clutterimagesink->x_lock = NULL;
    }

  if (clutterimagesink->flow_lock) 
    {
      g_mutex_free (clutterimagesink->flow_lock);
      clutterimagesink->flow_lock = NULL;
    }

  if (clutterimagesink->pool_lock) 
    {
      g_mutex_free (clutterimagesink->pool_lock);
      clutterimagesink->pool_lock = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_clutterimagesink_init (GstClutterImageSink * clutterimagesink)
{
  clutterimagesink->display_name = NULL;
  clutterimagesink->context  = NULL;
  clutterimagesink->clutterimage = NULL;
  clutterimagesink->cur_image = NULL;

  clutterimagesink->event_thread = NULL;
  clutterimagesink->running = FALSE;

  clutterimagesink->fps_n = 0;
  clutterimagesink->fps_d = 1;

  clutterimagesink->x_lock    = g_mutex_new ();
  clutterimagesink->flow_lock = g_mutex_new ();

  clutterimagesink->par       = NULL;

  clutterimagesink->pool_lock = g_mutex_new ();
  clutterimagesink->buffer_pool = NULL;

  clutterimagesink->keep_aspect = FALSE;
}

static void
gst_clutterimagesink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &gst_clutterimagesink_details);

  gst_element_class_add_pad_template (element_class,
    gst_static_pad_template_get (&gst_clutterimagesink_sink_template_factory));
}

static void
gst_clutterimagesink_class_init (GstClutterImageSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  parent_class = g_type_class_ref (GST_TYPE_VIDEO_SINK);

  gobject_class->finalize = gst_clutterimagesink_finalize;
  gobject_class->set_property = gst_clutterimagesink_set_property;
  gobject_class->get_property = gst_clutterimagesink_get_property;

  g_object_class_install_property (gobject_class, PROP_VIDEO_TEXTURE,
				   g_param_spec_pointer ("video-texture", 
							"Video-Texture", 
							"Video Texture",
							G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
				   g_param_spec_boolean ("force-aspect-ratio",
							 "Force aspect ratio",
	     "When enabled, reverse caps negotiation (scaling) will respect "
					            "original aspect ratio", 
							 FALSE, 
							 G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
				   g_param_spec_string ("pixel-aspect-ratio", 
							"Pixel Aspect Ratio",
							"The pixel aspect ratio of the device", 
							"1/1", 
							G_PARAM_READWRITE));

  gstelement_class->change_state = gst_clutterimagesink_change_state;

  gstbasesink_class->get_caps 
    = GST_DEBUG_FUNCPTR (gst_clutterimagesink_getcaps);

  gstbasesink_class->set_caps 
    = GST_DEBUG_FUNCPTR (gst_clutterimagesink_setcaps);

  gstbasesink_class->buffer_alloc =
      GST_DEBUG_FUNCPTR (gst_clutterimagesink_buffer_alloc);

  gstbasesink_class->get_times 
    = GST_DEBUG_FUNCPTR (gst_clutterimagesink_get_times);

  gstbasesink_class->preroll 
    = GST_DEBUG_FUNCPTR (gst_clutterimagesink_show_frame);

  gstbasesink_class->render 
    = GST_DEBUG_FUNCPTR (gst_clutterimagesink_show_frame);
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
gst_clutterimagesink_get_type (void)
{
  static GType clutterimagesink_type = 0;

  if (!clutterimagesink_type) {
    static const GTypeInfo clutterimagesink_info = {
      sizeof (GstClutterImageSinkClass),
      gst_clutterimagesink_base_init,
      NULL,
      (GClassInitFunc) gst_clutterimagesink_class_init,
      NULL,
      NULL,
      sizeof (GstClutterImageSink),
      0,
      (GInstanceInitFunc) gst_clutterimagesink_init,
    };
    static const GInterfaceInfo iface_info = {
      (GInterfaceInitFunc) gst_clutterimagesink_interface_init,
      NULL,
      NULL,
    };
    static const GInterfaceInfo navigation_info = {
      (GInterfaceInitFunc) gst_clutterimagesink_navigation_init,
      NULL,
      NULL,
    };
    clutterimagesink_type 
      = g_type_register_static (GST_TYPE_VIDEO_SINK,
				"GstClutterImageSink", 
				&clutterimagesink_info, 0);

    g_type_add_interface_static (clutterimagesink_type, 
				 GST_TYPE_IMPLEMENTS_INTERFACE,
				 &iface_info);

    g_type_add_interface_static (clutterimagesink_type, 
				 GST_TYPE_NAVIGATION,
				 &navigation_info);
  }
  
  return clutterimagesink_type;
}

GST_DEBUG_CATEGORY (gst_debug_clutterimagesink);

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "clutterimagesink",
          GST_RANK_SECONDARY, GST_TYPE_CLUTTERIMAGESINK))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_debug_clutterimagesink, "clutterimagesink", 0,
      "clutterimagesink element");

  return TRUE;
}

#define GST_LICENSE "LGPL"
#define GST_PACKAGE "GStreamer"
#define GST_ORIGIN  "http://o-hand.com"

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "clutterimagesink",
    "Clutter Video Sink",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE, GST_ORIGIN)
