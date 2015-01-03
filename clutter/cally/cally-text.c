/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2009 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * Some parts are based on GailLabel, GailEntry, GailTextView from GAIL
 * GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
 *
 * Implementation of atk_text_get_text_[before/at/after]_offset
 * copied from gtkpango.c, part of GTK+ project
 * Copyright (c) 2010 Red Hat, Inc.
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
 */

/**
 * SECTION:cally-text
 * @short_description: Implementation of the ATK interfaces for a #ClutterText
 * @see_also: #ClutterText
 *
 * #CallyText implements the required ATK interfaces of
 * #ClutterText, #AtkText and #AtkEditableText
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cally-text.h"
#include "cally-actor-private.h"

#include "clutter-color.h"
#include "clutter-main.h"
#include "clutter-text.h"

static void cally_text_finalize   (GObject *obj);

/* AtkObject */
static void                   cally_text_real_initialize (AtkObject *obj,
                                                          gpointer   data);
static AtkStateSet*           cally_text_ref_state_set   (AtkObject *obj);

/* atkaction */

static void                   _cally_text_activate_action (CallyActor *cally_actor);
static void                   _check_activate_action      (CallyText   *cally_text,
                                                           ClutterText *clutter_text);

/* AtkText */
static void                   cally_text_text_interface_init     (AtkTextIface *iface);
static gchar*                 cally_text_get_text                (AtkText *text,
                                                                  gint     start_offset,
                                                                  gint     end_offset);
static gunichar               cally_text_get_character_at_offset (AtkText *text,
                                                                  gint     offset);
static gchar*                 cally_text_get_text_before_offset  (AtkText	 *text,
                                                                  gint		  offset,
                                                                  AtkTextBoundary  boundary_type,
                                                                  gint		 *start_offset,
                                                                  gint		 *end_offset);
static gchar*	              cally_text_get_text_at_offset      (AtkText	 *text,
                                                                  gint             offset,
                                                                  AtkTextBoundary  boundary_type,
                                                                  gint		 *start_offset,
                                                                  gint		 *end_offset);
static gchar*	              cally_text_get_text_after_offset   (AtkText	 *text,
                                                                  gint             offset,
                                                                  AtkTextBoundary  boundary_type,
                                                                  gint		 *start_offset,
                                                                  gint		 *end_offset);
static gint                   cally_text_get_caret_offset        (AtkText *text);
static gboolean               cally_text_set_caret_offset        (AtkText *text,
                                                                  gint offset);
static gint                   cally_text_get_character_count     (AtkText *text);
static gint                   cally_text_get_n_selections        (AtkText *text);
static gchar*                 cally_text_get_selection           (AtkText *text,
                                                                  gint    selection_num,
                                                                  gint    *start_offset,
                                                                  gint    *end_offset);
static gboolean               cally_text_add_selection           (AtkText *text,
                                                                  gint     start_offset,
                                                                  gint     end_offset);
static gboolean              cally_text_remove_selection         (AtkText *text,
                                                                  gint    selection_num);
static gboolean              cally_text_set_selection            (AtkText *text,
                                                                  gint	  selection_num,
                                                                  gint    start_offset,
                                                                  gint    end_offset);
static AtkAttributeSet*      cally_text_get_run_attributes       (AtkText *text,
                                                                  gint    offset,
                                                                  gint    *start_offset,
                                                                  gint    *end_offset);
static AtkAttributeSet*      cally_text_get_default_attributes   (AtkText *text);
static void                  cally_text_get_character_extents    (AtkText *text,
                                                                  gint offset,
                                                                  gint *x,
                                                                  gint *y,
                                                                  gint *width,
                                                                  gint *height,
                                                                  AtkCoordType coords);
static gint                  cally_text_get_offset_at_point      (AtkText *text,
                                                                  gint x,
                                                                  gint y,
                                                                  AtkCoordType coords);

static void                  _cally_text_get_selection_bounds    (ClutterText *clutter_text,
                                                                  gint        *start_offset,
                                                                  gint        *end_offset);
static void                  _cally_text_insert_text_cb          (ClutterText *clutter_text,
                                                                  gchar       *new_text,
                                                                  gint         new_text_length,
                                                                  gint        *position,
                                                                  gpointer     data);
static void                 _cally_text_delete_text_cb           (ClutterText *clutter_text,
                                                                  gint         start_pos,
                                                                  gint         end_pos,
                                                                  gpointer     data);
static gboolean             _idle_notify_insert                  (gpointer data);
static void                 _notify_insert                       (CallyText *cally_text);
static void                 _notify_delete                       (CallyText *cally_text);

/* AtkEditableText */
static void                 cally_text_editable_text_interface_init (AtkEditableTextIface *iface);
static void                 cally_text_set_text_contents            (AtkEditableText *text,
                                                                     const gchar *string);
static void                 cally_text_insert_text                  (AtkEditableText *text,
                                                                     const gchar *string,
                                                                     gint length,
                                                                     gint *position);
static void                 cally_text_delete_text                  (AtkEditableText *text,
                                                                     gint start_pos,
                                                                     gint end_pos);

/* CallyActor */
static void                 cally_text_notify_clutter               (GObject    *obj,
                                                                     GParamSpec *pspec);

static gboolean             _check_for_selection_change             (CallyText *cally_text,
                                                                     ClutterText *clutter_text);

/* Misc functions */
static AtkAttributeSet*     _cally_misc_add_attribute (AtkAttributeSet *attrib_set,
                                                       AtkTextAttribute attr,
                                                       gchar           *value);

static AtkAttributeSet*     _cally_misc_layout_get_run_attributes (AtkAttributeSet *attrib_set,
                                                                   ClutterText     *clutter_text,
                                                                   gint            offset,
                                                                   gint            *start_offset,
                                                                   gint            *end_offset);

static AtkAttributeSet*     _cally_misc_layout_get_default_attributes (AtkAttributeSet *attrib_set,
                                                                       ClutterText *text);

static int                  _cally_misc_get_index_at_point (ClutterText *clutter_text,
                                                            gint         x,
                                                            gint         y,
                                                            AtkCoordType coords);

struct _CallyTextPrivate
{
  /* Cached ClutterText values*/
  gint cursor_position;
  gint selection_bound;

  /* text_changed::insert stuff */
  const gchar *signal_name_insert;
  gint position_insert;
  gint length_insert;
  guint insert_idle_handler;

  /* text_changed::delete stuff */
  const gchar *signal_name_delete;
  gint position_delete;
  gint length_delete;

  /* action */
  guint activate_action_id;
};

G_DEFINE_TYPE_WITH_CODE (CallyText,
                         cally_text,
                         CALLY_TYPE_ACTOR,
                         G_ADD_PRIVATE (CallyText)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT,
                                                cally_text_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_EDITABLE_TEXT,
                                                cally_text_editable_text_interface_init));

static void
cally_text_class_init (CallyTextClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);
  CallyActorClass *cally_class  = CALLY_ACTOR_CLASS (klass);

  gobject_class->finalize = cally_text_finalize;

  class->initialize = cally_text_real_initialize;
  class->ref_state_set = cally_text_ref_state_set;

  cally_class->notify_clutter = cally_text_notify_clutter;
}

static void
cally_text_init (CallyText *cally_text)
{
  CallyTextPrivate *priv = cally_text_get_instance_private (cally_text);

  cally_text->priv = priv;

  priv->cursor_position = 0;
  priv->selection_bound = 0;

  priv->signal_name_insert = NULL;
  priv->position_insert = -1;
  priv->length_insert = -1;
  priv->insert_idle_handler = 0;

  priv->signal_name_delete = NULL;
  priv->position_delete = -1;
  priv->length_delete = -1;

  priv->activate_action_id = 0;
}

