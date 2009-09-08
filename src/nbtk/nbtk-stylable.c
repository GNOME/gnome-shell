/*
 * nbtk-stylable.c: Interface for stylable objects
 *
 * Copyright 2008 Intel Corporation
 * Copyright 2009 Intel Corporation.
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 *             Thomas Wood <thomas@linux.intel.com>
 *
 */

/**
 * SECTION:nbtk-stylable
 * @short_description: Interface for stylable objects
 *
 * Stylable objects are classes that can have "style properties", that is
 * properties that can be changed by attaching a #NbtkStyle to them.
 *
 * Objects can choose to subclass #NbtkWidget, and thus inherit all the
 * #NbtkWidget style properties; or they can subclass #NbtkWidget and
 * reimplement the #NbtkStylable interface to add new style properties
 * specific for them (and their subclasses); or, finally, they can simply
 * subclass #GObject and implement #NbtkStylable to install new properties.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib-object.h>
#include <gobject/gvaluecollector.h>
#include <gobject/gobjectnotifyqueue.c>

#include "nbtk-marshal.h"
#include "nbtk-private.h"
#include "nbtk-stylable.h"

enum
{
  STYLE_CHANGED,
  STYLE_NOTIFY,
  CHANGED,

  LAST_SIGNAL
};

static GObjectNotifyContext property_notify_context = { 0, };

static GParamSpecPool *style_property_spec_pool = NULL;

static GQuark          quark_real_owner         = 0;
static GQuark          quark_style              = 0;

static guint stylable_signals[LAST_SIGNAL] = { 0, };

static void
nbtk_stylable_notify_dispatcher (GObject     *gobject,
                                 guint        n_pspecs,
                                 GParamSpec **pspecs)
{
  guint i;

  for (i = 0; i < n_pspecs; i++)
    g_signal_emit (gobject, stylable_signals[STYLE_NOTIFY],
                   g_quark_from_string (pspecs[i]->name),
                   pspecs[i]);
}

static void
nbtk_stylable_base_finalize (gpointer g_iface)
{
  GList *list, *node;

  list = g_param_spec_pool_list_owned (style_property_spec_pool,
                                       G_TYPE_FROM_INTERFACE (g_iface));

  for (node = list; node; node = node->next)
    {
      GParamSpec *pspec = node->data;

      g_param_spec_pool_remove (style_property_spec_pool, pspec);
      g_param_spec_unref (pspec);
    }

  g_list_free (list);
}

static void
nbtk_stylable_base_init (gpointer g_iface)
{
  static gboolean initialised = FALSE;

  if (G_UNLIKELY (!initialised))
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_iface);

      initialised = TRUE;

      quark_real_owner = g_quark_from_static_string ("nbtk-stylable-real-owner-quark");
      quark_style = g_quark_from_static_string ("nbtk-stylable-style-quark");

      style_property_spec_pool = g_param_spec_pool_new (FALSE);

      property_notify_context.quark_notify_queue = g_quark_from_static_string ("NbtkStylable-style-property-notify-queue");
      property_notify_context.dispatcher = nbtk_stylable_notify_dispatcher;

      /**
       * NbtkStylable:style:
       *
       * The #NbtkStyle attached to a stylable object.
       */
      g_object_interface_install_property (g_iface,
                                           g_param_spec_object ("style",
                                                                "Style",
                                                                "A style object",
                                                                NBTK_TYPE_STYLE,
                                                                NBTK_PARAM_READWRITE));

      /**
       * NbtkStylable::style-changed:
       * @stylable: the #NbtkStylable that received the signal
       * @old_style: the previously set #NbtkStyle for @stylable
       *
       * The ::style-changed signal is emitted each time one of the style
       * properties have changed.
       */
      stylable_signals[STYLE_CHANGED] =
        g_signal_new (I_("style-changed"),
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (NbtkStylableIface, style_changed),
                      NULL, NULL,
                      _nbtk_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

  /**
   * NbtkStylable::stylable-changed:
   * @actor: the actor that received the signal
   *
   * The ::changed signal is emitted each time any of the properties of the
   * stylable has changed.
   */
  stylable_signals[CHANGED] =
    g_signal_new (I_("stylable-changed"),
                  iface_type,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (NbtkStylableIface, stylable_changed),
                  NULL, NULL,
                  _nbtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

      stylable_signals[STYLE_NOTIFY] =
        g_signal_new (I_("style-notify"),
                      iface_type,
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (NbtkStylableIface, style_notify),
                      NULL, NULL,
                      _nbtk_marshal_VOID__PARAM,
                      G_TYPE_NONE, 1,
                      G_TYPE_PARAM);
    }
}

