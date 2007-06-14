#ifndef __CLUTTER_VBOX_H__
#define __CLUTTER_VBOX_H__

#include <clutter/clutter-box.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_VBOX               (clutter_vbox_get_type ())
#define CLUTTER_VBOX(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_VBOX, ClutterVBox))
#define CLUTTER_IS_VBOX(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_VBOX))
#define CLUTTER_VBOX_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_VBOX, ClutterVBoxClass))
#define CLUTTER_IS_VBOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_VBOX))
#define CLUTTER_VBOX_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_VBOX, ClutterVBoxClass))

typedef struct _ClutterVBox             ClutterVBox;
typedef struct _ClutterVBoxClass        ClutterVBoxClass;

struct _ClutterVBox
{
  ClutterBox parent_instance;
};

struct _ClutterVBoxClass
{
  ClutterBoxClass parent_class;
};

GType         clutter_vbox_get_type (void) G_GNUC_CONST;
ClutterActor *clutter_vbox_new      (void);

G_END_DECLS

#endif /* __CLUTTER_VBOX_H__ */
