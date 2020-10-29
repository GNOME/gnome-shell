/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-entry.c: Plain entry actor
 *
 * Copyright 2008, 2009 Intel Corporation
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2010 Florian Müllner
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

/**
 * SECTION:st-entry
 * @short_description: Widget for displaying text
 *
 * #StEntry is a simple widget for displaying text. It derives from
 * #StWidget to add extra style and placement functionality over
 * #ClutterText. The internal #ClutterText is publicly accessibly to allow
 * applications to set further properties.
 *
 * #StEntry supports the following pseudo style states:
 *
 * - `focus`: the widget has focus
 * - `indeterminate`: the widget is showing the hint text or actor
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>

#include "st-entry.h"

#include "st-icon.h"
#include "st-label.h"
#include "st-settings.h"
#include "st-widget.h"
#include "st-texture-cache.h"
#include "st-clipboard.h"
#include "st-private.h"

#include "st-widget-accessible.h"


/* properties */
enum
{
  PROP_0,

  PROP_CLUTTER_TEXT,
  PROP_PRIMARY_ICON,
  PROP_SECONDARY_ICON,
  PROP_HINT_TEXT,
  PROP_HINT_ACTOR,
  PROP_TEXT,
  PROP_INPUT_PURPOSE,
  PROP_INPUT_HINTS,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

/* signals */
enum
{
  PRIMARY_ICON_CLICKED,
  SECONDARY_ICON_CLICKED,

  LAST_SIGNAL
};

#define ST_ENTRY_PRIV(x) st_entry_get_instance_private ((StEntry *) x)


typedef struct _StEntryPrivate StEntryPrivate;
struct _StEntryPrivate
{
  ClutterActor *entry;

  ClutterActor *primary_icon;
  ClutterActor *secondary_icon;

  ClutterActor *hint_actor;

  gfloat        spacing;

  gboolean      has_ibeam;

