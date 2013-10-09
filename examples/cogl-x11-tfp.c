#include <cogl/cogl.h>
#include <cogl/cogl-xlib.h>
#include <cogl/winsys/cogl-texture-pixmap-x11.h>
#include <glib.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/extensions/Xcomposite.h>

#define X11_FOREIGN_EVENT_MASK \
  (KeyPressMask | \
   KeyReleaseMask | \
   ButtonPressMask | \
   ButtonReleaseMask | \
   PointerMotionMask)

#define TFP_XWIN_WIDTH 200
#define TFP_XWIN_HEIGHT 200

static void
update_cogl_x11_event_mask (CoglOnscreen *onscreen,
                            uint32_t event_mask,
                            void *user_data)
{
  Display *xdpy = user_data;
  XSetWindowAttributes attrs;
  uint32_t xwin;

  attrs.event_mask = event_mask | X11_FOREIGN_EVENT_MASK;
  xwin = cogl_x11_onscreen_get_window_xid (onscreen);

  XChangeWindowAttributes (xdpy,
                           (Window)xwin,
                           CWEventMask,
                           &attrs);
}

int
main (int argc, char **argv)
{
  Display *xdpy;
  int composite_error = 0, composite_event = 0;
  CoglRenderer *renderer;
  CoglSwapChain *chain;
  CoglOnscreenTemplate *onscreen_template;
  CoglDisplay *display;
  CoglContext *ctx;
  CoglOnscreen *onscreen;
  CoglFramebuffer *fb;
  CoglError *error = NULL;
  uint32_t visual;
  XVisualInfo template, *xvisinfo;
  int visinfos_count;
  XSetWindowAttributes xattr;
  unsigned long mask;
  Window xwin;
  int screen;
  Window tfp_xwin;
  Pixmap pixmap;
  CoglTexturePixmapX11 *tfp;
  GC gc;

  g_print ("NB: Don't use this example as a benchmark since there is "
           "no synchonization between X window updates and onscreen "
           "framebuffer updates!\n");

  /* Since we want to test external ownership of the X display,
   * connect to X manually... */
  xdpy = XOpenDisplay (NULL);
  if (!xdpy)
    {
      fprintf (stderr, "Failed to open X Display\n");
      return 1;
    }

  XSynchronize (xdpy, True);

  if (XCompositeQueryExtension (xdpy, &composite_event, &composite_error))
    {
      int major = 0, minor = 0;
      if (XCompositeQueryVersion (xdpy, &major, &minor))
        {
          if (major != 0 || minor < 3)
            g_error ("Missing XComposite extension >= 0.3");
        }
    }

  /* Conceptually choose a GPU... */
  renderer = cogl_renderer_new ();
  /* FIXME: This should conceptually be part of the configuration of
   * a renderer. */
  cogl_xlib_renderer_set_foreign_display (renderer, xdpy);
  if (!cogl_renderer_connect (renderer, &error))
    {
      fprintf (stderr, "Failed to connect to a renderer: %s\n",
               error->message);
    }

  chain = cogl_swap_chain_new ();
  cogl_swap_chain_set_has_alpha (chain, TRUE);

  /* Conceptually declare upfront the kinds of windows we anticipate
   * creating so that when we configure the display pipeline we can avoid
   * having an impedance miss-match between the format of windows and the
   * format the display pipeline expects. */
  onscreen_template = cogl_onscreen_template_new (chain);
  cogl_object_unref (chain);

  /* Conceptually setup a display pipeline */
  display = cogl_display_new (renderer, onscreen_template);
  cogl_object_unref (renderer);
  if (!cogl_display_setup (display, &error))
    {
      fprintf (stderr, "Failed to setup a display pipeline: %s\n",
               error->message);
      return 1;
    }

  ctx = cogl_context_new (display, &error);
  if (!ctx)
    {
      fprintf (stderr, "Failed to create context: %s\n", error->message);
      return 1;
    }

  onscreen = cogl_onscreen_new (ctx, 640, 480);

  /* We want to test that Cogl can handle foreign X windows... */

  visual = cogl_x11_onscreen_get_visual_xid (onscreen);
  if (!visual)
    {
      fprintf (stderr, "Failed to query an X visual suitable for the "
               "configured CoglOnscreen framebuffer\n");
      return 1;
    }

  template.visualid = visual;
  xvisinfo = XGetVisualInfo (xdpy, VisualIDMask, &template, &visinfos_count);

  /* window attributes */
  xattr.background_pixel = WhitePixel (xdpy, DefaultScreen (xdpy));
  xattr.border_pixel = 0;
  xattr.colormap = XCreateColormap (xdpy,
                                    DefaultRootWindow (xdpy),
                                    xvisinfo->visual,
                                    AllocNone);
  mask = CWBorderPixel | CWColormap;

  xwin = XCreateWindow (xdpy,
                        DefaultRootWindow (xdpy),
                        0, 0,
                        800, 600,
                        0,
                        xvisinfo->depth,
                        InputOutput,
                        xvisinfo->visual,
                        mask, &xattr);

  XFree (xvisinfo);

  cogl_x11_onscreen_set_foreign_window_xid (onscreen, xwin,
                                            update_cogl_x11_event_mask,
                                            xdpy);

  XMapWindow (xdpy, xwin);

  XCompositeRedirectSubwindows (xdpy, xwin, CompositeRedirectManual);

  screen = DefaultScreen (xdpy);
  tfp_xwin = XCreateSimpleWindow (xdpy, xwin,
				  0, 0, TFP_XWIN_WIDTH, TFP_XWIN_HEIGHT,
				  0,
				  WhitePixel (xdpy, screen),
				  WhitePixel (xdpy, screen));
  XMapWindow (xdpy, tfp_xwin);

  gc = XCreateGC (xdpy, tfp_xwin, 0, NULL);

  pixmap = XCompositeNameWindowPixmap (xdpy, tfp_xwin);

  tfp = cogl_texture_pixmap_x11_new (ctx, pixmap, TRUE, &error);
  if (!tfp)
    {
      fprintf (stderr, "Failed to create CoglTexturePixmapX11: %s",
               error->message);
      return 1;
    }

  fb = onscreen;

  for (;;)
    {
      unsigned long pixel;
      CoglPipeline *pipeline;

      while (XPending (xdpy))
        {
          XEvent event;
          KeySym keysym;
          XNextEvent (xdpy, &event);
          switch (event.type)
            {
            case KeyRelease:
              keysym = XLookupKeysym (&event.xkey, 0);
              if (keysym == XK_q || keysym == XK_Q || keysym == XK_Escape)
                return 0;
            }
          cogl_xlib_renderer_handle_event (renderer, &event);
        }

      pixel =
        g_random_int_range (0, 255) << 24 |
        g_random_int_range (0, 255) << 16 |
        g_random_int_range (0, 255) << 8;
        g_random_int_range (0, 255);
      XSetForeground (xdpy, gc, pixel);
      XFillRectangle (xdpy, tfp_xwin, gc, 0, 0, TFP_XWIN_WIDTH, TFP_XWIN_HEIGHT);
      XFlush (xdpy);

      cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);
      pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_layer_texture (pipeline, 0, tfp);
      cogl_framebuffer_draw_rectangle (fb, pipeline, -0.8, 0.8, 0.8, -0.8);
      cogl_object_unref (pipeline);
      cogl_onscreen_swap_buffers (onscreen);
    }

  return 0;
}
