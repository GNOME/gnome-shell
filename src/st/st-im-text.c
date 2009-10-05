/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-im-text.c
 *
 * This started as a copy of ClutterIMText converted to use
 * GtkIMContext rather than ClutterIMContext. Original code:
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * SECTION:StIMText
 * @short_description: Text widget with input method support
 * @stability: Unstable
 * @see_also: #ClutterText
 * @include: st-imtext/st-imtext.h
 *
 * #StIMText derives from ClutterText and hooks up better text input
 * via #GtkIMContext. It is meant to be a drop-in replacement for
 * ClutterIMText but using GtkIMContext rather than ClutterIMContext.
 */

/* Places where this actor doesn't support all of GtkIMContext:
 *
 *  A) It doesn't support preedit. This makes it fairly useless for
 *     most complicated input methods. Fixing this requires support
 *     directly in ClutterText, since there is no way to wedge a
 *     preedit string in externally.
 *  B) It doesn't support surrounding context via the
 *     :retrieve-surrounding and :delete-surrounding signals. This could
 *     be added here, but  only affects a small number of input methods
 *     and really doesn't make a lot of sense without A)
 *
 * Another problem that will show up with usage in GNOME Shell's overview
 * is that the user may have trouble seeing and interacting with ancilliary
 * windows shown by the IM.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <X11/extensions/XKB.h>

#include "st-im-text.h"

#define ST_IM_TEXT_GET_PRIVATE(obj)    \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ST_TYPE_IM_TEXT, StIMTextPrivate))

struct _StIMTextPrivate
{
  GtkIMContext *im_context;
  GdkWindow *window;

  guint need_im_reset : 1;
};

static void st_im_text_commit_cb (GtkIMContext *context,
                                  const gchar  *str,
                                  StIMText     *imtext);

G_DEFINE_TYPE (StIMText, st_im_text, CLUTTER_TYPE_TEXT)

static void
st_im_text_dispose (GObject *object)
{
  StIMTextPrivate *priv = ST_IM_TEXT (object)->priv;

  g_signal_handlers_disconnect_by_func (priv->im_context,
                                        (void *) st_im_text_commit_cb,
                                        object);

  g_object_unref (priv->im_context);
  priv->im_context = NULL;
}

static void
update_im_cursor_location (StIMText *self)
{
  StIMTextPrivate *priv = self->priv;
  ClutterText *clutter_text = CLUTTER_TEXT (self);
  ClutterActor *parent;
  gint position;
  gfloat cursor_x, cursor_y, cursor_height;
  gfloat actor_x, actor_y;
  GdkRectangle area;

  position = clutter_text_get_cursor_position (clutter_text);
  clutter_text_position_to_coords (clutter_text, position,
                                   &cursor_x, &cursor_y, &cursor_height);

  /* This is a workaround for a bug in Clutter where
   * clutter_actor_get_transformed_position doesn't work during
   * clutter_actor_paint() because the actor has already set up
   * a model-view matrix.
   *
   * http://bugzilla.openedhand.com/show_bug.cgi?id=1115
   */
  actor_x = actor_y = 0.;
  parent = CLUTTER_ACTOR (self);
  while (parent)
    {
      gfloat x, y;

      clutter_actor_get_position (parent, &x, &y);
      actor_x += x;
      actor_y += y;

      parent = clutter_actor_get_parent (parent);
    }

  area.x = (int)(0.5 + cursor_x + actor_x);
  area.y = (int)(0.5 + cursor_y + actor_y);
  area.width = 0;
  area.height = (int)(0.5 + cursor_height);

  gtk_im_context_set_cursor_location (priv->im_context, &area);
}

static void
st_im_text_commit_cb (GtkIMContext *context,
                      const gchar  *str,
                      StIMText     *imtext)
{
  ClutterText *clutter_text = CLUTTER_TEXT (imtext);

  if (clutter_text_get_editable (clutter_text))
    {
      clutter_text_delete_selection (clutter_text);
      clutter_text_insert_text (clutter_text, str,
                                clutter_text_get_cursor_position (clutter_text));
    }
}

static void
reset_im_context (StIMText *self)
{
  StIMTextPrivate *priv = self->priv;

  if (priv->need_im_reset)
    {
      gtk_im_context_reset (priv->im_context);
      priv->need_im_reset = FALSE;
    }
}

static void
st_im_text_paint (ClutterActor *actor)
{
  StIMText *self = ST_IM_TEXT (actor);
  ClutterText *clutter_text = CLUTTER_TEXT (actor);

  /* This updates the cursor position as a side-effect */
  if (CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->paint)
    CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->paint (actor);

  if (clutter_text_get_editable (clutter_text))
    update_im_cursor_location (self);
}

