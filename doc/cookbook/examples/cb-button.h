/* inclusion guard */
#ifndef __CB_BUTTON_H__
#define __CB_BUTTON_H__

/* include any dependencies */
#include <clutter/clutter.h>

/* GObject implementation */

/* declare this function signature to remove compilation errors with -Wall;
 * the cb_button_get_type() function is actually added via the
 * G_DEFINE_TYPE macro in the .c file
 */
GType cb_button_get_type (void);

/* GObject type macros */
/* returns the class type identifier (GType) for CbButton */
#define CB_TYPE_BUTTON            (cb_button_get_type ())

/* cast obj to a CbButton object structure*/
#define CB_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CB_TYPE_BUTTON, CbButton))

/* check whether obj is a CbButton */
#define CB_IS_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CB_TYPE_BUTTON))

/* cast klass to CbButtonClass class structure */
#define CB_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CB_TYPE_BUTTON, CbButtonClass))

/* check whether klass is a member of the CbButtonClass */
#define CB_IS_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CB_TYPE_BUTTON))

/* get the CbButtonClass structure for a CbButton obj */
#define CB_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CB_TYPE_BUTTON, CbButtonClass))

/*
 * Private instance fields; see
 * http://www.gotw.ca/gotw/024.htm for the rationale
 */
typedef struct _CbButtonPrivate CbButtonPrivate;
typedef struct _CbButton        CbButton;
typedef struct _CbButtonClass   CbButtonClass;

/* object structure */
struct _CbButton
{
  /*<private>*/
  ClutterActor parent_instance;

  /* structure containing private members */
  /*<private>*/
  CbButtonPrivate *priv;
};

/* class structure */
struct _CbButtonClass
{
  /* signals */
  void (* clicked)(CbButton *button);

  /*<private>*/
  ClutterActorClass parent_class;
};

/* public API */

/* constructor - note this returns a ClutterActor instance */
ClutterActor *cb_button_new (void);

/* getter */
const gchar *cb_button_get_text (CbButton *self);

/* setters - these are wrappers round functions
 * which change properties of the internal actors
 */
void cb_button_set_text (CbButton    *self,
                         const gchar *text);

void cb_button_set_background_color (CbButton           *self,
                                     const ClutterColor *color);

void cb_button_set_text_color (CbButton           *self,
                               const ClutterColor *color);

#endif /* __CB_BUTTON_H__ */
