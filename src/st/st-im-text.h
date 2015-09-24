/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-im-text.h: Text widget with input method support
 *
 * Copyright 2009 Red Hat, Inc.
 *
 * This is a copy of ClutterIMText converted to use GtkIMContext rather
 * than ClutterIMContext. Original code:
 *
 * Author: raymond liu <raymond.liu@intel.com>
 *
 * Copyright (C) 2009, Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_IM_TEXT_H__
#define __ST_IM_TEXT_H__

G_BEGIN_DECLS

#include <clutter/clutter.h>

#define ST_TYPE_IM_TEXT               (st_im_text_get_type ())
G_DECLARE_FINAL_TYPE (StIMText, st_im_text, ST, IM_TEXT, ClutterText)

typedef struct _StIMText              StIMText;
typedef struct _StIMTextPrivate       StIMTextPrivate;

struct _StIMText
{
  ClutterText parent_instance;

  StIMTextPrivate *priv;
};

ClutterActor   *st_im_text_new               (const gchar    *text);
void            st_im_text_set_input_purpose (StIMText       *imtext,
                                              GtkInputPurpose purpose);
GtkInputPurpose st_im_text_get_input_purpose (StIMText       *imtext);
void            st_im_text_set_input_hints   (StIMText       *imtext,
                                              GtkInputHints   hints);
GtkInputHints   st_im_text_get_input_hints   (StIMText       *imtext);
gboolean        st_im_text_has_preedit       (StIMText       *imtext);

void            st_im_text_set_event_window  (GdkWindow      *window);

G_END_DECLS

#endif /* __ST_IM_TEXT_H__ */
