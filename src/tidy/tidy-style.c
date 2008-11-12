#ifndef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include <clutter/clutter-behaviour.h>
#include <clutter/clutter-color.h>

#include "tidy-style.h"
#include "tidy-marshal.h"
#include "tidy-debug.h"

enum
{
  CHANGED,

  LAST_SIGNAL
};

#define TIDY_STYLE_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_STYLE, TidyStylePrivate))

typedef struct {
  GType value_type;
  gchar *value_name;
  GValue value;
} StyleProperty;

typedef struct {
  gchar *name;
  GType behaviour_type;
  GArray *parameters;
  guint duration;
  ClutterAlphaFunc alpha_func;
} StyleEffect;

struct _TidyStylePrivate
{
  GHashTable *properties;
  GHashTable *effects;
};

static guint style_signals[LAST_SIGNAL] = { 0, };

static const gchar *tidy_default_font_name          = "Sans 12px";
static const ClutterColor tidy_default_text_color   = { 0x00, 0x00, 0x00, 0xff };
static const ClutterColor tidy_default_bg_color     = { 0xcc, 0xcc, 0xcc, 0xff };
static const ClutterColor tidy_default_active_color = { 0xf5, 0x79, 0x00, 0xff };

static TidyStyle *default_style = NULL;

G_DEFINE_TYPE (TidyStyle, tidy_style, G_TYPE_OBJECT);

static StyleProperty *
style_property_new (const gchar *value_name,
                    GType        value_type)
{
  StyleProperty *retval;

  retval = g_slice_new0 (StyleProperty);
  retval->value_type = value_type;
  retval->value_name = g_strdup (value_name);
  g_value_init (&retval->value, value_type);

  return retval;
}

static void
style_property_free (gpointer data)
{
  if (G_LIKELY (data))
    {
      StyleProperty *sp = data;

      g_free (sp->value_name);
      g_value_unset (&sp->value);
    }
}

static StyleEffect *
style_effect_new (const gchar *name)
{
  StyleEffect *retval;

  retval = g_slice_new0 (StyleEffect);
  retval->name = g_strdup (name);
  retval->behaviour_type = G_TYPE_INVALID;

  return retval;
}

static void
style_effect_free (gpointer data)
{
  if (G_LIKELY (data))
    {
      StyleEffect *effect = data;

      g_free (effect->name);

      if (effect->parameters)
        {
          gint i;

          for (i = 0; i < effect->parameters->len; i++)
            {
              GParameter *param;

              param = &g_array_index (effect->parameters, GParameter, i);

              g_free ((gchar *) param->name);
              g_value_unset (&param->value);
            }

          g_array_free (effect->parameters, TRUE);
          effect->parameters = NULL;
        }

      g_slice_free (StyleEffect, effect);
    }
}

static void
init_defaults (TidyStyle *style)
{
  TidyStylePrivate *priv = style->priv;
  
  {
    StyleProperty *sp;

    sp = style_property_new (TIDY_FONT_NAME, G_TYPE_STRING);
    g_value_set_string (&sp->value, tidy_default_font_name);

    g_hash_table_insert (priv->properties, sp->value_name, sp);
  }

  {
    StyleProperty *sp;

    sp = style_property_new (TIDY_BACKGROUND_COLOR, CLUTTER_TYPE_COLOR);
    g_value_set_boxed (&sp->value, &tidy_default_bg_color);

    g_hash_table_insert (priv->properties, sp->value_name, sp);
  }

  {
    StyleProperty *sp;

    sp = style_property_new (TIDY_ACTIVE_COLOR, CLUTTER_TYPE_COLOR);
    g_value_set_boxed (&sp->value, &tidy_default_active_color);

    g_hash_table_insert (priv->properties, sp->value_name, sp);
  }

  {
    StyleProperty *sp;

    sp = style_property_new (TIDY_TEXT_COLOR, CLUTTER_TYPE_COLOR);
    g_value_set_boxed (&sp->value, &tidy_default_text_color);

    g_hash_table_insert (priv->properties, sp->value_name, sp);
  }
}

