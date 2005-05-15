#include <clutter/cltr.h>

int Paused = 0;

void
handle_xevent(CltrWidget *win, XEvent *xev, void *cookie)
{
  KeySym          kc;
  CltrVideo      *video = CLTR_VIDEO(cookie);

  if (xev->type == KeyPress)
    {
      XKeyEvent *xkeyev = &xev->xkey;

      kc = XKeycodeToKeysym(xkeyev->display, xkeyev->keycode, 0);

      switch (kc)
	{
	case XK_Return:
	  if (Paused)
	    cltr_video_play (video, NULL);
	  else
	    cltr_video_pause (video);
	  Paused ^= 1;

	  break;
	}
    }

}

int
main (int argc, char *argv[])
{
  CltrWidget *win, *video, *label;
  CltrFont   *font;
  PixbufPixel col = { 0x66, 0x00, 0x00, 0x99 };

  cltr_init (&argc, &argv);

  if (argc != 2) {
    g_print ("usage: %s <video filename>\n", argv[0]);
    exit (-1);
  }

  font = font_new("Baubau 72");

  win = cltr_window_new(800, 600);

  video = cltr_video_new(800, 600);

  cltr_video_set_source(CLTR_VIDEO(video), argv[1]);

  cltr_widget_add_child(win, video, 0, 0);  

  label = cltr_label_new("Clutter", font, &col);

  cltr_widget_add_child(win, label, 10, 400);  

  cltr_window_on_xevent(CLTR_WINDOW(win), handle_xevent, video);

  cltr_widget_show_all(win);

  cltr_video_play(CLTR_VIDEO(video), NULL);

  cltr_main_loop();

  exit (0);
}
