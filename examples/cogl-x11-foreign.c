#include <cogl/cogl.h>
#include <cogl/cogl-xlib.h>
#include <glib.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define X11_FOREIGN_EVENT_MASK \
  (KeyPressMask | \
   KeyReleaseMask | \
   ButtonPressMask | \
   ButtonReleaseMask | \
   PointerMotionMask)

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

static void
resize_handler (CoglOnscreen *onscreen,
                int width,
                int height,
                void *user_data)
{
  CoglFramebuffer *fb = user_data;
  cogl_framebuffer_set_viewport (fb, width / 4, height / 4, width / 2, height / 2);
}

int
main (int argc, char **argv)
{
  Display *xdpy;
  CoglRenderer *renderer;
  CoglSwapChain *chain;
  CoglOnscreenTemplate *onscreen_template;
  CoglDisplay *display;
  CoglContext *ctx;
  CoglOnscreen *onscreen;
  CoglFramebuffer *fb;
  CoglPipeline *pipeline;
  CoglError *error = NULL;
  uint32_t visual;
  XVisualInfo template, *xvisinfo;
  int visinfos_count;
  XSetWindowAttributes xattr;
  unsigned long mask;
  Window xwin;
  CoglVertexP2C4 triangle_vertices[] = {
      {0, 0.7, 0xff, 0x00, 0x00, 0xff},
      {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
      {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };
  CoglPrimitive *triangle;


  /* Since we want to test external ownership of the X display,
   * connect to X manually... */
  xdpy = XOpenDisplay (NULL);
  if (!xdpy)
    {
      fprintf (stderr, "Failed to open X Display\n");
      return 1;
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

  fb = COGL_FRAMEBUFFER (onscreen);

  cogl_onscreen_set_resizable (onscreen, TRUE);
  cogl_onscreen_add_resize_callback (onscreen, resize_handler, onscreen, NULL);

  triangle = cogl_primitive_new_p2c4 (ctx, COGL_VERTICES_MODE_TRIANGLES,
                                      3, triangle_vertices);
  pipeline = cogl_pipeline_new (ctx);
  for (;;)
    {
      CoglPollFD *poll_fds;
      int n_poll_fds;
      int64_t timeout;

      while (XPending (xdpy))
        {
          XEvent event;
          XNextEvent (xdpy, &event);
          switch (event.type)
            {
            case KeyRelease:
            case ButtonRelease:
              return 0;
            }
          cogl_xlib_renderer_handle_event (renderer, &event);
        }

      /* After forwarding native events directly to Cogl you should
       * then allow Cogl to dispatch any corresponding event
       * callbacks, such as resize notification callbacks...
       */
      cogl_poll_renderer_get_info (cogl_context_get_renderer (ctx),
                                   &poll_fds, &n_poll_fds, &timeout);
      g_poll ((GPollFD *) poll_fds, n_poll_fds, 0);
      cogl_poll_renderer_dispatch (cogl_context_get_renderer (ctx),
                                   poll_fds, n_poll_fds);

      cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);
      cogl_primitive_draw (triangle, fb, pipeline);
      cogl_onscreen_swap_buffers (onscreen);
    }

  return 0;
}