static gboolean
tidy_style_load_from_file (TidyStyle    *style,
                           const gchar  *filename,
                           GError      **error)
{
  GKeyFile *rc_file;
  GError *internal_error;

  rc_file = g_key_file_new ();
  
  internal_error = NULL;
  g_key_file_load_from_file (rc_file, filename, 0, &internal_error);
  if (internal_error)
    {
      g_key_file_free (rc_file);
      /* if the specified files does not exist then just ignore it
       * and fall back to the default values; if, instead, the file
       * is not accessible or is malformed, propagate the error
       */
      if (internal_error->domain == G_FILE_ERROR &&
          internal_error->code == G_FILE_ERROR_NOENT)
        {
          g_error_free (internal_error);
          return TRUE;
        }

      g_propagate_error (error, internal_error);
      return FALSE;
    }

  g_key_file_free (rc_file);

  return TRUE;
}

static void
tidy_style_load (TidyStyle *style)
{
  const gchar *env_var;
  gchar *rc_file = NULL;
  GError *error;
  
  init_defaults (style);

  env_var = g_getenv ("TIDY_RC_FILE");
  if (env_var && *env_var)
    rc_file = g_strdup (env_var);
  
  if (!rc_file)
    rc_file = g_build_filename (g_get_user_config_dir (),
                                "tidy",
                                "tidyrc",
                                NULL);

  error = NULL;
  if (!tidy_style_load_from_file (style, rc_file, &error))
    {
      g_critical ("Unable to load resource file `%s': %s",
                  rc_file,
                  error->message);
      g_error_free (error);
    }
  
  g_free (rc_file);
}

static void
tidy_style_finalize (GObject *gobject)
{
  TidyStylePrivate *priv = TIDY_STYLE (gobject)->priv;

  g_hash_table_destroy (priv->properties);
  g_hash_table_destroy (priv->effects);

  G_OBJECT_CLASS (tidy_style_parent_class)->finalize (gobject);
}

static void
tidy_style_class_init (TidyStyleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TidyStylePrivate));

  gobject_class->finalize = tidy_style_finalize;

  style_signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TidyStyleClass, changed),
                  NULL, NULL,
                  _tidy_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
tidy_style_init (TidyStyle *style)
{
  TidyStylePrivate *priv;

  style->priv = priv = TIDY_STYLE_GET_PRIVATE (style);

  priv->properties = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            NULL,
                                            style_property_free);
  priv->effects = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         NULL,
                                         style_effect_free);

  tidy_style_load (style);
}

/* need to unref */
TidyStyle *
tidy_style_new (void)
{
  return g_object_new (TIDY_TYPE_STYLE, NULL);
}

/* never ref/unref */
TidyStyle *
tidy_style_get_default (void)
{
  if (G_LIKELY (default_style))
    return default_style;

  default_style = g_object_new (TIDY_TYPE_STYLE, NULL);
  
  return default_style;
}

static StyleProperty *
tidy_style_find_property (TidyStyle   *style,
                          const gchar *property_name)
{
  return g_hash_table_lookup (style->priv->properties, property_name);
}

gboolean
tidy_style_has_property (TidyStyle   *style,
                         const gchar *property_name)
{
  g_return_val_if_fail (TIDY_IS_STYLE (style), FALSE);
  g_return_val_if_fail (property_name != NULL, FALSE);

  return (tidy_style_find_property (style, property_name) != NULL);
}

void
tidy_style_add_property (TidyStyle   *style,
                         const gchar *property_name,
                         GType        property_type)
{
  StyleProperty *property;

  g_return_if_fail (TIDY_IS_STYLE (style));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (property_type != G_TYPE_INVALID);

  property = tidy_style_find_property (style, property_name);
  if (G_UNLIKELY (property))
    {
      g_warning ("A property named `%s', with type %s already exists.",
                 property->value_name,
                 g_type_name (property->value_type));
      return;
    }

  property = style_property_new (property_name, property_type);
  g_hash_table_insert (style->priv->properties, property->value_name, property);

  g_signal_emit (style, style_signals[CHANGED], 0);
}

