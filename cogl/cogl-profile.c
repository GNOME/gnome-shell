
#ifdef COGL_ENABLE_PROFILE

#include "cogl-profile.h"

#include <stdlib.h>

UProfContext *_cogl_uprof_context;


static void __attribute__ ((constructor))
cogl_uprof_constructor (void)
{
  _cogl_uprof_context = uprof_context_new ("Cogl");
}

static void __attribute__ ((destructor))
cogl_uprof_destructor (void)
{
  if (getenv ("COGL_PROFILE_OUTPUT_REPORT"))
    {
      UProfReport *report = uprof_report_new ("Cogl report");
      uprof_report_add_context (report, _cogl_uprof_context);
      uprof_report_print (report);
      uprof_report_unref (report);
    }
  uprof_context_unref (_cogl_uprof_context);
}

#endif