GType
nbtk_stylable_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    {
      GTypeInfo stylable_info = {
        sizeof (NbtkStylableIface),
        nbtk_stylable_base_init,
        nbtk_stylable_base_finalize
      };

      our_type = g_type_register_static (G_TYPE_INTERFACE,
                                         I_("NbtkStylable"),
                                         &stylable_info, 0);
    }

  return our_type;
}

void
nbtk_stylable_freeze_notify (NbtkStylable *stylable)
{
  g_return_if_fail (NBTK_IS_STYLABLE (stylable));

  g_object_ref (stylable);
  g_object_notify_queue_freeze (G_OBJECT (stylable), &property_notify_context);
  g_object_unref (stylable);
}

void
nbtk_stylable_thaw_notify (NbtkStylable *stylable)
{
  GObjectNotifyQueue *nqueue;

  g_return_if_fail (NBTK_IS_STYLABLE (stylable));

  g_object_ref (stylable);

  nqueue = g_object_notify_queue_from_object (G_OBJECT (stylable),
                                              &property_notify_context);

  if (!nqueue || !nqueue->freeze_count)
    g_warning ("%s: property-changed notification for %s(%p) is not frozen",
               G_STRFUNC, G_OBJECT_TYPE_NAME (stylable), stylable);
  else
    g_object_notify_queue_thaw (G_OBJECT (stylable), nqueue);

  g_object_unref (stylable);
}

void
nbtk_stylable_notify (NbtkStylable *stylable,
                      const gchar  *property_name)
{
  GParamSpec *pspec;

  g_return_if_fail (NBTK_IS_STYLABLE (stylable));
  g_return_if_fail (property_name != NULL);

  g_object_ref (stylable);

  pspec = g_param_spec_pool_lookup (style_property_spec_pool,
                                    property_name,
                                    G_OBJECT_TYPE (stylable),
                                    TRUE);

  if (!pspec)
    g_warning ("%s: object class `%s' has no style property named `%s'",
               G_STRFUNC,
               G_OBJECT_TYPE_NAME (stylable),
               property_name);
  else
    {
      GObjectNotifyQueue *nqueue;

      nqueue = g_object_notify_queue_freeze (G_OBJECT (stylable),
                                              &property_notify_context);
      g_object_notify_queue_add (G_OBJECT (stylable), nqueue, pspec);
      g_object_notify_queue_thaw (G_OBJECT (stylable), nqueue);
    }

  g_object_unref (stylable);
}

/**
 * nbtk_stylable_iface_install_property:
 * @iface: a #NbtkStylableIface
 * @owner_type: #GType of the style property owner
 * @pspec: a #GParamSpec
 *
 * Installs a property for @owner_type using @pspec as the property
 * description.
 *
 * This function should be used inside the #NbtkStylableIface initialization
 * function of a class, for instance:
 *
 * <informalexample><programlisting>
 * G_DEFINE_TYPE_WITH_CODE (FooActor, foo_actor, CLUTTER_TYPE_ACTOR,
 *                          G_IMPLEMENT_INTERFACE (NBTK_TYPE_STYLABLE,
 *                                                 nbtk_stylable_init));
 * ...
 * static void
 * nbtk_stylable_init (NbtkStylableIface *iface)
 * {
 *   static gboolean is_initialized = FALSE;
 *
 *   if (!is_initialized)
 *     {
 *       ...
 *       nbtk_stylable_iface_install_property (stylable,
 *                                             FOO_TYPE_ACTOR,
 *                                             g_param_spec_int ("x-spacing",
 *                                                               "X Spacing",
 *                                                               "Horizontal spacing",
 *                                                               -1, G_MAXINT,
 *                                                               2,
 *                                                               G_PARAM_READWRITE));
 *       ...
 *     }
 * }
 * </programlisting></informalexample>
 */
void
nbtk_stylable_iface_install_property (NbtkStylableIface *iface,
                                      GType              owner_type,
                                      GParamSpec        *pspec)
{
  g_return_if_fail (NBTK_IS_STYLABLE_IFACE (iface));
  g_return_if_fail (owner_type != G_TYPE_INVALID);
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (pspec->flags & G_PARAM_READABLE);
  g_return_if_fail (!(pspec->flags & (G_PARAM_CONSTRUCT_ONLY | G_PARAM_CONSTRUCT
)));

  if (g_param_spec_pool_lookup (style_property_spec_pool, pspec->name,
                                owner_type,
                                FALSE))
    {
      g_warning ("%s: class `%s' already contains a style property named `%s'",
                 G_STRLOC,
                 g_type_name (owner_type),
                 pspec->name);
      return;
    }

  g_param_spec_ref_sink (pspec);
  g_param_spec_set_qdata_full (pspec, quark_real_owner,
                               g_strdup (g_type_name (owner_type)),
                               g_free);

  g_param_spec_pool_insert (style_property_spec_pool,
                            pspec,
                            owner_type);
}

