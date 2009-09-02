#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_LAYOUT_MANAGER_H__
#define __CLUTTER_LAYOUT_MANAGER_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-container.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_LAYOUT_MANAGER             (clutter_layout_manager_get_type ())
#define CLUTTER_LAYOUT_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_LAYOUT_MANAGER, ClutterLayoutManager))
#define CLUTTER_IS_LAYOUT_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_LAYOUT_MANAGER))
#define CLUTTER_LAYOUT_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_LAYOUT_MANAGER, ClutterLayoutManagerClass))
#define CLUTTER_IS_LAYOUT_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_LAYOUT_MANAGER))
#define CLUTTER_LAYOUT_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_LAYOUT_MANAGER, ClutterLayoutManagerClass))

typedef struct _ClutterLayoutManager            ClutterLayoutManager;
typedef struct _ClutterLayoutManagerClass       ClutterLayoutManagerClass;

/**
 * ClutterLayoutManager:
 *
 * The #ClutterLayoutManager structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterLayoutManager
{
  GInitiallyUnowned parent_instance;
};

/**
 * ClutterLayoutManagerClass:
 *
 * The #ClutterLayoutManagerClass structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterLayoutManagerClass
{
  GInitiallyUnownedClass parent_class;

  void (* get_preferred_width)  (ClutterLayoutManager   *manager,
                                 ClutterContainer       *container,
                                 gfloat                  for_height,
                                 gfloat                 *minimum_width_p,
                                 gfloat                 *natural_width_p);
  void (* get_preferred_height) (ClutterLayoutManager   *manager,
                                 ClutterContainer       *container,
                                 gfloat                  for_width,
                                 gfloat                 *minimum_height_p,
                                 gfloat                 *natural_height_p);
  void (* allocate)             (ClutterLayoutManager   *manager,
                                 ClutterContainer       *container,
                                 const ClutterActorBox  *allocation,
                                 ClutterAllocationFlags  flags);

  void (* _clutter_padding_1) (void);
  void (* _clutter_padding_2) (void);
  void (* _clutter_padding_3) (void);
  void (* _clutter_padding_4) (void);
  void (* _clutter_padding_5) (void);
  void (* _clutter_padding_6) (void);
  void (* _clutter_padding_7) (void);
  void (* _clutter_padding_8) (void);
};

GType clutter_layout_manager_get_type (void) G_GNUC_CONST;

void clutter_layout_manager_get_preferred_width  (ClutterLayoutManager   *manager,
                                                  ClutterContainer       *container,
                                                  gfloat                  for_height,
                                                  gfloat                 *min_width_p,
                                                  gfloat                 *nat_width_p);
void clutter_layout_manager_get_preferred_height (ClutterLayoutManager   *manager,
                                                  ClutterContainer       *container,
                                                  gfloat                  for_width,
                                                  gfloat                 *min_height_p,
                                                  gfloat                 *nat_height_p);
void clutter_layout_manager_allocate             (ClutterLayoutManager   *manager,
                                                  ClutterContainer       *container,
                                                  const ClutterActorBox  *allocation,
                                                  ClutterAllocationFlags  flags);

G_END_DECLS

#endif /* __CLUTTER_LAYOUT_MANAGER_H__ */
