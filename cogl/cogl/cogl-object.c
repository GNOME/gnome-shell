/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <string.h>

#include "cogl-util.h"
#include "cogl-types.h"
#include "cogl-object-private.h"
#include "cogl-gtype-private.h"

COGL_GTYPE_DEFINE_BASE_CLASS (Object, object);

void *
cogl_object_ref (void *object)
{
  CoglObject *obj = object;

  _COGL_RETURN_VAL_IF_FAIL (object != NULL, NULL);

  obj->ref_count++;
  return object;
}

CoglHandle
cogl_handle_ref (CoglHandle handle)
{
  return cogl_object_ref (handle);
}

void
_cogl_object_default_unref (void *object)
{
  CoglObject *obj = object;

  _COGL_RETURN_IF_FAIL (object != NULL);
  _COGL_RETURN_IF_FAIL (obj->ref_count > 0);

  if (--obj->ref_count < 1)
    {
      void (*free_func)(void *obj);

      if (obj->n_user_data_entries)
        {
          int i;
          int count = MIN (obj->n_user_data_entries,
                           COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES);

          for (i = 0; i < count; i++)
            {
              CoglUserDataEntry *entry = &obj->user_data_entry[i];
              if (entry->destroy)
                entry->destroy (entry->user_data, obj);
            }

          if (obj->user_data_array != NULL)
            {
              for (i = 0; i < obj->user_data_array->len; i++)
                {
                  CoglUserDataEntry *entry =
                    &g_array_index (obj->user_data_array,
                                    CoglUserDataEntry, i);

                  if (entry->destroy)
                    entry->destroy (entry->user_data, obj);
                }
              g_array_free (obj->user_data_array, TRUE);
            }
        }

      COGL_OBJECT_DEBUG_FREE (obj);
      free_func = obj->klass->virt_free;
      free_func (obj);
    }
}

void
cogl_object_unref (void *obj)
{
  void (* unref_func) (void *) = ((CoglObject *) obj)->klass->virt_unref;
  unref_func (obj);
}

void
cogl_handle_unref (CoglHandle handle)
{
  cogl_object_unref (handle);
}

GType
cogl_handle_get_type (void)
{
  static GType our_type = 0;

  /* XXX: We are keeping the "CoglHandle" name for now incase it would
   * break bindings to change to "CoglObject" */
  if (G_UNLIKELY (our_type == 0))
    our_type = g_boxed_type_register_static (g_intern_static_string ("CoglHandle"),
                                             (GBoxedCopyFunc) cogl_object_ref,
                                             (GBoxedFreeFunc) cogl_object_unref);

  return our_type;
}

/* XXX: Unlike for cogl_object_get_user_data this code will return
 * an empty entry if available and no entry for the given key can be
 * found. */
static CoglUserDataEntry *
_cogl_object_find_entry (CoglObject *object, CoglUserDataKey *key)
{
  CoglUserDataEntry *entry = NULL;
  int count;
  int i;

  count = MIN (object->n_user_data_entries,
               COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES);

  for (i = 0; i < count; i++)
    {
      CoglUserDataEntry *current = &object->user_data_entry[i];
      if (current->key == key)
        return current;
      if (current->user_data == NULL)
        entry = current;
    }

  if (G_UNLIKELY (object->user_data_array != NULL))
    {
      for (i = 0; i < object->user_data_array->len; i++)
        {
          CoglUserDataEntry *current =
            &g_array_index (object->user_data_array, CoglUserDataEntry, i);

          if (current->key == key)
            return current;
          if (current->user_data == NULL)
            entry = current;
        }
    }

  return entry;
}

void
_cogl_object_set_user_data (CoglObject *object,
                            CoglUserDataKey *key,
                            void *user_data,
                            CoglUserDataDestroyInternalCallback destroy)
{
  CoglUserDataEntry new_entry;
  CoglUserDataEntry *entry;

  if (user_data)
    {
      new_entry.key = key;
      new_entry.user_data = user_data;
      new_entry.destroy = destroy;
    }
  else
    memset (&new_entry, 0, sizeof (new_entry));

  entry = _cogl_object_find_entry (object, key);
  if (entry)
    {
      if (G_LIKELY (entry->destroy))
        entry->destroy (entry->user_data, object);
    }
  else
    {
      /* NB: Setting a value of NULL is documented to delete the
       * corresponding entry so we can return immediately in this
       * case. */
      if (user_data == NULL)
        return;

      if (G_LIKELY (object->n_user_data_entries <
                    COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES))
        entry = &object->user_data_entry[object->n_user_data_entries++];
      else
        {
          if (G_UNLIKELY (object->user_data_array == NULL))
            {
              object->user_data_array =
                g_array_new (FALSE, FALSE, sizeof (CoglUserDataEntry));
            }

          g_array_set_size (object->user_data_array,
                            object->user_data_array->len + 1);
          entry =
            &g_array_index (object->user_data_array, CoglUserDataEntry,
                            object->user_data_array->len - 1);

          object->n_user_data_entries++;
        }
    }

  *entry = new_entry;
}

void
cogl_object_set_user_data (CoglObject *object,
                           CoglUserDataKey *key,
                           void *user_data,
                           CoglUserDataDestroyCallback destroy)
{
  _cogl_object_set_user_data (object, key, user_data,
                              (CoglUserDataDestroyInternalCallback)destroy);
}

void *
cogl_object_get_user_data (CoglObject *object, CoglUserDataKey *key)
{
  int count;
  int i;

  count = MIN (object->n_user_data_entries,
               COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES);

  for (i = 0; i < count; i++)
    {
      CoglUserDataEntry *entry = &object->user_data_entry[i];
      if (entry->key == key)
        return entry->user_data;
    }

  if (object->user_data_array != NULL)
    {
      for (i = 0; i < object->user_data_array->len; i++)
        {
          CoglUserDataEntry *entry =
            &g_array_index (object->user_data_array, CoglUserDataEntry, i);

          if (entry->key == key)
            return entry->user_data;
        }
    }

  return NULL;
}

void
cogl_debug_object_foreach_type (CoglDebugObjectForeachTypeCallback func,
                                void *user_data)
{
  GHashTableIter iter;
  unsigned long *instance_count;
  CoglDebugObjectTypeInfo info;

  g_hash_table_iter_init (&iter, _cogl_debug_instances);
  while (g_hash_table_iter_next (&iter,
                                 (void *) &info.name,
                                 (void *) &instance_count))
    {
      info.instance_count = *instance_count;
      func (&info, user_data);
    }
}

static void
print_instances_cb (const CoglDebugObjectTypeInfo *info,
                    void *user_data)
{
  g_print ("\t%s: %lu\n", info->name, info->instance_count);
}

void
cogl_debug_object_print_instances (void)
{
  g_print ("Cogl instances:\n");

  cogl_debug_object_foreach_type (print_instances_cb, NULL);
}
