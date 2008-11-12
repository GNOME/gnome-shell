/* tidy-stylable.c: Interface for stylable objects
 */

/**
 * SECTION:tidy-stylable
 * @short_description: Interface for stylable objects
 *
 * Stylable objects are classes that can have "style properties", that is
 * properties that can be changed by attaching a #TidyStyle to them.
 *
 * Objects can choose to subclass #TidyActor, and thus inherit all the
 * #TidyActor style properties; or they can subclass #TidyActor and
 * reimplement the #TidyStylable interface to add new style properties
 * specific for them (and their subclasses); or, finally, they can simply
 * subclass #GObject and implement #TidyStylable to install new properties.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib-object.h>
#include <gobject/gvaluecollector.h>
#include <gobject/gobjectnotifyqueue.c>

#include "tidy-marshal.h"
#include "tidy-private.h"
#include "tidy-stylable.h"

enum
{
  STYLE_SET,
  STYLE_NOTIFY,

  LAST_SIGNAL
};

static GObjectNotifyContext property_notify_context = { 0, };

static GParamSpecPool *style_property_spec_pool = NULL;

static GQuark          quark_real_owner         = 0;
static GQuark          quark_style              = 0;

static guint stylable_signals[LAST_SIGNAL] = { 0, };

static void
tidy_stylable_notify_dispatcher (GObject     *gobject,
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
tidy_stylable_base_finalize (gpointer g_iface)
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
tidy_stylable_base_init (gpointer g_iface)
{
  static gboolean initialised = FALSE;

  if (G_UNLIKELY (!initialised))
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_iface);

      initialised = TRUE;

      quark_real_owner = g_quark_from_static_string ("tidy-stylable-real-owner-quark");
      quark_style = g_quark_from_static_string ("tidy-stylable-style-quark");

      style_property_spec_pool = g_param_spec_pool_new (FALSE);

      property_notify_context.quark_notify_queue = g_quark_from_static_string ("TidyStylable-style-property-notify-queue");
      property_notify_context.dispatcher = tidy_stylable_notify_dispatcher;

      /**
       * TidyStylable:style:
       *
       * The #TidyStyle attached to a stylable object.
       */
      g_object_interface_install_property (g_iface,
                                           g_param_spec_object ("style",
                                                                "Style",
                                                                "A style object",
                                                                TIDY_TYPE_STYLE,
                                                                TIDY_PARAM_READWRITE));

      /**
       * TidyStylable::style-set:
       * @stylable: the #TidyStylable that received the signal
       * @old_style: the previously set #TidyStyle for @stylable
       *
       * The ::style-set signal is emitted each time the #TidyStyle attached
       * to @stylable has been changed.
       */
      stylable_signals[STYLE_SET] =
        g_signal_new (I_("style-set"),
                      iface_type,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (TidyStylableIface, style_set),
                      NULL, NULL,
                      _tidy_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      TIDY_TYPE_STYLE);
      stylable_signals[STYLE_NOTIFY] =
        g_signal_new (I_("style-notify"),
                      iface_type,
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (TidyStylableIface, style_notify),
                      NULL, NULL,
                      _tidy_marshal_VOID__PARAM,
                      G_TYPE_NONE, 1,
                      G_TYPE_PARAM);
    }
}

GType
tidy_stylable_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    {
      GTypeInfo stylable_info = {
        sizeof (TidyStylableIface),
        tidy_stylable_base_init,
        tidy_stylable_base_finalize
      };

      our_type = g_type_register_static (G_TYPE_INTERFACE,
                                         I_("TidyStylable"),
                                         &stylable_info, 0);
    }

  return our_type;
}

void
tidy_stylable_freeze_notify (TidyStylable *stylable)
{
  g_return_if_fail (TIDY_IS_STYLABLE (stylable));

  g_object_ref (stylable);
  g_object_notify_queue_freeze (G_OBJECT (stylable), &property_notify_context);
  g_object_unref (stylable);
}

