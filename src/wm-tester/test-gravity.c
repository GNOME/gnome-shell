#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>

int gravities[10] = {
  NorthWestGravity, 
  NorthGravity,  
  NorthEastGravity,
  WestGravity,      
  CenterGravity, 
  EastGravity,
  SouthWestGravity,
  SouthGravity,
  SouthEastGravity,
  StaticGravity
};

typedef struct
{
  int x, y, width, height;
} Rectangle;

Window windows[10];
int doubled[10] = { 0, };
Rectangle window_rects[10];

#define WINDOW_WIDTH 100
#define WINDOW_HEIGHT 100

int x_offset[3] = { 0, - WINDOW_WIDTH/2,  -WINDOW_WIDTH };
int y_offset[3] = { 0, - WINDOW_HEIGHT/2,  -WINDOW_HEIGHT };
double screen_x_fraction[3] = { 0, 0.5, 1.0 };
double screen_y_fraction[3] = { 0, 0.5, 1.0 };
int screen_width;
int screen_height;

static const char*
window_gravity_to_string (int gravity)
{
  switch (gravity)
    {
    case NorthWestGravity:
      return "NorthWestGravity";
      break;
    case NorthGravity:
      return "NorthGravity";
      break;
    case NorthEastGravity:
      return "NorthEastGravity";
      break;
    case WestGravity:
      return "WestGravity";
      break;
    case CenterGravity:
      return "CenterGravity";
      break;
    case EastGravity:
      return "EastGravity";
      break;
    case SouthWestGravity:
      return "SouthWestGravity";
      break;
    case SouthGravity:
      return "SouthGravity";
      break;
    case SouthEastGravity:
      return "SouthEastGravity";
      break;
    case StaticGravity:
      return "StaticGravity";
      break;
    default:
      return "NorthWestGravity";
      break;
    }
}

static void
calculate_position (int i, int doubled, int *x, int *y)
{
  if (i == 9)
    {
      *x = 150;
      *y = 150;
    }
  else 
    {
      int xoff = x_offset[i % 3];
      int yoff = y_offset[i / 3];
      if (doubled)
        {
          xoff *= 2;
          yoff *= 2;
        }
      
      *x = screen_x_fraction[i % 3] * screen_width + xoff;
      *y = screen_y_fraction[i / 3] * screen_height + yoff;
    }
}

static int
find_window (Window window)
{
  int i;
  for (i=0; i<10; i++)
    {
      if (windows[i] == window)
        return i;
    }

  return -1;
}

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
} MotifWmHints, MwmHints;

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

