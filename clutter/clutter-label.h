/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
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

#ifndef __CLUTTER_LABEL_H__
#define __CLUTTER_LABEL_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-color.h>
#include <pango/pango.h>


G_BEGIN_DECLS

#define CLUTTER_TYPE_LABEL (clutter_label_get_type ())

#define CLUTTER_LABEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_LABEL, ClutterLabel))

#define CLUTTER_LABEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_LABEL, ClutterLabelClass))

#define CLUTTER_IS_LABEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_LABEL))

#define CLUTTER_IS_LABEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_LABEL))

#define CLUTTER_LABEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_LABEL, ClutterLabelClass))

typedef struct _ClutterLabel ClutterLabel;
typedef struct _ClutterLabelClass ClutterLabelClass;
typedef struct _ClutterLabelPrivate ClutterLabelPrivate;

struct _ClutterLabel
{
  ClutterActor         parent;

  /*< private >*/
  ClutterLabelPrivate   *priv;
};

struct _ClutterLabelClass 
{
  /*< private >*/
  ClutterActorClass parent_class;

  void (*_clutter_label_1) (void);
  void (*_clutter_label_2) (void);
  void (*_clutter_label_3) (void);
  void (*_clutter_label_4) (void);
}; 

GType clutter_label_get_type (void) G_GNUC_CONST;

ClutterActor *        clutter_label_new                (void);

ClutterActor*         clutter_label_new_full           (const gchar        *font_name,
							const gchar        *text,
							const ClutterColor *color);

ClutterActor *        clutter_label_new_with_text      (const gchar        *font_name,
                                                        const gchar        *text);
void                  clutter_label_set_text           (ClutterLabel       *label,
						        const gchar        *text);
G_CONST_RETURN gchar *clutter_label_get_text           (ClutterLabel       *label);
void                  clutter_label_set_font_name      (ClutterLabel       *label,
						        const gchar        *font_name);
G_CONST_RETURN gchar *clutter_label_get_font_name      (ClutterLabel       *label);
void                  clutter_label_set_color          (ClutterLabel       *label,
						        const ClutterColor *color);
void                  clutter_label_get_color          (ClutterLabel       *label,
						        ClutterColor       *color);
void                  clutter_label_set_ellipsize      (ClutterLabel       *label,
                                                        PangoEllipsizeMode  mode);
PangoEllipsizeMode    clutter_label_get_ellipsize      (ClutterLabel       *label);
void                  clutter_label_set_line_wrap      (ClutterLabel       *label,
                                                        gboolean            wrap);
gboolean              clutter_label_get_line_wrap      (ClutterLabel       *label);
void                  clutter_label_set_line_wrap_mode (ClutterLabel       *label,
                                                        PangoWrapMode       wrap_mode);
PangoWrapMode         clutter_label_get_line_wrap_mode (ClutterLabel       *label);
PangoLayout *         clutter_label_get_layout         (ClutterLabel       *label);
void                  clutter_label_set_attributes     (ClutterLabel       *label,
                                                        PangoAttrList      *attrs);
PangoAttrList *       clutter_label_get_attributes     (ClutterLabel       *label);
void                  clutter_label_set_use_markup     (ClutterLabel       *label,
                                                        gboolean            setting);
gboolean              clutter_label_get_use_markup     (ClutterLabel       *label);
void                  clutter_label_set_alignment      (ClutterLabel       *label,
                                                        PangoAlignment      alignment);
PangoAlignment        clutter_label_get_alignment      (ClutterLabel       *label);
void                  clutter_label_set_justify        (ClutterLabel       *label,
                                                        gboolean            justify);
gboolean              clutter_label_get_justify        (ClutterLabel       *label);

G_END_DECLS

#endif /* __CLUTTER_LABEL_H__ */
