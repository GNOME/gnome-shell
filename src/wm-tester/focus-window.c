#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char **argv)
{
  Display *d;
  Window w;
  const char *w_str;
  char *end;
  
  if (argc != 2)
    {
      fprintf (stderr, "Usage: focus-window WINDOWID\n");
      exit (1);
    }
  
  d = XOpenDisplay (NULL);

  w_str = argv[1];
  end = NULL;
  
  w = strtoul (w_str, &end, 16);
  if (end == w_str)
    {
      fprintf (stderr, "Usage: focus-window WINDOWID\n");
      exit (1);
    }

  printf ("Setting input focus to 0x%lx\n", w);
  XSetInputFocus (d, w, RevertToPointerRoot, CurrentTime);
  XFlush (d);
  
  return 0;
}