static void
cally_text_finalize   (GObject *obj)
{
  CallyText *cally_text = CALLY_TEXT (obj);

/*   g_object_unref (cally_text->priv->textutil); */
/*   cally_text->priv->textutil = NULL; */

  if (cally_text->priv->insert_idle_handler)
    {
      g_source_remove (cally_text->priv->insert_idle_handler);
      cally_text->priv->insert_idle_handler = 0;
    }

  G_OBJECT_CLASS (cally_text_parent_class)->finalize (obj);
}

/**
 * cally_text_new:
 * @actor: a #ClutterActor
 *
 * Creates a new #CallyText for the given @actor. @actor must be a
 * #ClutterText.
 *
 * Return value: the newly created #AtkObject
 *
 * Since: 1.4
 */
AtkObject*
cally_text_new (ClutterActor *actor)
{
  GObject   *object     = NULL;
  AtkObject *accessible = NULL;

  g_return_val_if_fail (CLUTTER_IS_TEXT (actor), NULL);

  object = g_object_new (CALLY_TYPE_TEXT, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, actor);

  return accessible;
}

/* atkobject.h */

static void
cally_text_real_initialize(AtkObject *obj,
                           gpointer   data)
{
  ClutterText *clutter_text = NULL;
  CallyText *cally_text = NULL;

  ATK_OBJECT_CLASS (cally_text_parent_class)->initialize (obj, data);

  g_return_if_fail (CLUTTER_TEXT (data));

  cally_text = CALLY_TEXT (obj);
  clutter_text = CLUTTER_TEXT (data);

  cally_text->priv->cursor_position = clutter_text_get_cursor_position (clutter_text);
  cally_text->priv->selection_bound = clutter_text_get_selection_bound (clutter_text);

  g_signal_connect (clutter_text, "insert-text",
                    G_CALLBACK (_cally_text_insert_text_cb),
                    cally_text);
  g_signal_connect (clutter_text, "delete-text",
                    G_CALLBACK (_cally_text_delete_text_cb),
                    cally_text);

  _check_activate_action (cally_text, clutter_text);

  if (clutter_text_get_password_char (clutter_text) != 0)
    atk_object_set_role (obj, ATK_ROLE_PASSWORD_TEXT);
  else
    atk_object_set_role (obj, ATK_ROLE_TEXT);
}

static AtkStateSet*
cally_text_ref_state_set   (AtkObject *obj)
{
  AtkStateSet *result = NULL;
  ClutterActor *actor = NULL;

  result = ATK_OBJECT_CLASS (cally_text_parent_class)->ref_state_set (obj);

  actor = CALLY_GET_CLUTTER_ACTOR (obj);

  if (actor == NULL)
    return result;

  if (clutter_text_get_editable (CLUTTER_TEXT (actor)))
    atk_state_set_add_state (result, ATK_STATE_EDITABLE);

  if (clutter_text_get_selectable (CLUTTER_TEXT (actor)))
    atk_state_set_add_state (result, ATK_STATE_SELECTABLE_TEXT);

  return result;
}

/***** pango stuff ****
 *
 * FIXME: all this pango related code used to implement
 * atk_text_get_text_[before/at/after]_offset was copied from GTK, and
 * should be on a common library (like pango itself).
 *
 *********************/

/*
 * _gtk_pango_move_chars:
 * @layout: a #PangoLayout
 * @offset: a character offset in @layout
 * @count: the number of characters to move from @offset
 *
 * Returns the position that is @count characters from the
 * given @offset. @count may be positive or negative.
 *
 * For the purpose of this function, characters are defined
 * by what Pango considers cursor positions.
 *
 * Returns: the new position
 */
static gint
_gtk_pango_move_chars (PangoLayout *layout,
                       gint         offset,
                       gint         count)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (count > 0 && offset < n_attrs - 1)
    {
      do
        offset++;
      while (offset < n_attrs - 1 && !attrs[offset].is_cursor_position);

      count--;
    }
  while (count < 0 && offset > 0)
    {
      do
        offset--;
      while (offset > 0 && !attrs[offset].is_cursor_position);

      count++;
    }

  return offset;
}

/*
 * _gtk_pango_move_words:
 * @layout: a #PangoLayout
 * @offset: a character offset in @layout
 * @count: the number of words to move from @offset
 *
 * Returns the position that is @count words from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is positive, the returned position will
 * be a word end, otherwise it will be a word start.
 * See the Pango documentation for details on how
 * word starts and ends are defined.
 *
 * Returns: the new position
 */
static gint
_gtk_pango_move_words (PangoLayout  *layout,
                       gint          offset,
                       gint          count)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (count > 0 && offset < n_attrs - 1)
    {
      do
        offset++;
      while (offset < n_attrs - 1 && !attrs[offset].is_word_end);

      count--;
    }
  while (count < 0 && offset > 0)
    {
      do
        offset--;
      while (offset > 0 && !attrs[offset].is_word_start);

      count++;
    }

  return offset;
}

/*
 * _gtk_pango_move_sentences:
 * @layout: a #PangoLayout
 * @offset: a character offset in @layout
 * @count: the number of sentences to move from @offset
 *
 * Returns the position that is @count sentences from the
 * given @offset. @count may be positive or negative.
 *
 * If @count is positive, the returned position will
 * be a sentence end, otherwise it will be a sentence start.
 * See the Pango documentation for details on how
 * sentence starts and ends are defined.
 *
 * Returns: the new position
 */
static gint
_gtk_pango_move_sentences (PangoLayout  *layout,
                           gint          offset,
                           gint          count)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (count > 0 && offset < n_attrs - 1)
    {
      do
        offset++;
      while (offset < n_attrs - 1 && !attrs[offset].is_sentence_end);

      count--;
    }
  while (count < 0 && offset > 0)
    {
      do
        offset--;
      while (offset > 0 && !attrs[offset].is_sentence_start);

      count++;
    }

  return offset;
}

/*
 * _gtk_pango_is_inside_word:
 * @layout: a #PangoLayout
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a word.
 *
 * Returns: %TRUE if @offset is inside a word
 */
static gboolean
_gtk_pango_is_inside_word (PangoLayout  *layout,
                           gint          offset)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_word_start || attrs[offset].is_word_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_word_start;

  return FALSE;
}

/*
 * _gtk_pango_is_inside_sentence:
 * @layout: a #PangoLayout
 * @offset: a character offset in @layout
 *
 * Returns whether the given position is inside
 * a sentence.
 *
 * Returns: %TRUE if @offset is inside a sentence
 */
static gboolean
_gtk_pango_is_inside_sentence (PangoLayout  *layout,
                               gint          offset)
{
  const PangoLogAttr *attrs;
  gint n_attrs;

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  while (offset >= 0 &&
         !(attrs[offset].is_sentence_start || attrs[offset].is_sentence_end))
    offset--;

  if (offset >= 0)
    return attrs[offset].is_sentence_start;

  return FALSE;
}

static void
pango_layout_get_line_before (PangoLayout     *layout,
                              AtkTextBoundary  boundary_type,
                              gint             offset,
                              gint            *start_offset,
                              gint            *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL, *prev_prev_line = NULL;
  gint index, start_index, end_index;
  const gchar *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = line->start_index;
      end_index = start_index + line->length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          if (prev_line)
            {
              switch (boundary_type)
                {
                case ATK_TEXT_BOUNDARY_LINE_START:
                  end_index = start_index;
                  start_index = prev_line->start_index;
                  break;
                case ATK_TEXT_BOUNDARY_LINE_END:
                  if (prev_prev_line)
                    start_index = prev_prev_line->start_index + prev_prev_line->length;
                  else
                    start_index = 0;
                  end_index = prev_line->start_index + prev_line->length;
                  break;
                default:
                  g_assert_not_reached();
                }
            }
          else
            start_index = end_index = 0;

          found = TRUE;
          break;
        }

      prev_prev_line = prev_line;
      prev_line = line;
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = prev_line->start_index + prev_line->length;
      end_index = start_index;
    }
  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

