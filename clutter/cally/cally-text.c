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

/*
 * IMPLEMENTATION NOTES:
 *
 * * AtkText: There are still some methods not implemented yet:
 *     atk_text_get_default_attributes
 *     atk_text_get_character_extents
 *     atk_text_get_offset_at_point
 *
 *     See details on bug CB#1733
 *
 * * AtkEditableText: some methods will not be implemented
 *
 *     * atk_editable_text_set_run_attributes: ClutterText has some
 *       properties equivalent to the AtkAttributte, but it doesn't
 *       allow you to define it by
 *
 *     * atk_editable_text_copy: Clutter has no Clipboard support
 *
 *     * atk_editable_text_paste: Clutter has no Clipboard support
 *
 *     * atk_editable_text_cut: Clutter has no Clipboard support. In
 *           this case, as cut is basically a copy&delete combination,
 *           we could have implemented it using the delete, but IMHO,
 *           it would be weird to cut a text, get the text removed and
 *           then not be able to paste the text
 *
 *     See details on bug CB#1734
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cally-text.h"
#include "cally-actor-private.h"

#include "clutter-main.h"

static void cally_text_class_init (CallyTextClass *klass);
static void cally_text_init       (CallyText *cally_text);
static void cally_text_finalize   (GObject *obj);

/* AtkObject */
static void                   cally_text_real_initialize (AtkObject *obj,
                                                          gpointer   data);
static const gchar *          cally_text_get_name        (AtkObject *obj);
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
                                                                   PangoLayout     *layout,
                                                                   gchar           *text,
                                                                   gint            offset,
                                                                   gint            *start_offset,
                                                                   gint            *end_offset);

G_DEFINE_TYPE_WITH_CODE (CallyText,
                         cally_text,
                         CALLY_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT,
                                                cally_text_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_EDITABLE_TEXT,
                                                cally_text_editable_text_interface_init));

#define CALLY_TEXT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CALLY_TYPE_TEXT, CallyTextPrivate))

struct _CallyTextPrivate
{
  /* Cached ClutterText values*/
  gint cursor_position;
  gint selection_bound;

  /* text_changed::insert stuff */
  gchar *signal_name_insert;
  gint position_insert;
  gint length_insert;
  guint insert_idle_handler;

  /* text_changed::delete stuff */
  gchar *signal_name_delete;
  gint position_delete;
  gint length_delete;

  /* action */
  guint activate_action_id;
};

static void
cally_text_class_init (CallyTextClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);
  CallyActorClass *cally_class  = CALLY_ACTOR_CLASS (klass);

  gobject_class->finalize = cally_text_finalize;

  class->initialize = cally_text_real_initialize;
  class->get_name   = cally_text_get_name;
  class->ref_state_set = cally_text_ref_state_set;

  cally_class->notify_clutter = cally_text_notify_clutter;

  g_type_class_add_private (gobject_class, sizeof (CallyTextPrivate));
}

static void
cally_text_init (CallyText *cally_text)
{
  CallyTextPrivate *priv = CALLY_TEXT_GET_PRIVATE (cally_text);

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

  obj->role = ATK_ROLE_TEXT;
}

static const gchar *
cally_text_get_name (AtkObject *obj)
{
  const gchar *name;

  g_return_val_if_fail (CALLY_IS_ACTOR (obj), NULL);

  name = ATK_OBJECT_CLASS (cally_text_parent_class)->get_name (obj);
  if (name == NULL)
    {
      ClutterActor *actor = NULL;

      actor = CALLY_GET_CLUTTER_ACTOR (obj);

      if (actor == NULL) /* State is defunct */
        name = NULL;
      else
        name = clutter_text_get_text (CLUTTER_TEXT (actor));
    }

  return name;
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
/*   iface->get_default_attributes  = cally_text_get_default_attributes; */
/*   iface->get_character_extents = */
/*   iface->get_offset_at_point = */

}

static gchar*
cally_text_get_text (AtkText *text,
                     gint start_offset,
                     gint end_offset)
{
  ClutterActor *actor = NULL;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* Object is defunct */
    return NULL;

  return clutter_text_get_chars (CLUTTER_TEXT (actor),
                                 start_offset, end_offset);
}

