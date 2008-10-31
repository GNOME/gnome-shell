/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <mutter-plugin.h>

void
mutter_plugin_query_screen_size (MutterPlugin *plugin,
                                 int          *width,
                                 int          *height)
{
}

ClutterActor *
mutter_plugin_get_overlay_group (MutterPlugin *plugin)
{
  return NULL;
}

ClutterActor *
mutter_plugin_get_stage (MutterPlugin *plugin)
{
  return NULL;
}