/**
 * nbtk_stylable_list_properties:
 * @stylable: a #NbtkStylable
 * @n_props: return location for the number of properties, or %NULL
 *
 * Retrieves all the #GParamSpec<!-- -->s installed by @stylable.
 *
 * Return value: an array of #GParamSpec<!-- -->s. Free it with
 *   g_free() when done.
 */
GParamSpec **
nbtk_stylable_list_properties (NbtkStylable *stylable,
                               guint        *n_props)
{
  GParamSpec **pspecs = NULL;
  guint n;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);

  pspecs = g_param_spec_pool_list (style_property_spec_pool,
                                   G_OBJECT_TYPE (stylable),
                                   &n);
  if (n_props)
    *n_props = n;

  return pspecs;
}

/**
 * nbtk_stylable_find_property:
 * @stylable: a #NbtkStylable
 * @property_name: the name of the property to find
 *
 * Finds the #GParamSpec installed by @stylable for the property
 * with @property_name.
 *
 * Return value: a #GParamSpec for the given property, or %NULL if
 *   no property with that name was found
 */
GParamSpec *
nbtk_stylable_find_property (NbtkStylable *stylable,
                             const gchar  *property_name)
{
  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (style_property_spec_pool,
                                   property_name,
                                   G_OBJECT_TYPE (stylable),
                                   TRUE);
}

static inline void
nbtk_stylable_get_property_internal (NbtkStylable *stylable,
                                     GParamSpec   *pspec,
                                     GValue       *value)
{
  NbtkStyle *style;
  GValue real_value = { 0, };

  style = nbtk_stylable_get_style (stylable);

  if (!style)
    {
      g_value_reset (value);
      return;
    }

  nbtk_style_get_property (style, stylable, pspec, &real_value);

  g_value_copy (&real_value, value);
  g_value_unset (&real_value);

}

/**
 * nbtk_stylable_get_property:
 * @stylable: a #NbtkStylable
 * @property_name: the name of the property
 * @value: return location for an empty #GValue
 *
 * Retrieves the value of @property_name for @stylable, and puts it
 * into @value.
 */
void
nbtk_stylable_get_property (NbtkStylable *stylable,
                            const gchar  *property_name,
                            GValue       *value)
{
  GParamSpec *pspec;

  g_return_if_fail (NBTK_IS_STYLABLE (stylable));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  pspec = nbtk_stylable_find_property (stylable, property_name);
  if (!pspec)
    {
      g_warning ("Stylable class `%s' doesn't have a property named `%s'",
                 g_type_name (G_OBJECT_TYPE (stylable)),
                 property_name);
      return;
    }

  if (!(pspec->flags & G_PARAM_READABLE))
    {
      g_warning ("Style property `%s' of class `%s' is not readable",
                 pspec->name,
                 g_type_name (G_OBJECT_TYPE (stylable)));
      return;
    }

  if (G_VALUE_TYPE (value) != G_PARAM_SPEC_VALUE_TYPE (pspec))
    {
      g_warning ("Passed value is not of the requested type `%s' for "
                 "the style property `%s' of class `%s'",
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
                 pspec->name,
                 g_type_name (G_OBJECT_TYPE (stylable)));
      return;
    }

  nbtk_stylable_get_property_internal (stylable, pspec, value);
}

/**
 * nbtk_stylable_get:
 * @stylable: a #NbtkStylable
 * @first_property_name: name of the first property to get
 * @Varargs: return location for the first property, followed optionally
 *   by more name/return location pairs, followed by %NULL
 *
 * Gets the style properties for @stylable.
 *
 * In general, a copy is made of the property contents and the called
 * is responsible for freeing the memory in the appropriate manner for
 * the property type.
 *
 * <example>
 * <title>Using nbtk_stylable_get(<!-- -->)</title>
 * <para>An example of using nbtk_stylable_get() to get the contents of
 * two style properties - one of type #G_TYPE_INT and one of type
 * #CLUTTER_TYPE_COLOR:</para>
 * <programlisting>
 *   gint x_spacing;
 *   ClutterColor *bg_color;
 *
 *   nbtk_stylable_get (stylable,
 *                      "x-spacing", &amp;x_spacing,
 *                      "bg-color", &amp;bg_color,
 *                      NULL);
 *
 *   /<!-- -->* do something with x_spacing and bg_color *<!-- -->/
 *
 *   clutter_color_free (bg_color);
 * </programlisting>
 * </example>
 */
