#ifndef __CLUTTER_BOX_H__
#define __CLUTTER_BOX_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-types.h>

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
  
  /* We need to put these in the instance structure, since this
   * is an abstract class meant to be subclassed; think of these
   * as "protected" attributes of the ClutterBox class
   */

  /* Allocation of the box */
  ClutterActorBox allocation;

  /* List of ClutterBoxChild structures */
  GList *children;

  /* Background color of the box */
  ClutterColor color;

  /* Margin between the inner border of the box and the children */
  ClutterMargin margin;

  /* Default padding for the children */
  ClutterPadding default_padding;
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
 * @child_coords: the original coordinates of the child
 * @pack_type: the type of packing used by the child
 * @padding: the padding around the child
 *
 * Packing data for children of a #ClutterBox.
 *
 * Since: 0.4
 */
struct _ClutterBoxChild
{
  ClutterActor *actor;
  ClutterActorBox child_coords;
  ClutterPackType pack_type;
  ClutterPadding padding;
};

GType    clutter_box_get_type            (void) G_GNUC_CONST;
void     clutter_box_set_color           (ClutterBox           *box,
                                          const ClutterColor   *color);
void     clutter_box_get_color           (ClutterBox           *box,
                                          ClutterColor         *color);
void     clutter_box_set_margin          (ClutterBox           *box,
                                          const ClutterMargin  *margin);
void     clutter_box_get_margin          (ClutterBox           *box,
                                          ClutterMargin        *margin);
void     clutter_box_set_default_padding (ClutterBox           *box,
                                          gint                  padding_top,
                                          gint                  padding_right,
                                          gint                  padding_bottom,
                                          gint                  padding_left);
void     clutter_box_get_default_padding (ClutterBox           *box,
                                          gint                 *padding_top,
                                          gint                 *padding_right,
                                          gint                 *padding_bottom,
                                          gint                 *padding_left);
void     clutter_box_pack                (ClutterBox           *box,
                                          ClutterActor         *actor,
                                          ClutterPackType       pack_type,
                                          const ClutterPadding *padding);
void     clutter_box_pack_defaults       (ClutterBox           *box,
                                          ClutterActor         *actor);
void     clutter_box_remove_all          (ClutterBox           *box);
gboolean clutter_box_query_child         (ClutterBox           *box,
                                          ClutterActor         *actor,
                                          ClutterBoxChild      *child);
gboolean clutter_box_query_nth_child     (ClutterBox           *box,
                                          gint                  index_,
                                          ClutterBoxChild      *child);

G_END_DECLS

#endif /* __CLUTTER_BOX_H__ */
