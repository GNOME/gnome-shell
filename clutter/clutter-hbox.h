#ifndef __CLUTTER_HBOX_H__
#define __CLUTTER_HBOX_H__

#include <clutter/clutter-box.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_HBOX               (clutter_hbox_get_type ())
#define CLUTTER_HBOX(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_HBOX, ClutterHBox))
#define CLUTTER_IS_HBOX(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_HBOX))
#define CLUTTER_HBOX_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_HBOX, ClutterHBoxClass))
#define CLUTTER_IS_HBOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_HBOX))
#define CLUTTER_HBOX_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_HBOX, ClutterHBoxClass))

typedef struct _ClutterHBox             ClutterHBox;
typedef struct _ClutterHBoxClass        ClutterHBoxClass;

struct _ClutterHBox
{
  ClutterBox parent_instance;
};

struct _ClutterHBoxClass
{
  ClutterBoxClass parent_class;
};

GType         clutter_hbox_get_type (void) G_GNUC_CONST;
ClutterActor *clutter_hbox_new      (void);

G_END_DECLS

#endif /* __CLUTTER_HBOX_H__ */
