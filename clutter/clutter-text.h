/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2006-2008 OpenedHand
 *
 * Authored By Øyvind Kolås <pippin@o-hand.com>
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

#ifndef __CLUTTER_TEXT_H__
#define __CLUTTER_TEXT_H__

#include <clutter/clutter-label.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TEXT (clutter_text_get_type ())

#define CLUTTER_TEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_TEXT, ClutterText))

#define CLUTTER_TEXT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_TEXT, ClutterTextClass))

#define CLUTTER_IS_TEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_TEXT))

#define CLUTTER_IS_TEXT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_TEXT))

#define CLUTTER_TEXT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_TEXT, ClutterTextClass))

typedef struct _ClutterText        ClutterText;
typedef struct _ClutterTextPrivate ClutterTextPrivate;
typedef struct _ClutterTextClass   ClutterTextClass;

struct _ClutterText
{
  ClutterLabel     parent_instance;

  /*< private >*/
  ClutterTextPrivate *priv;
};

struct _ClutterTextClass
{
  ClutterLabelClass parent_class;

  void (* text_changed) (ClutterText        *text);
  void (* activate)     (ClutterText        *text);
  void (* cursor_event) (ClutterText        *text,
                         ClutterGeometry *geometry);
};

GType clutter_text_get_type (void) G_GNUC_CONST;

ClutterActor *clutter_text_new_full            (const gchar        *font_name,
                                             const gchar        *text,
                                             const ClutterColor *color);
ClutterActor *clutter_text_new_with_text       (const gchar        *font_name,
                                             const gchar        *text);

void          clutter_text_set_editable        (ClutterText           *label,
                                             gboolean            editable);
gboolean      clutter_text_get_editable        (ClutterText           *label);
void          clutter_text_set_activatable     (ClutterText           *label,
                                             gboolean            activatable);
gboolean      clutter_text_get_activatable      (ClutterText          *label);

gint          clutter_text_get_cursor_position (ClutterText           *label);
void          clutter_text_set_cursor_position (ClutterText           *label,
                                             gint                position);
void          clutter_text_set_cursor_visible  (ClutterText           *label,
                                             gboolean            cursor_visible);
gboolean      clutter_text_get_cursor_visible  (ClutterText           *label);
void          clutter_text_set_cursor_color    (ClutterText           *text,
                                             const ClutterColor *color);
void          clutter_text_get_cursor_color    (ClutterText           *text,
                                             ClutterColor       *color);
void          clutter_text_set_selectable      (ClutterText           *label,
                                             gboolean            selectable);
gboolean      clutter_text_get_selectable      (ClutterText           *label);
void          clutter_text_set_selection_bound (ClutterText           *text,
                                             gint                selection_bound);
gint          clutter_text_get_selection_bound (ClutterText           *text);
gchar *       clutter_text_get_selection       (ClutterText           *text);
void          clutter_text_insert_unichar      (ClutterText           *ttext,
                                             gunichar            wc);


/* add a custom action that can be used in keybindings */
void clutter_text_add_action (ClutterText    *ttext,
                           const gchar *name,
                           gboolean (*func) (ClutterText            *ttext,
                                             const gchar         *commandline,
                                             ClutterEvent        *event));

/* invoke an action registered by you or one of the tidy text default actions */
gboolean clutter_text_action      (ClutterText            *ttext,
                                const gchar         *commandline,
                                ClutterEvent        *event);

void     clutter_text_mappings_clear (ClutterText *ttext);

/* Add a keybinding to handle for the default keypress vfunc handler */
void     clutter_text_add_mapping (ClutterText           *ttext,
                                guint               keyval,
                                ClutterModifierType state,
                                const gchar        *commandline);

G_END_DECLS

#endif /* __CLUTTER_TEXT_H__ */
