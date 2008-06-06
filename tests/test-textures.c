#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <clutter/clutter.h>

#ifdef USE_GDKPIXBUF

GdkPixbuf*
make_pixbuf (int width, int height, int bpp, int has_alpha)
{
#define CHECK_SIZE 20

  GdkPixbuf *px;
  gint       x,y, rowstride, n_channels, i = 0;

  px = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
		       has_alpha,
		       8,
		       width,
		       height);

  if (!px) return NULL;

  rowstride  = gdk_pixbuf_get_rowstride (px);
  n_channels = gdk_pixbuf_get_n_channels (px);

  for (y = 0; y < height; y++)
    {
      i = 0;
      for (x = 0; x < width; x++)
	{
	  guchar *p;
	  
	  p = gdk_pixbuf_get_pixels (px) + y * rowstride + x * n_channels;
	  
	  p[0] = p[1] = p[2] = 0; p[3] = 0xff;
	  
	  if (x && y && y % CHECK_SIZE && x % CHECK_SIZE)
	    {
	      if (x % CHECK_SIZE == 1)
		{
		  if (++i > 3) 
		    i = 0;
		}
	      p[i] = 0xff;
	    }
	}
    }

  return px;
}

#define SPIN()   while (g_main_context_pending (NULL)) \
                     g_main_context_iteration (NULL, FALSE);

#endif /* USE_GDKPIXBUF */

int
main (int argc, char *argv[])
{
#ifdef USE_GDKPIXBUF
  ClutterActor    *texture;
  ClutterActor    *stage;
  GdkPixbuf       *pixbuf;
  gint             i, j;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_show_all (CLUTTER_ACTOR (stage));

  SPIN();

  for (i=100; i<5000; i += 100)
    for (j=0; j<4; j++)
      {
	pixbuf = make_pixbuf (i+j, i+j, 4, TRUE);
	
	if (!pixbuf)
	  g_error("%ix%i pixbuf creation failed", i+j, i+j);
	
	printf("o %ix%i pixbuf... ", i+j, i+j);

	texture = clutter_texture_new ();
	clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (texture),
					   gdk_pixbuf_get_pixels (pixbuf),
					   gdk_pixbuf_get_has_alpha (pixbuf),
					   gdk_pixbuf_get_width (pixbuf),
					   gdk_pixbuf_get_height (pixbuf),
					   gdk_pixbuf_get_rowstride (pixbuf),
					   gdk_pixbuf_get_has_alpha (pixbuf)
					   ? 4 : 3,
					   0, NULL);
	
	g_object_unref (pixbuf);
	
	if (!texture)
	  g_error("Pixbuf creation failed");
	
	printf("uploaded to texture...\n");
	
	clutter_container_add (CLUTTER_CONTAINER (stage), texture, NULL);
	clutter_actor_set_size (texture, 400, 400);
	clutter_actor_show (texture);

	/* Hide & show to unreaise then realise the texture */
	clutter_actor_hide (texture);
	clutter_actor_show (texture);	

	SPIN();

        clutter_container_remove (CLUTTER_CONTAINER (stage), texture, NULL);
    }
#endif /* USE_GDKPIXBUF */

  return EXIT_SUCCESS;
}
