/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-button.h: Plain button actor
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_BUTTON_H__
#define __ST_BUTTON_H__

G_BEGIN_DECLS

#include <st/st-bin.h>

#define ST_TYPE_BUTTON (st_button_get_type ())
G_DECLARE_DERIVABLE_TYPE (StButton, st_button, ST, BUTTON, StBin)

struct _StButtonClass
{
  StBinClass parent_class;

  /* vfuncs, not signals */
  void (* transition) (StButton     *button);

  /* signals */
  void (* clicked) (StButton *button, int clicked_button);
};

StWidget    *st_button_new             (void);
StWidget    *st_button_new_with_label  (const gchar  *text);
const gchar *st_button_get_label       (StButton     *button);
void         st_button_set_label       (StButton     *button,
                                        const gchar  *text);
const char  *st_button_get_icon_name   (StButton     *button);
void         st_button_set_icon_name   (StButton     *button,
                                        const char   *icon_name);
void         st_button_set_toggle_mode (StButton     *button,
                                        gboolean      toggle);
gboolean     st_button_get_toggle_mode (StButton     *button);
void         st_button_set_checked     (StButton     *button,
                                        gboolean      checked);
gboolean     st_button_get_checked     (StButton     *button);

void         st_button_fake_release    (StButton     *button);

/**
 * StButtonMask:
 * @ST_BUTTON_ONE: button 1 (left)
 * @ST_BUTTON_TWO: button 2 (middle)
 * @ST_BUTTON_THREE: button 3 (right)
 *
 * A mask representing which mouse buttons an #StButton responds to.
 */
typedef enum {
  ST_BUTTON_ONE   = (1 << 0),
  ST_BUTTON_TWO   = (1 << 1),
  ST_BUTTON_THREE = (1 << 2),
} StButtonMask;

#define ST_BUTTON_MASK_FROM_BUTTON(button) (1 << ((button) - 1))

void         st_button_set_button_mask (StButton     *button,
                                        StButtonMask  mask);
StButtonMask st_button_get_button_mask (StButton     *button);

G_END_DECLS

#endif /* __ST_BUTTON_H__ */
