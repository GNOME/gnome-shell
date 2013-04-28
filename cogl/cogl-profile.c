#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef COGL_ENABLE_PROFILE

#include "cogl-profile.h"
#include "cogl-debug.h"
#include "cogl-i18n-private.h"

#include <stdlib.h>

UProfContext *_cogl_uprof_context;

static CoglBool
debug_option_getter (void *user_data)
{
  unsigned int shift = GPOINTER_TO_UINT (user_data);
  return COGL_DEBUG_ENABLED (shift);
}

static void
debug_option_setter (CoglBool value, void *user_data)
{
  unsigned int shift = GPOINTER_TO_UINT (user_data);

  if (value)
    COGL_DEBUG_SET_FLAG (shift);
  else
    COGL_DEBUG_CLEAR_FLAG (shift);
}

static void
print_exit_report (void)
{
  if (getenv ("COGL_PROFILE_OUTPUT_REPORT"))
    {
      UProfContext *mainloop_context;
      UProfTimerResult *mainloop_timer;
      UProfReport *report;

      /* NB: uprof provides a shared context for mainloop statistics
       * which needs to be setup by the application which controls the
       * mainloop.
       *
       * If no "Mainloop" timer has been setup then we print a warning
       * since we can't provide a meaningful Cogl report without one.
       */
      mainloop_context = uprof_get_mainloop_context ();
      mainloop_timer = uprof_context_get_timer_result (mainloop_context,
                                                       "Mainloop");
      /* just bail out if the mainloop timer wasn't hit */
      if (!mainloop_timer)
        {
          g_warning ("\n\n"
                     "No UProf \"Mainloop\" timer was setup by the "
                     "application therefore we\ncan't provide a meaningful "
                     "profile report.\n"
                     "\n"
                     "This should be done automatically if you are using Clutter "
                     "(if\nbuilt with --enable-profile)\n"
                     "\n"
                     "If you aren't using Clutter then you can declare a "
                     "\"Mainloop\" UProf\ntimer in your application like this:\n\n"
                     "  UPROF_STATIC_TIMER (mainloop_timer, \n"
                     "                      NULL,\n"
                     "                      \"Mainloop\",\n"
                     "                      \"Time in glib mainloop\",\n"
                     "                      0);\n"
                     "\n"
                     "And start/stop it around your mainloop like this:\n"
                     "\n"
                     "  UPROF_TIMER_START (uprof_get_mainloop_context (), mainloop_timer);\n"
                     "  g_main_loop_run (loop);\n"
                     "  UPROF_TIMER_STOP (uprof_get_mainloop_context (), mainloop_timer);\n");
          return;
        }

      report = uprof_report_new ("Cogl report");
      uprof_report_add_context (report, _cogl_uprof_context);
      uprof_report_print (report);
      uprof_report_unref (report);
    }
  uprof_context_unref (_cogl_uprof_context);
}

void
_cogl_uprof_init (void)
{
  _cogl_uprof_context = uprof_context_new ("Cogl");
  uprof_context_link (_cogl_uprof_context, uprof_get_mainloop_context ());
#define OPT(MASK_NAME, GROUP, NAME, NAME_FORMATTED, DESCRIPTION) \
  G_STMT_START { \
    int shift = COGL_DEBUG_ ## MASK_NAME; \
    uprof_context_add_boolean_option (_cogl_uprof_context, \
                                      g_dgettext (GETTEXT_PACKAGE, GROUP), \
                                      NAME, \
                                      g_dgettext (GETTEXT_PACKAGE, \
                                                  NAME_FORMATTED), \
                                      g_dgettext (GETTEXT_PACKAGE, \
                                                  DESCRIPTION),    \
                                      debug_option_getter, \
                                      debug_option_setter, \
                                      GUINT_TO_POINTER (shift)); \
  } G_STMT_END;

#include "cogl-debug-options.h"
#undef OPT

  atexit (print_exit_report);
}

void
_cogl_profile_trace_message (const char *format, ...)
{
  va_list ap;

  va_start (ap, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, format, ap);
  va_end (ap);

  if (_cogl_uprof_context)
    uprof_context_vtrace_message (_cogl_uprof_context, format, ap);
}

#endif
