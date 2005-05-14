#include "cltr-core.h"
#include "cltr-private.h"

int
cltr_init(int *argc, char ***argv)
{

#define GLX_SAMPLE_BUFFERS_ARB             100000
#define GLX_SAMPLES_ARB                    100001


  int  gl_attributes[] =
    {
      GLX_RGBA, 
      GLX_DOUBLEBUFFER,
      GLX_STENCIL_SIZE, 1, 
      GLX_DEPTH_SIZE, 24,

      /*
      GLX_SAMPLE_BUFFERS_ARB, 1, 
      GLX_SAMPLES_ARB, 0,

      */
      /*
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      */
      0
    };

  XVisualInfo	       *vinfo;  

  gst_init (argc, argv);

  if (!g_thread_supported ())
    g_thread_init (NULL);

  /* XInitThreads (); */

  if ((CltrCntx.xdpy = XOpenDisplay(getenv("DISPLAY"))) == NULL)
    {
      return 0;
    }

  CltrCntx.xscreen   = DefaultScreen(CltrCntx.xdpy);
  CltrCntx.xwin_root = RootWindow(CltrCntx.xdpy, CltrCntx.xscreen);

  CLTR_DBG("EXT : %s", glXQueryExtensionsString( CltrCntx.xdpy, 
					      CltrCntx.xscreen));


  if ((vinfo = glXChooseVisual(CltrCntx.xdpy, 
			       CltrCntx.xscreen,
			       gl_attributes)) == NULL)
    {
      fprintf(stderr, "Unable to find visual\n");
      return 0;
    }

  CltrCntx.gl_context = glXCreateContext(CltrCntx.xdpy, vinfo, 0, True);

  pixel_set_vals(&CltrCntx.colors[CLTR_COL_BG], 0xff, 0xff, 0xff, 0xff );
  pixel_set_vals(&CltrCntx.colors[CLTR_COL_BDR],0xff, 0xff, 0xff, 0xff );
  pixel_set_vals(&CltrCntx.colors[CLTR_COL_FG], 0xff, 0xff, 0xff, 0xff );

  cltr_events_init();

  return 1;
}

int 
cltr_display_width(void)
{
  ClutterMainContext *ctx = CLTR_CONTEXT();

  return DisplayWidth(ctx->xdpy, ctx->xscreen);
}

int 
cltr_display_height(void)
{
  ClutterMainContext *ctx = CLTR_CONTEXT();

  return DisplayHeight(ctx->xdpy, ctx->xscreen);
}

PixbufPixel*
cltr_core_get_color(CltrNamedColor col)
{
  ClutterMainContext *ctx = CLTR_CONTEXT();

  return &ctx->colors[col];
}
