#include "cogl-gtype-private.h"

#include <gobject/gvaluecollector.h>

void
_cogl_gtype_object_init_value (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

void
_cogl_gtype_object_free_value (GValue *value)
{
  if (value->data[0].v_pointer != NULL)
    cogl_object_unref (value->data[0].v_pointer);
}

void
_cogl_gtype_object_copy_value (const GValue *src,
                               GValue       *dst)
{
  if (src->data[0].v_pointer != NULL)
    dst->data[0].v_pointer = cogl_object_ref (src->data[0].v_pointer);
  else
    dst->data[0].v_pointer = NULL;
}

gpointer
_cogl_gtype_object_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

gchar *
_cogl_gtype_object_collect_value (GValue      *value,
                                  guint        n_collect_values,
                                  GTypeCValue *collect_values,
                                  guint        collect_flags)
{
  CoglObject *object;

  object = collect_values[0].v_pointer;

  if (object == NULL)
    {
      value->data[0].v_pointer = NULL;
      return NULL;
    }

  if (object->klass == NULL)
    return g_strconcat ("invalid unclassed CoglObject pointer for "
                        "value type '",
                        G_VALUE_TYPE_NAME (value),
                        "'",
                        NULL);

  value->data[0].v_pointer = cogl_object_ref (object);

  return NULL;
}

gchar *
_cogl_gtype_object_lcopy_value (const GValue *value,
                                guint         n_collect_values,
                                GTypeCValue  *collect_values,
                                guint         collect_flags)
{
  CoglObject **object_p = collect_values[0].v_pointer;

  if (object_p == NULL)
    return g_strconcat ("value location for '",
                        G_VALUE_TYPE_NAME (value),
                        "' passed as NULL",
                        NULL);

  if (value->data[0].v_pointer == NULL)
    *object_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *object_p = value->data[0].v_pointer;
  else
    *object_p = cogl_object_ref (value->data[0].v_pointer);

  return NULL;
}

void
_cogl_gtype_object_class_base_init (CoglObjectClass *klass)
{
}

void
_cogl_gtype_object_class_base_finalize (CoglObjectClass *klass)
{
}

void
_cogl_gtype_object_class_init (CoglObjectClass *klass)
{
}

void
_cogl_gtype_object_init (CoglObject *object)
{
}

void
_cogl_gtype_dummy_iface_init (gpointer iface)
{
}

/**
 * cogl_object_value_set_object:
 * @value: a #GValue initialized with %COGL_GTYPE_TYPE_OBJECT
 * @object: (type Cogl.GtypeObject) (allow-none): a #CoglGtypeObject, or %NULL
 *
 * Sets the contents of a #GValue initialized with %COGL_GTYPE_TYPE_OBJECT.
 *
 */
void
cogl_object_value_set_object (GValue   *value,
                              gpointer  object)
{
  CoglObject *old_object;

  old_object = value->data[0].v_pointer;

  if (object != NULL)
    {
      /* take over ownership */
      value->data[0].v_pointer = object;
    }
  else
    value->data[0].v_pointer = NULL;

  if (old_object != NULL)
    cogl_object_unref (old_object);
}

/**
 * cogl_object_value_get_object:
 * @value: a #GValue initialized with %COGL_GTYPE_TYPE_OBJECT
 *
 * Retrieves a pointer to the #CoglGtypeObject contained inside
 * the passed #GValue.
 *
 * Return value: (transfer none) (type Cogl.GtypeObject): a pointer to
 *   a #CoglGtypeObject, or %NULL
 */
gpointer
cogl_object_value_get_object (const GValue *value)
{
  return value->data[0].v_pointer;
}
