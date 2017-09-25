#ifndef __CLUTTER_SETTINGS_X11_H__
#define __CLUTTER_SETTINGS_X11_H__

/* XSETTINGS key names to ClutterSettings properties */
static const struct {
  const char *xsetting_name;
  const char *settings_property;
} _clutter_settings_map[] = {
  { "Net/DoubleClickDistance", "double-click-distance" },
  { "Net/DndDragThreshold",    "dnd-drag-threshold" },
  { "Gtk/FontName",            "font-name" },
  { "Xft/Antialias",           "font-antialias" },
  { "Xft/Hinting",             "font-hinting" },
  { "Xft/HintStyle",           "font-hint-style" },
  { "Xft/RGBA",                "font-subpixel-order" },
  { "Fontconfig/Timestamp",    "fontconfig-timestamp" },
};

static const gint _n_clutter_settings_map = G_N_ELEMENTS (_clutter_settings_map);

#define CLUTTER_SETTING_X11_NAME(id)    (_clutter_settings_map[(id)].xsetting_name)
#define CLUTTER_SETTING_PROPERTY(id)    (_clutter_settings_map[(id)].settings_property)

#endif /* __CLUTTER_SETTINGS_X11_H__ */