  CoglPipeline *text_shadow_material;
  gfloat        shadow_width;
  gfloat        shadow_height;
};

static guint entry_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (StEntry, st_entry, ST_TYPE_WIDGET);

static GType st_entry_accessible_get_type (void) G_GNUC_CONST;

static void
st_entry_set_property (GObject      *gobject,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  StEntry *entry = ST_ENTRY (gobject);

  switch (prop_id)
    {
    case PROP_PRIMARY_ICON:
      st_entry_set_primary_icon (entry, g_value_get_object (value));
      break;

    case PROP_SECONDARY_ICON:
      st_entry_set_secondary_icon (entry, g_value_get_object (value));
      break;

    case PROP_HINT_TEXT:
      st_entry_set_hint_text (entry, g_value_get_string (value));
      break;

    case PROP_HINT_ACTOR:
      st_entry_set_hint_actor (entry, g_value_get_object (value));
      break;

    case PROP_TEXT:
      st_entry_set_text (entry, g_value_get_string (value));
      break;

    case PROP_INPUT_PURPOSE:
      st_entry_set_input_purpose (entry, g_value_get_enum (value));
      break;

    case PROP_INPUT_HINTS:
      st_entry_set_input_hints (entry, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_entry_get_property (GObject    *gobject,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (gobject);

  switch (prop_id)
    {
    case PROP_CLUTTER_TEXT:
      g_value_set_object (value, priv->entry);
      break;

    case PROP_PRIMARY_ICON:
      g_value_set_object (value, priv->primary_icon);
      break;

    case PROP_SECONDARY_ICON:
      g_value_set_object (value, priv->secondary_icon);
      break;

    case PROP_HINT_TEXT:
      g_value_set_string (value, st_entry_get_hint_text (ST_ENTRY (gobject)));
      break;

    case PROP_HINT_ACTOR:
      g_value_set_object (value, priv->hint_actor);
      break;

    case PROP_TEXT:
      g_value_set_string (value, clutter_text_get_text (CLUTTER_TEXT (priv->entry)));
      break;

    case PROP_INPUT_PURPOSE:
      g_value_set_enum (value, clutter_text_get_input_purpose (CLUTTER_TEXT (priv->entry)));
      break;

    case PROP_INPUT_HINTS:
      g_value_set_flags (value, clutter_text_get_input_hints (CLUTTER_TEXT (priv->entry)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_entry_dispose (GObject *object)
{
  StEntry *entry = ST_ENTRY (object);
  StEntryPrivate *priv = ST_ENTRY_PRIV (entry);

  cogl_clear_object (&priv->text_shadow_material);

  G_OBJECT_CLASS (st_entry_parent_class)->dispose (object);
}

static void
st_entry_update_hint_visibility (StEntry *self)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (self);
  gboolean hint_visible =
    priv->hint_actor != NULL &&
    !clutter_text_has_preedit (CLUTTER_TEXT (priv->entry)) &&
    strcmp (clutter_text_get_text (CLUTTER_TEXT (priv->entry)), "") == 0;

  if (priv->hint_actor)
    g_object_set (priv->hint_actor, "visible", hint_visible, NULL);

  if (hint_visible)
    st_widget_add_style_pseudo_class (ST_WIDGET (self), "indeterminate");
  else
    st_widget_remove_style_pseudo_class (ST_WIDGET (self), "indeterminate");
}

static void
st_entry_style_changed (StWidget *self)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (self);
  StThemeNode *theme_node;
  ClutterColor color;
  gdouble size;

  cogl_clear_object (&priv->text_shadow_material);

  theme_node = st_widget_get_theme_node (self);

  _st_set_text_from_style (CLUTTER_TEXT (priv->entry), theme_node);

  if (st_theme_node_lookup_length (theme_node, "caret-size", TRUE, &size))
    clutter_text_set_cursor_size (CLUTTER_TEXT (priv->entry), (int)(.5 + size));

  if (st_theme_node_lookup_color (theme_node, "caret-color", TRUE, &color))
    clutter_text_set_cursor_color (CLUTTER_TEXT (priv->entry), &color);

  if (st_theme_node_lookup_color (theme_node, "selection-background-color", TRUE, &color))
    clutter_text_set_selection_color (CLUTTER_TEXT (priv->entry), &color);

  if (st_theme_node_lookup_color (theme_node, "selected-color", TRUE, &color))
    clutter_text_set_selected_text_color (CLUTTER_TEXT (priv->entry), &color);

  ST_WIDGET_CLASS (st_entry_parent_class)->style_changed (self);
}

static gboolean
st_entry_navigate_focus (StWidget         *widget,
                         ClutterActor     *from,
                         StDirectionType   direction)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (widget);

  /* This is basically the same as st_widget_real_navigate_focus(),
   * except that widget is behaving as a proxy for priv->entry (which
   * isn't an StWidget and so has no can-focus flag of its own).
   */

  if (from == priv->entry)
    return FALSE;
  else if (st_widget_get_can_focus (widget) &&
           clutter_actor_is_mapped (priv->entry))
    {
      clutter_actor_grab_key_focus (priv->entry);
      return TRUE;
    }
  else
    return FALSE;
}

static void
st_entry_get_preferred_width (ClutterActor *actor,
                              gfloat        for_height,
                              gfloat       *min_width_p,
                              gfloat       *natural_width_p)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (actor);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  gfloat hint_w, hint_min_w, icon_w;

  st_theme_node_adjust_for_height (theme_node, &for_height);

  clutter_actor_get_preferred_width (priv->entry, for_height,
                                     min_width_p,
                                     natural_width_p);

  if (priv->hint_actor)
    {
      clutter_actor_get_preferred_width (priv->hint_actor, -1,
                                         &hint_min_w, &hint_w);

      if (min_width_p && hint_min_w > *min_width_p)
        *min_width_p = hint_min_w;

      if (natural_width_p && hint_w > *natural_width_p)
        *natural_width_p = hint_w;
    }

  if (priv->primary_icon)
    {
      clutter_actor_get_preferred_width (priv->primary_icon, -1, NULL, &icon_w);

      if (min_width_p)
        *min_width_p += icon_w + priv->spacing;

      if (natural_width_p)
        *natural_width_p += icon_w + priv->spacing;
    }

  if (priv->secondary_icon)
    {
      clutter_actor_get_preferred_width (priv->secondary_icon,
                                         -1, NULL, &icon_w);

      if (min_width_p)
        *min_width_p += icon_w + priv->spacing;

      if (natural_width_p)
        *natural_width_p += icon_w + priv->spacing;
    }

  st_theme_node_adjust_preferred_width (theme_node, min_width_p, natural_width_p);
}

static void
st_entry_get_preferred_height (ClutterActor *actor,
                               gfloat        for_width,
                               gfloat       *min_height_p,
                               gfloat       *natural_height_p)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (actor);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  gfloat hint_h, icon_h;

  st_theme_node_adjust_for_width (theme_node, &for_width);

  clutter_actor_get_preferred_height (priv->entry, for_width,
                                      min_height_p,
                                      natural_height_p);

  if (priv->hint_actor)
    {
      clutter_actor_get_preferred_height (priv->hint_actor, -1, NULL, &hint_h);

      if (min_height_p && hint_h > *min_height_p)
        *min_height_p = hint_h;

      if (natural_height_p && hint_h > *natural_height_p)
        *natural_height_p = hint_h;
    }

  if (priv->primary_icon)
    {
      clutter_actor_get_preferred_height (priv->primary_icon,
                                          -1, NULL, &icon_h);

      if (min_height_p && icon_h > *min_height_p)
        *min_height_p = icon_h;

      if (natural_height_p && icon_h > *natural_height_p)
        *natural_height_p = icon_h;
    }

  if (priv->secondary_icon)
    {
      clutter_actor_get_preferred_height (priv->secondary_icon,
                                          -1, NULL, &icon_h);

      if (min_height_p && icon_h > *min_height_p)
        *min_height_p = icon_h;

      if (natural_height_p && icon_h > *natural_height_p)
        *natural_height_p = icon_h;
    }

  st_theme_node_adjust_preferred_height (theme_node, min_height_p, natural_height_p);
}

static void
st_entry_allocate (ClutterActor          *actor,
                   const ClutterActorBox *box)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (actor);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  ClutterActorBox content_box, child_box, icon_box, hint_box;
  gfloat icon_w, icon_h;
  gfloat hint_w, hint_min_w, hint_h;
  gfloat entry_h, min_h, pref_h, avail_h;
  ClutterActor *left_icon, *right_icon;
  gboolean is_rtl;

  is_rtl = clutter_actor_get_text_direction (actor) == CLUTTER_TEXT_DIRECTION_RTL;

  if (is_rtl)
    {
      right_icon = priv->primary_icon;
      left_icon = priv->secondary_icon;
    }
  else
    {
      left_icon = priv->primary_icon;
      right_icon = priv->secondary_icon;
    }

  clutter_actor_set_allocation (actor, box);

  st_theme_node_get_content_box (theme_node, box, &content_box);

  avail_h = content_box.y2 - content_box.y1;

  child_box.x1 = content_box.x1;
  child_box.x2 = content_box.x2;

  if (left_icon)
    {
      clutter_actor_get_preferred_width (left_icon, -1, NULL, &icon_w);
      clutter_actor_get_preferred_height (left_icon, -1, NULL, &icon_h);

      icon_box.x1 = content_box.x1;
      icon_box.x2 = icon_box.x1 + icon_w;

      icon_box.y1 = (int) (content_box.y1 + avail_h / 2 - icon_h / 2);
      icon_box.y2 = icon_box.y1 + icon_h;

      clutter_actor_allocate (left_icon, &icon_box);

      /* reduce the size for the entry */
      child_box.x1 = MIN (child_box.x2, child_box.x1 + icon_w + priv->spacing);
    }

  if (right_icon)
    {
      clutter_actor_get_preferred_width (right_icon, -1, NULL, &icon_w);
      clutter_actor_get_preferred_height (right_icon, -1, NULL, &icon_h);

      icon_box.x2 = content_box.x2;
      icon_box.x1 = icon_box.x2 - icon_w;

      icon_box.y1 = (int) (content_box.y1 + avail_h / 2 - icon_h / 2);
      icon_box.y2 = icon_box.y1 + icon_h;

      clutter_actor_allocate (right_icon, &icon_box);

      /* reduce the size for the entry */
      child_box.x2 = MAX (child_box.x1, child_box.x2 - icon_w - priv->spacing);
    }

  if (priv->hint_actor)
    {
      /* now allocate the hint actor */
      hint_box = child_box;

      clutter_actor_get_preferred_width (priv->hint_actor, -1, &hint_min_w, &hint_w);
      clutter_actor_get_preferred_height (priv->hint_actor, -1, NULL, &hint_h);

      hint_w = CLAMP (hint_w, hint_min_w, child_box.x2 - child_box.x1);

      if (is_rtl)
        hint_box.x1 = hint_box.x2 - hint_w;
      else
        hint_box.x2 = hint_box.x1 + hint_w;

      hint_box.y1 = ceil (content_box.y1 + avail_h / 2 - hint_h / 2);
      hint_box.y2 = hint_box.y1 + hint_h;

      clutter_actor_allocate (priv->hint_actor, &hint_box);
    }

  clutter_actor_get_preferred_height (priv->entry, child_box.x2 - child_box.x1,
                                      &min_h, &pref_h);

  entry_h = CLAMP (pref_h, min_h, avail_h);

  child_box.y1 = (int) (content_box.y1 + avail_h / 2 - entry_h / 2);
  child_box.y2 = child_box.y1 + entry_h;

  clutter_actor_allocate (priv->entry, &child_box);
}

static void
clutter_text_reactive_changed_cb (ClutterActor *text,
                                  GParamSpec   *pspec,
                                  gpointer      user_data)
{
  ClutterActor *stage;

  if (clutter_actor_get_reactive (text))
    return;

  if (!clutter_actor_has_key_focus (text))
    return;

  stage = clutter_actor_get_stage (text);
  if (stage == NULL)
    return;

  clutter_stage_set_key_focus (CLUTTER_STAGE (stage), NULL);
}

static void
clutter_text_focus_in_cb (ClutterText  *text,
                          ClutterActor *actor)
{
  st_widget_add_style_pseudo_class (ST_WIDGET (actor), "focus");
  clutter_text_set_cursor_visible (text, TRUE);
}

static void
clutter_text_focus_out_cb (ClutterText  *text,
                           ClutterActor *actor)
{
  st_widget_remove_style_pseudo_class (ST_WIDGET (actor), "focus");
  clutter_text_set_cursor_visible (text, FALSE);
}

static void
clutter_text_cursor_changed (ClutterText *text,
                             StEntry     *entry)
{
  st_entry_update_hint_visibility (entry);
}

static void
clutter_text_changed_cb (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  StEntry *entry = ST_ENTRY (user_data);
  StEntryPrivate *priv = ST_ENTRY_PRIV (entry);

  st_entry_update_hint_visibility (entry);

  /* Since the text changed, force a regen of the shadow texture */
  cogl_clear_object (&priv->text_shadow_material);

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_TEXT]);
}

