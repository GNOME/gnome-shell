#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>

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

Window windows[10];

int x_offset[3] = { 0, -50,  -100 };
int y_offset[3] = { 0, -50,  -100 };
double screen_x_fraction[3] = { 0, 0.5, 1.0 };
double screen_y_fraction[3] = { 0, 0.5, 1.0 };
int screen_width;
int screen_height;

void calculate_position (int i, int *x, int *y)
{
  if (i == 9)
    {
      *x = 150;
      *y = 150;
    }
  else 
    {
      *x = screen_x_fraction[i % 3] * screen_width + x_offset[i % 3];
      *y = screen_y_fraction[i / 3] * screen_height + y_offset[i / 3];
    }
}

int main (int argc, char **argv)
{
  Display *d;
  Window w;
  XSizeHints hints;
  int i;
  int screen;
  XEvent ev;
  
  d = XOpenDisplay (NULL);

  screen = DefaultScreen (d);
  screen_width = DisplayWidth (d, screen);
  screen_height = DisplayHeight (d, screen);

  for (i=0; i<10; i++)
    {
      int x, y;
      
      calculate_position (i, &x, &y);

      w = XCreateSimpleWindow(d, RootWindow(d, screen), 
			      x, y, 100, 100, 0, 
			      WhitePixel(d, screen), WhitePixel(d, screen));

      windows[i] = w;

      XSelectInput (d, w, ButtonPressMask);
      
      hints.flags = USPosition | PMinSize | PMaxSize | PWinGravity;
      
      hints.min_width = 100;
      hints.min_height = 100;
      hints.max_width = 200;
      hints.max_height = 200;
      hints.win_gravity = gravities[i];
      
      XSetWMNormalHints (d, w, &hints);
      XMapWindow (d, w);
    }

  while (1)
    {
      XNextEvent (d, &ev);

      if (ev.xany.type == ButtonPress)
	{
	  for (i=0; i<10; i++)
	    {
	      if (windows[i] == ev.xbutton.window)
		{
		  if (ev.xbutton.button == Button1) 
		    { 
		      int x, y;
		      
		      calculate_position (i, &x, &y);
		      w = XMoveWindow (d, windows[i], x, y); 
		    }
		  else 
		    {
		      w = XResizeWindow (d, windows[i], 200, 200);
		    }
		  }
	      }
	}
    }
  
  return 0;
}

