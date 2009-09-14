#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BIN_LAYOUT_H__
#define __CLUTTER_BIN_LAYOUT_H__

#include <clutter/clutter-layout-manager.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BIN_LAYOUT                 (clutter_bin_layout_get_type ())
#define CLUTTER_BIN_LAYOUT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BIN_LAYOUT, ClutterBinLayout))
#define CLUTTER_IS_BIN_LAYOUT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BIN_LAYOUT))
#define CLUTTER_BIN_LAYOUT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BIN_LAYOUT, ClutterBinLayoutClass))
#define CLUTTER_IS_BIN_LAYOUT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BIN_LAYOUT))
#define CLUTTER_BIN_LAYOUT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BIN_LAYOUT, ClutterBinLayoutClass))

typedef struct _ClutterBinLayout                ClutterBinLayout;
typedef struct _ClutterBinLayoutPrivate         ClutterBinLayoutPrivate;
typedef struct _ClutterBinLayoutClass           ClutterBinLayoutClass;

/**
 * ClutterBinAlignment:
 * @CLUTTER_BIN_ALIGNMENT_FIXED: Fixed position alignment; the
 *   #ClutterBinLayout will honour the fixed position provided
 *   by the actors themselves when allocating them
 * @CLUTTER_BIN_ALIGNMENT_FILL: Fill the allocation size
 * @CLUTTER_BIN_ALIGNMENT_START: Position the actors at the top
 *   or left side of the container, depending on the axis
 * @CLUTTER_BIN_ALIGNMENT_END: Position the actors at the bottom
 *   or right side of the container, depending on the axis
 * @CLUTTER_BIN_ALIGNMENT_CENTER: Position the actors at the
 *   center of the container, depending on the axis
 *
 * The alignment policies available on each axis for #ClutterBinLayout
 *
 * Since: 1.2
 */
typedef enum {
  CLUTTER_BIN_ALIGNMENT_FIXED,
  CLUTTER_BIN_ALIGNMENT_FILL,
  CLUTTER_BIN_ALIGNMENT_START,
  CLUTTER_BIN_ALIGNMENT_END,
  CLUTTER_BIN_ALIGNMENT_CENTER
} ClutterBinAlignment;

/**
 * ClutterBinLayout:
 *
 * The #ClutterBinLayout structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterBinLayout
{
  /*< private >*/
  ClutterLayoutManager parent_instance;

  ClutterBinLayoutPrivate *priv;
};

/**
 * ClutterBinLayoutClass:
 *
 * The #ClutterBinLayoutClass structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterBinLayoutClass
{
  /*< private >*/
  ClutterLayoutManagerClass parent_class;
};

GType clutter_bin_layout_get_type (void) G_GNUC_CONST;

ClutterLayoutManager *clutter_bin_layout_new           (ClutterBinAlignment  align_x,
                                                        ClutterBinAlignment  align_y);

void                  clutter_bin_layout_set_alignment (ClutterBinLayout    *self,
                                                        ClutterContainer    *container,
                                                        ClutterActor        *child,
                                                        ClutterBinAlignment  x_align,
                                                        ClutterBinAlignment  y_align);
void                  clutter_bin_layout_get_alignment (ClutterBinLayout    *self,
                                                        ClutterContainer    *container,
                                                        ClutterActor        *child,
                                                        ClutterBinAlignment *x_align,
                                                        ClutterBinAlignment *y_align);

G_END_DECLS

#endif /* __CLUTTER_BIN_LAYOUT_H__ */