static void
st_entry_clipboard_callback (StClipboard *clipboard,
                             const gchar *text,
                             gpointer     data)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (data);
  ClutterText *ctext = (ClutterText*)priv->entry;
  gint cursor_pos;

  if (!text)
    return;

  /* delete the current selection before pasting */
  clutter_text_delete_selection (ctext);

  /* "paste" the clipboard text into the entry */
  cursor_pos = clutter_text_get_cursor_position (ctext);
  clutter_text_insert_text (ctext, text, cursor_pos);
}

static gboolean
clutter_text_button_press_event (ClutterActor       *actor,
                                 ClutterButtonEvent *event,
                                 gpointer            user_data)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (user_data);

  if (event->button == 2 &&
      clutter_text_get_editable (CLUTTER_TEXT (priv->entry)))
    {
      StSettings *settings;
      gboolean primary_paste_enabled;

      settings = st_settings_get ();
      g_object_get (settings, "primary-paste", &primary_paste_enabled, NULL);

      if (primary_paste_enabled)
        {
          StClipboard *clipboard;

          clipboard = st_clipboard_get_default ();

          /* By the time the clipboard callback is called,
           * the rest of the signal handlers will have
           * run, making the text cursor to be in the correct
           * place.
           */
          st_clipboard_get_text (clipboard,
                                 ST_CLIPBOARD_TYPE_PRIMARY,
                                 st_entry_clipboard_callback,
                                 user_data);
        }
    }

  return FALSE;
}

