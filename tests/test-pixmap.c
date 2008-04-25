

#if HAVE_CLUTTER_GLX
#include <config.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <clutter/x11/clutter-x11-texture-pixmap.h>

#include <clutter/glx/clutter-glx.h>
#include <clutter/glx/clutter-glx-texture-pixmap.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <string.h>

#define IMAGE "redhand.png"

#ifdef USE_GDKPIXBUF

static gboolean
stage_press_cb (ClutterActor    *actor,
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

#define r ((guint32)(*(p)))
#define g ((guint32)(*(p+1)))
#define b ((guint32)(*(p+2)))
#define a ((guint32)(*(p+3)))
        guint32 pixel =
            ((a << 24) & 0xFF000000  ) |
            ((r << 16) & 0x00FF0000  ) |
            ((g <<  8) & 0x0000FF00) |
            ((b)       & 0x000000FF );

        *((guint32 *)d) = pixel;

      }
#undef r
#undef g
#undef b
#undef a

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
#endif

int
main (int argc, char **argv)
{
#ifdef USE_GDKPIXBUF
  ClutterActor         *stage, *tex;
  Pixmap                pixmap;
  guint                 w, h, d;
  const ClutterColor    gry = { 0x99, 0x99, 0x99, 0xFF };
  Window                win_remote;

  clutter_init (&argc, &argv);

  if (argc < 2)
    g_error ("usage: %s <window id>", argv[0]);

  win_remote = atol(argv[1]);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &gry);

  pixmap = win_remote;

  /*
  XCompositeRedirectWindow(clutter_x11_get_default_display(),
                           win_remote,
                           CompositeRedirectAutomatic);

  pixmap = XCompositeNameWindowPixmap (clutter_x11_get_default_display(), 
                                       win_remote);
  */

  tex = clutter_x11_texture_pixmap_new_with_pixmap (pixmap);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), tex);

  clutter_x11_texture_pixmap_set_automatic (CLUTTER_X11_TEXTURE_PIXMAP (tex), 
                                            TRUE);

#ifdef HAVE_CLUTTER_GLX
  pixmap = create_pixmap (&w, &h, &d);

  tex = clutter_glx_texture_pixmap_new_with_pixmap (pixmap);

  clutter_actor_set_position (tex,
                              clutter_actor_get_width (stage) 
                              - clutter_actor_get_width (tex),
                              0);

  clutter_x11_texture_pixmap_set_automatic (CLUTTER_X11_TEXTURE_PIXMAP (tex), 
                                            FALSE);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               tex);
#endif

  g_signal_connect (stage, "button-press-event", 
                    G_CALLBACK (stage_press_cb), (gpointer)pixmap);

  clutter_actor_show (stage);

  clutter_main ();
#endif

}

#else
int main(int argc, char **argv){return 0;};
#endif
