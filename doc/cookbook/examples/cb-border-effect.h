#ifndef __CB_BORDER_EFFECT_H__
#define __CB_BORDER_EFFECT_H__

#include <clutter/clutter.h>

GType cb_border_effect_get_type (void);

#define CB_TYPE_BORDER_EFFECT (cb_border_effect_get_type ())
#define CB_BORDER_EFFECT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                                           CB_TYPE_BORDER_EFFECT, \
                                                           CbBorderEffect))
#define CB_IS_BORDER_EFFECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                                      CB_TYPE_BORDER_EFFECT))
#define CB_BORDER_EFFECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                                                   CB_TYPE_BORDER_EFFECT, \
                                                                   CbBorderEffectClass))
#define CB_IS_BORDER_EFFECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                                                   CB_TYPE_BORDER_EFFECT))
#define CB_BORDER_EFFECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                                                     CB_TYPE_BORDER_EFFECT, \
                                                                     CbBorderEffectClass))

typedef struct _CbBorderEffectPrivate CbBorderEffectPrivate;
typedef struct _CbBorderEffect        CbBorderEffect;
typedef struct _CbBorderEffectClass   CbBorderEffectClass;

/* object */
struct _CbBorderEffect
{
  ClutterEffect          parent_instance;
  CbBorderEffectPrivate *priv;
};

/* class */
struct _CbBorderEffectClass
{
  ClutterEffectClass parent_class;
};

ClutterEffect *cb_border_effect_new (gfloat              width,
                                     const ClutterColor *color);

void cb_border_effect_set_color (CbBorderEffect     *self,
                                 const ClutterColor *color);

void cb_border_effect_get_color (CbBorderEffect *self,
                                 ClutterColor   *color);

void cb_border_effect_set_width (CbBorderEffect *self,
                                 gfloat          width);

gfloat cb_border_effect_get_width (CbBorderEffect *self);

#endif /* __CB_BORDER_EFFECT_H__ */