static gboolean
st_entry_key_press_event (ClutterActor    *actor,
                          ClutterKeyEvent *event)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (actor);

  /* This is expected to handle events that were emitted for the inner
     ClutterText. They only reach this function if the ClutterText
     didn't handle them */

  /* paste */
  if (((event->modifier_state & CLUTTER_CONTROL_MASK)
       && event->keyval == CLUTTER_KEY_v) ||
      ((event->modifier_state & CLUTTER_CONTROL_MASK)
       && event->keyval == CLUTTER_KEY_V) ||
      ((event->modifier_state & CLUTTER_SHIFT_MASK)
       && event->keyval == CLUTTER_KEY_Insert))
    {
      StClipboard *clipboard;

      clipboard = st_clipboard_get_default ();

      st_clipboard_get_text (clipboard,
                             ST_CLIPBOARD_TYPE_CLIPBOARD,
                             st_entry_clipboard_callback,
                             actor);

      return TRUE;
    }

  /* copy */
  if ((event->modifier_state & CLUTTER_CONTROL_MASK)
      && (event->keyval == CLUTTER_KEY_c || event->keyval == CLUTTER_KEY_C) &&
      clutter_text_get_password_char ((ClutterText*) priv->entry) == 0)
    {
      StClipboard *clipboard;
      gchar *text;

      clipboard = st_clipboard_get_default ();

      text = clutter_text_get_selection ((ClutterText*) priv->entry);

      if (text && strlen (text))
        st_clipboard_set_text (clipboard,
                               ST_CLIPBOARD_TYPE_CLIPBOARD,
                               text);

      g_free (text);

      return TRUE;
    }


  /* cut */
  if ((event->modifier_state & CLUTTER_CONTROL_MASK)
      && (event->keyval == CLUTTER_KEY_x || event->keyval == CLUTTER_KEY_X) &&
      clutter_text_get_password_char ((ClutterText*) priv->entry) == 0)
    {
      StClipboard *clipboard;
      gchar *text;

      clipboard = st_clipboard_get_default ();

      text = clutter_text_get_selection ((ClutterText*) priv->entry);

      if (text && strlen (text))
        {
          st_clipboard_set_text (clipboard,
                                 ST_CLIPBOARD_TYPE_CLIPBOARD,
                                 text);

          /* now delete the text */
          clutter_text_delete_selection ((ClutterText *) priv->entry);
        }

      g_free (text);

      return TRUE;
    }


  /* delete to beginning of line */
  if ((event->modifier_state & CLUTTER_CONTROL_MASK) &&
      (event->keyval == CLUTTER_KEY_u || event->keyval == CLUTTER_KEY_U))
    {
      int pos = clutter_text_get_cursor_position ((ClutterText *)priv->entry);
      clutter_text_delete_text ((ClutterText *)priv->entry, 0, pos);

      return TRUE;
    }


  /* delete to end of line */
  if ((event->modifier_state & CLUTTER_CONTROL_MASK) &&
      (event->keyval == CLUTTER_KEY_k || event->keyval == CLUTTER_KEY_K))
    {
      ClutterTextBuffer *buffer = clutter_text_get_buffer ((ClutterText *)priv->entry);
      int pos = clutter_text_get_cursor_position ((ClutterText *)priv->entry);
      clutter_text_buffer_delete_text (buffer, pos, -1);

      return TRUE;
    }

  return CLUTTER_ACTOR_CLASS (st_entry_parent_class)->key_press_event (actor, event);
}

static void
st_entry_key_focus_in (ClutterActor *actor)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (actor);

  /* We never want key focus. The ClutterText should be given first
     pass for all key events */
  clutter_actor_grab_key_focus (priv->entry);
}

static StEntryCursorFunc cursor_func = NULL;
static gpointer          cursor_func_data = NULL;

/**
 * st_entry_set_cursor_func: (skip)
 *
 * This function is for private use by libgnome-shell.
 * Do not ever use.
 */
void
st_entry_set_cursor_func (StEntryCursorFunc func,
                          gpointer          data)
{
  cursor_func = func;
  cursor_func_data = data;
}

static void
st_entry_set_cursor (StEntry  *entry,
                     gboolean  use_ibeam)
{
  if (cursor_func)
    cursor_func (entry, use_ibeam, cursor_func_data);

  ((StEntryPrivate *)ST_ENTRY_PRIV (entry))->has_ibeam = use_ibeam;
}

static gboolean
st_entry_enter_event (ClutterActor         *actor,
                      ClutterCrossingEvent *event)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (actor);
  if (event->source == priv->entry && event->related != NULL)
    st_entry_set_cursor (ST_ENTRY (actor), TRUE);

  return CLUTTER_ACTOR_CLASS (st_entry_parent_class)->enter_event (actor, event);
}

static gboolean
st_entry_leave_event (ClutterActor         *actor,
                      ClutterCrossingEvent *event)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (actor);
  if (event->source == priv->entry && event->related != NULL)
    st_entry_set_cursor (ST_ENTRY (actor), FALSE);

  return CLUTTER_ACTOR_CLASS (st_entry_parent_class)->leave_event (actor, event);
}

