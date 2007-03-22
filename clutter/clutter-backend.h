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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CLUTTER_BACKEND_H__
#define __CLUTTER_BACKEND_H__

#include <glib-object.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-event.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND            (clutter_backend_get_type ())
#define CLUTTER_BACKEND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND, ClutterBackend))
#define CLUTTER_IS_BACKEND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND))
#define CLUTTER_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND, ClutterBackendClass))
#define CLUTTER_IS_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND))
#define CLUTTER_BACKEND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND, ClutterBackendClass))

typedef struct _ClutterBackend          ClutterBackend;
typedef struct _ClutterBackendClass     ClutterBackendClass;

struct _ClutterBackend
{
  GObject parent_instance;

  /*< private >*/
  /* events queue: every backend must implement one */
  GQueue *events_queue;
  gpointer queue_head;

  /* settings */
  guint double_click_time;
  guint double_click_distance;

  /* multiple button click detection */
  guint32 button_click_time[2];
  guint32 button_number[2];
  gint button_x[2];
  gint button_y[2];
};

struct _ClutterBackendClass
{
  GObjectClass parent_class;

  /* vfuncs */
  gboolean      (* pre_parse)   (ClutterBackend  *backend,
                                 GError         **error);
  gboolean      (* post_parse)  (ClutterBackend  *backend,
                                 GError         **error);
  gboolean      (* init_stage)  (ClutterBackend  *backend,
                                 GError         **error);
  void          (* init_events) (ClutterBackend  *backend);
  ClutterActor *(* get_stage)   (ClutterBackend  *backend);
  void          (* add_options) (ClutterBackend  *backend,
                                 GOptionGroup    *group);
};

GType         clutter_backend_get_type    (void) G_GNUC_CONST;
ClutterActor *clutter_backend_get_stage   (ClutterBackend  *backend);
void          clutter_backend_add_options (ClutterBackend  *backend,
                                           GOptionGroup    *group);
gboolean      clutter_backend_pre_parse   (ClutterBackend  *backend,
                                           GError         **error);
gboolean      clutter_backend_post_parse  (ClutterBackend  *backend,
                                           GError         **error);
gboolean      clutter_backend_init_stage  (ClutterBackend  *backend,
                                           GError         **error);
void          clutter_backend_init_events (ClutterBackend  *backend);

ClutterEvent *clutter_backend_get_event   (ClutterBackend *backend);
ClutterEvent *clutter_backend_peek_event  (ClutterBackend *backend);
void          clutter_backend_put_event   (ClutterBackend *backend,
                                           ClutterEvent   *event);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_H__ */
