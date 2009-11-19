#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <glib.h>

static void
calc_rects (XRectangle *rects, int width, int height)
{
  int w = (width - 21) / 3;
  int h = (height - 21) / 3;
  int i;

  i = 0;
  while (i < 9)
    {
      rects[i].width = w;
      rects[i].height = h;
      ++i;
    }
  
  /* NW */
  rects[0].x = 0;
  rects[0].y = 0;

  /* N */
  rects[1].x = width / 2 - w / 2;
  rects[1].y = 0;

  /* NE */
  rects[2].x = width - w;
  rects[2].y = 0;

  /* E */
  rects[3].x = width - w;
  rects[3].y = height / 2 - h / 2;

  /* SE */
  rects[4].x = width - w;
  rects[4].y = height - h;

  /* S */
  rects[5].x = width / 2 - w / 2;
  rects[5].y = height - h;

  /* SW */
  rects[6].x = 0;
  rects[6].y = height - h;

  /* W */
  rects[7].x = 0;
  rects[7].y = height / 2 - h / 2;

  /* Center */
  rects[8].x = width / 2 - w / 2;
  rects[8].y = height / 2 - h / 2;
}

static Bool
all_events (Display  *display,
            XEvent   *event,
            XPointer  arg)
{
  return True;
}

static void
get_size (Display *d, Drawable draw,
          int *xp, int *yp, int *widthp, int *heightp)
{
  int x, y;
  unsigned int width=0, height=0, border=0, depth=0;
  Window root;
  
  XGetGeometry (d, draw, &root, &x, &y, &width, &height, &border, &depth);

  if (xp)
    *xp = x;
  if (yp)
    *yp = y;
  if (widthp)
    *widthp = width;
  if (heightp)
    *heightp = height;
}

int
main (int argc, char **argv)
{
  Display *d;
  Window w, cw;
  XSizeHints hints;
  int screen;
  XEvent ev;
  int x, y, width, height;
  Pixmap pix;
  GC gc;  
  XGCValues gc_vals;
  XSetWindowAttributes set_attrs;
  XWindowChanges changes;
  XRectangle rects[9];
  gboolean redraw_pending;
  unsigned int mask;
  
  d = XOpenDisplay (NULL);

  screen = DefaultScreen (d);

  /* Print some debug spew to show how StaticGravity works */     
  w = XCreateSimpleWindow (d, RootWindow (d, screen), 
                           0, 0, 100, 100, 0,
                           WhitePixel (d, screen),
                           WhitePixel (d, screen));
  cw = XCreateSimpleWindow (d, w,
                            0, 0, 100, 100, 0,
                            WhitePixel (d, screen),
                            WhitePixel (d, screen));
  set_attrs.win_gravity = StaticGravity;
      
  XChangeWindowAttributes (d, cw,
                           CWWinGravity,
                           &set_attrs);
  
  get_size (d, w, &x, &y, &width, &height);
  
  g_print ("Parent is %d,%d  %d x %d before configuring parent\n",
           x, y, width, height);
  
  get_size (d, cw, &x, &y, &width, &height);

  g_print ("Child is %d,%d  %d x %d before configuring parent\n",
           x, y, width, height);
  
  changes.x = 10;
  changes.y = 10;
  changes.width = 110;
  changes.height = 110;
  /* last mask wins */
  mask = CWX | CWY;
  mask = CWWidth | CWHeight;
  mask = CWX | CWY | CWWidth | CWHeight;
  
  XConfigureWindow (d, w, mask, &changes);
  XSync (d, False);

  get_size (d, w, &x, &y, &width, &height);

  g_print ("Parent is %d,%d  %d x %d after configuring parent\n",
           x, y, width, height);
  
  get_size (d, cw, &x, &y, &width, &height);

  g_print ("Child is %d,%d  %d x %d after configuring parent\n",
           x, y, width, height);  

  XDestroyWindow (d, w);
  
  /* The window that gets displayed */
  
  x = 20;
  y = 20;
  width = 100;
  height = 100;

  calc_rects (rects, width, height);
  
  w = XCreateSimpleWindow (d, RootWindow (d, screen), 
                           x, y, width, height, 0,
                           WhitePixel (d, screen),
                           WhitePixel (d, screen));
  
  set_attrs.bit_gravity = StaticGravity;
      
  XChangeWindowAttributes (d, w,
                           CWBitGravity,
                           &set_attrs);
  
  XSelectInput (d, w,
                ButtonPressMask | ExposureMask | StructureNotifyMask);
  
  hints.flags = PMinSize;
  
  hints.min_width = 100;
  hints.min_height = 100;
  
  XSetWMNormalHints (d, w, &hints);
  XMapWindow (d, w);

  redraw_pending = FALSE;
  while (1)
    {
      XNextEvent (d, &ev);
      
      switch (ev.xany.type)
        {
        case ButtonPress:
          if (ev.xbutton.button == 3)
            {
              g_print ("Exiting on button 3 press\n");
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
          calc_rects (rects, width, height);
          
          pix = XCreatePixmap (d, w, width, height,
                               DefaultDepth (d, screen));
          
          gc_vals.foreground = WhitePixel (d, screen);
          
          gc = XCreateGC (d, pix, GCForeground, &gc_vals);
          
          XFillRectangle (d, pix, gc, 0, 0, width, height);
              
          /* Draw rectangles at each gravity point */
          gc_vals.foreground = BlackPixel (d, screen);
          XChangeGC (d, gc, GCForeground, &gc_vals);
          
          XFillRectangles (d, pix, gc, rects, G_N_ELEMENTS (rects));
          
          XCopyArea (d, pix, w, gc, 0, 0, width, height, 0, 0);
          
          XFreePixmap (d, pix);
          XFreeGC (d, gc);

          redraw_pending = FALSE;
        }
    }

  /* This program has an infinite loop above so a return statement would
   * just cause compiler warnings.
   */
}