static void
st_entry_paint (ClutterActor        *actor,
                ClutterPaintContext *paint_context)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (actor);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  StShadow *shadow_spec = st_theme_node_get_text_shadow (theme_node);
  ClutterActorClass *parent_class;

  st_widget_paint_background (ST_WIDGET (actor), paint_context);

  if (shadow_spec)
    {
      ClutterActorBox allocation;
      float width, height;

      clutter_actor_get_allocation_box (priv->entry, &allocation);
      clutter_actor_box_get_size (&allocation, &width, &height);

      if (priv->text_shadow_material == NULL ||
          width != priv->shadow_width ||
          height != priv->shadow_height)
        {
          CoglPipeline *material;

          cogl_clear_object (&priv->text_shadow_material);

          material = _st_create_shadow_pipeline_from_actor (shadow_spec,
                                                            priv->entry);

          priv->shadow_width = width;
          priv->shadow_height = height;
          priv->text_shadow_material = material;
        }

      if (priv->text_shadow_material != NULL)
        {
          CoglFramebuffer *framebuffer =
            clutter_paint_context_get_framebuffer (paint_context);

          _st_paint_shadow_with_opacity (shadow_spec,
                                         framebuffer,
                                         priv->text_shadow_material,
                                         &allocation,
                                         clutter_actor_get_paint_opacity (priv->entry));
        }
    }

  /* Since we paint the background ourselves, chain to the parent class
   * of StWidget, to avoid painting it twice.
   * This is needed as we still want to paint children.
   */
  parent_class = g_type_class_peek_parent (st_entry_parent_class);
  parent_class->paint (actor, paint_context);
}

static void
st_entry_unmap (ClutterActor *actor)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (actor);
  if (priv->has_ibeam)
    st_entry_set_cursor (ST_ENTRY (actor), FALSE);

  CLUTTER_ACTOR_CLASS (st_entry_parent_class)->unmap (actor);
}

static gboolean
st_entry_get_paint_volume (ClutterActor       *actor,
                           ClutterPaintVolume *volume)
{
  return clutter_paint_volume_set_from_allocation (volume, actor);
}