void
tidy_stylable_thaw_notify (TidyStylable *stylable)
{
  GObjectNotifyQueue *nqueue;
  
  g_return_if_fail (TIDY_IS_STYLABLE (stylable));
  
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
tidy_stylable_notify (TidyStylable *stylable,
                      const gchar  *property_name)
{
  GParamSpec *pspec;
    
  g_return_if_fail (TIDY_IS_STYLABLE (stylable));
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
 * tidy_stylable_iface_install_property:
 * @iface: a #TidyStylableIface
 * @owner_type: #GType of the style property owner
 * @pspec: a #GParamSpec
 *
 * Installs a property for @owner_type using @pspec as the property
 * description.
 *
 * This function should be used inside the #TidyStylableIface initialization
 * function of a class, for instance:
 *
 * <informalexample><programlisting>
 * G_DEFINE_TYPE_WITH_CODE (FooActor, foo_actor, CLUTTER_TYPE_ACTOR,
 *                          G_IMPLEMENT_INTERFACE (TIDY_TYPE_STYLABLE,
 *                                                 tidy_stylable_init));
 * ...
 * static void
 * tidy_stylable_init (TidyStylableIface *iface)
 * {
 *   static gboolean is_initialized = FALSE;
 *
 *   if (!is_initialized)
 *     {
 *       ...
 *       tidy_stylable_iface_install_property (stylable,
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
tidy_stylable_iface_install_property (TidyStylableIface *iface,
                                      GType              owner_type,
                                      GParamSpec        *pspec)
{
  g_return_if_fail (TIDY_IS_STYLABLE_IFACE (iface));
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
 * tidy_stylable_list_properties:
 * @stylable: a #TidyStylable
 * @n_props: return location for the number of properties, or %NULL
 *
 * Retrieves all the #GParamSpec<!-- -->s installed by @stylable.
 *
 * Return value: an array of #GParamSpec<!-- -->s. Free it with
 *   g_free() when done.
 */
GParamSpec **
tidy_stylable_list_properties (TidyStylable *stylable,
                               guint        *n_props)
{
  GParamSpec **pspecs = NULL;
  guint n;

  g_return_val_if_fail (TIDY_IS_STYLABLE (stylable), NULL);

  pspecs = g_param_spec_pool_list (style_property_spec_pool,
                                   G_OBJECT_TYPE (stylable),
                                   &n);
  if (n_props)
    *n_props = n;

  return pspecs;
}

/**
 * tidy_stylable_find_property:
 * @stylable: a #TidyStylable
 * @property_name: the name of the property to find
 *
 * Finds the #GParamSpec installed by @stylable for the property
 * with @property_name.
 *
 * Return value: a #GParamSpec for the given property, or %NULL if
 *   no property with that name was found
 */
GParamSpec *
tidy_stylable_find_property (TidyStylable *stylable,
                             const gchar  *property_name)
{
  g_return_val_if_fail (TIDY_IS_STYLABLE (stylable), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (style_property_spec_pool,
                                   property_name,
                                   G_OBJECT_TYPE (stylable),
                                   TRUE);
}

static inline void
tidy_stylable_set_property_internal (TidyStylable       *stylable,
                                     GParamSpec         *pspec,
                                     const GValue       *value,
                                     GObjectNotifyQueue *nqueue)
{
  GValue tmp_value = { 0, };

  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  if (!g_value_transform (value, &tmp_value))
    g_warning ("unable to set property `%s' of type `%s' from value of type `%s'",
               pspec->name,
               g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
               G_VALUE_TYPE_NAME (value));
  else if (g_param_value_validate (pspec, &tmp_value) &&
           !(pspec->flags & G_PARAM_LAX_VALIDATION))
    {
      gchar *contents = g_strdup_value_contents (value);

      g_warning ("value \"%s\" of type `%s' is invalid or out of range for property `%s' of type `%s'",
                 contents,
                 G_VALUE_TYPE_NAME (value),
                 pspec->name,
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      g_free (contents);
    }
  else
    {
      TidyStyle *style = tidy_stylable_get_style (stylable);
      gchar *real_name;

      real_name = g_strconcat (g_param_spec_get_qdata (pspec, quark_real_owner),
                               "::",
                               pspec->name,
                               NULL);

      if (!tidy_style_has_property (style, real_name))
        tidy_style_add_property (style, real_name,
                                 G_PARAM_SPEC_VALUE_TYPE (pspec));

      tidy_style_set_property (style, real_name, &tmp_value);
      g_object_notify_queue_add (G_OBJECT (stylable), nqueue, pspec);

      g_free (real_name);
    }

  g_value_unset (&tmp_value);
}

static inline void
tidy_stylable_get_property_internal (TidyStylable *stylable,
                                     GParamSpec   *pspec,
                                     GValue       *value)
{
  TidyStyle *style;
  GValue real_value = { 0, };
  gchar *real_name;

  real_name = g_strconcat (g_param_spec_get_qdata (pspec, quark_real_owner),
                           "::",
                           pspec->name,
                           NULL);

  style = tidy_stylable_get_style (stylable);
  if (!tidy_style_has_property (style, real_name))
    {
      /* the style has no property set, use the default value
       * from the GParamSpec
       */
      g_param_value_set_default (pspec, value);
      g_free (real_name);
      return;
    }

  tidy_style_get_property (style, real_name, &real_value);

  g_value_copy (&real_value, value);
  g_value_unset (&real_value);

  g_free (real_name);
}

/**
 * tidy_stylable_get_property:
 * @stylable: a #TidyStylable
 * @property_name: the name of the property
 * @value: return location for an empty #GValue
 *
 * Retrieves the value of @property_name for @stylable, and puts it
 * into @value.
 */
void
tidy_stylable_get_property (TidyStylable *stylable,
                            const gchar  *property_name,
                            GValue       *value)
{
  GParamSpec *pspec;

  g_return_if_fail (TIDY_IS_STYLABLE (stylable));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  pspec = tidy_stylable_find_property (stylable, property_name);
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

  tidy_stylable_get_property_internal (stylable, pspec, value);
}

/**
 * tidy_stylable_set_property:
 * @stylable: a #TidyStylable
 * @property_name: the name of the property to set
 * @value: an initialized #GValue
 *
 * Sets the property @property_name with @value.
 */
void
tidy_stylable_set_property (TidyStylable *stylable,
                            const gchar  *property_name,
                            const GValue *value)
{
  GObjectNotifyQueue *nqueue;
  GParamSpec *pspec;

  g_return_if_fail (TIDY_IS_STYLABLE (stylable));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  g_object_ref (stylable);

  nqueue = g_object_notify_queue_freeze (G_OBJECT (stylable),
                                         &property_notify_context);

  pspec = tidy_stylable_find_property (stylable, property_name);
  if (!pspec)
    {
      g_warning ("Stylable class `%s' doesn't have a property named `%s'",
                 g_type_name (G_OBJECT_TYPE (stylable)),
                 property_name);
    }
  else if (!(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("Style property `%s' of class `%s' is not readable",
                 pspec->name,
                 g_type_name (G_OBJECT_TYPE (stylable)));
    }
  else if (G_VALUE_TYPE (value) != G_PARAM_SPEC_VALUE_TYPE (pspec))
    {
      g_warning ("Passed value is not of the requested type `%s' for "
                 "the style property `%s' of class `%s'",
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
                 pspec->name,
                 g_type_name (G_OBJECT_TYPE (stylable)));
    }
  else
    tidy_stylable_set_property_internal (stylable, pspec, value, nqueue);

  g_object_notify_queue_thaw (G_OBJECT (stylable), nqueue);
  g_object_unref (stylable);
}

static void
tidy_stylable_get_valist (TidyStylable *stylable,
                          const gchar  *first_property_name,
                          va_list       varargs)
{
  const gchar *name;

  g_object_ref (stylable);

  name = first_property_name;

  while (name)
    {
      GParamSpec *pspec;
      GValue value = { 0, };
      gchar *error;

      pspec = tidy_stylable_find_property (stylable, name);
      if (!pspec)
        {
          g_warning ("%s: no style property named `%s' found for class `%s'",
                     G_STRLOC,
                     name,
                     g_type_name (G_OBJECT_TYPE (stylable)));
          break;
        }

      if (!(pspec->flags & G_PARAM_READABLE))
        {
          g_warning ("Style property `%s' of class `%s' is not readable",
                     pspec->name,
                     g_type_name (G_OBJECT_TYPE (stylable)));
          break;
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      tidy_stylable_get_property_internal (stylable, pspec, &value);

      G_VALUE_LCOPY (&value, varargs, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          g_value_unset (&value);
          break;
        }

      g_value_unset (&value);

      name = va_arg (varargs, gchar*);
    }

  g_object_unref (stylable);
}

static void
tidy_stylable_set_valist (TidyStylable *stylable,
                          const gchar  *first_property_name,
                          va_list       varargs)
{
  GObjectNotifyQueue *nqueue;
  const gchar *name;

  g_object_ref (stylable);

  nqueue = g_object_notify_queue_freeze (G_OBJECT (stylable),
                                         &property_notify_context);

  name = first_property_name;

  while (name)
    {
      GParamSpec *pspec;
      GValue value = { 0, };
      gchar *error;

      pspec = tidy_stylable_find_property (stylable, name);
      if (!pspec)
        {
          g_warning ("%s: no style property named `%s' found for class `%s'",
                     G_STRLOC,
                     name,
                     g_type_name (G_OBJECT_TYPE (stylable)));
          break;
        }

      if (!(pspec->flags & G_PARAM_WRITABLE) ||
          (pspec->flags & G_PARAM_CONSTRUCT_ONLY))
        {
          g_warning ("Style property `%s' of class `%s' is not writable",
                     pspec->name,
                     g_type_name (G_OBJECT_TYPE (stylable)));
          break;
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));

      G_VALUE_COLLECT (&value, varargs, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          g_value_unset (&value);
          break;
        }

      tidy_stylable_set_property_internal (stylable, pspec, &value, nqueue);
      g_value_unset (&value);

      name = va_arg (varargs, gchar*);
    }

  g_object_notify_queue_thaw (G_OBJECT (stylable), nqueue);
  g_object_unref (stylable);
}

/**
 * tidy_stylable_get:
 * @stylable: a #TidyStylable
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
 * <title>Using tidy_stylable_get(<!-- -->)</title>
 * <para>An example of using tidy_stylable_get() to get the contents of
 * two style properties - one of type #G_TYPE_INT and one of type
 * #CLUTTER_TYPE_COLOR:</para>
 * <programlisting>
 *   gint x_spacing;
 *   ClutterColor *bg_color;
 *
 *   tidy_stylable_get (stylable,
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
tidy_stylable_get (TidyStylable *stylable,
                   const gchar  *first_property_name,
                                 ...)
{
  va_list args;

  g_return_if_fail (TIDY_IS_STYLABLE (stylable));
  g_return_if_fail (first_property_name != NULL);

  va_start (args, first_property_name);
  tidy_stylable_get_valist (stylable, first_property_name, args);
  va_end (args);
}

/**
 * tidy_stylable_set:
 * @stylable: a #TidyStylable
 * @first_property_name: name of the first property to set
 * @Varargs: value for the first property, followed optionally by
 *   more name/value pairs, followed by %NULL
 *
 * Sets the style properties of @stylable.
 */
void
tidy_stylable_set (TidyStylable *stylable,
                   const gchar  *first_property_name,
                   ...)
{
  va_list args;

  g_return_if_fail (TIDY_IS_STYLABLE (stylable));
  g_return_if_fail (first_property_name != NULL);

  va_start (args, first_property_name);
  tidy_stylable_set_valist (stylable, first_property_name, args);
  va_end (args);
}

/**
 * tidy_stylable_get_style:
 * @stylable: a #TidyStylable
 *
 * Retrieves the #TidyStyle used by @stylable. This function does not
 * alter the reference count of the returned object.
 *
 * Return value: a #TidyStyle
 */
TidyStyle *
tidy_stylable_get_style (TidyStylable *stylable)
{
  TidyStylableIface *iface;

  g_return_val_if_fail (TIDY_IS_STYLABLE (stylable), NULL);

  iface = TIDY_STYLABLE_GET_IFACE (stylable);
  if (iface->get_style)
    return iface->get_style (stylable);

  return g_object_get_data (G_OBJECT (stylable), "tidy-stylable-style");
}

/**
 * tidy_stylable_set_style:
 * @stylable: a #TidyStylable
 * @style: a #TidyStyle
 *
 * Sets @style as the new #TidyStyle to be used by @stylable.
 *
 * The #TidyStylable will take ownership of the passed #TidyStyle.
 *
 * After the #TidyStle has been set, the TidyStylable::style-set signal
 * will be emitted.
 */
void
tidy_stylable_set_style (TidyStylable *stylable,
                         TidyStyle    *style)
{
  TidyStylableIface *iface;
  TidyStyle *old_style;

  g_return_if_fail (TIDY_IS_STYLABLE (stylable));
  g_return_if_fail (TIDY_IS_STYLE (style));

  iface = TIDY_STYLABLE_GET_IFACE (stylable);

  old_style = tidy_stylable_get_style (stylable);
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

  g_signal_emit (stylable, stylable_signals[STYLE_SET], 0, old_style);
  g_object_unref (old_style);

  g_object_notify (G_OBJECT (stylable), "style");
}