/* Returns a new reference to window */
static GdkWindow *
window_for_actor (ClutterActor *actor)
{
  GdkDisplay *display = gdk_display_get_default ();
  ClutterActor *stage;
  Window xwindow;
  GdkWindow *window;

  stage = clutter_actor_get_stage (actor);
  xwindow = clutter_x11_get_stage_window ((ClutterStage *)stage);

  window = gdk_window_lookup_for_display (display, xwindow);
  if (window)
    g_object_ref (window);
  else
    window = gdk_window_foreign_new_for_display (display, xwindow);

  return window;
}

static void
st_im_text_realize (ClutterActor *actor)
{
  StIMTextPrivate *priv = ST_IM_TEXT (actor)->priv;

  priv->window = window_for_actor (actor);
  gtk_im_context_set_client_window (priv->im_context, priv->window);
}

static void
st_im_text_unrealize (ClutterActor *actor)
{
  StIMText *self = ST_IM_TEXT (actor);
  StIMTextPrivate *priv = self->priv;

  reset_im_context (self);
  gtk_im_context_set_client_window (priv->im_context, NULL);
  g_object_unref (priv->window);
  priv->window = NULL;
}

static gboolean
key_is_modifier (guint16 keyval)
{
  /* See gdkkeys-x11.c:_gdk_keymap_key_is_modifier() for how this
   * really should be implemented */

  switch (keyval)
    {
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Control_L:
    case GDK_Control_R:
    case GDK_Caps_Lock:
    case GDK_Shift_Lock:
    case GDK_Meta_L:
    case GDK_Meta_R:
    case GDK_Alt_L:
    case GDK_Alt_R:
    case GDK_Super_L:
    case GDK_Super_R:
    case GDK_Hyper_L:
    case GDK_Hyper_R:
    case GDK_ISO_Lock:
    case GDK_ISO_Level2_Latch:
    case GDK_ISO_Level3_Shift:
    case GDK_ISO_Level3_Latch:
    case GDK_ISO_Level3_Lock:
    case GDK_ISO_Level5_Shift:
    case GDK_ISO_Level5_Latch:
    case GDK_ISO_Level5_Lock:
    case GDK_ISO_Group_Shift:
    case GDK_ISO_Group_Latch:
    case GDK_ISO_Group_Lock:
      return TRUE;
    default:
      return FALSE;
    }
}

static GdkEventKey *
key_event_to_gdk (ClutterKeyEvent *event_clutter)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkKeymap *keymap = gdk_keymap_get_for_display (display);
  GdkEventKey *event_gdk;
  event_gdk = (GdkEventKey *)gdk_event_new ((event_clutter->type == CLUTTER_KEY_PRESS) ?
                                            GDK_KEY_PRESS : GDK_KEY_RELEASE);

  event_gdk->window = window_for_actor ((ClutterActor *)event_clutter->stage);
  event_gdk->send_event = FALSE;
  event_gdk->time = event_clutter->time;
  /* This depends on ClutterModifierType and GdkModifierType being
   * identical, which they are currently. (They both match the X
   * modifier state in the low 16-bits and have the same extensions.) */
  event_gdk->state = event_clutter->modifier_state;
  event_gdk->keyval = event_clutter->keyval;
  event_gdk->hardware_keycode = event_clutter->hardware_keycode;
  /* For non-proper non-XKB support, we'd need a huge cut-and-paste
   * from gdkkeys-x11.c; this is a macro that just shifts a few bits
   * out of state, so won't make the situation worse if the server
   * doesn't support XKB; we'll just end up with group == 0 */
  event_gdk->group = XkbGroupForCoreState (event_gdk->state);

  gdk_keymap_translate_keyboard_state (keymap, event_gdk->hardware_keycode,
                                       event_gdk->state, event_gdk->group,
                                       &event_gdk->keyval, NULL, NULL, NULL);

  if (event_clutter->unicode_value)
    {
      /* This is not particularly close to what GDK does - event_gdk->string
       * is supposed to be in the locale encoding, and have control keys
       * as control characters, etc. See gdkevents-x11.c:translate_key_event().
       * Hopefully no input method is using event.string.
       */
      char buf[6];

      event_gdk->length = g_unichar_to_utf8 (event_clutter->unicode_value, buf);
      event_gdk->string = g_strndup (buf, event_gdk->length);
    }

  event_gdk->is_modifier = key_is_modifier (event_gdk->keyval);

  return event_gdk;
}

static gboolean
st_im_text_button_press_event (ClutterActor       *actor,
                               ClutterButtonEvent *event)
{
  /* The button press indicates the user moving the cursor, or selecting
   * etc, so we should abort any current preedit operation. ClutterText
   * treats all buttons identically, so so do we.
   */
  reset_im_context (ST_IM_TEXT (actor));

  if (CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->button_press_event)
    return CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->button_press_event (actor, event);
  else
    return FALSE;
}

