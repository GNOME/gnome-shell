#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <gmodule.h>

#include <clutter/clutter.h>

guchar*
make_rgba_data (int width, int height, int bpp, int has_alpha, int *rowstride_p)
{
#define CHECK_SIZE 20

  gint       x,y, rowstride, i = 0;
  guchar *pixels;

  g_assert(bpp == 4);
  g_assert(has_alpha == TRUE);

  rowstride = width * bpp;
  *rowstride_p = rowstride;

  pixels = g_try_malloc (height * rowstride);
  if (!pixels)
    return NULL;

  for (y = 0; y < height; y++)
    {
      i = 0;
      for (x = 0; x < width; x++)
	{
	  guchar *p;
	  
	  p = pixels + y * rowstride + x * bpp;
	  
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

  return pixels;
}

#define SPIN()   while (g_main_context_pending (NULL)) \
                     g_main_context_iteration (NULL, FALSE);

G_MODULE_EXPORT int
test_textures_main (int argc, char *argv[])
{
  ClutterActor    *texture;
  ClutterActor    *stage;
  gint             i, j;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_show_all (CLUTTER_ACTOR (stage));

  SPIN();

  for (i=100; i<=5000; i += 100)
    for (j=0; j<4; j++)
      {
        const int width = i+j;
        const int height = i+j;
        const gboolean has_alpha = TRUE;
        const int bpp = has_alpha ? 4 : 3;
        int rowstride;
        guchar *pixels;

        pixels = make_rgba_data (width, height, bpp, has_alpha, &rowstride);
        if (!pixels)
          g_error("No memory for %ix%i RGBA data failed", width, height);

        printf("o %ix%i texture... ", width, height);

        texture = clutter_texture_new ();
        if (!clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (texture),
                                                pixels,
                                                has_alpha,
                                                width,
                                                height,
                                                rowstride,
                                                bpp,
                                                0, NULL))
          g_error("texture creation failed");
        g_free(pixels);
	
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

  return EXIT_SUCCESS;
}
