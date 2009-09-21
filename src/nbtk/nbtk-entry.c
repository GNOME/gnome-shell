/*
 * nbtk-entry.c: Plain entry actor
 *
 * Copyright 2008, 2009 Intel Corporation
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
 *
 * Written by: Thomas Wood <thomas.wood@intel.com>
 *
 */

/**
 * SECTION:nbtk-entry
 * @short_description: Widget for displaying text
 *
 * #NbtkEntry is a simple widget for displaying text. It derives from
 * #NbtkWidget to add extra style and placement functionality over
 * #ClutterText. The internal #ClutterText is publicly accessibly to allow
 * applications to set further properties.
 *
 * #NbtkEntry supports the following pseudo style states:
 * <itemizedlist>
 *  <listitem>
 *   <para>focus: the widget has focus</para>
 *  </listitem>
 *  <listitem>
 *   <para>indeterminate: the widget is showing the hint text</para>
 *  </listitem>
 * </itemizedlist>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>
#include <clutter-imcontext/clutter-imtext.h>

#include "nbtk-entry.h"

#include "nbtk-widget.h"
#include "nbtk-texture-cache.h"
#include "nbtk-marshal.h"
#include "nbtk-clipboard.h"

#define HAS_FOCUS(actor) (clutter_actor_get_stage (actor) && clutter_stage_get_key_focus ((ClutterStage *) clutter_actor_get_stage (actor)) == actor)


/* properties */
enum
{
  PROP_0,

  PROP_CLUTTER_TEXT,
  PROP_HINT_TEXT,
  PROP_TEXT,
};

/* signals */
enum
{
  PRIMARY_ICON_CLICKED,
  SECONDARY_ICON_CLICKED,

  LAST_SIGNAL
};

#define NBTK_ENTRY_GET_PRIVATE(obj)     (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NBTK_TYPE_ENTRY, NbtkEntryPrivate))
#define NBTK_ENTRY_PRIV(x) ((NbtkEntry *) x)->priv


struct _NbtkEntryPrivate
{
  ClutterActor *entry;
  gchar        *hint;

  ClutterActor *primary_icon;
  ClutterActor *secondary_icon;

  gfloat   spacing;
};

static guint entry_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (NbtkEntry, nbtk_entry, NBTK_TYPE_WIDGET);