static void
pango_layout_get_line_at (PangoLayout     *layout,
                          AtkTextBoundary  boundary_type,
                          gint             offset,
                          gint            *start_offset,
                          gint            *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL;
  gint index, start_index, end_index;
  const gchar *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = line->start_index;
      end_index = start_index + line->length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          switch (boundary_type)
            {
            case ATK_TEXT_BOUNDARY_LINE_START:
              if (pango_layout_iter_next_line (iter))
                end_index = pango_layout_iter_get_line (iter)->start_index;
              break;
            case ATK_TEXT_BOUNDARY_LINE_END:
              if (prev_line)
                start_index = prev_line->start_index + prev_line->length;
              break;
            default:
              g_assert_not_reached();
            }

          found = TRUE;
          break;
        }

      prev_line = line;
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = prev_line->start_index + prev_line->length;
      end_index = start_index;
    }
  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

static void
pango_layout_get_line_after (PangoLayout     *layout,
                             AtkTextBoundary  boundary_type,
                             gint             offset,
                             gint            *start_offset,
                             gint            *end_offset)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line, *prev_line = NULL;
  gint index, start_index, end_index;
  const gchar *text;
  gboolean found = FALSE;

  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, offset) - text;
  iter = pango_layout_get_iter (layout);
  do
    {
      line = pango_layout_iter_get_line (iter);
      start_index = line->start_index;
      end_index = start_index + line->length;

      if (index >= start_index && index <= end_index)
        {
          /* Found line for offset */
          if (pango_layout_iter_next_line (iter))
            {
              line = pango_layout_iter_get_line (iter);
              switch (boundary_type)
                {
                case ATK_TEXT_BOUNDARY_LINE_START:
                  start_index = line->start_index;
                  if (pango_layout_iter_next_line (iter))
                    end_index = pango_layout_iter_get_line (iter)->start_index;
                  else
                    end_index = start_index + line->length;
                  break;
                case ATK_TEXT_BOUNDARY_LINE_END:
                  start_index = end_index;
                  end_index = line->start_index + line->length;
                  break;
                default:
                  g_assert_not_reached();
                }
            }
          else
            start_index = end_index;

          found = TRUE;
          break;
        }

      prev_line = line;
    }
  while (pango_layout_iter_next_line (iter));

  if (!found)
    {
      start_index = prev_line->start_index + prev_line->length;
      end_index = start_index;
    }
  pango_layout_iter_free (iter);

  *start_offset = g_utf8_pointer_to_offset (text, text + start_index);
  *end_offset = g_utf8_pointer_to_offset (text, text + end_index);
}

/*
 * _gtk_pango_get_text_at:
 * @layout: a #PangoLayout
 * @boundary_type: a #AtkTextBoundary
 * @offset: a character offset in @layout
 * @start_offset: return location for the start of the returned text
 * @end_offset: return location for the end of the return text
 *
 * Gets a slice of the text from @layout at @offset.
 *
 * The @boundary_type determines the size of the returned slice of
 * text. For the exact semantics of this function, see
 * atk_text_get_text_after_offset().
 *
 * Returns: a newly allocated string containing a slice of text
 *     from layout. Free with g_free().
 */
