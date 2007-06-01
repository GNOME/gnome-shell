/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Neil Jagdish Patel <njp@o-hand.com
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef _HAVE_CLUTTER_ENTRY_H
#define _HAVE_CLUTTER_ENTRY_H

#include <clutter/clutter-actor.h>
#include <clutter/clutter-color.h>
#include <clutter/clutter-event.h>
#include <pango/pango.h>


G_BEGIN_DECLS

#define CLUTTER_TYPE_ENTRY (clutter_entry_get_type ())

#define CLUTTER_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_ENTRY, ClutterEntry))

#define CLUTTER_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_ENTRY, ClutterEntryClass))

#define CLUTTER_IS_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_ENTRY))

#define CLUTTER_IS_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_ENTRY))

#define CLUTTER_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_ENTRY, ClutterEntryClass))

typedef struct _ClutterEntry ClutterEntry;
typedef struct _ClutterEntryClass ClutterEntryClass;
typedef struct _ClutterEntryPrivate ClutterEntryPrivate;

struct _ClutterEntry
{
  ClutterActor         parent;

  /*< private >*/
  ClutterEntryPrivate   *priv;
};

struct _ClutterEntryClass 
{
  /*< private >*/
  ClutterActorClass parent_class;
  
  /* signals */
  void     (* text_changed)         (ClutterEntry           *stage);
  
  /* padding for future */
  void (*_clutter_entry_1) (void);
  void (*_clutter_entry_2) (void);
  void (*_clutter_entry_3) (void);
  void (*_clutter_entry_4) (void);
}; 

GType clutter_entry_get_type (void) G_GNUC_CONST;

ClutterActor *        clutter_entry_new                (void);

ClutterActor*         clutter_entry_new_full           (const gchar  *font_name,
							const gchar  *text,
							ClutterColor *color);

ClutterActor *        clutter_entry_new_with_text      (const gchar        *font_name,
                                                        const gchar        *text);
void                  clutter_entry_set_text           (ClutterEntry       *entry,
						        const gchar        *text);
G_CONST_RETURN gchar *clutter_entry_get_text           (ClutterEntry       *entry);
void                  clutter_entry_set_font_name      (ClutterEntry       *entry,
						        const gchar        *font_name);
G_CONST_RETURN gchar *clutter_entry_get_font_name      (ClutterEntry       *entry);
void                  clutter_entry_set_color          (ClutterEntry       *entry,
						        const ClutterColor *color);
void                  clutter_entry_get_color          (ClutterEntry       *entry,
						        ClutterColor       *color);
PangoLayout *         clutter_entry_get_layout         (ClutterEntry       *entry);
void                  clutter_entry_set_alignment      (ClutterEntry       *entry,
                                                        PangoAlignment      alignment);
PangoAlignment        clutter_entry_get_alignment      (ClutterEntry       *entry);
void                  clutter_entry_set_position       (ClutterEntry       *entry,
                                                        gint                position);
gint                  clutter_entry_get_position       (ClutterEntry       *entry);
void                  clutter_entry_handle_key_event   (ClutterEntry       *entry,
                                                        ClutterKeyEvent    *kev);
void                  clutter_entry_add                (ClutterEntry       *entry,
                                                        gunichar            wc);
void                  clutter_entry_remove             (ClutterEntry       *entry,
                                                        guint               len);
void                  clutter_entry_insert_text        (ClutterEntry       *entry,
                                                        const gchar        *text,
                                                        gssize              position);
void                  clutter_entry_delete_text        (ClutterEntry       *entry,
                                                        gssize              start_pos,
                                                        gssize              end_pos);
void                  clutter_entry_set_visible_cursor (ClutterEntry       *entry,
                                                        gboolean            visible);
gboolean              clutter_entry_get_visible_cursor (ClutterEntry       *entry);
G_END_DECLS

#endif /* _HAVE_CLUTTER_ENTRY_H */