static void
nbtk_entry_set_property (GObject      *gobject,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  NbtkEntry *entry = NBTK_ENTRY (gobject);

  switch (prop_id)
    {
    case PROP_HINT_TEXT:
      nbtk_entry_set_hint_text (entry, g_value_get_string (value));
      break;

    case PROP_TEXT:
      nbtk_entry_set_text (entry, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_entry_get_property (GObject    *gobject,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (gobject);

  switch (prop_id)
    {
    case PROP_CLUTTER_TEXT:
      g_value_set_object (value, priv->entry);
      break;

    case PROP_HINT_TEXT:
      g_value_set_string (value, priv->hint);
      break;

    case PROP_TEXT:
      g_value_set_string (value, clutter_text_get_text (CLUTTER_TEXT (priv->entry)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_entry_dispose (GObject *object)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (object);

  if (priv->entry)
    {
      clutter_actor_unparent (priv->entry);
      priv->entry = NULL;
    }
}

static void
nbtk_entry_finalize (GObject *object)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (object);

  g_free (priv->hint);
  priv->hint = NULL;
}

static void
nbtk_entry_style_changed (NbtkWidget *self)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (self);
  ShellThemeNode *theme_node;
  ClutterColor color;
  const PangoFontDescription *font;
  gchar *font_string;

  theme_node = nbtk_widget_get_theme_node (self);

  shell_theme_node_get_foreground_color (theme_node, &color);
  clutter_text_set_color (CLUTTER_TEXT (priv->entry), &color);

  if (shell_theme_node_get_color (theme_node, "caret-color", FALSE, &color))
      clutter_text_set_cursor_color (CLUTTER_TEXT (priv->entry), &color);

  if (shell_theme_node_get_color (theme_node, "selection-background-color", FALSE, &color))
    clutter_text_set_selection_color (CLUTTER_TEXT (priv->entry), &color);

  font = shell_theme_node_get_font (theme_node);
  font_string = pango_font_description_to_string (font);
  clutter_text_set_font_name (CLUTTER_TEXT (priv->entry), font_string);
  g_free (font_string);

  NBTK_WIDGET_CLASS (nbtk_entry_parent_class)->style_changed (self);
}

static void
nbtk_entry_get_preferred_width (ClutterActor *actor,
                                gfloat   for_height,
                                gfloat  *min_width_p,
                                gfloat  *natural_width_p)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (actor);
  ShellThemeNode *theme_node = nbtk_widget_get_theme_node (NBTK_WIDGET (actor));
  gfloat icon_w;

  shell_theme_node_adjust_for_height (theme_node, &for_height);

  clutter_actor_get_preferred_width (priv->entry, for_height,
                                     min_width_p,
                                     natural_width_p);

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

  shell_theme_node_adjust_preferred_width (theme_node, min_width_p, natural_width_p);
}

static void
nbtk_entry_get_preferred_height (ClutterActor *actor,
                                 gfloat   for_width,
                                 gfloat  *min_height_p,
                                 gfloat  *natural_height_p)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (actor);
  ShellThemeNode *theme_node = nbtk_widget_get_theme_node (NBTK_WIDGET (actor));
  gfloat icon_h;

  shell_theme_node_adjust_for_width (theme_node, &for_width);

  clutter_actor_get_preferred_height (priv->entry, for_width,
                                      min_height_p,
                                      natural_height_p);

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

  shell_theme_node_adjust_preferred_height (theme_node, min_height_p, natural_height_p);
}

static void
nbtk_entry_allocate (ClutterActor          *actor,
                     const ClutterActorBox *box,
                     ClutterAllocationFlags flags)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (actor);
  ShellThemeNode *theme_node = nbtk_widget_get_theme_node (NBTK_WIDGET (actor));
  ClutterActorClass *parent_class;
  ClutterActorBox content_box, child_box, icon_box;
  gfloat icon_w, icon_h;
  gfloat entry_h, min_h, pref_h, avail_h;

  parent_class = CLUTTER_ACTOR_CLASS (nbtk_entry_parent_class);
  parent_class->allocate (actor, box, flags);

  shell_theme_node_get_content_box (theme_node, box, &content_box);

  avail_h = content_box.y2 - content_box.y1;

  child_box.x1 = content_box.x1;
  child_box.x2 = content_box.x2;

  if (priv->primary_icon)
    {
      clutter_actor_get_preferred_width (priv->primary_icon,
                                         -1, NULL, &icon_w);
      clutter_actor_get_preferred_height (priv->primary_icon,
                                          -1, NULL, &icon_h);

      icon_box.x1 = content_box.x1;
      icon_box.x2 = icon_box.x1 + icon_w;

      icon_box.y1 = (int) (content_box.y1 + avail_h / 2 - icon_h / 2);
      icon_box.y2 = icon_box.y1 + icon_h;

      clutter_actor_allocate (priv->primary_icon,
                              &icon_box,
                              flags);

      /* reduce the size for the entry */
      child_box.x1 += icon_w + priv->spacing;
    }

  if (priv->secondary_icon)
    {
      clutter_actor_get_preferred_width (priv->secondary_icon,
                                         -1, NULL, &icon_w);
      clutter_actor_get_preferred_height (priv->secondary_icon,
                                          -1, NULL, &icon_h);

      icon_box.x2 = content_box.x2;
      icon_box.x1 = icon_box.x2 - icon_w;

      icon_box.y1 = (int) (content_box.y1 + avail_h / 2 - icon_h / 2);
      icon_box.y2 = icon_box.y1 + icon_h;

      clutter_actor_allocate (priv->secondary_icon,
                              &icon_box,
                              flags);

      /* reduce the size for the entry */
      child_box.x2 -= icon_w - priv->spacing;
    }

  clutter_actor_get_preferred_height (priv->entry, child_box.x2 - child_box.x1,
                                      &min_h, &pref_h);

  entry_h = CLAMP (pref_h, min_h, avail_h);

  child_box.y1 = (int) (content_box.y1 + avail_h / 2 - entry_h / 2);
  child_box.y2 = child_box.y1 + entry_h;

  clutter_actor_allocate (priv->entry, &child_box, flags);
}

static void
clutter_text_focus_in_cb (ClutterText *text,
                          ClutterActor *actor)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (actor);

  /* remove the hint if visible */
  if (priv->hint
      && !strcmp (clutter_text_get_text (text), priv->hint))
    {
      clutter_text_set_text (text, "");
    }
  nbtk_widget_set_style_pseudo_class (NBTK_WIDGET (actor), "focus");
  clutter_text_set_cursor_visible (text, TRUE);
}

