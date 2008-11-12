/* tidy-button.c: Plain button actor
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>

#include "tidy-button.h"

#include "tidy-debug.h"
#include "tidy-marshal.h"
#include "tidy-stylable.h"

enum
{
  PROP_0,

  PROP_LABEL
};

enum
{
  CLICKED,

  LAST_SIGNAL
};

#define TIDY_BUTTON_GET_PRIVATE(obj)    \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_BUTTON, TidyButtonPrivate))

struct _TidyButtonPrivate
{
  gchar *text;

  ClutterTimeline *timeline;
  ClutterEffectTemplate *press_tmpl;

  guint8 old_opacity;

  guint is_pressed : 1;
};

static guint button_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TidyButton, tidy_button, TIDY_TYPE_FRAME);

static void
tidy_button_real_pressed (TidyButton *button)
{
  TidyButtonPrivate *priv = button->priv;
  ClutterActor *actor = CLUTTER_ACTOR (button);

  if (G_UNLIKELY (!priv->press_tmpl))
    {
      priv->timeline = clutter_timeline_new_for_duration (250);
      priv->press_tmpl = clutter_effect_template_new (priv->timeline,
                                                      clutter_sine_inc_func);
      clutter_effect_template_set_timeline_clone (priv->press_tmpl, FALSE);
    }

  if (clutter_timeline_is_playing (priv->timeline))
    {
      clutter_timeline_stop (priv->timeline);
      clutter_actor_set_opacity (actor, priv->old_opacity);
    }

  priv->old_opacity = clutter_actor_get_opacity (actor);

  clutter_effect_fade (priv->press_tmpl, actor,
                       0x44,
                       NULL, NULL);
}

static void
tidy_button_real_released (TidyButton *button)
{
  TidyButtonPrivate *priv = button->priv;
  ClutterActor *actor = CLUTTER_ACTOR (button);

  if (G_UNLIKELY (!priv->press_tmpl))
    {
      priv->timeline = clutter_timeline_new_for_duration (250);
      priv->press_tmpl = clutter_effect_template_new (priv->timeline,
                                                      clutter_sine_inc_func);
      clutter_effect_template_set_timeline_clone (priv->press_tmpl, FALSE);
    }

  if (clutter_timeline_is_playing (priv->timeline))
    clutter_timeline_stop (priv->timeline);

  clutter_effect_fade (priv->press_tmpl, actor,
                       priv->old_opacity,
                       NULL, NULL);
}

static void
tidy_button_construct_child (TidyButton *button)
{
  TidyButtonPrivate *priv = button->priv;
  gchar *font_name;
  ClutterColor *text_color;
  ClutterActor *label;

  if (!priv->text)
    return;

  tidy_stylable_get (TIDY_STYLABLE (button),
                     "font-name", &font_name,
                     "text-color", &text_color,
                     NULL);

  label = g_object_new (CLUTTER_TYPE_LABEL,
                        "font-name", font_name,
                        "text", priv->text,
                        "color", text_color,
                        "alignment", PANGO_ALIGN_CENTER,
                        "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                        "use-markup", TRUE,
                        "wrap", FALSE,
                        NULL);

  clutter_actor_show (label);
  clutter_container_add_actor (CLUTTER_CONTAINER (button), label);

  clutter_color_free (text_color);
  g_free (font_name);
}

static gboolean
tidy_button_button_press (ClutterActor       *actor,
                          ClutterButtonEvent *event)
{
  if (event->button == 1 &&
      event->click_count == 1)
    {
      TidyButton *button = TIDY_BUTTON (actor);
      TidyButtonClass *klass = TIDY_BUTTON_GET_CLASS (button);

      button->priv->is_pressed = TRUE;

      clutter_grab_pointer (actor);

      if (klass->pressed)
        klass->pressed (button);

      return TRUE;
    }
  
  return FALSE;
}

static gboolean
tidy_button_button_release (ClutterActor       *actor,
                            ClutterButtonEvent *event)
{
  if (event->button == 1)
    {
      TidyButton *button = TIDY_BUTTON (actor);
      TidyButtonClass *klass = TIDY_BUTTON_GET_CLASS (button);

      if (!button->priv->is_pressed)
        return FALSE;

      clutter_ungrab_pointer ();

      button->priv->is_pressed = FALSE;

      if (klass->released)
        klass->released (button);

      g_signal_emit (button, button_signals[CLICKED], 0);

      return TRUE;
    }

  return FALSE;
}

static gboolean
tidy_button_leave (ClutterActor         *actor,
                   ClutterCrossingEvent *event)
{
  TidyButton *button = TIDY_BUTTON (actor);

  if (button->priv->is_pressed)
    {
      TidyButtonClass *klass = TIDY_BUTTON_GET_CLASS (button);

      clutter_ungrab_pointer ();

      button->priv->is_pressed = FALSE;

      if (klass->released)
        klass->released (button);
    }

  return FALSE;
}

static void
tidy_button_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  TidyButton *button = TIDY_BUTTON (gobject);

  switch (prop_id)
    {
    case PROP_LABEL:
      tidy_button_set_label (button, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_button_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  TidyButtonPrivate *priv = TIDY_BUTTON (gobject)->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, priv->text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_button_finalize (GObject *gobject)
{
  TidyButtonPrivate *priv = TIDY_BUTTON (gobject)->priv;

  g_free (priv->text);

  G_OBJECT_CLASS (tidy_button_parent_class)->finalize (gobject);
}

static void
tidy_button_dispose (GObject *gobject)
{
  TidyButtonPrivate *priv = TIDY_BUTTON (gobject)->priv;

  if (priv->press_tmpl)
    {
      g_object_unref (priv->press_tmpl);
      g_object_unref (priv->timeline);

      priv->press_tmpl = NULL;
      priv->timeline = NULL;
    }

  G_OBJECT_CLASS (tidy_button_parent_class)->dispose (gobject);
}

static void
tidy_button_class_init (TidyButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TidyButtonPrivate));

  klass->pressed = tidy_button_real_pressed;
  klass->released = tidy_button_real_released;

  gobject_class->set_property = tidy_button_set_property;
  gobject_class->get_property = tidy_button_get_property;
  gobject_class->dispose = tidy_button_dispose;
  gobject_class->finalize = tidy_button_finalize;

  actor_class->button_press_event = tidy_button_button_press;
  actor_class->button_release_event = tidy_button_button_release;
  actor_class->leave_event = tidy_button_leave;

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        "Label",
                                                        "Label of the button",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  button_signals[CLICKED] =
    g_signal_new ("clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TidyButtonClass, clicked),
                  NULL, NULL,
                  _tidy_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
tidy_button_init (TidyButton *button)
{
  button->priv = TIDY_BUTTON_GET_PRIVATE (button);
}

ClutterActor *
tidy_button_new (void)
{
  return g_object_new (TIDY_TYPE_BUTTON, NULL);
}

ClutterActor *
tidy_button_new_with_label (const gchar *text)
{
  return g_object_new (TIDY_TYPE_BUTTON, "label", text, NULL);
}

G_CONST_RETURN gchar *
tidy_button_get_label (TidyButton *button)
{
  g_return_val_if_fail (TIDY_IS_BUTTON (button), NULL);

  return button->priv->text;
}

void
tidy_button_set_label (TidyButton  *button,
                       const gchar *text)
{
  TidyButtonPrivate *priv;

  g_return_if_fail (TIDY_IS_BUTTON (button));

  priv = button->priv;

  g_free (priv->text);
  priv->text = g_strdup (text);

  tidy_button_construct_child (button);

  g_object_notify (G_OBJECT (button), "label");
}
