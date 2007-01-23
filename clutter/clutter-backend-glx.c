#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <glib.h>

#include "clutter-main.h"

static Display   *_xdpy = NULL;
static Window     _xwin_root;
static int        _xscreen;

static gchar *clutter_display_name     = NULL;
static int    clutter_screen           = 0;

static int TrappedErrorCode = 0;
static int (*old_error_handler) (Display *, XErrorEvent *);

static int
error_handler(Display     *xdpy,
	      XErrorEvent *error)
{
  TrappedErrorCode = error->error_code;
  return 0;
}

/**
 * clutter_util_trap_x_errors:
 *
 * Trap X errors so they don't cause an abort.
 */
void
clutter_glx_trap_x_errors(void)
{
  TrappedErrorCode  = 0;
  old_error_handler = XSetErrorHandler(error_handler);
}

/**
 * clutter_util_untrap_x_errors:
 *
 * Stop trapping X errors.
 *
 * Return value: 0 if there was no error, or the last X error that occurred.
 */
int
clutter_glx_untrap_x_errors(void)
{
  XSetErrorHandler(old_error_handler);
  return TrappedErrorCode;
}


/**
 * clutter_glx_display:
 *
 * Retrieves the X display that Clutter is using
 *
 * Return value: A pointer to an X Display structure.
 */
Display*
clutter_glx_display (void)
{
  return _xdpy;
}

/**
 * clutter_glx_screen:
 *
 * Retrieves the X screen that Clutter is using.
 *
 * Return value: the X screen ID
 */
int
clutter_glx_screen (void)
{
  return _xscreen;
}

/**
 * clutter_glx_root_window:
 *
 * FIXME
 *
 * Return value: FIXME
 */
Window
clutter_glx_root_window (void)
{
  return _xwin_root;
}

static GOptionEntry clutter_glx_args[] = {
  { "display", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &clutter_display_name,
    "X display to use", "DISPLAY" },
  { "screen", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &clutter_screen,
    "X screen to use", "SCREEN" },
  { NULL, }
};


static gboolean
pre_parse_hook (GOptionContext  *context,
                GOptionGroup    *group,
                gpointer         data,
                GError         **error)
{
  const char *env_string;

  env_string = g_getenv ("DISPLAY");
  if (env_string)
    {
      clutter_display_name = g_strdup (env_string);
      env_string = NULL;
    }

  return TRUE;
}

static gboolean
post_parse_hook (GOptionContext  *context,
                 GOptionGroup    *group,
                 gpointer         data,
                 GError         **error)
{
  _xdpy = XOpenDisplay (clutter_display_name);

  if (_xdpy)
    {
      if (clutter_screen == 0)
        _xscreen = DefaultScreen (_xdpy);
      else
        {
          Screen *xscreen;

          xscreen  = ScreenOfDisplay (_xdpy, clutter_screen);
          _xscreen = XScreenNumberOfScreen (xscreen);
        }

      _xwin_root = RootWindow (_xdpy, _xscreen);

      /* we don't need it anymore */
      g_free (clutter_display_name);
    }
  else
    {
      g_set_error (error, 
		   clutter_init_error_quark (),
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to connect to X Server DISPLAY.");
      return FALSE;
    }

  return TRUE;
}

gboolean
clutter_backend_init (GOptionContext *context) 
{
  GOptionGroup   *group;

  group = g_option_group_new ("clutter-glx",
                              "Clutter GLX Options",
                              "Show Clutter GLX Options",
                              NULL,
                              NULL);

  g_option_group_set_parse_hooks (group, pre_parse_hook, post_parse_hook);
  g_option_group_add_entries (group, clutter_glx_args);
  g_option_context_add_group (context, group);

  return TRUE;
}