static gchar *
_gtk_pango_get_text_at (PangoLayout     *layout,
                        AtkTextBoundary  boundary_type,
                        gint             offset,
                        gint            *start_offset,
                        gint            *end_offset)
{
  const gchar *text;
  gint start, end;
  const PangoLogAttr *attrs;
  gint n_attrs;

  text = pango_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = offset;
  end = start;

  switch (boundary_type)
    {
    case ATK_TEXT_BOUNDARY_CHAR:
      end = _gtk_pango_move_chars (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_START:
      if (!attrs[start].is_word_start)
        start = _gtk_pango_move_words (layout, start, -1);
      if (_gtk_pango_is_inside_word (layout, end))
        end = _gtk_pango_move_words (layout, end, 1);
      while (!attrs[end].is_word_start && end < n_attrs - 1)
        end = _gtk_pango_move_chars (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_END:
      if (_gtk_pango_is_inside_word (layout, start) &&
          !attrs[start].is_word_start)
        start = _gtk_pango_move_words (layout, start, -1);
      while (!attrs[start].is_word_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      end = _gtk_pango_move_words (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (!attrs[start].is_sentence_start)
        start = _gtk_pango_move_sentences (layout, start, -1);
      if (_gtk_pango_is_inside_sentence (layout, end))
        end = _gtk_pango_move_sentences (layout, end, 1);
      while (!attrs[end].is_sentence_start && end < n_attrs - 1)
        end = _gtk_pango_move_chars (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      if (_gtk_pango_is_inside_sentence (layout, start) &&
          !attrs[start].is_sentence_start)
        start = _gtk_pango_move_sentences (layout, start, -1);
      while (!attrs[start].is_sentence_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      end = _gtk_pango_move_sentences (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
      pango_layout_get_line_at (layout, boundary_type, offset, &start, &end);
      break;
    }

  *start_offset = start;
  *end_offset = end;

  g_assert (start <= end);

  return g_utf8_substring (text, start, end);
}

/*
 * _gtk_pango_get_text_before:
 * @layout: a #PangoLayout
 * @boundary_type: a #AtkTextBoundary
 * @offset: a character offset in @layout
 * @start_offset: return location for the start of the returned text
 * @end_offset: return location for the end of the return text
 *
 * Gets a slice of the text from @layout before @offset.
 *
 * The @boundary_type determines the size of the returned slice of
 * text. For the exact semantics of this function, see
 * atk_text_get_text_before_offset().
 *
 * Returns: a newly allocated string containing a slice of text
 *     from layout. Free with g_free().
 */
static gchar *
_gtk_pango_get_text_before (PangoLayout     *layout,
                            AtkTextBoundary  boundary_type,
                            gint             offset,
                            gint            *start_offset,
                            gint            *end_offset)
{
  const gchar *text;
  gint start, end;
  const PangoLogAttr *attrs;
  gint n_attrs;

  text = pango_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = offset;
  end = start;

  switch (boundary_type)
    {
    case ATK_TEXT_BOUNDARY_CHAR:
      start = _gtk_pango_move_chars (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_START:
      if (!attrs[start].is_word_start)
        start = _gtk_pango_move_words (layout, start, -1);
      end = start;
      start = _gtk_pango_move_words (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_END:
      if (_gtk_pango_is_inside_word (layout, start) &&
          !attrs[start].is_word_start)
        start = _gtk_pango_move_words (layout, start, -1);
      while (!attrs[start].is_word_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      end = start;
      start = _gtk_pango_move_words (layout, start, -1);
      while (!attrs[start].is_word_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (!attrs[start].is_sentence_start)
        start = _gtk_pango_move_sentences (layout, start, -1);
      end = start;
      start = _gtk_pango_move_sentences (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      if (_gtk_pango_is_inside_sentence (layout, start) &&
          !attrs[start].is_sentence_start)
        start = _gtk_pango_move_sentences (layout, start, -1);
      while (!attrs[start].is_sentence_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      end = start;
      start = _gtk_pango_move_sentences (layout, start, -1);
      while (!attrs[start].is_sentence_end && start > 0)
        start = _gtk_pango_move_chars (layout, start, -1);
      break;

    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
      pango_layout_get_line_before (layout, boundary_type, offset, &start, &end);
      break;
    }

  *start_offset = start;
  *end_offset = end;

  g_assert (start <= end);

  return g_utf8_substring (text, start, end);
}

/*
 * _gtk_pango_get_text_after:
 * @layout: a #PangoLayout
 * @boundary_type: a #AtkTextBoundary
 * @offset: a character offset in @layout
 * @start_offset: return location for the start of the returned text
 * @end_offset: return location for the end of the return text
 *
 * Gets a slice of the text from @layout after @offset.
 *
 * The @boundary_type determines the size of the returned slice of
 * text. For the exact semantics of this function, see
 * atk_text_get_text_after_offset().
 *
 * Returns: a newly allocated string containing a slice of text
 *     from layout. Free with g_free().
 */
static gchar *
_gtk_pango_get_text_after (PangoLayout     *layout,
                           AtkTextBoundary  boundary_type,
                           gint             offset,
                           gint            *start_offset,
                           gint            *end_offset)
{
  const gchar *text;
  gint start, end;
  const PangoLogAttr *attrs;
  gint n_attrs;

  text = pango_layout_get_text (layout);

  if (text[0] == 0)
    {
      *start_offset = 0;
      *end_offset = 0;
      return g_strdup ("");
    }

  attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = offset;
  end = start;

  switch (boundary_type)
    {
    case ATK_TEXT_BOUNDARY_CHAR:
      start = _gtk_pango_move_chars (layout, start, 1);
      end = start;
      end = _gtk_pango_move_chars (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_WORD_START:
      if (_gtk_pango_is_inside_word (layout, end))
        end = _gtk_pango_move_words (layout, end, 1);
      while (!attrs[end].is_word_start && end < n_attrs - 1)
        end = _gtk_pango_move_chars (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        {
          end = _gtk_pango_move_words (layout, end, 1);
          while (!attrs[end].is_word_start && end < n_attrs - 1)
            end = _gtk_pango_move_chars (layout, end, 1);
        }
      break;

    case ATK_TEXT_BOUNDARY_WORD_END:
      end = _gtk_pango_move_words (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        end = _gtk_pango_move_words (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (_gtk_pango_is_inside_sentence (layout, end))
        end = _gtk_pango_move_sentences (layout, end, 1);
      while (!attrs[end].is_sentence_start && end < n_attrs - 1)
        end = _gtk_pango_move_chars (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        {
          end = _gtk_pango_move_sentences (layout, end, 1);
          while (!attrs[end].is_sentence_start && end < n_attrs - 1)
            end = _gtk_pango_move_chars (layout, end, 1);
        }
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      end = _gtk_pango_move_sentences (layout, end, 1);
      start = end;
      if (end < n_attrs - 1)
        end = _gtk_pango_move_sentences (layout, end, 1);
      break;

    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
      pango_layout_get_line_after (layout, boundary_type, offset, &start, &end);
      break;
    }

  *start_offset = start;
  *end_offset = end;

  g_assert (start <= end);

  return g_utf8_substring (text, start, end);
}

/***** atktext.h ******/

static void
cally_text_text_interface_init (AtkTextIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_text                = cally_text_get_text;
  iface->get_character_at_offset = cally_text_get_character_at_offset;
  iface->get_text_before_offset  = cally_text_get_text_before_offset;
  iface->get_text_at_offset      = cally_text_get_text_at_offset;
  iface->get_text_after_offset   = cally_text_get_text_after_offset;
  iface->get_character_count     = cally_text_get_character_count;
  iface->get_caret_offset        = cally_text_get_caret_offset;
  iface->set_caret_offset        = cally_text_set_caret_offset;
  iface->get_n_selections        = cally_text_get_n_selections;
  iface->get_selection           = cally_text_get_selection;
  iface->add_selection           = cally_text_add_selection;
  iface->remove_selection        = cally_text_remove_selection;
  iface->set_selection           = cally_text_set_selection;
  iface->get_run_attributes      = cally_text_get_run_attributes;
  iface->get_default_attributes  = cally_text_get_default_attributes;
  iface->get_character_extents   = cally_text_get_character_extents;
  iface->get_offset_at_point     = cally_text_get_offset_at_point;

}

static gchar*
cally_text_get_text (AtkText *text,
                     gint start_offset,
                     gint end_offset)
{
  ClutterActor *actor = NULL;
  PangoLayout *layout = NULL;
  const gchar *string = NULL;
  gint character_count = 0;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* Object is defunct */
    return NULL;

  /* we use the pango layout instead of clutter_text_get_chars because
     it take into account password-char */

  layout = clutter_text_get_layout (CLUTTER_TEXT (actor));
  string = pango_layout_get_text (layout);
  character_count = pango_layout_get_character_count (layout);

  if (end_offset == -1 || end_offset > character_count)
    end_offset = character_count;

  if (string[0] == 0)
    return g_strdup("");
  else
    return g_utf8_substring (string, start_offset, end_offset);
}

static gunichar
cally_text_get_character_at_offset (AtkText *text,
                                    gint     offset)
{
  ClutterActor *actor      = NULL;
  const gchar  *string     = NULL;
  gchar        *index      = NULL;
  gunichar      unichar;
  PangoLayout  *layout = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return '\0';

  /* we use the pango layout instead of clutter_text_get_chars because
     it take into account password-char */

  layout = clutter_text_get_layout (CLUTTER_TEXT (actor));
  string = pango_layout_get_text (layout);

  if (offset >= g_utf8_strlen (string, -1))
    {
      unichar = '\0';
    }
  else
    {
      index = g_utf8_offset_to_pointer (string, offset);

      unichar = g_utf8_get_char (index);
    }

  return unichar;
}

static gchar*
cally_text_get_text_before_offset (AtkText	    *text,
				   gint		    offset,
				   AtkTextBoundary  boundary_type,
				   gint		    *start_offset,
				   gint		    *end_offset)
{
  ClutterActor *actor        = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return NULL;

  return _gtk_pango_get_text_before (clutter_text_get_layout (CLUTTER_TEXT (actor)),
                                     boundary_type, offset,
                                     start_offset, end_offset);
}

static gchar*
cally_text_get_text_at_offset (AtkText         *text,
                               gint             offset,
                               AtkTextBoundary  boundary_type,
                               gint            *start_offset,
                               gint            *end_offset)
{
  ClutterActor *actor        = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return NULL;

  return _gtk_pango_get_text_at (clutter_text_get_layout (CLUTTER_TEXT (actor)),
                                 boundary_type, offset,
                                 start_offset, end_offset);
}

static gchar*
cally_text_get_text_after_offset (AtkText         *text,
                                  gint             offset,
                                  AtkTextBoundary  boundary_type,
                                  gint            *start_offset,
                                  gint            *end_offset)
{
  ClutterActor *actor        = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return NULL;

  return _gtk_pango_get_text_after (clutter_text_get_layout (CLUTTER_TEXT (actor)),
                                    boundary_type, offset,
                                    start_offset, end_offset);
}

static gint
cally_text_get_caret_offset (AtkText *text)
{
  ClutterActor *actor        = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return -1;

  return clutter_text_get_cursor_position (CLUTTER_TEXT (actor));
}

static gboolean
cally_text_set_caret_offset (AtkText *text,
                             gint offset)
{
  ClutterActor *actor        = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return FALSE;

  clutter_text_set_cursor_position (CLUTTER_TEXT (actor), offset);

  /* like in gailentry, we suppose that this always works, as clutter text
     doesn't return anything */
  return TRUE;
}

static gint
cally_text_get_character_count (AtkText *text)
{
  ClutterActor *actor = NULL;
  ClutterText *clutter_text = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return 0;

  clutter_text = CLUTTER_TEXT (actor);
  return g_utf8_strlen (clutter_text_get_text (clutter_text), -1);
}

static gint
cally_text_get_n_selections (AtkText *text)
{
  ClutterActor *actor           = NULL;
  gint          selection_bound = -1;
  gint          cursor_position = -1;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return 0;

  if (!clutter_text_get_selectable (CLUTTER_TEXT (actor)))
    return 0;

  selection_bound = clutter_text_get_selection_bound (CLUTTER_TEXT (actor));
  cursor_position = clutter_text_get_cursor_position (CLUTTER_TEXT (actor));

  if (selection_bound == cursor_position)
    return 0;
  else
    return 1;
}

static gchar*
cally_text_get_selection (AtkText *text,
			  gint     selection_num,
                          gint    *start_offset,
                          gint    *end_offset)
{
  ClutterActor *actor = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return NULL;

 /* As in gailentry, only let the user get the selection if one is set, and if
  * the selection_num is 0.
  */
  if (selection_num != 0)
     return NULL;

  _cally_text_get_selection_bounds (CLUTTER_TEXT (actor), start_offset, end_offset);

  if (*start_offset != *end_offset)
    return clutter_text_get_selection (CLUTTER_TEXT (actor));
  else
     return NULL;
}

/* ClutterText only allows one selection. So this method will set the selection
   if no selection exists, but as in gailentry, it will not change the current
   selection */
static gboolean
cally_text_add_selection (AtkText *text,
                          gint	   start_offset,
                          gint	   end_offset)
{
  ClutterActor *actor;
  gint select_start, select_end;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return FALSE;

  _cally_text_get_selection_bounds (CLUTTER_TEXT (actor),
                                    &select_start, &select_end);

 /* Like in gailentry, if there is already a selection, then don't allow another
  * to be added, since ClutterText only supports one selected region.
  */
  if (select_start == select_end)
    {
      clutter_text_set_selection (CLUTTER_TEXT (actor),
                                  start_offset, end_offset);

      return TRUE;
    }
  else
    return FALSE;
}


static gboolean
cally_text_remove_selection (AtkText *text,
                             gint    selection_num)
{
  ClutterActor *actor        = NULL;
  gint          caret_pos    = -1;
  gint          select_start = -1;
  gint          select_end   = -1;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return FALSE;

  /* only one selection is allowed */
  if (selection_num != 0)
     return FALSE;

  _cally_text_get_selection_bounds (CLUTTER_TEXT (actor),
                                    &select_start, &select_end);

  if (select_start != select_end)
    {
     /* Setting the start & end of the selected region to the caret position
      * turns off the selection.
      */
      caret_pos = clutter_text_get_cursor_position (CLUTTER_TEXT (actor));
      clutter_text_set_selection (CLUTTER_TEXT (actor),
                                  caret_pos, caret_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
cally_text_set_selection (AtkText *text,
			  gint	  selection_num,
                          gint    start_offset,
                          gint    end_offset)
{
  ClutterActor *actor        = NULL;
  gint          select_start = -1;
  gint          select_end   = -1;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return FALSE;

 /* Like in gailentry, only let the user move the selection if one is set,
  * and if the selection_num is 0
  */
  if (selection_num != 0)
     return FALSE;

  _cally_text_get_selection_bounds (CLUTTER_TEXT (actor),
                                    &select_start, &select_end);

  if (select_start != select_end)
    {
      clutter_text_set_selection (CLUTTER_TEXT (actor),
                                  start_offset, end_offset);
      return TRUE;
    }
  else
    return FALSE;
}

static AtkAttributeSet*
cally_text_get_run_attributes (AtkText *text,
                               gint    offset,
                               gint    *start_offset,
                               gint    *end_offset)
{
  ClutterActor    *actor        = NULL;
  ClutterText     *clutter_text = NULL;
  AtkAttributeSet *at_set       = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return NULL;

  /* Clutter don't have any reference to the direction*/

  clutter_text = CLUTTER_TEXT (actor);

  at_set = _cally_misc_layout_get_run_attributes (at_set,
                                                  clutter_text,
                                                  offset,
                                                  start_offset,
                                                  end_offset);

  return at_set;
}

static AtkAttributeSet*
cally_text_get_default_attributes (AtkText *text)
{
  ClutterActor    *actor        = NULL;
  ClutterText     *clutter_text = NULL;
  AtkAttributeSet *at_set       = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return NULL;

  clutter_text = CLUTTER_TEXT (actor);

  at_set = _cally_misc_layout_get_default_attributes (at_set, clutter_text);

  return at_set;
}

static void cally_text_get_character_extents (AtkText *text,
                                              gint offset,
                                              gint *xp,
                                              gint *yp,
                                              gint *widthp,
                                              gint *heightp,
                                              AtkCoordType coords)
{
  ClutterActor    *actor        = NULL;
  ClutterText     *clutter_text = NULL;
  gint x = 0, y = 0, width = 0, height = 0;
  gint index, x_window, y_window, x_toplevel, y_toplevel;
  gint x_layout, y_layout;
  PangoLayout *layout;
  PangoRectangle extents;
  const gchar *text_value;
  ClutterVertex verts[4];

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    goto done;

  clutter_text = CLUTTER_TEXT (actor);

  text_value = clutter_text_get_text (clutter_text);
  index = g_utf8_offset_to_pointer (text_value, offset) - text_value;

  layout = clutter_text_get_layout (clutter_text);
  pango_layout_index_to_pos (layout, index, &extents);

  /* handle RTL text layout */
  if (extents.width < 0)
    {
      extents.x += extents.width;
      extents.width = -extents.width;
    }

  clutter_actor_get_abs_allocation_vertices (actor, verts);
  x_window = verts[0].x;
  y_window = verts[0].y;

  clutter_text_get_layout_offsets (clutter_text, &x_layout, &y_layout);

  x = (extents.x / PANGO_SCALE) + x_layout + x_window;
  y = (extents.y / PANGO_SCALE) + y_layout + y_window;
  width = extents.width / PANGO_SCALE;
  height = extents.height / PANGO_SCALE;

  if (coords == ATK_XY_SCREEN)
    {
      _cally_actor_get_top_level_origin (actor, &x_toplevel, &y_toplevel);
      x += x_toplevel;
      y += y_toplevel;
    }

done:
  if (widthp)
    *widthp = width;

  if (heightp)
    *heightp = height;

  if (xp)
    *xp = x;

  if (yp)
    *yp = y;
}

static gint
cally_text_get_offset_at_point (AtkText *text,
                                gint x,
                                gint y,
                                AtkCoordType coords)
{
  ClutterActor    *actor        = NULL;
  ClutterText     *clutter_text = NULL;
  const gchar *text_value;
  gint index;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return -1;

  clutter_text = CLUTTER_TEXT (actor);

  index = _cally_misc_get_index_at_point (clutter_text, x, y, coords);
  text_value = clutter_text_get_text (clutter_text);
  if (index == -1)
    return g_utf8_strlen (text_value, -1);
  else
    return g_utf8_pointer_to_offset (text_value, text_value + index);
}


/******** Auxiliar private methods ******/

/* ClutterText only maintains the current cursor position and a extra selection
   bound, but this could be before or after the cursor. This method returns
   the start and end positions in a proper order (so start<=end). This is
   similar to the function gtk_editable_get_selection_bounds */
static void
_cally_text_get_selection_bounds   (ClutterText *clutter_text,
                                    gint        *start_offset,
                                    gint        *end_offset)
{
  gint pos = -1;
  gint selection_bound = -1;

  pos = clutter_text_get_cursor_position (clutter_text);
  selection_bound = clutter_text_get_selection_bound (clutter_text);

  if (pos < selection_bound)
    {
      *start_offset = pos;
      *end_offset = selection_bound;
    }
  else
    {
      *start_offset = selection_bound;
      *end_offset = pos;
    }
}

static void
_cally_text_delete_text_cb (ClutterText *clutter_text,
                            gint         start_pos,
                            gint         end_pos,
                            gpointer     data)
{
  CallyText *cally_text = NULL;

  g_return_if_fail (CALLY_IS_TEXT (data));

  /* Ignore zero lengh deletions */
  if (end_pos - start_pos == 0)
    return;

  cally_text = CALLY_TEXT (data);

  if (!cally_text->priv->signal_name_delete)
    {
      cally_text->priv->signal_name_delete = "text_changed::delete";
      cally_text->priv->position_delete = start_pos;
      cally_text->priv->length_delete = end_pos - start_pos;
    }

  _notify_delete (cally_text);
}

static void
_cally_text_insert_text_cb (ClutterText *clutter_text,
                            gchar       *new_text,
                            gint         new_text_length,
                            gint        *position,
                            gpointer     data)
{
  CallyText *cally_text = NULL;

  g_return_if_fail (CALLY_IS_TEXT (data));

  cally_text = CALLY_TEXT (data);

  if (!cally_text->priv->signal_name_insert)
    {
      cally_text->priv->signal_name_insert = "text_changed::insert";
      cally_text->priv->position_insert = *position;
      cally_text->priv->length_insert = g_utf8_strlen (new_text, new_text_length);
    }

  /*
   * The signal will be emitted when the cursor position is updated,
   * or in an idle handler if it not updated.
   */
  if (cally_text->priv->insert_idle_handler == 0)
    cally_text->priv->insert_idle_handler = clutter_threads_add_idle (_idle_notify_insert,
                                                                      cally_text);
}

/***** atkeditabletext.h ******/

static void
cally_text_editable_text_interface_init (AtkEditableTextIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->set_text_contents = cally_text_set_text_contents;
  iface->insert_text = cally_text_insert_text;
  iface->delete_text = cally_text_delete_text;

  iface->set_run_attributes = NULL;
  iface->copy_text = NULL;
  iface->cut_text = NULL;
  iface->paste_text = NULL;
}

static void
cally_text_set_text_contents (AtkEditableText *text,
                              const gchar *string)
{
  ClutterActor *actor = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL)
    return;

  if (!clutter_text_get_editable (CLUTTER_TEXT (actor)))
    return;

  clutter_text_set_text (CLUTTER_TEXT (actor),
                         string);
}


static void
cally_text_insert_text (AtkEditableText *text,
                        const gchar *string,
                        gint length,
                        gint *position)
{
  ClutterActor *actor = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL)
    return;

  if (!clutter_text_get_editable (CLUTTER_TEXT (actor)))
    return;

  if (length < 0)
    length = g_utf8_strlen (string, -1);

  clutter_text_insert_text (CLUTTER_TEXT (actor),
                            string, *position);

  /* we suppose that the text insertion will be succesful,
     clutter-text doesn't warn about it. A option would be search for
     the text, but it seems not really required */
  *position += length;
}

static void cally_text_delete_text (AtkEditableText *text,
                                    gint start_pos,
                                    gint end_pos)
{
  ClutterActor *actor = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL)
    return;

  if (!clutter_text_get_editable (CLUTTER_TEXT (actor)))
    return;

  clutter_text_delete_text (CLUTTER_TEXT (actor),
                            start_pos, end_pos);
}

/* CallyActor */
static void
cally_text_notify_clutter (GObject    *obj,
                           GParamSpec *pspec)
{
  ClutterText *clutter_text = NULL;
  CallyText *cally_text = NULL;
  AtkObject *atk_obj = NULL;

  clutter_text = CLUTTER_TEXT (obj);
  atk_obj = clutter_actor_get_accessible (CLUTTER_ACTOR (obj));
  cally_text = CALLY_TEXT (atk_obj);

  if (g_strcmp0 (pspec->name, "position") == 0)
    {
      /* the selection can change also for the cursor position */
      if (_check_for_selection_change (cally_text, clutter_text))
        g_signal_emit_by_name (atk_obj, "text_selection_changed");

      g_signal_emit_by_name (atk_obj, "text_caret_moved",
                             clutter_text_get_cursor_position (clutter_text));
    }
  else if (g_strcmp0 (pspec->name, "selection-bound") == 0)
    {
      if (_check_for_selection_change (cally_text, clutter_text))
        g_signal_emit_by_name (atk_obj, "text_selection_changed");
    }
  else if (g_strcmp0 (pspec->name, "editable") == 0)
    {
      atk_object_notify_state_change (atk_obj, ATK_STATE_EDITABLE,
                                      clutter_text_get_editable (clutter_text));
    }
  else if (g_strcmp0 (pspec->name, "activatable") == 0)
    {
      _check_activate_action (cally_text, clutter_text);
    }
  else if (g_strcmp0 (pspec->name, "password-char") == 0)
    {
      if (clutter_text_get_password_char (clutter_text) != 0)
        atk_object_set_role (atk_obj, ATK_ROLE_PASSWORD_TEXT);
      else
        atk_object_set_role (atk_obj, ATK_ROLE_TEXT);
    }
  else
    {
      CALLY_ACTOR_CLASS (cally_text_parent_class)->notify_clutter (obj, pspec);
    }
}

static gboolean
_check_for_selection_change (CallyText *cally_text,
                             ClutterText *clutter_text)
{
  gboolean ret_val = FALSE;
  gint clutter_pos = -1;
  gint clutter_bound = -1;

  clutter_pos = clutter_text_get_cursor_position (clutter_text);
  clutter_bound = clutter_text_get_selection_bound (clutter_text);

  if (clutter_pos != clutter_bound)
    {
      if (clutter_pos != cally_text->priv->cursor_position ||
          clutter_bound != cally_text->priv->selection_bound)
        /*
         * This check is here as this function can be called for
         * notification of selection_bound and current_pos.  The
         * values of current_pos and selection_bound may be the same
         * for both notifications and we only want to generate one
         * text_selection_changed signal.
         */
        ret_val = TRUE;
    }
  else
    {
      /* We had a selection */
      ret_val = (cally_text->priv->cursor_position != cally_text->priv->selection_bound);
    }

  cally_text->priv->cursor_position = clutter_pos;
  cally_text->priv->selection_bound = clutter_bound;

  return ret_val;
}

static gboolean
_idle_notify_insert (gpointer data)
{
  CallyText *cally_text = NULL;

  cally_text = CALLY_TEXT (data);
  cally_text->priv->insert_idle_handler = 0;

  _notify_insert (cally_text);

  return FALSE;
}

static void
_notify_insert (CallyText *cally_text)
{
  if (cally_text->priv->signal_name_insert)
    {
      g_signal_emit_by_name (cally_text,
                             cally_text->priv->signal_name_insert,
                             cally_text->priv->position_insert,
                             cally_text->priv->length_insert);
      cally_text->priv->signal_name_insert = NULL;
    }
}

static void
_notify_delete (CallyText *cally_text)
{
  if (cally_text->priv->signal_name_delete)
    {
      g_signal_emit_by_name (cally_text,
                             cally_text->priv->signal_name_delete,
                             cally_text->priv->position_delete,
                             cally_text->priv->length_delete);
      cally_text->priv->signal_name_delete = NULL;
    }
}
/* atkaction */

static void
_cally_text_activate_action (CallyActor *cally_actor)
{
  ClutterActor *actor = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (cally_actor);

  clutter_text_activate (CLUTTER_TEXT (actor));
}

static void
_check_activate_action (CallyText   *cally_text,
                        ClutterText *clutter_text)
{

  if (clutter_text_get_activatable (clutter_text))
    {
      if (cally_text->priv->activate_action_id != 0)
        return;

      cally_text->priv->activate_action_id = cally_actor_add_action (CALLY_ACTOR (cally_text),
                                                                     "activate", NULL, NULL,
                                                                     _cally_text_activate_action);
    }
  else
    {
      if (cally_text->priv->activate_action_id == 0)
        return;

      if (cally_actor_remove_action (CALLY_ACTOR (cally_text),
                                     cally_text->priv->activate_action_id))
        {
          cally_text->priv->activate_action_id = 0;
        }
    }
}

/* GailTextUtil/GailMisc reimplementation methods */

/**
 * _cally_misc_add_attribute:
 *
 * Reimplementation of gail_misc_layout_get_run_attributes (check this
 * function for more documentation).
 *
 * Returns: A pointer to the new #AtkAttributeSet.
 **/
static AtkAttributeSet*
_cally_misc_add_attribute (AtkAttributeSet *attrib_set,
                           AtkTextAttribute attr,
                           gchar           *value)
{
  AtkAttributeSet *return_set;
  AtkAttribute *at = g_malloc (sizeof (AtkAttribute));
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = value;
  return_set = g_slist_prepend(attrib_set, at);
  return return_set;
}


static gint
_cally_atk_attribute_lookup_func (gconstpointer data,
                                  gconstpointer user_data)
{
    AtkTextAttribute attr = (AtkTextAttribute) user_data;
    AtkAttribute *at = (AtkAttribute *) data;
    if (!g_strcmp0 (at->name, atk_text_attribute_get_name (attr)))
        return 0;
    return -1;
}

static gboolean
_cally_misc_find_atk_attribute (AtkAttributeSet *attrib_set,
                                AtkTextAttribute attr)
{
  GSList* result = g_slist_find_custom ((GSList*) attrib_set,
                                        (gconstpointer) attr,
                                        _cally_atk_attribute_lookup_func);
  return (result != NULL);
}

/**
 * _cally_misc_layout_atk_attributes_from_pango:
 *
 * Store the pango attributes as their ATK equivalent in an existing
 * #AtkAttributeSet.
 *
 * Returns: A pointer to the updated #AtkAttributeSet.
 **/
static AtkAttributeSet*
_cally_misc_layout_atk_attributes_from_pango (AtkAttributeSet *attrib_set,
                                              PangoAttrIterator *iter)
{
  PangoAttrString *pango_string;
  PangoAttrInt *pango_int;
  PangoAttrColor *pango_color;
  PangoAttrLanguage *pango_lang;
  PangoAttrFloat *pango_float;
  gchar *value = NULL;

  if ((pango_string = (PangoAttrString*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_FAMILY)) != NULL)
    {
      value = g_strdup_printf("%s", pango_string->value);
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                              ATK_TEXT_ATTR_FAMILY_NAME,
                                              value);
    }
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_STYLE)) != NULL)
    {
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_STYLE,
      g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STYLE, pango_int->value)));
    }
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_WEIGHT)) != NULL)
    {
      value = g_strdup_printf("%i", pango_int->value);
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_WEIGHT,
                                            value);
    }
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_VARIANT)) != NULL)
    {
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_VARIANT,
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_VARIANT, pango_int->value)));
    }
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_STRETCH)) != NULL)
    {
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_STRETCH,
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STRETCH, pango_int->value)));
    }
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_SIZE)) != NULL)
    {
      value = g_strdup_printf("%i", pango_int->value / PANGO_SCALE);
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_SIZE,
                                            value);
    }
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_UNDERLINE)) != NULL)
    {
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_UNDERLINE,
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_UNDERLINE, pango_int->value)));
    }
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_STRIKETHROUGH)) != NULL)
    {
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_STRIKETHROUGH,
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STRIKETHROUGH, pango_int->value)));
    }
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_RISE)) != NULL)
    {
      value = g_strdup_printf("%i", pango_int->value);
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_RISE,
                                            value);
    }
  if ((pango_lang = (PangoAttrLanguage*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_LANGUAGE)) != NULL)
    {
      value = g_strdup( pango_language_to_string( pango_lang->value));
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_LANGUAGE,
                                            value);
    }
  if ((pango_float = (PangoAttrFloat*) pango_attr_iterator_get (iter,
                                   PANGO_ATTR_SCALE)) != NULL)
    {
      value = g_strdup_printf("%g", pango_float->value);
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_SCALE,
                                            value);
    }
  if ((pango_color = (PangoAttrColor*) pango_attr_iterator_get (iter,
                                    PANGO_ATTR_FOREGROUND)) != NULL)
    {
      value = g_strdup_printf ("%u,%u,%u",
                               pango_color->color.red,
                               pango_color->color.green,
                               pango_color->color.blue);
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_FG_COLOR,
                                            value);
    }
  if ((pango_color = (PangoAttrColor*) pango_attr_iterator_get (iter,
                                     PANGO_ATTR_BACKGROUND)) != NULL)
    {
      value = g_strdup_printf ("%u,%u,%u",
                               pango_color->color.red,
                               pango_color->color.green,
                               pango_color->color.blue);
      attrib_set = _cally_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_BG_COLOR,
                                            value);
    }

  return attrib_set;
}