static void
clutter_text_focus_out_cb (ClutterText  *text,
                           ClutterActor *actor)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (actor);

  /* add a hint if the entry is empty */
  if (priv->hint && !strcmp (clutter_text_get_text (text), ""))
    {
      clutter_text_set_text (text, priv->hint);
      nbtk_widget_set_style_pseudo_class (NBTK_WIDGET (actor), "indeterminate");
    }
  else
    {
      nbtk_widget_set_style_pseudo_class (NBTK_WIDGET (actor), NULL);
    }
  clutter_text_set_cursor_visible (text, FALSE);
}

static void
nbtk_entry_paint (ClutterActor *actor)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (actor);
  ClutterActorClass *parent_class;

  parent_class = CLUTTER_ACTOR_CLASS (nbtk_entry_parent_class);
  parent_class->paint (actor);

  clutter_actor_paint (priv->entry);

  if (priv->primary_icon)
    clutter_actor_paint (priv->primary_icon);

  if (priv->secondary_icon)
    clutter_actor_paint (priv->secondary_icon);
}

static void
nbtk_entry_pick (ClutterActor *actor,
                 const ClutterColor *c)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (actor);

  CLUTTER_ACTOR_CLASS (nbtk_entry_parent_class)->pick (actor, c);

  clutter_actor_paint (priv->entry);

  if (priv->primary_icon)
    clutter_actor_paint (priv->primary_icon);

  if (priv->secondary_icon)
    clutter_actor_paint (priv->secondary_icon);
}

static void
nbtk_entry_map (ClutterActor *actor)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY (actor)->priv;

  CLUTTER_ACTOR_CLASS (nbtk_entry_parent_class)->map (actor);

  clutter_actor_map (priv->entry);

  if (priv->primary_icon)
    clutter_actor_map (priv->primary_icon);

  if (priv->secondary_icon)
    clutter_actor_map (priv->secondary_icon);
}

static void
nbtk_entry_unmap (ClutterActor *actor)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY (actor)->priv;

  CLUTTER_ACTOR_CLASS (nbtk_entry_parent_class)->unmap (actor);

  clutter_actor_unmap (priv->entry);

  if (priv->primary_icon)
    clutter_actor_unmap (priv->primary_icon);

  if (priv->secondary_icon)
    clutter_actor_unmap (priv->secondary_icon);
}

