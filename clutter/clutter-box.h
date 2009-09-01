#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BOX_H__
#define __CLUTTER_BOX_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-container.h>
#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BOX                (clutter_box_get_type ())
#define CLUTTER_BOX(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BOX, ClutterBox))
#define CLUTTER_IS_BOX(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BOX))
#define CLUTTER_BOX_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BOX, ClutterBoxClass))
#define CLUTTER_IS_BOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BOX))
#define CLUTTER_BOX_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BOX, ClutterBoxClass))

typedef struct _ClutterBox              ClutterBox;
typedef struct _ClutterBoxPrivate       ClutterBoxPrivate;
typedef struct _ClutterBoxClass         ClutterBoxClass;

struct _ClutterBox
{
  ClutterActor parent_instance;

  ClutterBoxPrivate *priv;
};

struct _ClutterBoxClass
{
  ClutterActorClass parent_class;
};

GType clutter_box_get_type (void) G_GNUC_CONST;

ClutterActor *clutter_box_new (ClutterLayoutManager *manager);

G_END_DECLS

#endif /* __CLUTTER_BOX_H__ */
