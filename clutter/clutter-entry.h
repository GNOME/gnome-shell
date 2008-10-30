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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_ENTRY_H__
#define __CLUTTER_ENTRY_H__

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
  /*< private >*/
  ClutterActor parent_instance;

  ClutterEntryPrivate   *priv;
};

/**
 * ClutterEntryClass:
 * @paint_cursor: virtual function for subclasses to use to draw a custom
 *   cursor instead of the default one
 * @text_changed: signal class handler for ClutterEntry::text-changed
 * @cursor_event: signal class handler for ClutterEntry::cursor-event
 * @activate: signal class handler for ClutterEntry::activate
 *
 * Class fo entry actors.
 *
 * Since: 0.4
 */
struct _ClutterEntryClass 
{
  /*< private >*/
  ClutterActorClass parent_class;
  
  /*< public >*/
  /* vfuncs, not signals */
  void (* paint_cursor) (ClutterEntry    *entry);
  
  /* signals */
  void (* text_changed) (ClutterEntry    *entry);
  void (* cursor_event) (ClutterEntry    *entry,
                         ClutterGeometry *geometry);
  void (* activate)     (ClutterEntry    *entry);
    
  /*< private >*/
  /* padding for future */
  void (*_clutter_entry_1) (void);
  void (*_clutter_entry_2) (void);
  void (*_clutter_entry_3) (void);
  void (*_clutter_entry_4) (void);
}; 

GType clutter_entry_get_type (void) G_GNUC_CONST;

ClutterActor *        clutter_entry_new                 (void);
ClutterActor *        clutter_entry_new_full            (const gchar        *font_name,
							 const gchar        *text,
							 const ClutterColor *color);
ClutterActor *        clutter_entry_new_with_text       (const gchar        *font_name,
                                                         const gchar        *text);
void                  clutter_entry_set_text            (ClutterEntry       *entry,
						         const gchar        *text);
G_CONST_RETURN gchar *clutter_entry_get_text            (ClutterEntry       *entry);
void                  clutter_entry_set_font_name       (ClutterEntry       *entry,
						         const gchar        *font_name);
G_CONST_RETURN gchar *clutter_entry_get_font_name       (ClutterEntry       *entry);
void                  clutter_entry_set_color           (ClutterEntry       *entry,
						         const ClutterColor *color);
void                  clutter_entry_get_color           (ClutterEntry       *entry,
						         ClutterColor       *color);
PangoLayout *         clutter_entry_get_layout          (ClutterEntry       *entry);
void                  clutter_entry_set_alignment       (ClutterEntry       *entry,
                                                         PangoAlignment      alignment);
PangoAlignment        clutter_entry_get_alignment       (ClutterEntry       *entry);
void                  clutter_entry_set_cursor_position (ClutterEntry       *entry,
                                                         gint                position);
gint                  clutter_entry_get_cursor_position (ClutterEntry       *entry);
void                  clutter_entry_insert_unichar      (ClutterEntry       *entry,
                                                         gunichar            wc);
void                  clutter_entry_delete_chars        (ClutterEntry       *entry,
                                                         guint               len);
void                  clutter_entry_insert_text         (ClutterEntry       *entry,
                                                         const gchar        *text,
                                                         gssize              position);
void                  clutter_entry_delete_text         (ClutterEntry       *entry,
                                                         gssize              start_pos,
                                                         gssize              end_pos);
void                  clutter_entry_set_visible_cursor  (ClutterEntry       *entry,
                                                         gboolean            visible);
gboolean              clutter_entry_get_visible_cursor  (ClutterEntry       *entry);

void                  clutter_entry_set_visibility      (ClutterEntry       *entry,
                                                         gboolean            visible);
gboolean              clutter_entry_get_visibility      (ClutterEntry       *entry);
void                  clutter_entry_set_invisible_char  (ClutterEntry       *entry,
                                                         gunichar            wc);
gunichar              clutter_entry_get_invisible_char  (ClutterEntry       *entry);
void                  clutter_entry_set_max_length      (ClutterEntry       *entry,
                                                         gint                max);
gint                  clutter_entry_get_max_length      (ClutterEntry       *entry);

#ifndef CLUTTER_DISABLE_DEPRECATED
void                  clutter_entry_handle_key_event    (ClutterEntry       *entry,
                                                         ClutterKeyEvent    *kev);
#endif

G_END_DECLS

#endif /* __CLUTTER_ENTRY_H__ */