static void
nbtk_entry_clipboard_callback (NbtkClipboard *clipboard,
                               const gchar   *text,
                               gpointer       data)
{
  ClutterText *ctext = (ClutterText*) ((NbtkEntry *) data)->priv->entry;
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
nbtk_entry_key_press_event (ClutterActor    *actor,
                            ClutterKeyEvent *event)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (actor);

  /* This is expected to handle events that were emitted for the inner
     ClutterText. They only reach this function if the ClutterText
     didn't handle them */

  /* paste */
  if ((event->modifier_state & CLUTTER_CONTROL_MASK)
      && event->keyval == CLUTTER_v)
    {
      NbtkClipboard *clipboard;

      clipboard = nbtk_clipboard_get_default ();

      nbtk_clipboard_get_text (clipboard, nbtk_entry_clipboard_callback, actor);

      return TRUE;
    }

  /* copy */
  if ((event->modifier_state & CLUTTER_CONTROL_MASK)
      && event->keyval == CLUTTER_c)
    {
      NbtkClipboard *clipboard;
      gchar *text;

      clipboard = nbtk_clipboard_get_default ();

      text = clutter_text_get_selection ((ClutterText*) priv->entry);

      if (text && strlen (text))
        nbtk_clipboard_set_text (clipboard, text);

      return TRUE;
    }


  /* cut */
  if ((event->modifier_state & CLUTTER_CONTROL_MASK)
      && event->keyval == CLUTTER_x)
    {
      NbtkClipboard *clipboard;
      gchar *text;

      clipboard = nbtk_clipboard_get_default ();

      text = clutter_text_get_selection ((ClutterText*) priv->entry);

      if (text && strlen (text))
        {
          nbtk_clipboard_set_text (clipboard, text);

          /* now delete the text */
          clutter_text_delete_selection ((ClutterText *) priv->entry);
        }

      return TRUE;
    }

  return FALSE;
}

static void
nbtk_entry_key_focus_in (ClutterActor *actor)
{
  NbtkEntryPrivate *priv = NBTK_ENTRY_PRIV (actor);

  /* We never want key focus. The ClutterText should be given first
     pass for all key events */
  clutter_actor_grab_key_focus (priv->entry);
}

