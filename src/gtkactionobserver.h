/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACTION_OBSERVER                            (gtk_action_observer_get_type ())

G_DECLARE_INTERFACE (GtkActionObserver, gtk_action_observer, GTK, ACTION_OBSERVER, GObject)

typedef struct _GtkActionObservable                         GtkActionObservable;

struct _GtkActionObserverInterface
{
  GTypeInterface g_iface;

  void (* action_added)           (GtkActionObserver    *observer,
                                   GtkActionObservable  *observable,
                                   const gchar          *action_name,
                                   const GVariantType   *parameter_type,
                                   gboolean              enabled,
                                   GVariant             *state);
  void (* action_enabled_changed) (GtkActionObserver    *observer,
                                   GtkActionObservable  *observable,
                                   const gchar          *action_name,
                                   gboolean              enabled);
  void (* action_state_changed)   (GtkActionObserver    *observer,
                                   GtkActionObservable  *observable,
                                   const gchar          *action_name,
                                   GVariant             *state);
  void (* action_removed)         (GtkActionObserver    *observer,
                                   GtkActionObservable  *observable,
                                   const gchar          *action_name);
  void (* primary_accel_changed)  (GtkActionObserver    *observer,
                                   GtkActionObservable  *observable,
                                   const gchar          *action_name,
                                   const gchar          *action_and_target);
};

void                    gtk_action_observer_action_added                (GtkActionObserver   *observer,
                                                                         GtkActionObservable *observable,
                                                                         const gchar         *action_name,
                                                                         const GVariantType  *parameter_type,
                                                                         gboolean             enabled,
                                                                         GVariant            *state);
void                    gtk_action_observer_action_enabled_changed      (GtkActionObserver   *observer,
                                                                         GtkActionObservable *observable,
                                                                         const gchar         *action_name,
                                                                         gboolean             enabled);
void                    gtk_action_observer_action_state_changed        (GtkActionObserver   *observer,
                                                                         GtkActionObservable *observable,
                                                                         const gchar         *action_name,
                                                                         GVariant            *state);
void                    gtk_action_observer_action_removed              (GtkActionObserver   *observer,
                                                                         GtkActionObservable *observable,
                                                                         const gchar         *action_name);
void                    gtk_action_observer_primary_accel_changed       (GtkActionObserver   *observer,
                                                                         GtkActionObservable *observable,
                                                                         const gchar         *action_name,
                                                                         const gchar         *action_and_target);

G_END_DECLS
