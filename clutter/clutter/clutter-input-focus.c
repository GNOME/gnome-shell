/*
 * Copyright (C) 2017,2018 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "clutter-build-config.h"

#include "clutter/clutter-input-focus.h"
#include "clutter/clutter-input-focus-private.h"
#include "clutter/clutter-input-method-private.h"

typedef struct _ClutterInputFocusPrivate ClutterInputFocusPrivate;

struct _ClutterInputFocusPrivate
{
  ClutterInputMethod *im;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ClutterInputFocus, clutter_input_focus, G_TYPE_OBJECT)

static void
clutter_input_focus_real_focus_in (ClutterInputFocus  *focus,
                                   ClutterInputMethod *im)
{
  ClutterInputFocusPrivate *priv;

  priv = clutter_input_focus_get_instance_private (focus);
  priv->im = im;
}

static void
clutter_input_focus_real_focus_out (ClutterInputFocus  *focus)
{
  ClutterInputFocusPrivate *priv;

  priv = clutter_input_focus_get_instance_private (focus);
  priv->im = NULL;
}

static void
clutter_input_focus_class_init (ClutterInputFocusClass *klass)
{
  klass->focus_in = clutter_input_focus_real_focus_in;
  klass->focus_out = clutter_input_focus_real_focus_out;
}

static void
clutter_input_focus_init (ClutterInputFocus *focus)
{
}

gboolean
clutter_input_focus_is_focused (ClutterInputFocus *focus)
{
  ClutterInputFocusPrivate *priv;

  priv = clutter_input_focus_get_instance_private (focus);

  return !!priv->im;
}

void
clutter_input_focus_reset (ClutterInputFocus *focus)
{
  ClutterInputFocusPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));
  g_return_if_fail (clutter_input_focus_is_focused (focus));

  priv = clutter_input_focus_get_instance_private (focus);

  clutter_input_method_reset (priv->im);
}

void
clutter_input_focus_set_cursor_location (ClutterInputFocus *focus,
                                         const ClutterRect *rect)
{
  ClutterInputFocusPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));
  g_return_if_fail (clutter_input_focus_is_focused (focus));

  priv = clutter_input_focus_get_instance_private (focus);

  clutter_input_method_set_cursor_location (priv->im, rect);
}

void
clutter_input_focus_set_surrounding (ClutterInputFocus *focus,
                                     const gchar       *text,
                                     guint              cursor,
                                     guint              anchor)
{
  ClutterInputFocusPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));
  g_return_if_fail (clutter_input_focus_is_focused (focus));

  priv = clutter_input_focus_get_instance_private (focus);

  clutter_input_method_set_surrounding (priv->im, text, cursor, anchor);
}

void
clutter_input_focus_set_content_hints (ClutterInputFocus            *focus,
                                       ClutterInputContentHintFlags  hints)
{
  ClutterInputFocusPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));
  g_return_if_fail (clutter_input_focus_is_focused (focus));

  priv = clutter_input_focus_get_instance_private (focus);

  clutter_input_method_set_content_hints (priv->im, hints);
}

void
clutter_input_focus_set_content_purpose (ClutterInputFocus          *focus,
                                         ClutterInputContentPurpose  purpose)
{
  ClutterInputFocusPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));
  g_return_if_fail (clutter_input_focus_is_focused (focus));

  priv = clutter_input_focus_get_instance_private (focus);

  clutter_input_method_set_content_purpose (priv->im, purpose);
}

gboolean
clutter_input_focus_filter_key_event (ClutterInputFocus     *focus,
                                      const ClutterKeyEvent *key)
{
  ClutterInputFocusPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_INPUT_FOCUS (focus), FALSE);
  g_return_val_if_fail (clutter_input_focus_is_focused (focus), FALSE);

  priv = clutter_input_focus_get_instance_private (focus);

  return clutter_input_method_filter_key_event (priv->im, key);
}

void
clutter_input_focus_set_can_show_preedit (ClutterInputFocus *focus,
                                          gboolean           can_show_preedit)
{
  ClutterInputFocusPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));
  g_return_if_fail (clutter_input_focus_is_focused (focus));

  priv = clutter_input_focus_get_instance_private (focus);

  clutter_input_method_set_can_show_preedit (priv->im, can_show_preedit);
}

void
clutter_input_focus_request_toggle_input_panel (ClutterInputFocus *focus)
{
  ClutterInputFocusPrivate *priv;

  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));
  g_return_if_fail (clutter_input_focus_is_focused (focus));

  priv = clutter_input_focus_get_instance_private (focus);

  clutter_input_method_toggle_input_panel (priv->im);
}

void
clutter_input_focus_focus_in (ClutterInputFocus  *focus,
                              ClutterInputMethod *im)
{
  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));
  g_return_if_fail (CLUTTER_IS_INPUT_METHOD (im));

  CLUTTER_INPUT_FOCUS_GET_CLASS (focus)->focus_in (focus, im);
}

void
clutter_input_focus_focus_out (ClutterInputFocus  *focus)
{
  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));

  CLUTTER_INPUT_FOCUS_GET_CLASS (focus)->focus_out (focus);
}

void
clutter_input_focus_commit (ClutterInputFocus *focus,
                            const gchar       *text)
{
  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));

  CLUTTER_INPUT_FOCUS_GET_CLASS (focus)->commit_text (focus, text);
}

void
clutter_input_focus_delete_surrounding (ClutterInputFocus *focus,
                                        guint              offset,
                                        guint              len)
{
  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));

  CLUTTER_INPUT_FOCUS_GET_CLASS (focus)->delete_surrounding (focus, offset, len);
}

void
clutter_input_focus_request_surrounding (ClutterInputFocus *focus)
{
  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));

  CLUTTER_INPUT_FOCUS_GET_CLASS (focus)->request_surrounding (focus);
}

void
clutter_input_focus_set_preedit_text (ClutterInputFocus *focus,
                                      const gchar       *preedit,
                                      guint              cursor)
{
  g_return_if_fail (CLUTTER_IS_INPUT_FOCUS (focus));

  CLUTTER_INPUT_FOCUS_GET_CLASS (focus)->set_preedit_text (focus, preedit, cursor);
}