static AtkAttributeSet*
_cally_misc_add_actor_color_to_attribute_set (AtkAttributeSet *attrib_set,
                                              ClutterText *clutter_text)
{
  ClutterColor color;
  gchar *value;

  clutter_text_get_color (clutter_text, &color);
  value = g_strdup_printf ("%u,%u,%u",
                           (guint) (color.red * 65535 / 255),
                           (guint) (color.green * 65535 / 255),
                           (guint) (color.blue * 65535 / 255));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_FG_COLOR,
                                          value);
  return attrib_set;
}


/**
 * _cally_misc_layout_get_run_attributes:
 *
 * Reimplementation of gail_misc_layout_get_run_attributes (check this
 * function for more documentation).
 *
 * Returns: A pointer to the #AtkAttributeSet.
 **/
static AtkAttributeSet*
_cally_misc_layout_get_run_attributes (AtkAttributeSet *attrib_set,
                                       ClutterText     *clutter_text,
                                       gint            offset,
                                       gint            *start_offset,
                                       gint            *end_offset)
{
  PangoAttrIterator *iter;
  PangoAttrList *attr;
  gint index, start_index, end_index;
  gboolean is_next = TRUE;
  glong len;
  PangoLayout *layout = clutter_text_get_layout (clutter_text);
  gchar *text = (gchar*) clutter_text_get_text (clutter_text);

  len = g_utf8_strlen (text, -1);
  /* Grab the attributes of the PangoLayout, if any */
  if ((attr = pango_layout_get_attributes (layout)) == NULL)
    {
      *start_offset = 0;
      *end_offset = len;
      _cally_misc_add_actor_color_to_attribute_set (attrib_set, clutter_text);
    }
  else
    {
      iter = pango_attr_list_get_iterator (attr);
      /* Get invariant range offsets */
      /* If offset out of range, set offset in range */
      if (offset > len)
        offset = len;
      else if (offset < 0)
        offset = 0;

      index = g_utf8_offset_to_pointer (text, offset) - text;
      pango_attr_iterator_range (iter, &start_index, &end_index);
      while (is_next)
        {
            if (index >= start_index && index < end_index)
              {
                *start_offset = g_utf8_pointer_to_offset (text,
                                                          text + start_index);
                if (end_index == G_MAXINT)
                    /* Last iterator */
                    end_index = len;

                *end_offset = g_utf8_pointer_to_offset (text,
                                                        text + end_index);
                break;
              }
            is_next = pango_attr_iterator_next (iter);
            pango_attr_iterator_range (iter, &start_index, &end_index);
        }

      /* Get attributes */
      attrib_set = _cally_misc_layout_atk_attributes_from_pango (attrib_set, iter);
      pango_attr_iterator_destroy (iter);
    }

  if (!_cally_misc_find_atk_attribute (attrib_set, ATK_TEXT_ATTR_FG_COLOR))
    attrib_set = _cally_misc_add_actor_color_to_attribute_set (attrib_set, clutter_text);

  return attrib_set;
}


