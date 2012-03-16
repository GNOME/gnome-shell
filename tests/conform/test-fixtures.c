
#include <clutter/clutter.h>
#include <cogl/cogl.h>

void
test_simple_rig (void)
{
  ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };
  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
}