void
tidy_style_get_property (TidyStyle   *style,
                         const gchar *property_name,
                         GValue      *value)
{
  StyleProperty *property;

  g_return_if_fail (TIDY_IS_STYLE (style));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  property = tidy_style_find_property (style, property_name);
  if (!property)
    {
      g_warning ("No style property named `%s' found.", property_name);
      return;
    }

  g_value_init (value, property->value_type);
  g_value_copy (&property->value, value);
}

void
tidy_style_set_property (TidyStyle    *style,
                         const gchar  *property_name,
                         const GValue *value)
{
  StyleProperty *property;

  g_return_if_fail (TIDY_IS_STYLE (style));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  property = tidy_style_find_property (style, property_name);
  if (!property)
    {
      g_warning ("No style property named `%s' found.", property_name);
      return;
    }

  g_value_copy (value, &property->value);

  g_signal_emit (style, style_signals[CHANGED], 0);
}

static StyleEffect *
tidy_style_find_effect (TidyStyle   *style,
                        const gchar *effect_name)
{
  return g_hash_table_lookup (style->priv->effects, effect_name);
}

void
tidy_style_add_effect (TidyStyle   *style,
                       const gchar *effect_name)
{
  StyleEffect *effect;

  effect = tidy_style_find_effect (style, effect_name);
  if (G_UNLIKELY (effect))
    {
      g_warning ("An effect named `%s', with type %s already exists.",
                 effect->name,
                 g_type_name (effect->behaviour_type));
      return;
    }

  effect = style_effect_new (effect_name);
  g_hash_table_replace (style->priv->effects,
                        effect->name,
                        effect);
}

gboolean
tidy_style_has_effect (TidyStyle   *style,
                       const gchar *effect_name)
{
  g_return_val_if_fail (TIDY_IS_STYLE (style), FALSE);
  g_return_val_if_fail (effect_name != NULL, FALSE);

  return (tidy_style_find_effect (style, effect_name) != NULL);
}

static void
tidy_style_set_effect_valist (TidyStyle   *style,
                              StyleEffect *effect,
                              const gchar *first_property_name,
                              va_list      varargs)
{
  GObjectClass *klass;
  const gchar *name;

  klass = g_type_class_ref (effect->behaviour_type);
  if (G_UNLIKELY (!klass))
    return;

  name = first_property_name;
  while (name)
    {
      GParamSpec *pspec;
      GParameter param = { 0, };
      GValue value = { 0, };
      gchar *error = NULL;

      pspec = g_object_class_find_property (klass, name);
      if (!pspec)
        {
          g_warning ("Unable to find the property `%s' for the "
                     "behaviour of type `%s'",
                     name,
                     g_type_name (effect->behaviour_type));
          break;
        }

      if (!(pspec->flags & G_PARAM_WRITABLE))
        {
          g_warning ("The property `%s' for the behaviour of type "
                     "`%s' is not writable",
                     pspec->name,
                     g_type_name (effect->behaviour_type));
          break;
        }

      param.name = g_strdup (pspec->name);

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));

      G_VALUE_COLLECT (&value, varargs, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          g_value_unset (&value);
          break;
        }

      g_value_init (&(param.value), G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_value_copy (&value, &(param.value));
      g_value_unset (&value);

      name = va_arg (varargs, gchar*);
    }

  g_type_class_unref (klass);
}

