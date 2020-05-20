/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-generic-accessible.c: generic accessible
 *
 * Copyright 2013 Igalia, S.L.
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

/**
 * SECTION:st-generic-accessible
 * @short_description: An accessible class with signals for
 * implementing specific Atk interfaces
 *
 * #StGenericAccessible is mainly a workaround for the current lack of
 * of a proper support for GValue at javascript. See bug#703412 for
 * more information. We implement the accessible interfaces, but proxy
 * the virtual functions into signals, which gjs can catch.
 *
 * #StGenericAccessible is an #StWidgetAccessible
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "st-generic-accessible.h"

static void atk_value_iface_init (AtkValueIface *iface);

G_DEFINE_TYPE_WITH_CODE(StGenericAccessible,
                        st_generic_accessible,
                        ST_TYPE_WIDGET_ACCESSIBLE,
                        G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE,
                                               atk_value_iface_init));
/* Signals */
enum
{
  GET_CURRENT_VALUE,
  GET_MAXIMUM_VALUE,
  GET_MINIMUM_VALUE,
  SET_CURRENT_VALUE,
  GET_MINIMUM_INCREMENT,
  LAST_SIGNAL
};

static guint st_generic_accessible_signals [LAST_SIGNAL] = { 0 };

static void
st_generic_accessible_init (StGenericAccessible *accessible)
{
}

static void
st_generic_accessible_class_init (StGenericAccessibleClass *klass)
{
  /**
   * StGenericAccessible::get-current-value:
   * @self: the #StGenericAccessible
   *
   * Emitted when atk_value_get_current_value() is called on
   * @self. Right now we only care about doubles, so the value is
   * directly returned by the signal.
   *
   * Returns: value of the current element.
   */
  st_generic_accessible_signals[GET_CURRENT_VALUE] =
    g_signal_new ("get-current-value",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_DOUBLE, 0);

  /**
   * StGenericAccessible::get-maximum-value:
   * @self: the #StGenericAccessible
   *
   * Emitted when atk_value_get_maximum_value() is called on
   * @self. Right now we only care about doubles, so the value is
   * directly returned by the signal.
   *
   * Returns: maximum value of the accessible.
   */
  st_generic_accessible_signals[GET_MAXIMUM_VALUE] =
    g_signal_new ("get-maximum-value",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_DOUBLE, 0);

  /**
   * StGenericAccessible::get-minimum-value:
   * @self: the #StGenericAccessible
   *
   * Emitted when atk_value_get_current_value() is called on
   * @self. Right now we only care about doubles, so the value is
   * directly returned by the signal.
   *
   * Returns: minimum value of the accessible.
   */
  st_generic_accessible_signals[GET_MINIMUM_VALUE] =
    g_signal_new ("get-minimum-value",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_DOUBLE, 0);

  /**
   * StGenericAccessible::get-minimum-increment:
   * @self: the #StGenericAccessible
   *
   * Emitted when atk_value_get_minimum_increment() is called on
   * @self. Right now we only care about doubles, so the value is
   * directly returned by the signal.
   *
   * Returns: value of the current element.
   */
  st_generic_accessible_signals[GET_MINIMUM_INCREMENT] =
    g_signal_new ("get-minimum-increment",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_DOUBLE, 0);

  /**
   * StGenericAccessible::set-current-value:
   * @self: the #StGenericAccessible
   * @new_value: the new value for the accessible
   *
   * Emitted when atk_value_set_current_value() is called on
   * @self. Right now we only care about doubles, so the value is
   * directly returned by the signal.
   */
  st_generic_accessible_signals[SET_CURRENT_VALUE] =
    g_signal_new ("set-current-value",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_DOUBLE);

}

static void
st_generic_accessible_get_current_value (AtkValue *obj,
                                         GValue   *value)
{
  gdouble current_value = 0;

  g_value_init (value, G_TYPE_DOUBLE);
  g_signal_emit (G_OBJECT (obj), st_generic_accessible_signals[GET_CURRENT_VALUE], 0, &current_value);
  g_value_set_double (value, current_value);
}

static void
st_generic_accessible_get_maximum_value (AtkValue *obj,
                                         GValue   *value)
{
  gdouble current_value = 0;

  g_value_init (value, G_TYPE_DOUBLE);
  g_signal_emit (G_OBJECT (obj), st_generic_accessible_signals[GET_MAXIMUM_VALUE], 0, &current_value);
  g_value_set_double (value, current_value);
}

static void
st_generic_accessible_get_minimum_value (AtkValue *obj,
                                         GValue   *value)
{
  gdouble current_value = 0;

  g_value_init (value, G_TYPE_DOUBLE);
  g_signal_emit (G_OBJECT (obj), st_generic_accessible_signals[GET_MINIMUM_VALUE], 0, &current_value);
  g_value_set_double (value, current_value);
}

static void
st_generic_accessible_get_minimum_increment (AtkValue *obj,
                                             GValue   *value)
{
  gdouble current_value = 0;

  g_value_init (value, G_TYPE_DOUBLE);
  g_signal_emit (G_OBJECT (obj), st_generic_accessible_signals[GET_MINIMUM_INCREMENT], 0, &current_value);
  g_value_set_double (value, current_value);
}

static gboolean
st_generic_accessible_set_current_value (AtkValue *obj,
                                         const GValue *value)
{
  gdouble current_value = 0;

  current_value = g_value_get_double (value);
  g_signal_emit (G_OBJECT (obj), st_generic_accessible_signals[SET_CURRENT_VALUE], 0, current_value);

  return TRUE; // we assume that the value was properly set
}

static void
atk_value_iface_init (AtkValueIface *iface)
{
  iface->get_current_value = st_generic_accessible_get_current_value;
  iface->get_maximum_value = st_generic_accessible_get_maximum_value;
  iface->get_minimum_value = st_generic_accessible_get_minimum_value;
  iface->get_minimum_increment = st_generic_accessible_get_minimum_increment;
  iface->set_current_value = st_generic_accessible_set_current_value;
}

/**
 * st_generic_accessible_new_for_actor:
 * @actor: a #Clutter Actor
 *
 * Create a new #StGenericAccessible for @actor.
 *
 * This is useful only for custom widgets that need a proxy for #AtkObject.
 *
 * Returns: (transfer full): a new #AtkObject
 */
AtkObject*
st_generic_accessible_new_for_actor (ClutterActor *actor)
{
  AtkObject *accessible = NULL;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  accessible = ATK_OBJECT (g_object_new (ST_TYPE_GENERIC_ACCESSIBLE,
                                         NULL));
  atk_object_initialize (accessible, actor);

  return accessible;
}