void
nbtk_stylable_get (NbtkStylable *stylable,
                   const gchar  *first_property_name,
                                 ...)
{
  NbtkStyle *style;
  va_list args;

  g_return_if_fail (NBTK_IS_STYLABLE (stylable));
  g_return_if_fail (first_property_name != NULL);

  style = nbtk_stylable_get_style (stylable);

  va_start (args, first_property_name);
  nbtk_style_get_valist (style, stylable, first_property_name, args);
  va_end (args);
}

/**
 * nbtk_stylable_get_default_value:
 * @stylable: a #NbtkStylable
 * @property_name: name of the property to query
 * @value_out: return location for the default value
 *
 * Query @stylable for the default value of property @property_name and
 * fill @value_out with the result.
 *
 * Returns: %TRUE if property @property_name exists and the default value has
 * been returned.
 */
gboolean
nbtk_stylable_get_default_value (NbtkStylable  *stylable,
                                 const gchar   *property_name,
                                 GValue        *value_out)
{
  GParamSpec *pspec;

  pspec = nbtk_stylable_find_property (stylable, property_name);
  if (!pspec)
    {
      g_warning ("%s: no style property named `%s' found for class `%s'",
                  G_STRLOC,
                  property_name,
                  g_type_name (G_OBJECT_TYPE (stylable)));
      return FALSE;
    }

  if (!(pspec->flags & G_PARAM_READABLE))
    {
      g_warning ("Style property `%s' of class `%s' is not readable",
                  pspec->name,
                  g_type_name (G_OBJECT_TYPE (stylable)));
      return FALSE;
    }

  g_value_init (value_out, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_param_value_set_default (pspec, value_out);
  return TRUE;
}

/**
 * nbtk_stylable_get_style:
 * @stylable: a #NbtkStylable
 *
 * Retrieves the #NbtkStyle used by @stylable. This function does not
 * alter the reference count of the returned object.
 *
 * Return value: a #NbtkStyle
 */
NbtkStyle *
nbtk_stylable_get_style (NbtkStylable *stylable)
{
  NbtkStylableIface *iface;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);

  iface = NBTK_STYLABLE_GET_IFACE (stylable);
  if (iface->get_style)
    return iface->get_style (stylable);

  return g_object_get_data (G_OBJECT (stylable), "nbtk-stylable-style");
}

/**
 * nbtk_stylable_set_style:
 * @stylable: a #NbtkStylable
 * @style: a #NbtkStyle
 *
 * Sets @style as the new #NbtkStyle to be used by @stylable.
 *
 * The #NbtkStylable will take ownership of the passed #NbtkStyle.
 *
 * After the #NbtkStle has been set, the NbtkStylable::style-set signal
 * will be emitted.
 */
void
nbtk_stylable_set_style (NbtkStylable *stylable,
                         NbtkStyle    *style)
{
  NbtkStylableIface *iface;
  NbtkStyle *old_style;

  g_return_if_fail (NBTK_IS_STYLABLE (stylable));
  g_return_if_fail (NBTK_IS_STYLE (style));

  iface = NBTK_STYLABLE_GET_IFACE (stylable);

  old_style = nbtk_stylable_get_style (stylable);
  g_object_ref (old_style);

  if (iface->set_style)
    iface->set_style (stylable, style);
  else
    {
      g_object_set_qdata_full (G_OBJECT (stylable),
                               quark_style,
                               g_object_ref_sink (style),
                               g_object_unref);
    }

  g_signal_emit (stylable, stylable_signals[STYLE_CHANGED], 0, old_style);
  g_object_unref (old_style);

  g_object_notify (G_OBJECT (stylable), "style");
}

/**
 * nbtk_stylable_get_container:
 * @stylable: a #NbtkStylable
 *
 * Obtain the parent #NbtkStylable that contains @stylable.
 *
 * Return value: The parent #NbtkStylable
 */
NbtkStylable*
nbtk_stylable_get_container (NbtkStylable *stylable)
{
  NbtkStylableIface *iface;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);

  iface = NBTK_STYLABLE_GET_IFACE (stylable);

  if (iface->get_container)
    return iface->get_container (stylable);
  else
    return NULL;
}

/**
 * nbtk_stylable_get_base_style:
 * @stylable: a #NbtkStylable
 *
 * Get the parent ancestor #NbtkStylable of @stylable.
 *
 * Return value: the parent #NbtkStylable
 */
