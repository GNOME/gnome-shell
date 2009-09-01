#ifndef __SHELL_BUTTON_BOX_H__
#define __SHELL_BUTTON_BOX_H__

#include <clutter/clutter.h>
#include "big/box.h"

#define SHELL_TYPE_BUTTON_BOX                 (shell_button_box_get_type ())
#define SHELL_BUTTON_BOX(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_BUTTON_BOX, ShellButtonBox))
#define SHELL_BUTTON_BOX_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_BUTTON_BOX, ShellButtonBoxClass))
#define SHELL_IS_BUTTON_BOX(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_BUTTON_BOX))
#define SHELL_IS_BUTTON_BOX_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_BUTTON_BOX))
#define SHELL_BUTTON_BOX_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_BUTTON_BOX, ShellButtonBoxClass))

typedef struct _ShellButtonBox        ShellButtonBox;
typedef struct _ShellButtonBoxClass   ShellButtonBoxClass;

typedef struct _ShellButtonBoxPrivate ShellButtonBoxPrivate;

struct _ShellButtonBox
{
    BigBox parent;

    ShellButtonBoxPrivate *priv;
};

struct _ShellButtonBoxClass
{
    BigBoxClass parent_class;
};

GType shell_button_box_get_type (void) G_GNUC_CONST;

void shell_button_box_fake_release (ShellButtonBox *box);

#endif /* __SHELL_BUTTON_BOX_H__ */
