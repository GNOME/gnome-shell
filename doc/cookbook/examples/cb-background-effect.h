#ifndef __CB_BACKGROUND_EFFECT_H__
#define __CB_BACKGROUND_EFFECT_H__

#include <clutter/clutter.h>

GType cb_background_effect_get_type (void);

#define CB_TYPE_BACKGROUND_EFFECT (cb_background_effect_get_type ())
#define CB_BACKGROUND_EFFECT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                                               CB_TYPE_BACKGROUND_EFFECT, \
                                                               CbBackgroundEffect))
#define CB_IS_BACKGROUND_EFFECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                                          CB_TYPE_BACKGROUND_EFFECT))
#define CB_BACKGROUND_EFFECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                                                       CB_TYPE_BACKGROUND_EFFECT, \
                                                                       CbBackgroundEffectClass))
#define CB_IS_BACKGROUND_EFFECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                                                       CB_TYPE_BACKGROUND_EFFECT))
#define CB_BACKGROUND_EFFECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                                                         CB_TYPE_BACKGROUND_EFFECT, \
                                                                         CbBackgroundEffectClass))

typedef struct _CbBackgroundEffectPrivate CbBackgroundEffectPrivate;
typedef struct _CbBackgroundEffect        CbBackgroundEffect;
typedef struct _CbBackgroundEffectClass   CbBackgroundEffectClass;

/* object */
struct _CbBackgroundEffect
{
  ClutterEffect              parent_instance;
  CbBackgroundEffectPrivate *priv;
};

/* class */
struct _CbBackgroundEffectClass
{
  ClutterEffectClass parent_class;
};

ClutterEffect *cb_background_effect_new ();

#endif /* __CB_BACKGROUND_EFFECT_H__ */
