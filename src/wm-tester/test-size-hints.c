#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <glib.h>

static Bool
all_events (Display  *display,
            XEvent   *event,
            XPointer  arg)
{
  return True;
}

#if 0
static void
get_size (Display *d, Drawable draw,
          int *xp, int *yp, int *widthp, int *heightp)
{
  int x, y;
  unsigned int width, height, border, depth;
  Window root;
  
  XGetGeometry (d, draw, &root, &x, &y, &width, &height, &border, &depth);

  if (xp)
    *xp = x;
  if (yp)
    *yp = y;
  if (widthp)
    *widthp = width;
  if (*heightp)
    *heightp = height;
}
#endif

int
main (int argc, char **argv)
{
  Display *d;
  Window zero_min_size;
  XSizeHints hints;
  int screen;
  XEvent ev;
  int x, y, width, height;
  Pixmap pix;
  GC gc;  
  XGCValues gc_vals;
  gboolean redraw_pending;
  
  d = XOpenDisplay (NULL);

  screen = DefaultScreen (d);

  x = 0;
  y = 0;
  width = 100;
  height = 100;
  
  zero_min_size = XCreateSimpleWindow (d, RootWindow (d, screen), 
                                       x, y, width, height, 0,
                                       WhitePixel (d, screen),
                                       WhitePixel (d, screen));  
  
  XSelectInput (d, zero_min_size,
                ButtonPressMask | ExposureMask | StructureNotifyMask);
  
  hints.flags = PMinSize;
  
  hints.min_width = 0;
  hints.min_height = 0;
  
  XSetWMNormalHints (d, zero_min_size, &hints);
  XMapWindow (d, zero_min_size);

  redraw_pending = FALSE;
  while (1)
    {
      XNextEvent (d, &ev);
      
      switch (ev.xany.type)
        {
        case ButtonPress:
          if (ev.xbutton.button == 1)
            {
              g_print ("Exiting on button 1 press\n");
              exit (0);
            }
          break;

        case ConfigureNotify:
          x = ev.xconfigure.x;
          y = ev.xconfigure.y;
          width = ev.xconfigure.width;
          height = ev.xconfigure.height;

          redraw_pending = TRUE;
          break;
          
        case Expose:
          redraw_pending = TRUE;
          break;
          
        default:
          break;
        }

      /* Primitive event compression */
      if (XCheckIfEvent (d, &ev, all_events, NULL))
        {
          XPutBackEvent (d, &ev);
        }
      else if (redraw_pending)
        {
          pix = XCreatePixmap (d, zero_min_size, width, height,
                               DefaultDepth (d, screen));
          
          gc_vals.foreground = WhitePixel (d, screen);
          
          gc = XCreateGC (d, pix, GCForeground, &gc_vals);
          
          XFillRectangle (d, pix, gc, 0, 0, width, height);
          
          XCopyArea (d, pix, zero_min_size, gc, 0, 0, width, height, 0, 0);
          
          XFreePixmap (d, pix);
          XFreeGC (d, gc);

          redraw_pending = FALSE;
        }
    }

  /* This program has an infinite loop above so a return statement would
   * just cause compiler warnings.
   */
}

