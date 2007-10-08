#ifndef __CLUTTER_SCRIPT_PRIVATE_H__
#define __CLUTTER_SCRIPT_PRIVATE_H__

#include <glib-object.h>
#include "clutter-script.h"

G_BEGIN_DECLS

typedef GType (* GTypeGetFunc) (void);

typedef struct {
  gchar *class_name;
  gchar *id;

  GList *properties;

  GType gtype;
  GObject *object;
} ObjectInfo;

typedef struct {
  gchar *property_name;
  GValue value;
} PropertyInfo;

GObject *clutter_script_construct_object (ClutterScript *script,
                                          ObjectInfo    *info);

G_END_DECLS

#endif /* __CLUTTER_SCRIPT_PRIVATE_H__ */