int main (int argc, char **argv)
{
  Display *d;
  Window w;
  XSizeHints hints;
  int i;
  int screen;
  XEvent ev;
  int noframes;
  
  if (argc > 1 && strcmp (argv[1], "--noframes") == 0)
    noframes = 1;
  else
    noframes = 0;
  
  d = XOpenDisplay (NULL);

  screen = DefaultScreen (d);
  screen_width = DisplayWidth (d, screen);
  screen_height = DisplayHeight (d, screen);

  for (i=0; i<10; i++)
    {
      int x, y;
      
      calculate_position (i, doubled[i], &x, &y);

      w = XCreateSimpleWindow (d, RootWindow (d, screen), 
                               x, y, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 
                               WhitePixel (d, screen), WhitePixel (d, screen));

      windows[i] = w;
      window_rects[i].x = x;
      window_rects[i].y = y;
      window_rects[i].width = WINDOW_WIDTH;
      window_rects[i].height = WINDOW_HEIGHT;
      
      XSelectInput (d, w, ButtonPressMask | ExposureMask | StructureNotifyMask);
      
      hints.flags = USPosition | PMinSize | PMaxSize | PWinGravity;
      
      hints.min_width = WINDOW_WIDTH / 2;
      hints.min_height = WINDOW_HEIGHT / 2;

#if 1
      /* we constrain max size below the "doubled" size so that
       * the WM will have to deal with constraints
       * at the same time it's dealing with configure request
       */
      hints.max_width = WINDOW_WIDTH * 2 - WINDOW_WIDTH / 2;
      hints.max_height = WINDOW_HEIGHT * 2 - WINDOW_HEIGHT / 2;
#else
      hints.max_width = WINDOW_WIDTH * 2 + WINDOW_WIDTH / 2;
      hints.max_height = WINDOW_HEIGHT * 2 + WINDOW_HEIGHT / 2;
#endif
      hints.win_gravity = gravities[i];
      
      XSetWMNormalHints (d, w, &hints);

      XStoreName (d, w, window_gravity_to_string (hints.win_gravity));

      if (noframes)
        {
          MotifWmHints mwm;
          Atom mwm_atom;
          
          mwm.decorations = 0;
          mwm.flags = MWM_HINTS_DECORATIONS;
          
          mwm_atom = XInternAtom (d, "_MOTIF_WM_HINTS", False);

          XChangeProperty (d, w, mwm_atom, mwm_atom,
                           32, PropModeReplace,
                           (unsigned char *)&mwm,
                           sizeof (MotifWmHints)/sizeof (long));
        }
      
      XMapWindow (d, w);
    }

  while (1)
    {
      XNextEvent (d, &ev);

      if (ev.xany.type == ConfigureNotify)
        {
          i = find_window (ev.xconfigure.window);
          
          if (i >= 0)
            {
              Window ignored;
              
              window_rects[i].width = ev.xconfigure.width;
              window_rects[i].height = ev.xconfigure.height;
              
              XClearArea (d, windows[i], 0, 0,
                          ev.xconfigure.width,
                          ev.xconfigure.height,
                          True);

              if (!ev.xconfigure.send_event)
                XTranslateCoordinates (d, windows[i], DefaultRootWindow (d),
                                       0, 0,
                                       &window_rects[i].x, &window_rects[i].y,
                                       &ignored);
              else
                {
                  window_rects[i].x = ev.xconfigure.x;
                  window_rects[i].y = ev.xconfigure.y;
                }
            }
        }
      else if (ev.xany.type == Expose)
        {          
          i = find_window (ev.xexpose.window);
          
          if (i >= 0)
            {
              GC gc;
              XGCValues values;
              char buf[256];
              
              values.foreground = BlackPixel (d, screen);
              
              gc = XCreateGC (d, windows[i],
                              GCForeground, &values);

              sprintf (buf,
                       "%d,%d",
                       window_rects[i].x,
                       window_rects[i].y);
              
              XDrawString (d, windows[i], gc, 10, 15,
                           buf, strlen (buf));

              sprintf (buf,
                       "%dx%d",
                       window_rects[i].width,
                       window_rects[i].height);
              
              XDrawString (d, windows[i], gc, 10, 35,
                           buf, strlen (buf));
              
              XFreeGC (d, gc);
            }
        }
      else if (ev.xany.type == ButtonPress)
	{
          i = find_window (ev.xbutton.window);

          if (i >= 0)
            {
              /* Button 1 = move, 2 = resize, 3 = both at once */
                  
              if (ev.xbutton.button == Button1) 
                { 
                  int x, y;
		      
                  calculate_position (i, doubled[i], &x, &y);
                  XMoveWindow (d, windows[i], x, y); 
                }
              else if (ev.xbutton.button == Button2)
                {
                  if (doubled[i])
                    XResizeWindow (d, windows[i], WINDOW_WIDTH, WINDOW_HEIGHT);
                  else
                    XResizeWindow (d, windows[i], WINDOW_WIDTH*2, WINDOW_HEIGHT*2);

                  doubled[i] = !doubled[i];
                }
              else if (ev.xbutton.button == Button3)
                {
                  int x, y;
		      
                  calculate_position (i, !doubled[i], &x, &y);

                  if (doubled[i])
                    XMoveResizeWindow (d, windows[i], x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
                  else
                    XMoveResizeWindow (d, windows[i], x, y, WINDOW_WIDTH*2, WINDOW_HEIGHT*2);

                  doubled[i] = !doubled[i];
                }
            }
	}
    }
  
  return 0;
}

