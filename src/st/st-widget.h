/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-widget.h: Base class for St actors
 *
 * Copyright 2007 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2009 Abderrahim Kitouni
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

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_WIDGET_H__
#define __ST_WIDGET_H__

#include <clutter/clutter.h>
#include <st/st-types.h>
#include <st/st-theme.h>
#include <st/st-theme-node.h>

G_BEGIN_DECLS

#define ST_TYPE_WIDGET                 (st_widget_get_type ())
G_DECLARE_DERIVABLE_TYPE (StWidget, st_widget, ST, WIDGET, ClutterActor)

/**
 * StDirectionType:
 * @ST_DIR_TAB_FORWARD: Move forward.
 * @ST_DIR_TAB_BACKWARD: Move backward.
 * @ST_DIR_UP: Move up.
 * @ST_DIR_DOWN: Move down.
 * @ST_DIR_LEFT: Move left.
 * @ST_DIR_RIGHT: Move right.
 *
 * Enumeration for focus direction.
 */
typedef enum
{
  ST_DIR_TAB_FORWARD,
  ST_DIR_TAB_BACKWARD,
  ST_DIR_UP,
  ST_DIR_DOWN,
  ST_DIR_LEFT,
  ST_DIR_RIGHT,
} StDirectionType;

typedef struct _StWidgetClass          StWidgetClass;

/**
 * StWidgetClass:
 *
 * Base class for stylable actors.
 */
struct _StWidgetClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /* signals */
  void     (* style_changed)       (StWidget         *self);
  void     (* popup_menu)          (StWidget         *self);

  /* vfuncs */

  /**
   * StWidgetClass::navigate_focus:
   * @self: the "top level" container
   * @from: (nullable): the actor that the focus is coming from
   * @direction: the direction focus is moving in
   */
  gboolean (* navigate_focus)      (StWidget         *self,
                                    ClutterActor     *from,
                                    StDirectionType   direction);
  GType    (* get_accessible_type) (void);

  GList *  (* get_focus_chain)     (StWidget         *widget);
};

void                  st_widget_set_style_pseudo_class    (StWidget        *actor,
                                                           const gchar     *pseudo_class_list);
void                  st_widget_add_style_pseudo_class    (StWidget        *actor,
                                                           const gchar     *pseudo_class);
void                  st_widget_remove_style_pseudo_class (StWidget        *actor,
                                                           const gchar     *pseudo_class);
const gchar *         st_widget_get_style_pseudo_class    (StWidget        *actor);
gboolean              st_widget_has_style_pseudo_class    (StWidget        *actor,
                                                           const gchar     *pseudo_class);

void                  st_widget_set_style_class_name      (StWidget        *actor,
                                                           const gchar     *style_class_list);
void                  st_widget_add_style_class_name      (StWidget        *actor,
                                                           const gchar     *style_class);
void                  st_widget_remove_style_class_name   (StWidget        *actor,
                                                           const gchar     *style_class);
const gchar *         st_widget_get_style_class_name      (StWidget        *actor);
gboolean              st_widget_has_style_class_name      (StWidget        *actor,
                                                           const gchar     *style_class);

void                  st_widget_set_style                 (StWidget        *actor,
                                                           const gchar     *style);
const gchar *         st_widget_get_style                 (StWidget        *actor);
void                  st_widget_set_track_hover           (StWidget        *widget,
                                                           gboolean         track_hover);
gboolean              st_widget_get_track_hover           (StWidget        *widget);
void                  st_widget_set_hover                 (StWidget        *widget,
                                                           gboolean         hover);
void                  st_widget_sync_hover                (StWidget        *widget);
gboolean              st_widget_get_hover                 (StWidget        *widget);
void                  st_widget_popup_menu                (StWidget        *self);

void                  st_widget_ensure_style              (StWidget        *widget);

void                  st_widget_set_can_focus             (StWidget        *widget,
                                                           gboolean         can_focus);
gboolean              st_widget_get_can_focus             (StWidget        *widget);
gboolean              st_widget_navigate_focus            (StWidget        *widget,
                                                           ClutterActor    *from,
                                                           StDirectionType  direction,
                                                           gboolean         wrap_around);

ClutterActor *        st_widget_get_label_actor           (StWidget        *widget);
void                  st_widget_set_label_actor           (StWidget        *widget,
                                                           ClutterActor    *label);

/* Only to be used by sub-classes of StWidget */
void                  st_widget_style_changed             (StWidget        *widget);
StThemeNode *         st_widget_get_theme_node            (StWidget        *widget);
StThemeNode *         st_widget_peek_theme_node           (StWidget        *widget);

GList *               st_widget_get_focus_chain           (StWidget        *widget);
void                  st_widget_paint_background          (StWidget            *widget,
                                                           ClutterPaintContext *paint_context);

/* debug methods */
char  *st_describe_actor       (ClutterActor *actor);

/* accessibility methods */
void                  st_widget_set_accessible_role      (StWidget    *widget,
                                                          AtkRole      role);
AtkRole               st_widget_get_accessible_role      (StWidget    *widget);
void                  st_widget_add_accessible_state     (StWidget    *widget,
                                                          AtkStateType state);
void                  st_widget_remove_accessible_state  (StWidget    *widget,
                                                          AtkStateType state);
void                  st_widget_set_accessible_name      (StWidget    *widget,
                                                          const gchar *name);
const gchar *         st_widget_get_accessible_name      (StWidget    *widget);
void                  st_widget_set_accessible           (StWidget    *widget,
                                                          AtkObject   *accessible);
G_END_DECLS

#endif /* __ST_WIDGET_H__ */