static void
nbtk_entry_class_init (NbtkEntryClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  NbtkWidgetClass *widget_class = NBTK_WIDGET_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (NbtkEntryPrivate));

  gobject_class->set_property = nbtk_entry_set_property;
  gobject_class->get_property = nbtk_entry_get_property;
  gobject_class->finalize = nbtk_entry_finalize;
  gobject_class->dispose = nbtk_entry_dispose;

  actor_class->get_preferred_width = nbtk_entry_get_preferred_width;
  actor_class->get_preferred_height = nbtk_entry_get_preferred_height;
  actor_class->allocate = nbtk_entry_allocate;
  actor_class->paint = nbtk_entry_paint;
  actor_class->pick = nbtk_entry_pick;
  actor_class->map = nbtk_entry_map;
  actor_class->unmap = nbtk_entry_unmap;

  actor_class->key_press_event = nbtk_entry_key_press_event;
  actor_class->key_focus_in = nbtk_entry_key_focus_in;

  widget_class->style_changed = nbtk_entry_style_changed;

  pspec = g_param_spec_object ("clutter-text",
			       "Clutter Text",
			       "Internal ClutterText actor",
			       CLUTTER_TYPE_TEXT,
			       G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_CLUTTER_TEXT, pspec);

  pspec = g_param_spec_string ("hint-text",
                               "Hint Text",
                               "Text to display when the entry is not focused "
                               "and the text property is empty",
                               NULL, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_HINT_TEXT, pspec);

  pspec = g_param_spec_string ("text",
                               "Text",
                               "Text of the entry",
                               NULL, G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TEXT, pspec);

  /* signals */
  /**
   * NbtkEntry::primary-icon-clicked:
   *
   * Emitted when the primary icon is clicked
   */
  entry_signals[PRIMARY_ICON_CLICKED] =
    g_signal_new ("primary-icon-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (NbtkEntryClass, primary_icon_clicked),
                  NULL, NULL,
                  _nbtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * NbtkEntry::secondary-icon-clicked:
   *
   * Emitted when the secondary icon is clicked
   */
  entry_signals[SECONDARY_ICON_CLICKED] =
    g_signal_new ("secondary-icon-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (NbtkEntryClass, secondary_icon_clicked),
                  NULL, NULL,
                  _nbtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
nbtk_entry_init (NbtkEntry *entry)
{
  NbtkEntryPrivate *priv;

  priv = entry->priv = NBTK_ENTRY_GET_PRIVATE (entry);

  priv->entry = g_object_new (CLUTTER_TYPE_IMTEXT,
                              "line-alignment", PANGO_ALIGN_LEFT,
                              "editable", TRUE,
                              "reactive", TRUE,
                              "single-line-mode", TRUE,
                              NULL);

  g_signal_connect (priv->entry, "key-focus-in",
                    G_CALLBACK (clutter_text_focus_in_cb), entry);

  g_signal_connect (priv->entry, "key-focus-out",
                    G_CALLBACK (clutter_text_focus_out_cb), entry);

  priv->spacing = 6.0f;

  clutter_actor_set_parent (priv->entry, CLUTTER_ACTOR (entry));
  clutter_actor_set_reactive ((ClutterActor *) entry, TRUE);

  /* set cursor hidden until we receive focus */
  clutter_text_set_cursor_visible ((ClutterText *) priv->entry, FALSE);
}

/**
 * nbtk_entry_new:
 * @text: text to set the entry to
 *
 * Create a new #NbtkEntry with the specified entry
 *
 * Returns: a new #NbtkEntry
 */
NbtkWidget *
nbtk_entry_new (const gchar *text)
{
  NbtkWidget *entry;

  /* add the entry to the stage, but don't allow it to be visible */
  entry = g_object_new (NBTK_TYPE_ENTRY,
                        "text", text,
                        NULL);

  return entry;
}

/**
 * nbtk_entry_get_text:
 * @entry: a #NbtkEntry
 *
 * Get the text displayed on the entry
 *
 * Returns: the text for the entry. This must not be freed by the application
 */
G_CONST_RETURN gchar *
nbtk_entry_get_text (NbtkEntry *entry)
{
  g_return_val_if_fail (NBTK_IS_ENTRY (entry), NULL);

  return clutter_text_get_text (CLUTTER_TEXT (entry->priv->entry));
}

/**
 * nbtk_entry_set_text:
 * @entry: a #NbtkEntry
 * @text: text to set the entry to
 *
 * Sets the text displayed on the entry
 */
void
nbtk_entry_set_text (NbtkEntry *entry,
                     const gchar *text)
{
  NbtkEntryPrivate *priv;

  g_return_if_fail (NBTK_IS_ENTRY (entry));

  priv = entry->priv;

  /* set a hint if we are blanking the entry */
  if (priv->hint
      && text && !strcmp ("", text)
      && !HAS_FOCUS (priv->entry))
    {
      text = priv->hint;
      nbtk_widget_set_style_pseudo_class (NBTK_WIDGET (entry), "indeterminate");
    }
  else
    {
      if (HAS_FOCUS (priv->entry))
        nbtk_widget_set_style_pseudo_class (NBTK_WIDGET (entry), "focus");
      else
        nbtk_widget_set_style_pseudo_class (NBTK_WIDGET (entry), NULL);
    }

  clutter_text_set_text (CLUTTER_TEXT (priv->entry), text);

  g_object_notify (G_OBJECT (entry), "text");
}

/**
 * nbtk_entry_get_clutter_text:
 * @entry: a #NbtkEntry
 *
 * Retrieve the internal #ClutterText so that extra parameters can be set
 *
 * Returns: (transfer none): the #ClutterText used by #NbtkEntry. The entry is
 * owned by the #NbtkEntry and should not be unref'ed by the application.
 */
ClutterActor*
nbtk_entry_get_clutter_text (NbtkEntry *entry)
{
  g_return_val_if_fail (NBTK_ENTRY (entry), NULL);

  return entry->priv->entry;
}

/**
 * nbtk_entry_set_hint_text:
 * @entry: a #NbtkEntry
 * @text: text to set as the entry hint
 *
 * Sets the text to display when the entry is empty and unfocused. When the
 * entry is displaying the hint, it has a pseudo class of "indeterminate".
 * A value of NULL unsets the hint.
 */
void
nbtk_entry_set_hint_text (NbtkEntry *entry,
                          const gchar *text)
{
  NbtkEntryPrivate *priv;

  g_return_if_fail (NBTK_IS_ENTRY (entry));

  priv = entry->priv;

  g_free (priv->hint);

  priv->hint = g_strdup (text);

  if (!strcmp (clutter_text_get_text (CLUTTER_TEXT (priv->entry)), ""))
    {
      clutter_text_set_text (CLUTTER_TEXT (priv->entry), priv->hint);
      nbtk_widget_set_style_pseudo_class (NBTK_WIDGET (entry), "indeterminate");
    }
}

/**
 * nbtk_entry_get_hint_text:
 * @entry: a #NbtkEntry
 *
 * Gets the text that is displayed when the entry is empty and unfocused
 *
 * Returns: the current value of the hint property. This string is owned by the
 * #NbtkEntry and should not be freed or modified.
 */
G_CONST_RETURN
gchar *
nbtk_entry_get_hint_text (NbtkEntry *entry)
{
  g_return_val_if_fail (NBTK_IS_ENTRY (entry), NULL);

  return entry->priv->hint;
}

static gboolean
_nbtk_entry_icon_press_cb (ClutterActor       *actor,
                           ClutterButtonEvent *event,
                           NbtkEntry          *entry)
{
  NbtkEntryPrivate *priv = entry->priv;

  if (actor == priv->primary_icon)
    g_signal_emit (entry, entry_signals[PRIMARY_ICON_CLICKED], 0);
  else
    g_signal_emit (entry, entry_signals[SECONDARY_ICON_CLICKED], 0);

  return FALSE;
}

static void
_nbtk_entry_set_icon_from_file (NbtkEntry     *entry,
                                ClutterActor **icon,
                                const gchar   *filename)
{
  if (*icon)
    {
      g_signal_handlers_disconnect_by_func (*icon,
                                            _nbtk_entry_icon_press_cb,
                                            entry);
      clutter_actor_unparent (*icon);
      *icon = NULL;
    }

  if (filename)
    {
      NbtkTextureCache *cache;

      cache = nbtk_texture_cache_get_default ();



      *icon = (ClutterActor*) nbtk_texture_cache_get_texture (cache, filename, FALSE);

      clutter_actor_set_reactive (*icon, TRUE);
      clutter_actor_set_parent (*icon, CLUTTER_ACTOR (entry));
      g_signal_connect (*icon, "button-release-event",
                        G_CALLBACK (_nbtk_entry_icon_press_cb), entry);
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (entry));
}

/**
 * nbtk_entry_set_primary_icon_from_file:
 * @entry: a #NbtkEntry
 * @filename: filename of an icon
 *
 * Set the primary icon of the entry to the given filename
 */
void
nbtk_entry_set_primary_icon_from_file (NbtkEntry   *entry,
                                       const gchar *filename)
{
  NbtkEntryPrivate *priv;

  g_return_if_fail (NBTK_IS_ENTRY (entry));

  priv = entry->priv;

  _nbtk_entry_set_icon_from_file (entry, &priv->primary_icon, filename);

}

/**
 * nbtk_entry_set_secondary_icon_from_file:
 * @entry: a #NbtkEntry
 * @filename: filename of an icon
 *
 * Set the primary icon of the entry to the given filename
 */
void
nbtk_entry_set_secondary_icon_from_file (NbtkEntry   *entry,
                                         const gchar *filename)
{
  NbtkEntryPrivate *priv;

  g_return_if_fail (NBTK_IS_ENTRY (entry));

  priv = entry->priv;

  _nbtk_entry_set_icon_from_file (entry, &priv->secondary_icon, filename);

}
