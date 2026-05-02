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

#include "gtkactionobserver.h"

G_BEGIN_DECLS

#define GTK_TYPE_ACTION_OBSERVABLE                          (gtk_action_observable_get_type ())

G_DECLARE_INTERFACE (GtkActionObservable, gtk_action_observable, GTK, ACTION_OBSERVABLE, GObject)

typedef struct _GtkActionObservableInterface                GtkActionObservableInterface;

struct _GtkActionObservableInterface
{
  GTypeInterface g_iface;

  void (* register_observer)   (GtkActionObservable *observable,
                                const gchar         *action_name,
                                GtkActionObserver   *observer);
  void (* unregister_observer) (GtkActionObservable *observable,
                                const gchar         *action_name,
                                GtkActionObserver   *observer);
};

G_END_DECLS
