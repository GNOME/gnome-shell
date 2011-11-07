#ifndef __CLUTTER_SETTINGS_GDK_H__
#define __CLUTTER_SETTINGS_GDK_H__

/* XSETTINGS key names to ClutterSettings properties */
static const struct {
  const char *gdk_setting_name;
  const char *settings_property;
  GType       type;
} _clutter_settings_map[] = {
  { "gtk-double-click-time",     "double-click-time",     G_TYPE_INT },
  { "gtk-double-click-distance", "double-click-distance", G_TYPE_INT },
  { "gtk-dnd-drag-threshold",    "dnd-drag-threshold",    G_TYPE_INT },
  { "gtk-font-name",             "font-name",             G_TYPE_STRING },
  { "gtk-xft-antialias",         "font-antialias",        G_TYPE_INT },
  { "gtk-xft-dpi",               "font-dpi",              G_TYPE_INT },
  { "gtk-xft-hinting",           "font-hinting",          G_TYPE_INT },
  { "gtk-xft-hintstyle",         "font-hint-style",       G_TYPE_STRING },
  { "gtk-xft-rgba",              "font-subpixel-order",   G_TYPE_STRING },
  { "gtk-fontconfig-timestamp",  "fontconfig-timestamp",  G_TYPE_UINT },
};

static const gint _n_clutter_settings_map = G_N_ELEMENTS (_clutter_settings_map);

#define CLUTTER_SETTING_TYPE(id)        (_clutter_settings_map[(id)].type)
#define CLUTTER_SETTING_GDK_NAME(id)    (_clutter_settings_map[(id)].gdk_setting_name)
#define CLUTTER_SETTING_PROPERTY(id)    (_clutter_settings_map[(id)].settings_property)

#endif /* __CLUTTER_SETTINGS_GDK_H__ */