/**
 * _cally_misc_layout_get_default_attributes:
 *
 * Reimplementation of gail_misc_layout_get_default_attributes (check this
 * function for more documentation).
 *
 * Returns: A pointer to the #AtkAttributeSet.
 **/
static AtkAttributeSet*
_cally_misc_layout_get_default_attributes (AtkAttributeSet *attrib_set,
                                           ClutterText *clutter_text)
{
  PangoLayout *layout;
  PangoContext *context;
  PangoLanguage* language;
  PangoFontDescription* font;
  PangoWrapMode mode;
  gchar *value = NULL;
  gint int_value;
  ClutterTextDirection text_direction;
  PangoAttrIterator *iter;
  PangoAttrList *attr;

  text_direction = clutter_actor_get_text_direction (CLUTTER_ACTOR (clutter_text));
  switch (text_direction)
    {
    case CLUTTER_TEXT_DIRECTION_DEFAULT:
      value = g_strdup ("none");
      break;

    case CLUTTER_TEXT_DIRECTION_LTR:
      value = g_strdup ("ltr");
      break;

    case CLUTTER_TEXT_DIRECTION_RTL:
      value = g_strdup ("rtl");
      break;

    default:
      value = g_strdup ("none");
      break;
    }
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_DIRECTION,
                                          value);

  layout = clutter_text_get_layout (clutter_text);
  context = pango_layout_get_context (layout);
  if (context)
    {
      if ((language = pango_context_get_language (context)))
        {
          value = g_strdup (pango_language_to_string (language));
          attrib_set = _cally_misc_add_attribute (attrib_set,
                                                  ATK_TEXT_ATTR_LANGUAGE, value);
        }

      if ((font = pango_context_get_font_description (context)))
        {
          value = g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STYLE,
                                                          pango_font_description_get_style (font)));
          attrib_set = _cally_misc_add_attribute (attrib_set,
                                                  ATK_TEXT_ATTR_STYLE,
                                                  value);

          value = g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_VARIANT,
                                                          pango_font_description_get_variant (font)));
          attrib_set = _cally_misc_add_attribute (attrib_set,
                                                  ATK_TEXT_ATTR_VARIANT, value);

          value = g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STRETCH,
                                                          pango_font_description_get_stretch (font)));
          attrib_set = _cally_misc_add_attribute (attrib_set,
                                                  ATK_TEXT_ATTR_STRETCH, value);

          value = g_strdup (pango_font_description_get_family (font));
          attrib_set = _cally_misc_add_attribute (attrib_set,
                                                  ATK_TEXT_ATTR_FAMILY_NAME, value);
          value = g_strdup_printf ("%d", pango_font_description_get_weight (font));
          attrib_set = _cally_misc_add_attribute (attrib_set,
                                                  ATK_TEXT_ATTR_WEIGHT, value);

          value = g_strdup_printf ("%i", pango_font_description_get_size (font) / PANGO_SCALE);
          attrib_set = _cally_misc_add_attribute (attrib_set,
                                                  ATK_TEXT_ATTR_SIZE, value);

        }

    }

  if (pango_layout_get_justify (layout))
    int_value = 3;
  else
    {
      PangoAlignment align;

      align = pango_layout_get_alignment (layout);
      if (align == PANGO_ALIGN_LEFT)
        int_value = 0;
      else if (align == PANGO_ALIGN_CENTER)
        int_value = 2;
      else /* if (align == PANGO_ALIGN_RIGHT) */
        int_value = 1;
    }
  value = g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_JUSTIFICATION,
                                                  int_value));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_JUSTIFICATION,
                                          value);

  mode = pango_layout_get_wrap (layout);
  if (mode == PANGO_WRAP_WORD)
    int_value = 2;
  else /* if (mode == PANGO_WRAP_CHAR) */
    int_value = 1;
  value = g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_WRAP_MODE,
                                                  int_value));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_WRAP_MODE, value);

  if ((attr = clutter_text_get_attributes (clutter_text)))
    {
      iter = pango_attr_list_get_iterator (attr);
      /* Get attributes */
      attrib_set = _cally_misc_layout_atk_attributes_from_pango (attrib_set, iter);
      pango_attr_iterator_destroy (iter);
    }


  if (!_cally_misc_find_atk_attribute (attrib_set, ATK_TEXT_ATTR_FG_COLOR))
    attrib_set = _cally_misc_add_actor_color_to_attribute_set (attrib_set,
                                                               clutter_text);

  value = g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_FG_STIPPLE, 0));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_FG_STIPPLE,
                                          value);

  value = g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_BG_STIPPLE, 0));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_BG_STIPPLE,
                                          value);
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_BG_FULL_HEIGHT,
                                          g_strdup_printf ("%i", 0));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP,
                                          g_strdup_printf ("%i", 0));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_PIXELS_BELOW_LINES,
                                          g_strdup_printf ("%i", 0));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_PIXELS_ABOVE_LINES,
                                          g_strdup_printf ("%i", 0));
  value = g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_EDITABLE,
                                                  clutter_text_get_editable (clutter_text)));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_EDITABLE,
                                          value);

  value = g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_INVISIBLE,
                                                  !CLUTTER_ACTOR_IS_VISIBLE (clutter_text)));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_INVISIBLE, value);

  value = g_strdup_printf ("%i", pango_layout_get_indent (layout));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_INDENT, value);
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_RIGHT_MARGIN,
                                          g_strdup_printf ("%i", 0));
  attrib_set = _cally_misc_add_attribute (attrib_set,
                                          ATK_TEXT_ATTR_LEFT_MARGIN,
                                          g_strdup_printf ("%i", 0));

  return attrib_set;
}

