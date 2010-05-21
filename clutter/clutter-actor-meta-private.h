#ifndef __CLUTTER_ACTOR_META_PRIVATE_H__
#define __CLUTTER_ACTOR_META_PRIVATE_H__

#include <clutter/clutter-actor-meta.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_META_GROUP         (_clutter_meta_group_get_type ())
#define CLUTTER_META_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_META_GROUP, ClutterMetaGroup))
#define CLUTTER_IS_META_GROUP(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_META_GROUP))

typedef struct _ClutterMetaGroup        ClutterMetaGroup;
typedef struct _ClutterMetaGroupClass   ClutterMetaGroupClass;

struct _ClutterMetaGroup
{
  GObject parent_instance;

  ClutterActor *actor;

  GList *meta;
};

struct _ClutterMetaGroupClass
{
  GObjectClass parent_class;
};

GType _clutter_meta_group_get_type (void) G_GNUC_CONST;

void                  _clutter_meta_group_add_meta    (ClutterMetaGroup *group,
                                                       ClutterActorMeta *meta);
void                  _clutter_meta_group_remove_meta (ClutterMetaGroup *group,
                                                       ClutterActorMeta *meta);
G_CONST_RETURN GList *_clutter_meta_group_peek_metas  (ClutterMetaGroup *group);
void                  _clutter_meta_group_clear_metas (ClutterMetaGroup *group);
ClutterActorMeta *    _clutter_meta_group_get_meta    (ClutterMetaGroup *group,
                                                       const gchar      *name);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_META_PRIVATE_H__ */