static gboolean
st_im_text_key_press_event (ClutterActor    *actor,
                            ClutterKeyEvent *event)
{
  StIMText *self = ST_IM_TEXT (actor);
  StIMTextPrivate *priv = self->priv;
  ClutterText *clutter_text = CLUTTER_TEXT (actor);
  gboolean result = FALSE;
  int old_position;

  if (clutter_text_get_editable (clutter_text))
    {
      GdkEventKey *event_gdk = key_event_to_gdk (event);

      if (gtk_im_context_filter_keypress (priv->im_context, event_gdk))
        {
          priv->need_im_reset = TRUE;
          result = TRUE;
        }

      gdk_event_free ((GdkEvent *)event_gdk);
    }

  /* ClutterText:position isn't properly notified, so we have to
   * check before/after to catch a keypress (like an arrow key)
   * moving the cursor position, which should reset the IM context.
   * (Resetting on notify::position would require a sentinel when
   * committing text)
   *
   * http://bugzilla.openedhand.com/show_bug.cgi?id=1830
   */
  old_position = clutter_text_get_cursor_position (clutter_text);

  if (!result &&
      CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->key_press_event)
    result = CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->key_press_event (actor, event);

  if (clutter_text_get_cursor_position (clutter_text) != old_position)
    reset_im_context (self);

  return result;
}

static gboolean
st_im_text_key_release_event (ClutterActor    *actor,
                              ClutterKeyEvent *event)
{
  StIMText *self = ST_IM_TEXT (actor);
  StIMTextPrivate *priv = self->priv;
  ClutterText *clutter_text = CLUTTER_TEXT (actor);
  GdkEventKey *event_gdk;
  gboolean result = FALSE;

  if (clutter_text_get_editable (clutter_text))
    {
      event_gdk = key_event_to_gdk (event);

      if (gtk_im_context_filter_keypress (priv->im_context, event_gdk))
        {
          priv->need_im_reset = TRUE;
          result = TRUE;
        }

      gdk_event_free ((GdkEvent *)event_gdk);
    }

  if (!result &&
      CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->key_release_event)
    result = CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->key_release_event (actor, event);

  return result;
}

static void
st_im_text_key_focus_in (ClutterActor *actor)
{
  StIMTextPrivate *priv = ST_IM_TEXT (actor)->priv;
  ClutterText *clutter_text = CLUTTER_TEXT (actor);

  if (clutter_text_get_editable (clutter_text))
    {
      priv->need_im_reset = TRUE;
      gtk_im_context_focus_in (priv->im_context);
    }

  if (CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->key_focus_in)
    CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->key_focus_in (actor);
}

static void
st_im_text_key_focus_out (ClutterActor *actor)
{
  StIMTextPrivate *priv = ST_IM_TEXT (actor)->priv;
  ClutterText *clutter_text = CLUTTER_TEXT (actor);

  if (clutter_text_get_editable (clutter_text))
    {
      priv->need_im_reset = TRUE;
      gtk_im_context_focus_out (priv->im_context);
    }

  if (CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->key_focus_out)
    CLUTTER_ACTOR_CLASS (st_im_text_parent_class)->key_focus_out (actor);
}

static void
st_im_text_class_init (StIMTextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (StIMTextPrivate));

  object_class->dispose = st_im_text_dispose;

  actor_class->paint = st_im_text_paint;
  actor_class->realize = st_im_text_realize;
  actor_class->unrealize = st_im_text_unrealize;

  actor_class->button_press_event = st_im_text_button_press_event;
  actor_class->key_press_event = st_im_text_key_press_event;
  actor_class->key_release_event = st_im_text_key_release_event;
  actor_class->key_focus_in = st_im_text_key_focus_in;
  actor_class->key_focus_out = st_im_text_key_focus_out;
}

static void
st_im_text_init (StIMText *self)
{
  StIMTextPrivate *priv;

  self->priv = priv = ST_IM_TEXT_GET_PRIVATE (self);

  priv->im_context = gtk_im_multicontext_new ();
  g_signal_connect (priv->im_context, "commit",
                    G_CALLBACK (st_im_text_commit_cb), self);
}

/**
 * st_im_text_new:
 * @text: text to set  to
 *
 * Create a new #StIMText with the specified text
 *
 * Returns: a new #ClutterActor
 */
ClutterActor *
st_im_text_new (const gchar *text)
{
  return g_object_new (ST_TYPE_IM_TEXT,
                       "text", text,
                       NULL);
}
