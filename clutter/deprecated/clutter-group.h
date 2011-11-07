#ifndef __CLUTTER_GROUP_DEPRECATED_H__
#define __CLUTTER_GROUP_DEPRECATED_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#ifndef CLUTTER_DISABLE_DEPRECATED

/* for Mr. Mallum only */
#define clutter_group_add(group,actor)                  G_STMT_START {  \
  ClutterActor *_actor = (ClutterActor *) (actor);                      \
  if (CLUTTER_IS_GROUP ((group)) && CLUTTER_IS_ACTOR ((_actor)))        \
    {                                                                   \
      ClutterContainer *_container = (ClutterContainer *) (group);      \
      clutter_container_add_actor (_container, _actor);                 \
    }                                                   } G_STMT_END

#endif /* CLUTTER_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __CLUTTER_GROUP_DEPRECATED_H__ */