static void
st_entry_class_init (StEntryClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  gobject_class->set_property = st_entry_set_property;
  gobject_class->get_property = st_entry_get_property;
  gobject_class->dispose = st_entry_dispose;

  actor_class->get_preferred_width = st_entry_get_preferred_width;
  actor_class->get_preferred_height = st_entry_get_preferred_height;
  actor_class->allocate = st_entry_allocate;
  actor_class->paint = st_entry_paint;
  actor_class->unmap = st_entry_unmap;
  actor_class->get_paint_volume = st_entry_get_paint_volume;

  actor_class->key_press_event = st_entry_key_press_event;
  actor_class->key_focus_in = st_entry_key_focus_in;

  actor_class->enter_event = st_entry_enter_event;
  actor_class->leave_event = st_entry_leave_event;

  widget_class->style_changed = st_entry_style_changed;
  widget_class->navigate_focus = st_entry_navigate_focus;
  widget_class->get_accessible_type = st_entry_accessible_get_type;

  /**
   * StEntry:clutter-text:
   *
   * The internal #ClutterText actor supporting the #StEntry.
   */
  props[PROP_CLUTTER_TEXT] =
    g_param_spec_object ("clutter-text",
                         "Clutter Text",
                         "Internal ClutterText actor",
                         CLUTTER_TYPE_TEXT,
                         ST_PARAM_READABLE);

  /**
   * StEntry:primary-icon:
   *
   * The #ClutterActor acting as the primary icon at the start of the #StEntry.
   */
  props[PROP_PRIMARY_ICON] =
    g_param_spec_object ("primary-icon",
                         "Primary Icon",
                         "Primary Icon actor",
                         CLUTTER_TYPE_ACTOR,
                         ST_PARAM_READWRITE);

  /**
   * StEntry:secondary-icon:
   *
   * The #ClutterActor acting as the secondary icon at the end of the #StEntry.
   */
  props[PROP_SECONDARY_ICON] =
    g_param_spec_object ("secondary-icon",
                         "Secondary Icon",
                         "Secondary Icon actor",
                         CLUTTER_TYPE_ACTOR,
                         ST_PARAM_READWRITE);

  /**
   * StEntry:hint-text:
   *
   * The text to display when the entry is empty and unfocused. Setting this
   * will replace the actor of #StEntry::hint-actor.
   */
  props[PROP_HINT_TEXT] =
    g_param_spec_string ("hint-text",
                         "Hint Text",
                         "Text to display when the entry is not focused "
                         "and the text property is empty",
                         NULL,
                         ST_PARAM_READWRITE);

  /**
   * StEntry:hint-actor:
   *
   * A #ClutterActor to display when the entry is empty and unfocused. Setting
   * this will replace the actor displaying #StEntry:hint-text.
   */
  props[PROP_HINT_ACTOR] =
    g_param_spec_object ("hint-actor",
                         "Hint Actor",
                         "An actor to display when the entry is not focused "
                         "and the text property is empty",
                         CLUTTER_TYPE_ACTOR,
                         ST_PARAM_READWRITE);

  /**
   * StEntry:text:
   *
   * The current text value of the #StEntry.
   */
  props[PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "Text of the entry",
                         NULL,
                         ST_PARAM_READWRITE);

  /**
   * StEntry:input-purpose:
   *
   * The #ClutterInputContentPurpose that helps on-screen keyboards and similar
   * input methods to decide which keys should be presented to the user.
   */
  props[PROP_INPUT_PURPOSE] =
    g_param_spec_enum ("input-purpose",
                       "Purpose",
                       "Purpose of the text field",
                       CLUTTER_TYPE_INPUT_CONTENT_PURPOSE,
                       CLUTTER_INPUT_CONTENT_PURPOSE_NORMAL,
                       ST_PARAM_READWRITE);

  /**
   * StEntry:input-hints:
   *
   * The #ClutterInputContentHintFlags providing additional hints (beyond
   * #StEntry:input-purpose) that allow input methods to fine-tune their
   * behaviour.
   */
  props[PROP_INPUT_HINTS] =
    g_param_spec_flags ("input-hints",
                        "hints",
                        "Hints for the text field behaviour",
                        CLUTTER_TYPE_INPUT_CONTENT_HINT_FLAGS,
                        0,
                        ST_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, N_PROPS, props);

  /* signals */
  /**
   * StEntry::primary-icon-clicked:
   * @self: the #StEntry
   *
   * Emitted when the primary icon is clicked.
   */
  entry_signals[PRIMARY_ICON_CLICKED] =
    g_signal_new ("primary-icon-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StEntryClass, primary_icon_clicked),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * StEntry::secondary-icon-clicked:
   * @self: the #StEntry
   *
   * Emitted when the secondary icon is clicked.
   */
  entry_signals[SECONDARY_ICON_CLICKED] =
    g_signal_new ("secondary-icon-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StEntryClass, secondary_icon_clicked),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
st_entry_init (StEntry *entry)
{
  StEntryPrivate *priv;

  priv = st_entry_get_instance_private (entry);

  priv->entry = g_object_new (CLUTTER_TYPE_TEXT,
                              "line-alignment", PANGO_ALIGN_LEFT,
                              "editable", TRUE,
                              "reactive", TRUE,
                              "single-line-mode", TRUE,
                              NULL);

  g_object_bind_property (G_OBJECT (entry), "reactive",
                          priv->entry, "reactive",
                          G_BINDING_DEFAULT);

  g_signal_connect(priv->entry, "notify::reactive",
                   G_CALLBACK (clutter_text_reactive_changed_cb), entry);

  g_signal_connect (priv->entry, "key-focus-in",
                    G_CALLBACK (clutter_text_focus_in_cb), entry);

  g_signal_connect (priv->entry, "key-focus-out",
                    G_CALLBACK (clutter_text_focus_out_cb), entry);

  g_signal_connect (priv->entry, "button-press-event",
                    G_CALLBACK (clutter_text_button_press_event), entry);

  g_signal_connect (priv->entry, "cursor-changed",
                    G_CALLBACK (clutter_text_cursor_changed), entry);

  g_signal_connect (priv->entry, "notify::text",
                    G_CALLBACK (clutter_text_changed_cb), entry);

  priv->spacing = 6.0f;

  priv->text_shadow_material = NULL;
  priv->shadow_width = -1.;
  priv->shadow_height = -1.;

  clutter_actor_add_child (CLUTTER_ACTOR (entry), priv->entry);
  clutter_actor_set_reactive ((ClutterActor *) entry, TRUE);

  /* set cursor hidden until we receive focus */
  clutter_text_set_cursor_visible ((ClutterText *) priv->entry, FALSE);
}

/**
 * st_entry_new:
 * @text: (nullable): text to set the entry to
 *
 * Create a new #StEntry with the specified text.
 *
 * Returns: a new #StEntry
 */
StWidget *
st_entry_new (const gchar *text)
{
  StWidget *entry;

  /* add the entry to the stage, but don't allow it to be visible */
  entry = g_object_new (ST_TYPE_ENTRY,
                        "text", text,
                        NULL);

  return entry;
}

/**
 * st_entry_get_text:
 * @entry: a #StEntry
 *
 * Get the text displayed on the entry. If @entry is empty, an empty string will
 * be returned instead of %NULL.
 *
 * Returns: (transfer none): the text for the entry
 */
const gchar *
st_entry_get_text (StEntry *entry)
{
  StEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_ENTRY (entry), NULL);

  priv = st_entry_get_instance_private (entry);

  return clutter_text_get_text (CLUTTER_TEXT (priv->entry));
}

/**
 * st_entry_set_text:
 * @entry: a #StEntry
 * @text: (nullable): text to set the entry to
 *
 * Sets the text displayed on the entry. If @text is %NULL, the #ClutterText
 * will instead be set to an empty string.
 */
void
st_entry_set_text (StEntry     *entry,
                   const gchar *text)
{
  StEntryPrivate *priv;

  g_return_if_fail (ST_IS_ENTRY (entry));

  priv = st_entry_get_instance_private (entry);

  clutter_text_set_text (CLUTTER_TEXT (priv->entry), text);

  /* Note: PROP_TEXT will get notfied from our notify::text handler connected
   * to priv->entry. */
}

/**
 * st_entry_get_clutter_text:
 * @entry: a #StEntry
 *
 * Retrieve the internal #ClutterText so that extra parameters can be set.
 *
 * Returns: (transfer none): the #ClutterText used by @entry
 */
ClutterActor*
st_entry_get_clutter_text (StEntry *entry)
{
  g_return_val_if_fail (ST_ENTRY (entry), NULL);

  return ((StEntryPrivate *)ST_ENTRY_PRIV (entry))->entry;
}

/**
 * st_entry_set_hint_text:
 * @entry: a #StEntry
 * @text: (nullable): text to set as the entry hint
 *
 * Sets the text to display when the entry is empty and unfocused. When the
 * entry is displaying the hint, it has a pseudo class of `indeterminate`.
 * A value of %NULL unsets the hint.
 */
void
st_entry_set_hint_text (StEntry     *entry,
                        const gchar *text)
{
  StWidget *label;

  g_return_if_fail (ST_IS_ENTRY (entry));

  label = st_label_new (text);
  st_widget_add_style_class_name (label, "hint-text");

  st_entry_set_hint_actor (ST_ENTRY (entry), CLUTTER_ACTOR (label));
}

/**
 * st_entry_get_hint_text:
 * @entry: a #StEntry
 *
 * Gets the text that is displayed when the entry is empty and unfocused or
 * %NULL if the #StEntry:hint-actor was set to an actor that is not a #StLabel.
 *
 * Unlike st_entry_get_text() this function may return %NULL if
 * #StEntry:hint-actor is not a #StLabel.
 *
 * Returns: (nullable) (transfer none): the current value of the hint property
 */
const gchar *
st_entry_get_hint_text (StEntry *entry)
{
  StEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_ENTRY (entry), NULL);

  priv = ST_ENTRY_PRIV (entry);

  if (priv->hint_actor != NULL && ST_IS_LABEL (priv->hint_actor))
    return st_label_get_text (ST_LABEL (priv->hint_actor));

  return NULL;
}

/**
 * st_entry_set_input_purpose:
 * @entry: a #StEntry
 * @purpose: the purpose
 *
 * Sets the #StEntry:input-purpose property which
 * can be used by on-screen keyboards and other input
 * methods to adjust their behaviour.
 */
void
st_entry_set_input_purpose (StEntry                    *entry,
                            ClutterInputContentPurpose  purpose)
{
  StEntryPrivate *priv;
  ClutterText *editable;

  g_return_if_fail (ST_IS_ENTRY (entry));

  priv = st_entry_get_instance_private (entry);
  editable = CLUTTER_TEXT (priv->entry);

  if (clutter_text_get_input_purpose (editable) != purpose)
    {
      clutter_text_set_input_purpose (editable, purpose);

      g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_INPUT_PURPOSE]);
    }
}

