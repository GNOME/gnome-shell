#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_FIXED_LAYOUT_H__
#define __CLUTTER_FIXED_LAYOUT_H__

#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_FIXED_LAYOUT               (clutter_fixed_layout_get_type ())
#define CLUTTER_FIXED_LAYOUT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_FIXED_LAYOUT, ClutterFixedLayout))
#define CLUTTER_IS_FIXED_LAYOUT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_FIXED_LAYOUT))
#define CLUTTER_FIXED_LAYOUT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_FIXED_LAYOUT, ClutterFixedLayoutClass))
#define CLUTTER_IS_FIXED_LAYOUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_FIXED_LAYOUT))
#define CLUTTER_FIXED_LAYOUT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_FIXED_LAYOUT, ClutterFixedLayoutClass))

typedef struct _ClutterFixedLayout              ClutterFixedLayout;
typedef struct _ClutterFixedLayoutClass         ClutterFixedLayoutClass;

struct _ClutterFixedLayout
{
  ClutterLayoutManager parent_instance;
};

struct _ClutterFixedLayoutClass
{
  ClutterLayoutManagerClass parent_class;
};

GType clutter_fixed_layout_get_type (void) G_GNUC_CONST;

ClutterLayoutManager *clutter_fixed_layout_new (void);

G_END_DECLS

#endif /* __CLUTTER_FIXED_LAYOUT_H__ */