static int
_cally_misc_get_index_at_point (ClutterText *clutter_text,
                                gint         x,
                                gint         y,
                                AtkCoordType coords)
{
  gint index, x_window, y_window, x_toplevel, y_toplevel;
  gint x_temp, y_temp;
  gboolean ret;
  ClutterVertex verts[4];
  PangoLayout *layout;
  gint x_layout, y_layout;

  clutter_text_get_layout_offsets (clutter_text, &x_layout, &y_layout);

  clutter_actor_get_abs_allocation_vertices (CLUTTER_ACTOR (clutter_text), verts);
  x_window = verts[0].x;
  y_window = verts[0].y;

  x_temp =  x - x_layout - x_window;
  y_temp =  y - y_layout - y_window;

  if (coords == ATK_XY_SCREEN)
    {
      _cally_actor_get_top_level_origin (CLUTTER_ACTOR (clutter_text), &x_toplevel,
                                         &y_toplevel);
      x_temp -= x_toplevel;
      y_temp -= y_toplevel;
    }

  layout = clutter_text_get_layout (clutter_text);
  ret = pango_layout_xy_to_index (layout,
                                  x_temp * PANGO_SCALE,
                                  y_temp * PANGO_SCALE,
                                  &index, NULL);

  if (!ret)
    {
      if (x_temp < 0 || y_temp < 0)
        index = 0;
      else
        index = -1;
    }
  return index;
}