void
tidy_style_set_effect  (TidyStyle        *style,
                        const gchar      *effect_name,
                        guint             duration,
                        GType             behaviour_type,
                        ClutterAlphaFunc  alpha_func,
                        const gchar      *first_property_name,
                        ...)
{
  StyleEffect *effect;
  va_list args;

  effect = tidy_style_find_effect (style, effect_name);
  if (!effect)
    {
      g_warning ("No effect named `%s' found.", effect_name);
      return;
    }

  if (effect->parameters)
    {
      gint i;

      for (i = 0; i < effect->parameters->len; i++)
        {
          GParameter *param;

          param = &g_array_index (effect->parameters, GParameter, i);

          g_free ((gchar *) param->name);
          g_value_unset (&param->value);
        }

      g_array_free (effect->parameters, TRUE);
      effect->parameters = NULL;
    }

  effect->duration = duration;
  effect->behaviour_type = behaviour_type;
  effect->alpha_func = alpha_func;
  effect->parameters = g_array_new (FALSE, FALSE, sizeof (GParameter));

  va_start (args, first_property_name);
  tidy_style_set_effect_valist (style, effect, first_property_name, args);
  va_end (args);
}

void
tidy_style_set_effectv (TidyStyle        *style,
                        const gchar      *effect_name,
                        guint             duration,
                        GType             behaviour_type,
                        ClutterAlphaFunc  alpha_func,
                        guint             n_parameters,
                        GParameter       *parameters)
{
  StyleEffect *effect;
  gint i;

  effect = tidy_style_find_effect (style, effect_name);
  if (!effect)
    {
      g_warning ("No effect named `%s' found.", effect_name);
      return;
    }

  if (effect->parameters)
    {
      gint i;

      for (i = 0; i < effect->parameters->len; i++)
        {
          GParameter *param;

          param = &g_array_index (effect->parameters, GParameter, i);

          g_free ((gchar *) param->name);
          g_value_unset (&param->value);
        }

      g_array_free (effect->parameters, TRUE);
      effect->parameters = NULL;
    }

  effect->duration = duration;
  effect->behaviour_type = behaviour_type;
  effect->alpha_func = alpha_func;
  effect->parameters = g_array_new (FALSE, FALSE, sizeof (GParameter));

  for (i = 0; i < n_parameters; i++)
    {
      GParameter param = { NULL, };

      param.name = g_strdup (parameters[i].name);

      g_value_init (&param.value, G_VALUE_TYPE (&parameters[i].value));
      g_value_copy (&parameters[i].value, &param.value);

      g_array_append_val (effect->parameters, param);
    }
}

static ClutterBehaviour *
tidy_style_construct_effect (TidyStyle   *style,
                             const gchar *effect_name)
{
  ClutterTimeline *timeline;
  ClutterAlpha *alpha;
  ClutterBehaviour *behaviour;
  StyleEffect *effect;

  effect = tidy_style_find_effect (style, effect_name);
  if (!effect)
    {
      g_warning ("No effect named `%s' found.", effect_name);
      return NULL;
    }

  timeline = clutter_timeline_new_for_duration (effect->duration);

  alpha = clutter_alpha_new_full (timeline, effect->alpha_func, NULL, NULL);
  g_object_unref (timeline);

  behaviour = g_object_newv (effect->behaviour_type,
                             effect->parameters->len,
                             (GParameter *) effect->parameters->data);

  clutter_behaviour_set_alpha (behaviour, alpha);

  /* we just unref the behaviour, which will take care of cleaning
   * up everything (alpha+timeline)
   */
  g_signal_connect_swapped (timeline,
                            "completed", G_CALLBACK (g_object_unref),
                            behaviour);

  return behaviour;
}

ClutterTimeline *
tidy_style_get_effect (TidyStyle    *style,
                       const gchar  *effect_name,
                       ClutterActor *actor)
{
  ClutterBehaviour *behaviour;
  ClutterAlpha *alpha;
  ClutterTimeline *timeline;

  g_return_val_if_fail (TIDY_IS_STYLE (style), NULL);
  g_return_val_if_fail (effect_name != NULL, NULL);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  behaviour = tidy_style_construct_effect (style, effect_name);
  if (!behaviour)
    return NULL;

  clutter_behaviour_apply (behaviour, actor);

  alpha = clutter_behaviour_get_alpha (behaviour);
  timeline = clutter_alpha_get_timeline (alpha);

  return timeline;
}
