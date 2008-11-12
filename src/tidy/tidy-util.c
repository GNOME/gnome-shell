#include "tidy-util.h"

/*  Hack to (mostly) fill glyph cache, useful on MBX.
 * 
 *  FIXME: untested 
*/
void
tidy_util_preload_glyphs (char *font, ...)
{
  va_list args;

  va_start (args, font);

  while (font)
    {
      /* Hold on to your hat.. */
      ClutterActor *foo;
      ClutterColor  text_color = { 0xff, 0xff, 0xff, 0xff };

      foo = clutter_label_new_full 
	                (font, 
			 "abcdefghijklmnopqrstuvwxyz"
			 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			 "1234567890&()*.,';:-_+=[]{}#@?><\"!`%\\|/ ", 
			 &text_color);
      if (foo)
	{
	  clutter_actor_realize(foo);
	  clutter_actor_paint(foo);
	  g_object_unref (foo);
	}

      font = va_arg (args, char*);
    }

  va_end (args);
}
