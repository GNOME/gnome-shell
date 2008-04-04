#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#include "clutter-actor.h"
#include "clutter-stage-window.h"
#include "clutter-private.h"

GType
clutter_stage_window_get_type (void)
{
  static GType stage_window_type = 0;

  if (G_UNLIKELY (stage_window_type == 0))
    {
      const GTypeInfo stage_window_info = {
        sizeof (ClutterStageWindowIface),
        NULL,
        NULL,
      };

      stage_window_type =
        g_type_register_static (G_TYPE_INTERFACE, I_("ClutterStageWindow"),
                                &stage_window_info, 0);

      g_type_interface_add_prerequisite (stage_window_type,
                                         CLUTTER_TYPE_ACTOR);
    }

  return stage_window_type;
}
