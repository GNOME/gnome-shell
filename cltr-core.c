#include "cltr-core.h"
#include "cltr-private.h"

int
cltr_init(int *argc, char ***argv)
{
  int  gl_attributes[] =
    {
      GLX_RGBA, 
      GLX_DOUBLEBUFFER,
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      0
    };

  XVisualInfo	       *vinfo;  


  g_thread_init (NULL);
  // XInitThreads ();

  if ((CltrCntx.xdpy = XOpenDisplay(getenv("DISPLAY"))) == NULL)
    {
      return 0;
    }

  CltrCntx.xscreen   = DefaultScreen(CltrCntx.xdpy);
  CltrCntx.xwin_root = RootWindow(CltrCntx.xdpy, CltrCntx.xscreen);

  if ((vinfo = glXChooseVisual(CltrCntx.xdpy, 
			       CltrCntx.xscreen,
			       gl_attributes)) == NULL)
    {
      fprintf(stderr, "Unable to find visual\n");
      return 0;
    }

  CltrCntx.gl_context = glXCreateContext(CltrCntx.xdpy, vinfo, 0, True);

  cltr_events_init();

  return 1;
}