/**
 * st_entry_get_input_purpose:
 * @entry: a #StEntry
 *
 * Gets the value of the #StEntry:input-purpose property.
 *
 * Returns: the input purpose of the entry
 */
ClutterInputContentPurpose
st_entry_get_input_purpose (StEntry *entry)
{
  StEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_ENTRY (entry), CLUTTER_INPUT_CONTENT_PURPOSE_NORMAL);

  priv = st_entry_get_instance_private (entry);
  return clutter_text_get_input_purpose (CLUTTER_TEXT (priv->entry));
}

/**
 * st_entry_set_input_hints:
 * @entry: a #StEntry
 * @hints: the hints
 *
 * Sets the #StEntry:input-hints property, which
 * allows input methods to fine-tune their behaviour.
 */
void
st_entry_set_input_hints (StEntry                      *entry,
                          ClutterInputContentHintFlags  hints)
{
  StEntryPrivate *priv;
  ClutterText *editable;

  g_return_if_fail (ST_IS_ENTRY (entry));

  priv = st_entry_get_instance_private (entry);
  editable = CLUTTER_TEXT (priv->entry);

  if (clutter_text_get_input_hints (editable) != hints)
    {
      clutter_text_set_input_hints (editable, hints);

      g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_INPUT_HINTS]);
    }
}

/**
 * st_entry_get_input_hints:
 * @entry: a #StEntry
 *
 * Gets the value of the #StEntry:input-hints property.
 *
 * Returns: the input hints for the entry
 */
ClutterInputContentHintFlags
st_entry_get_input_hints (StEntry *entry)
{
  StEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_ENTRY (entry), 0);

  priv = st_entry_get_instance_private (entry);
  return clutter_text_get_input_hints (CLUTTER_TEXT (priv->entry));
}

static void
_st_entry_icon_clicked_cb (ClutterClickAction *action,
                           ClutterActor       *actor,
                           StEntry            *entry)
{
  StEntryPrivate *priv = ST_ENTRY_PRIV (entry);

  if (!clutter_actor_get_reactive (CLUTTER_ACTOR (entry)))
    return;

  if (actor == priv->primary_icon)
    g_signal_emit (entry, entry_signals[PRIMARY_ICON_CLICKED], 0);
  else
    g_signal_emit (entry, entry_signals[SECONDARY_ICON_CLICKED], 0);
}

static void
_st_entry_set_icon (StEntry       *entry,
                    ClutterActor **icon,
                    ClutterActor  *new_icon)
{
  if (*icon)
    {
      clutter_actor_remove_action_by_name (*icon, "entry-icon-action");
      clutter_actor_remove_child (CLUTTER_ACTOR (entry), *icon);
      *icon = NULL;
    }

  if (new_icon)
    {
      ClutterAction *action;

      *icon = g_object_ref (new_icon);

      clutter_actor_set_reactive (*icon, TRUE);
      clutter_actor_add_child (CLUTTER_ACTOR (entry), *icon);

      action = clutter_click_action_new ();
      clutter_actor_add_action_with_name (*icon, "entry-icon-action", action);
      g_signal_connect (action, "clicked",
                        G_CALLBACK (_st_entry_icon_clicked_cb), entry);
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (entry));
}

/**
 * st_entry_set_primary_icon:
 * @entry: a #StEntry
 * @icon: (nullable): a #ClutterActor
 *
 * Set the primary icon of the entry to @icon.
 */
void
st_entry_set_primary_icon (StEntry      *entry,
                           ClutterActor *icon)
{
  StEntryPrivate *priv;

  g_return_if_fail (ST_IS_ENTRY (entry));

  priv = st_entry_get_instance_private (entry);

  _st_entry_set_icon (entry, &priv->primary_icon, icon);
}

/**
 * st_entry_get_primary_icon:
 * @entry: a #StEntry
 *
 * Get the value of the #StEntry:primary-icon property.
 *
 * Returns: (nullable) (transfer none): a #ClutterActor
 */
ClutterActor *
st_entry_get_primary_icon (StEntry *entry)
{
  StEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_ENTRY (entry), NULL);

  priv = ST_ENTRY_PRIV (entry);
  return priv->primary_icon;
}

