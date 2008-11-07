#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gmodule.h>

#if HAVE_CLUTTER_GLX

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <clutter/x11/clutter-x11-texture-pixmap.h>

#include <clutter/glx/clutter-glx.h>
#include <clutter/glx/clutter-glx-texture-pixmap.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#define IMAGE "redhand.png"

# ifdef USE_GDKPIXBUF
# include <gdk-pixbuf/gdk-pixbuf.h>

static gboolean disable_x11 = FALSE;
static gboolean disable_glx = FALSE;

static GOptionEntry g_options[] = 
{
  { "disable-x11",
    0, 0,
    G_OPTION_ARG_NONE,
    &disable_x11,
    "Disable redirection through X11 pixmap",
    NULL },
  { "disable-glx",
    0, 0,
    G_OPTION_ARG_NONE,
    &disable_glx,
    "Disable redirection through GLX pixmap",
    NULL },

  { NULL }
};

static gboolean
stage_key_release_cb (ClutterActor    *actor,
		      ClutterEvent    *event,
		      gpointer         data)
{
  switch (clutter_key_event_symbol (&event->key))
    {
    case CLUTTER_q:
    case CLUTTER_Q:
      clutter_main_quit ();
      break;
    }
  return FALSE;
}

static gboolean
stage_button_press_cb (ClutterActor    *actor,
		       ClutterEvent    *event,
		       gpointer         data)
{
  Pixmap        pxm = (Pixmap)data;
  Display      *dpy = clutter_x11_get_default_display ();
  GC            gc;
  XGCValues     gc_values = {0};


  gc = XCreateGC (dpy,
                  pxm,
                  0,
                  &gc_values);

  XDrawLine (dpy, pxm, gc, 0, 0, 100, 100); 

  return FALSE;
}

Pixmap
create_pixmap (guint *width, guint *height, guint *depth)
{
  Display      *dpy = clutter_x11_get_default_display ();
  Pixmap        pixmap;
  GdkPixbuf    *pixbuf;
  GError       *error = NULL;
  XImage       *image;
  char         *data, *d;
  guchar       *p, *line, *endofline, *end;
  guint         w, h, rowstride;
  GC            gc;
  XGCValues     gc_values = {0};

  pixbuf = gdk_pixbuf_new_from_file (IMAGE, &error);
  if (error)
    g_error (error->message);

  /* We assume that the image had an alpha channel */
  g_assert (gdk_pixbuf_get_has_alpha (pixbuf));

  w = gdk_pixbuf_get_width  (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  data = g_malloc (w * h * 4);
  image = XCreateImage (dpy,
                        None,
                        32,
                        ZPixmap,
                        0,
                        data,
                        w, h,
                        8,
                        w * 4);

  p = gdk_pixbuf_get_pixels (pixbuf);
  d  = data;
  end = p + rowstride*h;

  /* Convert from RGBA as contained in the pixmap to ARGB as used in X */
  for (line = p; line < end ; line += rowstride)
  {
    p = line;
    endofline = p + 4 * w;

    for (p = line; p < endofline; p += 4, d+=4)
      {

#  define r ((guint32)(*(p)))
#  define g ((guint32)(*(p+1)))
#  define b ((guint32)(*(p+2)))
#  define a ((guint32)(*(p+3)))
        guint32 pixel =
            ((a << 24) & 0xFF000000  ) |
            ((r << 16) & 0x00FF0000  ) |
            ((g <<  8) & 0x0000FF00) |
            ((b)       & 0x000000FF );

        *((guint32 *)d) = pixel;

      }
#  undef r
#  undef g
#  undef b
#  undef a

  }

  g_object_unref (pixbuf);

  pixmap = XCreatePixmap (dpy,
                          DefaultRootWindow (dpy),
                          w, h,
                          32);

  gc = XCreateGC (dpy,
                  pixmap,
                  0,
                  &gc_values);

  XPutImage (dpy,
             pixmap,
             gc,
             image,
             0, 0,
             0, 0,
             w, h);

  XFreeGC (dpy, gc);
  XDestroyImage (image);

  if (width) *width = w;
  if (height) *height = h;
  if (depth) *depth = 32;

  return pixmap;
}
# endif /* USE_GDKPIXBUF */

G_MODULE_EXPORT int
test_pixmap_main (int argc, char **argv)
{
#ifdef USE_GDKPIXBUF
  GOptionContext       *context;
  ClutterActor         *stage, *tex;
  Pixmap                pixmap;
  const ClutterColor    gry = { 0x99, 0x99, 0x99, 0xFF };
  Window                win_remote;
  guint w, h, d;

  clutter_init (&argc, &argv);

  context = g_option_context_new (" - text-pixmap options");
  g_option_context_add_main_entries (context, g_options, NULL);
  g_option_context_parse (context, &argc, &argv, NULL);	

  if (argc < 2)
    g_error ("usage: %s <window id>", argv[0]);

  win_remote = strtol (argv[1], NULL, 0);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &gry);

  /* a pixmap */
  pixmap = create_pixmap (&w, &h, &d);

  tex = clutter_x11_texture_pixmap_new_with_pixmap (pixmap);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), tex);
  /* oddly, the actor's size is 0 until it is realized, even though
     pixmap-height is set */
  clutter_actor_set_position (tex, 0, 
                              clutter_actor_get_height (stage)
                              - clutter_actor_get_height (tex));

  /* a window */
  if (!disable_x11)
    {
      tex = clutter_x11_texture_pixmap_new_with_window (win_remote);

      clutter_actor_set_position (tex, 0, 0);

      clutter_container_add_actor (CLUTTER_CONTAINER (stage), tex);

      clutter_x11_texture_pixmap_set_automatic (CLUTTER_X11_TEXTURE_PIXMAP (tex), 
						TRUE);
    }

#  ifdef HAVE_CLUTTER_GLX
  /* a window with glx */
  if (!disable_glx)
    {
      tex = clutter_glx_texture_pixmap_new_with_window (win_remote);

      clutter_actor_set_position (tex,
				  clutter_actor_get_width (stage) 
				  - clutter_actor_get_width (tex),
				  0);

      clutter_container_add_actor (CLUTTER_CONTAINER (stage), tex);

      clutter_x11_texture_pixmap_set_automatic (CLUTTER_X11_TEXTURE_PIXMAP (tex), 
						TRUE);
    }
#endif /* HAVE_CLUTTER_GLX */

  g_signal_connect (stage, "key-release-event", 
                    G_CALLBACK (stage_key_release_cb), (gpointer)pixmap);
  g_signal_connect (stage, "button-press-event", 
                    G_CALLBACK (stage_button_press_cb), (gpointer)pixmap);

  clutter_actor_show_all (stage);

  clutter_main ();
# endif /* USE_GDKPIXBUF */

  return EXIT_SUCCESS;
}

#else /* HAVE_CLUTTER_GLX */
int main (int argc, char **argv) { return EXIT_SUCCESS; };
#endif /* HAVE_CLUTTER_GLX */
