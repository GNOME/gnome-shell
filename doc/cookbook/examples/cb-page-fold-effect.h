#ifndef __CB_PAGE_FOLD_EFFECT_H__
#define __CB_PAGE_FOLD_EFFECT_H__

#include <clutter/clutter.h>

GType cb_page_fold_effect_get_type (void);

#define CB_TYPE_PAGE_FOLD_EFFECT (cb_page_fold_effect_get_type ())
#define CB_PAGE_FOLD_EFFECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                                                         CB_TYPE_PAGE_FOLD_EFFECT, \
                                                                         CbPageFoldEffect))
#define CB_IS_PAGE_FOLD_EFFECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                                         CB_TYPE_PAGE_FOLD_EFFECT))
#define CB_PAGE_FOLD_EFFECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                                                      CB_TYPE_PAGE_FOLD_EFFECT, \
                                                                      CbPageFoldEffectClass))
#define CB_IS_PAGE_FOLD_EFFECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                                                      CB_TYPE_PAGE_FOLD_EFFECT))
#define CB_PAGE_FOLD_EFFECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                                                        CB_TYPE_PAGE_FOLD_EFFECT, \
                                                                        CbPageFoldEffectClass))

typedef struct _CbPageFoldEffectPrivate CbPageFoldEffectPrivate;
typedef struct _CbPageFoldEffect        CbPageFoldEffect;
typedef struct _CbPageFoldEffectClass   CbPageFoldEffectClass;

/* object */
struct _CbPageFoldEffect
{
  ClutterDeformEffect      parent_instance;
  CbPageFoldEffectPrivate *priv;
};

/* class */
struct _CbPageFoldEffectClass
{
  ClutterDeformEffectClass parent_class;
};

ClutterEffect *cb_page_fold_effect_new (gdouble angle,
                                        gdouble period);
void cb_page_fold_effect_set_angle (CbPageFoldEffect *effect,
                                    gdouble           angle);
void cb_page_fold_effect_set_period (CbPageFoldEffect *effect,
                                     gdouble           period);
gdouble cb_page_fold_effect_get_period (CbPageFoldEffect *effect);
gdouble cb_page_fold_effect_get_angle (CbPageFoldEffect *effect);

#endif /* __CB_PAGE_FOLD_EFFECT_H__ */