static gunichar
cally_text_get_character_at_offset (AtkText *text,
                                    gint     offset)
{
  ClutterActor *actor      = NULL;
  gchar        *string     = NULL;
  gchar        *index      = NULL;
  gunichar      unichar;

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return '\0';

  string = clutter_text_get_chars (CLUTTER_TEXT (actor), 0, -1);
  if (offset >= g_utf8_strlen (string, -1))
    {
      unichar = '\0';
    }
  else
    {
      index = g_utf8_offset_to_pointer (string, offset);

      unichar = g_utf8_get_char(index);
    }

  g_free(string);

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
#if 0
  ClutterText  *clutter_text = NULL;
  CallyText    *cally_text   = NULL;
#endif

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return NULL;

#if 0
  clutter_text = CLUTTER_TEXT (actor);
  cally_text = CALLY_TEXT (text);

  return gail_text_util_get_text (cally_text->priv->textutil,
                                  clutter_text_get_layout (clutter_text),
                                  GAIL_BEFORE_OFFSET,
                                  boundary_type,
                                  offset,
                                  start_offset, end_offset);
#endif

  return NULL;
}

static gchar*
cally_text_get_text_at_offset (AtkText         *text,
                               gint             offset,
                               AtkTextBoundary  boundary_type,
                               gint            *start_offset,
                               gint            *end_offset)
{
  ClutterActor *actor        = NULL;
#if 0
  ClutterText  *clutter_text = NULL;
  CallyText    *cally_text   = NULL;
#endif

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return NULL;

#if 0
  clutter_text = CLUTTER_TEXT (actor);
  cally_text = CALLY_TEXT (text);

  return gail_text_util_get_text (cally_text->priv->textutil,
                                  clutter_text_get_layout (clutter_text),
                                  GAIL_AT_OFFSET,
                                  boundary_type,
                                  offset,
                                  start_offset, end_offset);
#endif

  return NULL;
}

static gchar*
cally_text_get_text_after_offset (AtkText         *text,
                                  gint             offset,
                                  AtkTextBoundary  boundary_type,
                                  gint            *start_offset,
                                  gint            *end_offset)
{
  ClutterActor *actor        = NULL;
#if 0
  ClutterText  *clutter_text = NULL;
  CallyText    *cally_text   = NULL;
#endif

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return NULL;

#if 0
  clutter_text = CLUTTER_TEXT (actor);
  cally_text = CALLY_TEXT (text);

  return gail_text_util_get_text (cally_text->priv->textutil,
                                  clutter_text_get_layout (clutter_text),
                                  GAIL_AFTER_OFFSET,
                                  boundary_type,
                                  offset,
                                  start_offset, end_offset);
#endif

  return NULL;
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

  actor = CALLY_GET_CLUTTER_ACTOR (text);
  if (actor == NULL) /* State is defunct */
    return 0;

  if (!clutter_text_get_selectable (CLUTTER_TEXT (actor)))
    return 0;

  selection_bound = clutter_text_get_selection_bound (CLUTTER_TEXT (actor));

  if (selection_bound > 0)
    return 1;
  else
    return 0;
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
                                                  clutter_text_get_layout (clutter_text),
                                                  (gchar*)clutter_text_get_text (clutter_text),
                                                  offset,
                                                  start_offset,
                                                  end_offset);

  return at_set;
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

  /* Not implemented, see IMPLEMENTATION NOTES*/
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
                                       PangoLayout     *layout,
                                       gchar           *text,
                                       gint            offset,
                                       gint            *start_offset,
                                       gint            *end_offset)
{
  PangoAttrIterator *iter;
  PangoAttrList *attr;
  PangoAttrString *pango_string;
  PangoAttrInt *pango_int;
  PangoAttrColor *pango_color;
  PangoAttrLanguage *pango_lang;
  PangoAttrFloat *pango_float;
  gint index, start_index, end_index;
  gboolean is_next = TRUE;
  gchar *value = NULL;
  glong len;

  len = g_utf8_strlen (text, -1);
  /* Grab the attributes of the PangoLayout, if any */
  if ((attr = pango_layout_get_attributes (layout)) == NULL)
    {
      *start_offset = 0;
      *end_offset = len;
      return attrib_set;
    }
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
  pango_attr_iterator_destroy (iter);
  return attrib_set;
}
