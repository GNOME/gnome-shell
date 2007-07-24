#ifndef __CLUTTER_BOX_H__
#define __CLUTTER_BOX_H__

#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BOX                (clutter_box_get_type ())
#define CLUTTER_BOX(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BOX, ClutterBox))
#define CLUTTER_IS_BOX(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BOX))
#define CLUTTER_BOX_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BOX, ClutterBoxClass))
#define CLUTTER_IS_BOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BOX))
#define CLUTTER_BOX_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BOX, ClutterBoxClass))

/**
 * ClutterPackType:
 * @CLUTTER_PACK_START: append child from the start
 * @CLUTTER_PACK_END: append child from the end
 *
 * Pack order for a #ClutterBox child.
 *
 * Since: 0.4
 */
typedef enum {
  CLUTTER_PACK_START,
  CLUTTER_PACK_END
} ClutterPackType;

typedef struct _ClutterBoxChild         ClutterBoxChild;
typedef struct _ClutterBox              ClutterBox; 
typedef struct _ClutterBoxClass         ClutterBoxClass;

struct _ClutterBox
{
  ClutterActor parent_instance;

  /*< private >*/

  /* list of ClutterBoxChild structures */
  GList *children;

  /* spacing between child actors */
  guint spacing;
};

struct _ClutterBoxClass
{
  ClutterActorClass parent_class;

  /* vfuncs, not signals */
  void (* pack_child)   (ClutterBox      *box,
                         ClutterBoxChild *child);
  void (* unpack_child) (ClutterBox      *box,
                         ClutterBoxChild *child);

  /* padding, for future expansion */
  void (*_clutter_reserved1) (void);
  void (*_clutter_reserved2) (void);
  void (*_clutter_reserved3) (void);
  void (*_clutter_reserved4) (void);
  void (*_clutter_reserved5) (void);
  void (*_clutter_reserved6) (void);
  void (*_clutter_reserved7) (void);
  void (*_clutter_reserved8) (void);
};

/**
 * ClutterBoxChild:
 * @actor: the child #ClutterActor
 * @pack_type: the type of packing used
 *
 * Packing data for children of a #ClutterBox.
 *
 * Since: 0.4
 */
struct _ClutterBoxChild
{
  ClutterActor *actor;

  ClutterPackType pack_type;
};

GType    clutter_box_get_type        (void) G_GNUC_CONST;
void     clutter_box_set_spacing     (ClutterBox      *box,
                                      guint            spacing);
guint    clutter_box_get_spacing     (ClutterBox      *box);
void     clutter_box_pack_start      (ClutterBox      *box,
                                      ClutterActor    *actor);
void     clutter_box_pack_end        (ClutterBox      *box,
                                      ClutterActor    *actor);
gboolean clutter_box_query_child     (ClutterBox      *box,
                                      ClutterActor    *actor,
                                      ClutterBoxChild *child);
gboolean clutter_box_query_nth_child (ClutterBox      *box,
                                      gint             index,
                                      ClutterBoxChild *child);

G_END_DECLS

#endif /* __CLUTTER_BOX_H__ */
