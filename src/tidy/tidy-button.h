/* tidy-button.h: Plain button actor
 *
 * Copyright (C) 2007 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 */

#ifndef __TIDY_BUTTON_H__
#define __TIDY_BUTTON_H__

#include <tidy/tidy-frame.h>

G_BEGIN_DECLS

#define TIDY_TYPE_BUTTON                (tidy_button_get_type ())
#define TIDY_BUTTON(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), TIDY_TYPE_BUTTON, TidyButton))
#define TIDY_IS_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TIDY_TYPE_BUTTON))
#define TIDY_BUTTON_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), TIDY_TYPE_BUTTON, TidyButtonClass))
#define TIDY_IS_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), TIDY_TYPE_BUTTON))
#define TIDY_BUTTON_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), TIDY_TYPE_BUTTON, TidyButtonClass))

typedef struct _TidyButton              TidyButton;
typedef struct _TidyButtonPrivate       TidyButtonPrivate;
typedef struct _TidyButtonClass         TidyButtonClass;

struct _TidyButton
{
  TidyFrame parent_instance;

  TidyButtonPrivate *priv;
};

struct _TidyButtonClass
{
  TidyFrameClass parent_class;

  /* vfuncs, not signals */
  void (* pressed)  (TidyButton *button);
  void (* released) (TidyButton *button);

  /* signals */
  void (* clicked) (TidyButton *button);
};

GType tidy_button_get_type (void) G_GNUC_CONST;

ClutterActor *        tidy_button_new            (void);
ClutterActor *        tidy_button_new_with_label (const gchar  *text);
G_CONST_RETURN gchar *tidy_button_get_label      (TidyButton   *button);
void                  tidy_button_set_label      (TidyButton   *button,
                                                  const gchar  *text);

G_END_DECLS

#endif /* __TIDY_BUTTON_H__ */
