/*
 * nbtk-button.h: Plain button actor
 *
 * Copyright 2007 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 *             Thomas Wood <thomas@linux.intel.com>
 *
 */

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_BUTTON_H__
#define __NBTK_BUTTON_H__

G_BEGIN_DECLS

#include <nbtk/nbtk-bin.h>

#define NBTK_TYPE_BUTTON                (nbtk_button_get_type ())
#define NBTK_BUTTON(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_BUTTON, NbtkButton))
#define NBTK_IS_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_BUTTON))
#define NBTK_BUTTON_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_BUTTON, NbtkButtonClass))
#define NBTK_IS_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_BUTTON))
#define NBTK_BUTTON_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_BUTTON, NbtkButtonClass))

typedef struct _NbtkButton              NbtkButton;
typedef struct _NbtkButtonPrivate       NbtkButtonPrivate;
typedef struct _NbtkButtonClass         NbtkButtonClass;

/**
 * NbtkButton:
 *
 * The contents of this structure is private and should only be accessed using
 * the provided API.
 */

struct _NbtkButton
{
  /*< private >*/
  NbtkBin parent_instance;

  NbtkButtonPrivate *priv;
};

struct _NbtkButtonClass
{
  NbtkBinClass parent_class;

  /* vfuncs, not signals */
  void (* pressed)  (NbtkButton *button);
  void (* released) (NbtkButton *button);
  void (* transition) (NbtkButton *button, ClutterActor *old_bg);

  /* signals */
  void (* clicked) (NbtkButton *button);
};

GType nbtk_button_get_type (void) G_GNUC_CONST;

NbtkWidget *          nbtk_button_new            (void);
NbtkWidget *          nbtk_button_new_with_label (const gchar  *text);
G_CONST_RETURN gchar *nbtk_button_get_label      (NbtkButton   *button);
void                  nbtk_button_set_label      (NbtkButton   *button,
                                                  const gchar  *text);
void                  nbtk_button_set_toggle_mode    (NbtkButton *button, gboolean toggle);
gboolean              nbtk_button_get_toggle_mode    (NbtkButton *button);
void                  nbtk_button_set_checked        (NbtkButton *button, gboolean checked);
gboolean              nbtk_button_get_checked        (NbtkButton *button);

G_END_DECLS

#endif /* __NBTK_BUTTON_H__ */
