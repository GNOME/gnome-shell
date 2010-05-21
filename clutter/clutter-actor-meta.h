#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_ACTOR_META_H__
#define __CLUTTER_ACTOR_META_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ACTOR_META                 (clutter_actor_meta_get_type ())
#define CLUTTER_ACTOR_META(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ACTOR_META, ClutterActorMeta))
#define CLUTTER_IS_ACTOR_META(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ACTOR_META))
#define CLUTTER_ACTOR_META_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ACTOR_META, ClutterActorMetaClass))
#define CLUTTER_IS_ACTOR_META_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ACTOR_META))
#define CLUTTER_ACTOR_META_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ACTOR_META, ClutterActorMetaClass))

typedef struct _ClutterActorMetaPrivate         ClutterActorMetaPrivate;
typedef struct _ClutterActorMetaClass           ClutterActorMetaClass;

/**
 * ClutterActorMeta:
 *
 * The <structname>ClutterActorMeta</structname> structure contains only
 * private data and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _ClutterActorMeta
{
  /*< private >*/
  GInitiallyUnowned parent_instance;

  ClutterActorMetaPrivate *priv;
};

/**
 * ClutterActorMetaClass:
 * @set_actor: virtual function, invoked when attaching and detaching
 *   a #ClutterActorMeta instance to a #ClutterActor
 *
 * The <structname>ClutterActorMetaClass</structname> structure contains
 * only private data
 *
 * Since: 1.4
 */
struct _ClutterActorMetaClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;

  /*< public >*/
  void (* set_actor) (ClutterActorMeta *meta,
                      ClutterActor     *actor);

  /*< private >*/
  void (* _clutter_meta1) (void);
  void (* _clutter_meta2) (void);
  void (* _clutter_meta3) (void);
  void (* _clutter_meta4) (void);
  void (* _clutter_meta5) (void);
  void (* _clutter_meta6) (void);
  void (* _clutter_meta7) (void);
};

GType clutter_actor_meta_get_type (void) G_GNUC_CONST;

void                  clutter_actor_meta_set_name    (ClutterActorMeta *meta,
                                                      const gchar      *name);
G_CONST_RETURN gchar *clutter_actor_meta_get_name    (ClutterActorMeta *meta);
void                  clutter_actor_meta_set_enabled (ClutterActorMeta *meta,
                                                      gboolean          is_enabled);
gboolean              clutter_actor_meta_get_enabled (ClutterActorMeta *meta);

ClutterActor *        clutter_actor_meta_get_actor   (ClutterActorMeta *meta);

/* private */
void                 _clutter_actor_meta_set_actor   (ClutterActorMeta *meta,
                                                      ClutterActor     *actor);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_META_H__ */