/**
 * st_entry_set_secondary_icon:
 * @entry: a #StEntry
 * @icon: (nullable): an #ClutterActor
 *
 * Set the secondary icon of the entry to @icon.
 */
void
st_entry_set_secondary_icon (StEntry      *entry,
                             ClutterActor *icon)
{
  StEntryPrivate *priv;

  g_return_if_fail (ST_IS_ENTRY (entry));

  priv = st_entry_get_instance_private (entry);

  _st_entry_set_icon (entry, &priv->secondary_icon, icon);
}

/**
 * st_entry_get_secondary_icon:
 * @entry: a #StEntry
 *
 * Get the value of the #StEntry:secondary-icon property.
 *
 * Returns: (nullable) (transfer none): a #ClutterActor
 */
ClutterActor *
st_entry_get_secondary_icon (StEntry *entry)
{
  StEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_ENTRY (entry), NULL);

  priv = ST_ENTRY_PRIV (entry);
  return priv->secondary_icon;
}

/**
 * st_entry_set_hint_actor:
 * @entry: a #StEntry
 * @hint_actor: (nullable): a #ClutterActor
 *
 * Set the hint actor of the entry to @hint_actor.
 */
void
st_entry_set_hint_actor (StEntry      *entry,
                         ClutterActor *hint_actor)
{
  StEntryPrivate *priv;

  g_return_if_fail (ST_IS_ENTRY (entry));

  priv = ST_ENTRY_PRIV (entry);

  if (priv->hint_actor != NULL)
    {
      clutter_actor_remove_child (CLUTTER_ACTOR (entry), priv->hint_actor);
      priv->hint_actor = NULL;
    }

  if (hint_actor != NULL)
    {
      priv->hint_actor = hint_actor;
      clutter_actor_add_child (CLUTTER_ACTOR (entry), priv->hint_actor);
    }

  st_entry_update_hint_visibility (entry);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (entry));
}

/**
 * st_entry_get_hint_actor:
 * @entry: a #StEntry
 *
 * Get the value of the #StEntry:hint-actor property.
 *
 * Returns: (nullable) (transfer none): a #ClutterActor
 */
ClutterActor *
st_entry_get_hint_actor (StEntry *entry)
{
  StEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_ENTRY (entry), NULL);

  priv = ST_ENTRY_PRIV (entry);
  return priv->hint_actor;
}

/******************************************************************************/
/*************************** ACCESSIBILITY SUPPORT ****************************/
/******************************************************************************/

#define ST_TYPE_ENTRY_ACCESSIBLE         (st_entry_accessible_get_type ())
#define ST_ENTRY_ACCESSIBLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ST_TYPE_ENTRY_ACCESSIBLE, StEntryAccessible))
#define ST_IS_ENTRY_ACCESSIBLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ST_TYPE_ENTRY_ACCESSIBLE))
#define ST_ENTRY_ACCESSIBLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    ST_TYPE_ENTRY_ACCESSIBLE, StEntryAccessibleClass))
#define ST_IS_ENTRY_ACCESSIBLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    ST_TYPE_ENTRY_ACCESSIBLE))
#define ST_ENTRY_ACCESSIBLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  ST_TYPE_ENTRY_ACCESSIBLE, StEntryAccessibleClass))

typedef struct _StEntryAccessible  StEntryAccessible;
typedef struct _StEntryAccessibleClass  StEntryAccessibleClass;

struct _StEntryAccessible
{
  StWidgetAccessible parent;
};

struct _StEntryAccessibleClass
{
  StWidgetAccessibleClass parent_class;
};

G_DEFINE_TYPE (StEntryAccessible, st_entry_accessible, ST_TYPE_WIDGET_ACCESSIBLE)

static void
st_entry_accessible_init (StEntryAccessible *self)
{
  /* initialization done on AtkObject->initialize */
}

static void
st_entry_accessible_initialize (AtkObject *obj,
                                gpointer   data)
{
  ATK_OBJECT_CLASS (st_entry_accessible_parent_class)->initialize (obj, data);

  /* StEntry is behaving as a ClutterText container */
  atk_object_set_role (obj, ATK_ROLE_PANEL);
}

static gint
st_entry_accessible_get_n_children (AtkObject *obj)
{
  StEntry *entry = NULL;
  StEntryPrivate *priv;

  g_return_val_if_fail (ST_IS_ENTRY_ACCESSIBLE (obj), 0);

  entry = ST_ENTRY (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj)));

  if (entry == NULL)
    return 0;

  priv = st_entry_get_instance_private (entry);
  if (priv->entry == NULL)
    return 0;
  else
    return 1;
}

static AtkObject*
st_entry_accessible_ref_child (AtkObject *obj,
                               gint       i)
{
  StEntry *entry = NULL;
  StEntryPrivate *priv;
  AtkObject *result = NULL;

  g_return_val_if_fail (ST_IS_ENTRY_ACCESSIBLE (obj), NULL);
  g_return_val_if_fail (i == 0, NULL);

  entry = ST_ENTRY (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj)));

  if (entry == NULL)
    return NULL;

  priv = st_entry_get_instance_private (entry);
  if (priv->entry == NULL)
    return NULL;

  result = clutter_actor_get_accessible (priv->entry);
  g_object_ref (result);

  return result;
}


static void
st_entry_accessible_class_init (StEntryAccessibleClass *klass)
{
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  atk_class->initialize = st_entry_accessible_initialize;
  atk_class->get_n_children = st_entry_accessible_get_n_children;
  atk_class->ref_child= st_entry_accessible_ref_child;
}