NbtkStylable*
nbtk_stylable_get_base_style (NbtkStylable *stylable)
{
  NbtkStylableIface *iface;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);

  iface = NBTK_STYLABLE_GET_IFACE (stylable);

  if (iface->get_base_style)
    return iface->get_base_style (stylable);
  else
    return NULL;
}


/**
 * nbtk_stylable_get_style_id:
 * @stylable: a #NbtkStylable
 *
 * Get the ID value of @stylable
 *
 * Return value: the id of @stylable
 */
const gchar*
nbtk_stylable_get_style_id (NbtkStylable *stylable)
{
  NbtkStylableIface *iface;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);

  iface = NBTK_STYLABLE_GET_IFACE (stylable);

  if (iface->get_style_id)
    return iface->get_style_id (stylable);
  else
    return NULL;
}

/**
 * nbtk_stylable_get_style_type:
 * @stylable: a #NbtkStylable
 *
 * Get the type name of @stylable
 *
 * Return value: the type name of @stylable
 */
const gchar*
nbtk_stylable_get_style_type (NbtkStylable *stylable)
{
  NbtkStylableIface *iface;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);

  iface = NBTK_STYLABLE_GET_IFACE (stylable);

  if (iface->get_style_type)
    return iface->get_style_type (stylable);
  else
    return G_OBJECT_TYPE_NAME (stylable);
}

/**
 * nbtk_stylable_get_style_class:
 * @stylable: a #NbtkStylable
 *
 * Get the style class name of @stylable
 *
 * Return value: the type name of @stylable
 */
const gchar*
nbtk_stylable_get_style_class (NbtkStylable *stylable)
{
  NbtkStylableIface *iface;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);

  iface = NBTK_STYLABLE_GET_IFACE (stylable);

  if (iface->get_style_class)
    return iface->get_style_class (stylable);
  else
    return NULL;
}

/**
 * nbtk_stylable_get_pseudo_class:
 * @stylable: a #NbtkStylable
 *
 * Get the pseudo class name of @stylable
 *
 * Return value: the pseudo class name of @stylable
 */
const gchar*
nbtk_stylable_get_pseudo_class (NbtkStylable *stylable)
{
  NbtkStylableIface *iface;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);

  iface = NBTK_STYLABLE_GET_IFACE (stylable);

  if (iface->get_pseudo_class)
    return iface->get_pseudo_class (stylable);
  else
    return NULL;
}

/**
 * nbtk_stylable_get_attribute:
 * @stylable: a #NbtkStylable
 * @name: attribute name
 *
 * Get the named attribute from @stylable
 *
 * Return value: the value of the attribute
 */
gchar*
nbtk_stylable_get_attribute (NbtkStylable *stylable,
                             const gchar  *name)
{
  NbtkStylableIface *iface;
  GValue value = { 0, };
  GValue string_value = { 0, };
  gchar *ret;
  GParamSpec *pspec;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), NULL);

  iface = NBTK_STYLABLE_GET_IFACE (stylable);

  if (iface->get_attribute)
    return iface->get_attribute (stylable, name);

  /* look up a generic gobject property */
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (stylable), name);

  /* if no such property exists, return NULL */
  if (pspec == NULL)
    return NULL;

  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_object_get_property (G_OBJECT (stylable), name, &value);

  g_value_init (&string_value, G_TYPE_STRING);
  if (g_value_transform (&value, &string_value))
    ret = g_strdup (g_value_get_string (&string_value));
  else
      ret = NULL;

  g_value_unset (&value);
  g_value_unset (&string_value);

  return ret;
}

/**
 * nbtk_stylable_get_viewport:
 * @stylable: a #NbtkStylable
 * @x: location to store X coordinate
 * @y: location to store Y coordinate
 * @width: location to store width
 * @height: location to store height
 *
 * Obtain the position and dimensions of @stylable.
 *
 * Return value: true if the function succeeded
 */
gboolean
nbtk_stylable_get_viewport (NbtkStylable *stylable,
                            gint         *x,
                            gint         *y,
                            gint         *width,
                            gint         *height)
{
  NbtkStylableIface *iface;

  g_return_val_if_fail (NBTK_IS_STYLABLE (stylable), FALSE);

  iface = NBTK_STYLABLE_GET_IFACE (stylable);
  if (iface->get_viewport)
    return iface->get_viewport (stylable, x, y, width, height);
  else
    return FALSE;
}


/**
 * nbtk_stylable_changed:
 * @stylable: A #NbtkStylable
 *
 * Emit the "stylable-changed" signal on @stylable
 */
void
nbtk_stylable_changed (NbtkStylable *stylable)
{
  g_signal_emit (stylable, stylable_signals[CHANGED], 0, NULL);
}
