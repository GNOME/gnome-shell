/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2008  Intel Corporation.
 *
 * Authored By: Øyvind Kolås <pippin@o-hand.com>
 *              Emmanuele Bassi <ebassi@linux.intel.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_TEXT_H__
#define __CLUTTER_TEXT_H__

#include <clutter/clutter-actor.h>
#include <pango/pango.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TEXT               (clutter_text_get_type ())
#define CLUTTER_TEXT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TEXT, ClutterText))
#define CLUTTER_TEXT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TEXT, ClutterTextClass))
#define CLUTTER_IS_TEXT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TEXT))
#define CLUTTER_IS_TEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TEXT))
#define CLUTTER_TEXT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TEXT, ClutterTextClass))

typedef struct _ClutterText        ClutterText;
typedef struct _ClutterTextPrivate ClutterTextPrivate;
typedef struct _ClutterTextClass   ClutterTextClass;

/**
 * ClutterText:
 *
 * The #ClutterText struct contains only private data.
 *
 * Since: 1.0
 */
struct _ClutterText
{
  /*< private >*/
  ClutterActor parent_instance;

  ClutterTextPrivate *priv;
};

/**
 * ClutterTextClass:
 * @text_changed: class handler for the #ClutterText::text-changed signal
 * @activate: class handler for the #ClutterText::activate signal
 * @cursor_event: class handler for the #ClutterText::cursor_event signal
 *
 * The #ClutterTextClass struct contains only private data.
 *
 * Since: 1.0
 */
struct _ClutterTextClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /*< public >*/
  /* signals, not vfuncs */
  void (* text_changed) (ClutterText           *self);
  void (* activate)     (ClutterText           *self);
  void (* cursor_event) (ClutterText           *self,
                         const ClutterGeometry *geometry);

  /*< private >*/
  /* padding for future expansion */
  void (* _clutter_reserved1) (void);
  void (* _clutter_reserved2) (void);
  void (* _clutter_reserved3) (void);
  void (* _clutter_reserved4) (void);
  void (* _clutter_reserved5) (void);
  void (* _clutter_reserved6) (void);
  void (* _clutter_reserved7) (void);
  void (* _clutter_reserved8) (void);
};

GType clutter_text_get_type (void) G_GNUC_CONST;

ClutterActor *        clutter_text_new                 (void);
ClutterActor *        clutter_text_new_full            (const gchar        *font_name,
                                                        const gchar        *text,
                                                        const ClutterColor *color);
ClutterActor *        clutter_text_new_with_text       (const gchar        *font_name,
                                                        const gchar        *text);

G_CONST_RETURN gchar *clutter_text_get_text            (ClutterText        *self);
void                  clutter_text_set_text            (ClutterText        *self,
                                                        const gchar        *text);
PangoLayout *         clutter_text_get_layout          (ClutterText        *self);
void                  clutter_text_set_color           (ClutterText        *self,
                                                        const ClutterColor *color);
void                  clutter_text_get_color           (ClutterText        *self,
                                                        ClutterColor       *color);
void                  clutter_text_set_font_name       (ClutterText        *self,
                                                        const gchar        *font_name);
G_CONST_RETURN gchar *clutter_text_get_font_name       (ClutterText        *self);

void                  clutter_text_set_ellipsize       (ClutterText        *self,
                                                        PangoEllipsizeMode  mode);
PangoEllipsizeMode    clutter_text_get_ellipsize       (ClutterText        *self);
void                  clutter_text_set_line_wrap       (ClutterText        *self,
                                                        gboolean            line_wrap);
gboolean              clutter_text_get_line_wrap       (ClutterText        *self);
void                  clutter_text_set_line_wrap_mode  (ClutterText        *self,
                                                        PangoWrapMode       wrap_mode);
PangoWrapMode         clutter_text_get_line_wrap_mode  (ClutterText        *self);
PangoLayout *         clutter_text_get_layout          (ClutterText        *self);
void                  clutter_text_set_attributes      (ClutterText        *self,
                                                        PangoAttrList      *attrs);
PangoAttrList *       clutter_text_get_attributes      (ClutterText        *self);
void                  clutter_text_set_use_markup      (ClutterText        *self,
                                                        gboolean            setting);
gboolean              clutter_text_get_use_markup      (ClutterText        *self);
void                  clutter_text_set_alignment       (ClutterText        *self,
                                                        PangoAlignment      alignment);
PangoAlignment        clutter_text_get_alignment       (ClutterText        *self);
void                  clutter_text_set_justify         (ClutterText        *self,
                                                        gboolean            justify);
gboolean              clutter_text_get_justify         (ClutterText        *self);

void                  clutter_text_insert_unichar      (ClutterText        *self,
                                                        gunichar            wc);
void                  clutter_text_delete_chars        (ClutterText        *self,
                                                        guint               n_chars);
void                  clutter_text_insert_text         (ClutterText        *self,
                                                        const gchar        *text,
                                                        gssize              position);
void                  clutter_text_delete_text         (ClutterText        *self,
                                                        gssize              start_pos,
                                                        gssize              end_pos);
gchar *               clutter_text_get_chars           (ClutterText        *self,
                                                        gssize              start_pos,
                                                        gssize              end_pos);
void                  clutter_text_set_editable        (ClutterText        *self,
                                                        gboolean            editable);
gboolean              clutter_text_get_editable        (ClutterText        *self);
void                  clutter_text_set_activatable     (ClutterText        *self,
                                                        gboolean            activatable);
gboolean              clutter_text_get_activatable     (ClutterText        *self);

gint                  clutter_text_get_cursor_position (ClutterText        *self);
void                  clutter_text_set_cursor_position (ClutterText        *self,
                                                        gint                position);
void                  clutter_text_set_cursor_visible  (ClutterText        *self,
                                                        gboolean            cursor_visible);
gboolean              clutter_text_get_cursor_visible  (ClutterText        *self);
void                  clutter_text_set_cursor_color    (ClutterText        *self,
                                                        const ClutterColor *color);
void                  clutter_text_get_cursor_color    (ClutterText        *self,
                                                        ClutterColor       *color);
void                  clutter_text_set_cursor_size     (ClutterText        *self,
                                                        gint                size);
guint                 clutter_text_get_cursor_size     (ClutterText        *self);
void                  clutter_text_set_selectable      (ClutterText        *self,
                                                        gboolean            selectable);
gboolean              clutter_text_get_selectable      (ClutterText        *self);
void                  clutter_text_set_selection_bound (ClutterText        *self,
                                                        gint                selection_bound);
gint                  clutter_text_get_selection_bound (ClutterText        *self);
void                  clutter_text_set_selection       (ClutterText        *self,
                                                        gssize              start_pos,
                                                        gssize              end_pos);
gchar *               clutter_text_get_selection       (ClutterText        *self);
void                  clutter_text_set_password_char   (ClutterText        *self,
                                                        gunichar            wc);
gunichar              clutter_text_get_password_char   (ClutterText        *self);
void                  clutter_text_set_max_length      (ClutterText        *self,
                                                        gint                max);
gint                  clutter_text_get_max_length      (ClutterText        *self);
gboolean              clutter_text_activate            (ClutterText        *self);

G_END_DECLS

#endif /* __CLUTTER_TEXT_H__ */
