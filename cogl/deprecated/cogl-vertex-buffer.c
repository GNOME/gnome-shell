/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

/* XXX: For an overview of the functionality implemented here, please
 * see cogl-vertex-buffer.h, which contains the gtk-doc section overview
 * for the Vertex Buffers API.
 */

/*
 * TODO: We need to do a better job of minimizing when we call glVertexPointer
 * and pals in enable_state_for_drawing_buffer
 *
 * We should have an internal 2-tuple cache of (VBO, offset) for each of them
 * so we can avoid some GL calls. We could have cogl wrappers for the
 * gl*Pointer funcs that look like this:
 *
 * cogl_vertex_pointer (n_components, gl_type, stride, vbo, offset);
 * cogl_color_pointer (n_components, gl_type, stride, vbo, offset);
 *
 * They would also accept NULL for the VBO handle to support old style vertex
 * arrays.
 *
 * TODO:
 * Actually hook this up to the cogl shaders infrastructure. The vertex
 * buffer API has been designed to allow adding of arbitrary attributes for use
 * with shaders, but this has yet to be actually plumbed together and tested.
 * The bits we are missing:
 * - cogl_program_use doesn't currently record within ctx-> which program
 *   is currently in use so a.t.m only Clutter knows the current shader.
 * - We don't query the current shader program for the generic vertex indices
 *   (using glGetAttribLocation) so that we can call glEnableVertexAttribArray
 *   with those indices.
 *   (currently we just make up consecutive indices)
 * - some dirty flag mechanims to know when the shader program has changed
 *   so we don't need to re-query it each time we draw a buffer.
 *
 * TODO
 * Expose API that lets developers get back a buffer handle for a particular
 * polygon so they may add custom attributes to them.
 * - It should be possible to query/modify attributes efficiently, in place,
 *   avoiding copies. It would not be acceptable to simply require that
 *   developers must query back the n_vertices of a buffer and then the
 *   n_components, type and stride etc of each attribute since there
 *   would be too many combinations to realistically handle.
 *
 * - In practice, some cases might be best solved with a higher level
 *   EditableMesh API, (see futher below) but for many cases I think an
 *   API like this might be appropriate:
 *
 * cogl_vertex_buffer_foreach_vertex (buffer_handle,
 *                                    (AttributesBufferIteratorFunc)callback,
 *			              "gl_Vertex", "gl_Color", NULL);
 * static void callback (CoglVertexBufferVertex *vert)
 * {
 *    GLfloat *pos = vert->attrib[0];
 *    GLubyte *color = vert->attrib[1];
 *    GLfloat *new_attrib = buf[vert->index];
 *
 *    new_attrib = pos*color;
 * }
 *
 * TODO
 * Think about a higher level Mesh API for building/modifying attribute buffers
 * - E.g. look at Blender for inspiration here. They can build a mesh from
 *   "MVert", "MFace" and "MEdge" primitives.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-vertex-buffer-private.h"
#include "cogl-texture-private.h"
#include "cogl-pipeline.h"
#include "cogl-pipeline-private.h"
#include "cogl-primitives.h"
#include "cogl-framebuffer-private.h"
#include "cogl-primitive-private.h"
#include "cogl-journal-private.h"
#include "cogl1-context.h"
#include "cogl-vertex-buffer.h"

#define PAD_FOR_ALIGNMENT(VAR, TYPE_SIZE) \
  (VAR = TYPE_SIZE + ((VAR - 1) & ~(TYPE_SIZE - 1)))

static void _cogl_vertex_buffer_free (CoglVertexBuffer *buffer);
static void _cogl_vertex_buffer_indices_free (CoglVertexBufferIndices *buffer_indices);
static CoglUserDataKey _cogl_vertex_buffer_pipeline_priv_key;

COGL_HANDLE_DEFINE (VertexBuffer, vertex_buffer);
COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING (vertex_buffer);
COGL_HANDLE_DEFINE (VertexBufferIndices, vertex_buffer_indices);

CoglHandle
cogl_vertex_buffer_new (unsigned int n_vertices)
{
  CoglVertexBuffer *buffer = g_slice_alloc (sizeof (CoglVertexBuffer));

  buffer->n_vertices = n_vertices;

  buffer->submitted_vbos = NULL;
  buffer->new_attributes = NULL;
  buffer->primitive = cogl_primitive_new (COGL_VERTICES_MODE_TRIANGLES,
                                          n_vertices, NULL);

  /* return COGL_INVALID_HANDLE; */
  return _cogl_vertex_buffer_handle_new (buffer);
}

unsigned int
cogl_vertex_buffer_get_n_vertices (CoglHandle handle)
{
  CoglVertexBuffer *buffer;

  if (!cogl_is_vertex_buffer (handle))
    return 0;

  buffer = handle;

  return buffer->n_vertices;
}

/* There are a number of standard OpenGL attributes that we deal with
 * specially. These attributes are all namespaced with a "gl_" prefix
 * so we should catch any typos instead of silently adding a custom
 * attribute.
 */
static CoglVertexBufferAttribFlags
validate_gl_attribute (const char *gl_attribute,
		       uint8_t n_components,
		       uint8_t *texture_unit)
{
  CoglVertexBufferAttribFlags type;
  char *detail_seperator = NULL;
  int name_len;

  detail_seperator = strstr (gl_attribute, "::");
  if (detail_seperator)
    name_len = detail_seperator - gl_attribute;
  else
    name_len = strlen (gl_attribute);

  if (strncmp (gl_attribute, "Vertex", name_len) == 0)
    {
      if (G_UNLIKELY (n_components == 1))
        g_critical ("glVertexPointer doesn't allow 1 component vertex "
                    "positions so we currently only support \"gl_Vertex\" "
                    "attributes where n_components == 2, 3 or 4");
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_VERTEX_ARRAY;
    }
  else if (strncmp (gl_attribute, "Color", name_len) == 0)
    {
      if (G_UNLIKELY (n_components != 3 && n_components != 4))
        g_critical ("glColorPointer expects 3 or 4 component colors so we "
                    "currently only support \"gl_Color\" attributes where "
                    "n_components == 3 or 4");
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_COLOR_ARRAY;
    }
  else if (strncmp (gl_attribute,
		    "MultiTexCoord",
		    strlen ("MultiTexCoord")) == 0)
    {
      unsigned int unit;

      if (sscanf (gl_attribute, "MultiTexCoord%u", &unit) != 1)
	{
	  g_warning ("gl_MultiTexCoord attributes should include a\n"
		     "texture unit number, E.g. gl_MultiTexCoord0\n");
	  unit = 0;
	}
      /* FIXME: validate any '::' delimiter for this case */
      *texture_unit = unit;
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_TEXTURE_COORD_ARRAY;
    }
  else if (strncmp (gl_attribute, "Normal", name_len) == 0)
    {
      if (G_UNLIKELY (n_components != 3))
        g_critical ("glNormalPointer expects 3 component normals so we "
                    "currently only support \"gl_Normal\" attributes where "
                    "n_components == 3");
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_NORMAL_ARRAY;
    }
  else
    {
      g_warning ("Unknown gl_* attribute name gl_%s\n", gl_attribute);
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_INVALID;
    }

  return type;
}

/* There are a number of standard OpenGL attributes that we deal with
 * specially. These attributes are all namespaced with a "gl_" prefix
 * so we should catch any typos instead of silently adding a custom
 * attribute.
 */
static CoglVertexBufferAttribFlags
validate_cogl_attribute (const char *cogl_attribute,
		         uint8_t n_components,
		         uint8_t *texture_unit)
{
  CoglVertexBufferAttribFlags type;
  char *detail_seperator = NULL;
  int name_len;

  detail_seperator = strstr (cogl_attribute, "::");
  if (detail_seperator)
    name_len = detail_seperator - cogl_attribute;
  else
    name_len = strlen (cogl_attribute);

  if (strncmp (cogl_attribute, "position_in", name_len) == 0)
    {
      if (G_UNLIKELY (n_components == 1))
        g_critical ("glVertexPointer doesn't allow 1 component vertex "
                    "positions so we currently only support "
                    "\"cogl_position_in\" attributes where "
                    "n_components == 2, 3 or 4");
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_VERTEX_ARRAY;
    }
  else if (strncmp (cogl_attribute, "color_in", name_len) == 0)
    {
      if (G_UNLIKELY (n_components != 3 && n_components != 4))
        g_critical ("glColorPointer expects 3 or 4 component colors so we "
                    "currently only support \"cogl_color_in\" attributes "
                    "where n_components == 3 or 4");
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_COLOR_ARRAY;
    }
  else if (strncmp (cogl_attribute,
		    "cogl_tex_coord",
		    strlen ("cogl_tex_coord")) == 0)
    {
      unsigned int unit;

      if (strcmp (cogl_attribute, "cogl_tex_coord_in") == 0)
        unit = 0;
      else if (sscanf (cogl_attribute, "cogl_tex_coord%u_in", &unit) != 1)
	{
	  g_warning ("texture coordinate attributes should either be "
                     "referenced as \"cogl_tex_coord_in\" or with a"
                     "texture unit number like \"cogl_tex_coord1_in\"");
	  unit = 0;
	}
      /* FIXME: validate any '::' delimiter for this case */
      *texture_unit = unit;
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_TEXTURE_COORD_ARRAY;
    }
  else if (strncmp (cogl_attribute, "normal_in", name_len) == 0)
    {
      if (G_UNLIKELY (n_components != 3))
        g_critical ("glNormalPointer expects 3 component normals so we "
                    "currently only support \"cogl_normal_in\" attributes "
                    "where n_components == 3");
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_NORMAL_ARRAY;
    }
  else
    {
      g_warning ("Unknown cogl_* attribute name cogl_%s\n", cogl_attribute);
      type = COGL_VERTEX_BUFFER_ATTRIB_FLAG_INVALID;
    }

  return type;
}

/* This validates that a custom attribute name is a valid GLSL variable name
 *
 * NB: attribute names may have a detail component delimited using '::' E.g.
 * custom_attrib::foo or custom_attrib::bar
 *
 * maybe I should hang a compiled regex somewhere to handle this
 */
static CoglBool
validate_custom_attribute_name (const char *attribute_name)
{
  char *detail_seperator = NULL;
  int name_len;
  int i;

  detail_seperator = strstr (attribute_name, "::");
  if (detail_seperator)
    name_len = detail_seperator - attribute_name;
  else
    name_len = strlen (attribute_name);

  if (name_len == 0
      || !g_ascii_isalpha (attribute_name[0])
      || attribute_name[0] != '_')
    return FALSE;

  for (i = 1; i < name_len; i++)
    if (!g_ascii_isalnum (attribute_name[i]) || attribute_name[i] != '_')
      return FALSE;

  return TRUE;
}

/* Iterates the CoglVertexBufferVBOs of a buffer and creates a flat list
 * of all the submitted attributes
 *
 * Note: The CoglVertexBufferAttrib structs are deep copied, except the
 * internal CoglAttribute pointer is set to NULL.
 */
static GList *
copy_submitted_attributes_list (CoglVertexBuffer *buffer)
{
  GList *tmp;
  GList *submitted_attributes = NULL;

  for (tmp = buffer->submitted_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferVBO *cogl_vbo = tmp->data;
      GList *tmp2;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglVertexBufferAttrib *attribute = tmp2->data;
	  CoglVertexBufferAttrib *copy =
            g_slice_alloc (sizeof (CoglVertexBufferAttrib));
	  *copy = *attribute;
          copy->name_without_detail =
            g_strdup (attribute->name_without_detail);
          copy->attribute = NULL;
	  submitted_attributes = g_list_prepend (submitted_attributes, copy);
	}
    }
  return submitted_attributes;
}

static size_t
sizeof_attribute_type (CoglAttributeType type)
{
  switch (type)
    {
    case COGL_ATTRIBUTE_TYPE_BYTE:
      return 1;
    case COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
      return 1;
    case COGL_ATTRIBUTE_TYPE_SHORT:
      return 2;
    case COGL_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
      return 2;
    case COGL_ATTRIBUTE_TYPE_FLOAT:
      return 4;
    }
  g_return_val_if_reached (0);
}

static size_t
strideof (CoglAttributeType type, int n_components)
{
  return sizeof_attribute_type (type) * n_components;
}

static char *
canonize_attribute_name (const char *attribute_name)
{
  char *detail_seperator = NULL;
  int name_len;

  if (strncmp (attribute_name, "gl_", 3) != 0)
    return g_strdup (attribute_name);

  /* skip past the "gl_" */
  attribute_name += 3;

  detail_seperator = strstr (attribute_name, "::");
  if (detail_seperator)
    name_len = detail_seperator - attribute_name;
  else
    {
      name_len = strlen (attribute_name);
      detail_seperator = "";
    }

  if (strncmp (attribute_name, "Vertex", name_len) == 0)
    return g_strconcat ("cogl_position_in", detail_seperator, NULL);
  else if (strncmp (attribute_name, "Color", name_len) == 0)
    return g_strconcat ("cogl_color_in", detail_seperator, NULL);
  else if (strncmp (attribute_name,
		    "MultiTexCoord",
		    strlen ("MultiTexCoord")) == 0)
    {
      unsigned int unit;

      if (sscanf (attribute_name, "MultiTexCoord%u", &unit) != 1)
	{
	  g_warning ("gl_MultiTexCoord attributes should include a\n"
		     "texture unit number, E.g. gl_MultiTexCoord0\n");
	  unit = 0;
	}
      return g_strdup_printf ("cogl_tex_coord%u_in%s",
                              unit, detail_seperator);
    }
  else if (strncmp (attribute_name, "Normal", name_len) == 0)
    return g_strconcat ("cogl_normal_in", detail_seperator, NULL);
  else
    {
      g_warning ("Unknown gl_* attribute name gl_%s\n", attribute_name);
      return g_strdup (attribute_name);
    }
}

void
cogl_vertex_buffer_add (CoglHandle         handle,
		        const char        *attribute_name,
			uint8_t            n_components,
			CoglAttributeType  type,
			CoglBool           normalized,
			uint16_t           stride,
			const void        *pointer)
{
  CoglVertexBuffer *buffer;
  char *cogl_attribute_name;
  GQuark name_quark;
  CoglBool modifying_an_attrib = FALSE;
  CoglVertexBufferAttrib *attribute;
  CoglVertexBufferAttribFlags flags = 0;
  uint8_t texture_unit = 0;
  GList *tmp;
  char *detail;

  if (!cogl_is_vertex_buffer (handle))
    return;

  buffer = handle;
  buffer->dirty_attributes = TRUE;

  cogl_attribute_name = canonize_attribute_name (attribute_name);
  name_quark = g_quark_from_string (cogl_attribute_name);

  /* The submit function works by diffing between submitted_attributes
   * and new_attributes to minimize the upload bandwidth + cost of
   * allocating new VBOs, so if there isn't already a list of new_attributes
   * we create one: */
  if (!buffer->new_attributes)
    buffer->new_attributes = copy_submitted_attributes_list (buffer);

  /* Note: we first look for an existing attribute that we are modifying
   * so we may skip needing to validate the name */
  for (tmp = buffer->new_attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferAttrib *submitted_attribute = tmp->data;
      if (submitted_attribute->name == name_quark)
	{
	  modifying_an_attrib = TRUE;

	  attribute = submitted_attribute;

	  /* since we will skip validate_gl/cogl_attribute in this case, we
           * need to pluck out the attribute type before overwriting the
           * flags: */
	  flags |=
            attribute->flags & COGL_VERTEX_BUFFER_ATTRIB_FLAG_TYPE_MASK;
	  break;
	}
    }

  if (!modifying_an_attrib)
    {
      /* Validate the attribute name, is suitable as a variable name */
      if (strncmp (attribute_name, "gl_", 3) == 0)
	{
          /* Note: we pass the original attribute name here so that
           * any warning messages correspond to the users original
           * attribute name... */
	  flags |= validate_gl_attribute (attribute_name + 3,
					  n_components,
					  &texture_unit);
	  if (flags & COGL_VERTEX_BUFFER_ATTRIB_FLAG_INVALID)
	    return;
	}
      else if (strncmp (attribute_name, "cogl_", 5) == 0)
	{
	  flags |= validate_cogl_attribute (attribute_name + 5,
					    n_components,
					    &texture_unit);
	  if (flags & COGL_VERTEX_BUFFER_ATTRIB_FLAG_INVALID)
	    return;
	}
      else
	{
	  flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_CUSTOM_ARRAY;
	  if (validate_custom_attribute_name (attribute_name))
	    return;
	}

      attribute = g_slice_alloc0 (sizeof (CoglVertexBufferAttrib));
    }

  attribute->name = name_quark;
  detail = strstr (cogl_attribute_name, "::");
  if (detail)
    attribute->name_without_detail = g_strndup (cogl_attribute_name,
                                                detail - cogl_attribute_name);
  else
    attribute->name_without_detail = g_strdup (cogl_attribute_name);
  attribute->type = type;
  attribute->n_components = n_components;
  if (stride == 0)
    stride = strideof (type, n_components);
  attribute->stride = stride;
  attribute->u.pointer = pointer;
  attribute->texture_unit = texture_unit;
  attribute->attribute = NULL;

  flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_ENABLED;

  /* Note: We currently just assume, if an attribute is *ever* updated
   * then it should be taged as frequently changing. */
  if (modifying_an_attrib)
    flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_FREQUENT_RESUBMIT;
  else
    flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_INFREQUENT_RESUBMIT;

  if (normalized)
    flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_NORMALIZED;
  attribute->flags = flags;

  attribute->span_bytes = buffer->n_vertices * attribute->stride;

  if (!modifying_an_attrib)
    buffer->new_attributes =
      g_list_prepend (buffer->new_attributes, attribute);

  g_free (cogl_attribute_name);
}

static void
_cogl_vertex_buffer_attrib_free (CoglVertexBufferAttrib *attribute)
{
  if (attribute->attribute)
    cogl_object_unref (attribute->attribute);
  g_free (attribute->name_without_detail);
  g_slice_free (CoglVertexBufferAttrib, attribute);
}

void
cogl_vertex_buffer_delete (CoglHandle handle,
			   const char *attribute_name)
{
  CoglVertexBuffer *buffer;
  char *cogl_attribute_name = canonize_attribute_name (attribute_name);
  GQuark name = g_quark_from_string (cogl_attribute_name);
  GList *tmp;

  g_free (cogl_attribute_name);

  if (!cogl_is_vertex_buffer (handle))
    return;

  buffer = handle;
  buffer->dirty_attributes = TRUE;

  /* The submit function works by diffing between submitted_attributes
   * and new_attributes to minimize the upload bandwidth + cost of
   * allocating new VBOs, so if there isn't already a list of new_attributes
   * we create one: */
  if (!buffer->new_attributes)
    buffer->new_attributes = copy_submitted_attributes_list (buffer);

  for (tmp = buffer->new_attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferAttrib *submitted_attribute = tmp->data;
      if (submitted_attribute->name == name)
	{
	  buffer->new_attributes =
	    g_list_delete_link (buffer->new_attributes, tmp);
          _cogl_vertex_buffer_attrib_free (submitted_attribute);
	  return;
	}
    }

  g_warning ("Failed to find an attribute named %s to delete\n",
	     attribute_name);
}

static void
set_attribute_enable (CoglHandle handle,
		      const char *attribute_name,
		      CoglBool state)
{
  CoglVertexBuffer *buffer;
  char *cogl_attribute_name = canonize_attribute_name (attribute_name);
  GQuark name_quark = g_quark_from_string (cogl_attribute_name);
  GList *tmp;

  g_free (cogl_attribute_name);

  if (!cogl_is_vertex_buffer (handle))
    return;

  buffer = handle;
  buffer->dirty_attributes = TRUE;

  /* NB: If a buffer is currently being edited, then there can be two seperate
   * lists of attributes; those that are currently submitted and a new list yet
   * to be submitted, we need to modify both. */

  for (tmp = buffer->new_attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferAttrib *attribute = tmp->data;
      if (attribute->name == name_quark)
	{
	  if (state)
	    attribute->flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_ENABLED;
	  else
	    attribute->flags &= ~COGL_VERTEX_BUFFER_ATTRIB_FLAG_ENABLED;
	  break;
	}
    }

  for (tmp = buffer->submitted_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferVBO *cogl_vbo = tmp->data;
      GList *tmp2;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglVertexBufferAttrib *attribute = tmp2->data;
	  if (attribute->name == name_quark)
	    {
	      if (state)
		attribute->flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_ENABLED;
	      else
		attribute->flags &= ~COGL_VERTEX_BUFFER_ATTRIB_FLAG_ENABLED;
	      return;
	    }
	}
    }

  g_warning ("Failed to %s attribute named %s/%s\n",
	     state == TRUE ? "enable" : "disable",
	     attribute_name, cogl_attribute_name);
}

void
cogl_vertex_buffer_enable (CoglHandle handle,
			       const char *attribute_name)
{
  set_attribute_enable (handle, attribute_name, TRUE);
}

void
cogl_vertex_buffer_disable (CoglHandle handle,
			    const char *attribute_name)
{
  set_attribute_enable (handle, attribute_name, FALSE);
}

/* Given an attribute that we know has already been submitted before, this
 * function looks for the existing VBO that contains it.
 *
 * Note: It will free redundant attribute struct once the corresponding
 * VBO has been found.
 */
static void
filter_already_submitted_attribute (CoglVertexBufferAttrib *attribute,
				    GList **reuse_vbos,
				    GList **submitted_vbos)
{
  GList *tmp;

  /* First check the cogl_vbos we already know are being reused since we
   * are more likley to get a match here */
  for (tmp = *reuse_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferVBO *cogl_vbo = tmp->data;
      GList *tmp2;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglVertexBufferAttrib *vbo_attribute = tmp2->data;

	  if (vbo_attribute->name == attribute->name)
	    {
	      vbo_attribute->flags &=
                ~COGL_VERTEX_BUFFER_ATTRIB_FLAG_UNUSED;
	      /* Note: we don't free the redundant attribute here, since it
	       * will be freed after all filtering in
               * cogl_vertex_buffer_submit */
	      return;
	    }
	}
    }

  for (tmp = *submitted_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferVBO *cogl_vbo = tmp->data;
      CoglVertexBufferAttrib *reuse_attribute = NULL;
      GList *tmp2;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglVertexBufferAttrib *vbo_attribute = tmp2->data;
	  if (vbo_attribute->name == attribute->name)
	    {
	      reuse_attribute = vbo_attribute;
	      /* Note: we don't free the redundant attribute here, since it
	       * will be freed after all filtering in
               * cogl_vertex_buffer_submit */

	      *submitted_vbos = g_list_remove_link (*submitted_vbos, tmp);
	      tmp->next = *reuse_vbos;
	      *reuse_vbos = tmp;
	      break;
	    }
	}

      if (!reuse_attribute)
	continue;

      /* Mark all but the matched attribute as UNUSED, so that when we
       * finish filtering all our attributes any attrributes still
       * marked as UNUSED can be removed from their cogl_vbo */
      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglVertexBufferAttrib *vbo_attribute = tmp2->data;
	  if (vbo_attribute != reuse_attribute)
	    vbo_attribute->flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_UNUSED;
	}

      return;
    }

  g_critical ("Failed to find the cogl vbo that corresponds to an\n"
	      "attribute that had apparently already been submitted!");
}

/* When we first mark a CoglVertexBufferVBO to be reused, we mark the
 * attributes as unsed, so that when filtering of attributes into VBOs is done
 * we can then prune the now unsed attributes. */
static void
remove_unused_attributes (CoglVertexBufferVBO *cogl_vbo)
{
  GList *tmp;
  GList *next;

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = next)
    {
      CoglVertexBufferAttrib *attribute = tmp->data;
      next = tmp->next;

      if (attribute->flags & COGL_VERTEX_BUFFER_ATTRIB_FLAG_UNUSED)
	{
	  cogl_vbo->attributes =
	    g_list_delete_link (cogl_vbo->attributes, tmp);
	  g_slice_free (CoglVertexBufferAttrib, attribute);
	}
    }
}

/* Give a newly added, strided, attribute, this function looks for a
 * CoglVertexBufferVBO that the attribute is interleved with. If it can't
 * find one then a new CoglVertexBufferVBO is allocated and added to the
 * list of new_strided_vbos.
 */
static void
filter_strided_attribute (CoglVertexBufferAttrib *attribute,
			  GList **new_vbos)
{
  GList *tmp;
  CoglVertexBufferVBO *new_cogl_vbo;

  for (tmp = *new_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferVBO *cogl_vbo = tmp->data;
      GList *tmp2;

      if (!(cogl_vbo->flags & COGL_VERTEX_BUFFER_VBO_FLAG_STRIDED))
	continue;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglVertexBufferAttrib *vbo_attribute = tmp2->data;
	  const char *attribute_start = attribute->u.pointer;
	  const char *vbo_attribute_start = vbo_attribute->u.pointer;

	  /* NB: All attributes have buffer->n_vertices values which
	   * simplifies determining which attributes are interleved
	   * since we assume they will start no farther than +- a
	   * stride away from each other:
	   */
	  if (attribute_start <= (vbo_attribute_start - vbo_attribute->stride)
	      || attribute_start
		 >= (vbo_attribute_start + vbo_attribute->stride))
	    continue; /* Not interleved */

	  cogl_vbo->attributes =
	    g_list_prepend (cogl_vbo->attributes, attribute);

	  if (attribute->flags &
              COGL_VERTEX_BUFFER_ATTRIB_FLAG_FREQUENT_RESUBMIT)
	    {
	      cogl_vbo->flags &=
                ~COGL_VERTEX_BUFFER_VBO_FLAG_INFREQUENT_RESUBMIT;
	      cogl_vbo->flags |=
                COGL_VERTEX_BUFFER_VBO_FLAG_FREQUENT_RESUBMIT;
	    }
	      return;
	}
    }
  new_cogl_vbo = g_slice_alloc (sizeof (CoglVertexBufferVBO));
  new_cogl_vbo->attributes = NULL;
  new_cogl_vbo->attributes =
    g_list_prepend (new_cogl_vbo->attributes, attribute);
  /* Any one of the interleved attributes will have the same span_bytes */
  new_cogl_vbo->attribute_buffer = NULL;
  new_cogl_vbo->buffer_bytes = attribute->span_bytes;
  new_cogl_vbo->flags = COGL_VERTEX_BUFFER_VBO_FLAG_STRIDED;

  if (attribute->flags & COGL_VERTEX_BUFFER_ATTRIB_FLAG_INFREQUENT_RESUBMIT)
    new_cogl_vbo->flags |= COGL_VERTEX_BUFFER_VBO_FLAG_INFREQUENT_RESUBMIT;
  else
    new_cogl_vbo->flags |= COGL_VERTEX_BUFFER_VBO_FLAG_FREQUENT_RESUBMIT;

  *new_vbos = g_list_prepend (*new_vbos, new_cogl_vbo);
  return;
}

/* This iterates through the list of submitted VBOs looking for one that
 * contains attribute. If found the list *link* is removed and returned */
static GList *
unlink_submitted_vbo_containing_attribute (GList **submitted_vbos,
					   CoglVertexBufferAttrib *attribute)
{
  GList *tmp;
  GList *next = NULL;

  for (tmp = *submitted_vbos; tmp != NULL; tmp = next)
    {
      CoglVertexBufferVBO *submitted_vbo = tmp->data;
      GList *tmp2;

      next = tmp->next;

      for (tmp2 = submitted_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglVertexBufferAttrib *submitted_attribute = tmp2->data;

	  if (submitted_attribute->name == attribute->name)
	    {
	      *submitted_vbos = g_list_remove_link (*submitted_vbos, tmp);
	      return tmp;
	    }
	}
    }

  return NULL;
}

/* Unlinks all the submitted VBOs that conflict with the new cogl_vbo and
 * returns them as a list. */
static GList *
get_submitted_vbo_conflicts (GList **submitted_vbos,
                             CoglVertexBufferVBO *cogl_vbo)
{
  GList *tmp;
  GList *conflicts = NULL;

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      GList *link =
	unlink_submitted_vbo_containing_attribute (submitted_vbos,
						   tmp->data);
      if (link)
	{
	  /* prepend the link to the list of conflicts: */
	  link->next = conflicts;
	  conflicts = link;
	}
    }
  return conflicts;
}

/* Any attributes in cogl_vbo gets removed from conflict_vbo */
static void
disassociate_conflicting_attributes (CoglVertexBufferVBO *conflict_vbo,
				     CoglVertexBufferVBO *cogl_vbo)
{
  GList *tmp;

  /* NB: The attributes list in conflict_vbo will be shrinking so
   * we iterate those in the inner loop. */

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferAttrib *attribute = tmp->data;
      GList *tmp2;
      for (tmp2 = conflict_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglVertexBufferAttrib *conflict_attribute = tmp2->data;

	  if (conflict_attribute->name == attribute->name)
	    {
	      _cogl_vertex_buffer_attrib_free (conflict_attribute);
	      conflict_vbo->attributes =
		g_list_delete_link (conflict_vbo->attributes, tmp2);
	      break;
	    }
	}
    }
}

static void
cogl_vertex_buffer_vbo_free (CoglVertexBufferVBO *cogl_vbo)
{
  GList *tmp;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    _cogl_vertex_buffer_attrib_free (tmp->data);
  g_list_free (cogl_vbo->attributes);

  if (cogl_vbo->flags & COGL_VERTEX_BUFFER_VBO_FLAG_SUBMITTED)
    cogl_object_unref (cogl_vbo->attribute_buffer);

  g_slice_free (CoglVertexBufferVBO, cogl_vbo);
}

/* This figures out the lowest attribute client pointer. (This pointer is used
 * to upload all the interleved attributes).
 *
 * In the process it also replaces the client pointer with the attributes
 * offset, and marks the attribute as submitted.
 */
static const void *
prep_strided_vbo_for_upload (CoglVertexBufferVBO *cogl_vbo)
{
  GList *tmp;
  const char *lowest_pointer = NULL;

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferAttrib *attribute = tmp->data;
      const char *client_pointer = attribute->u.pointer;

      if (!lowest_pointer || client_pointer < lowest_pointer)
	lowest_pointer = client_pointer;
    }

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferAttrib *attribute = tmp->data;
      const char *client_pointer = attribute->u.pointer;
      attribute->u.vbo_offset = client_pointer - lowest_pointer;
      attribute->flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_SUBMITTED;
    }

  return lowest_pointer;
}

static CoglBool
upload_multipack_vbo_via_map_buffer (CoglVertexBufferVBO *cogl_vbo)
{
  GList *tmp;
  unsigned int offset = 0;
  uint8_t *buf;

  _COGL_GET_CONTEXT (ctx, FALSE);

  buf = cogl_buffer_map (COGL_BUFFER (cogl_vbo->attribute_buffer),
                         COGL_BUFFER_ACCESS_WRITE,
                         COGL_BUFFER_MAP_HINT_DISCARD);
  if (!buf)
    return FALSE;

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferAttrib *attribute = tmp->data;
      gsize attribute_size = attribute->span_bytes;
      gsize type_size = sizeof_attribute_type (attribute->type);

      PAD_FOR_ALIGNMENT (offset, type_size);

      memcpy (buf + offset, attribute->u.pointer, attribute_size);

      attribute->u.vbo_offset = offset;
      attribute->flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_SUBMITTED;
      offset += attribute_size;
    }

  cogl_buffer_unmap (COGL_BUFFER (cogl_vbo->attribute_buffer));

  return TRUE;
}

static void
upload_multipack_vbo_via_buffer_sub_data (CoglVertexBufferVBO *cogl_vbo)
{
  GList *l;
  unsigned int offset = 0;

  for (l = cogl_vbo->attributes; l != NULL; l = l->next)
    {
      CoglVertexBufferAttrib *attribute = l->data;
      gsize attribute_size = attribute->span_bytes;
      gsize type_size = sizeof_attribute_type (attribute->type);

      PAD_FOR_ALIGNMENT (offset, type_size);

      cogl_buffer_set_data (COGL_BUFFER (cogl_vbo->attribute_buffer),
                            offset,
                            attribute->u.pointer,
                            attribute_size);

      attribute->u.vbo_offset = offset;
      attribute->flags |= COGL_VERTEX_BUFFER_ATTRIB_FLAG_SUBMITTED;
      offset += attribute_size;
    }
}

static void
upload_attributes (CoglVertexBufferVBO *cogl_vbo)
{
  CoglBufferUpdateHint usage;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (cogl_vbo->flags & COGL_VERTEX_BUFFER_VBO_FLAG_FREQUENT_RESUBMIT)
    usage = COGL_BUFFER_UPDATE_HINT_DYNAMIC;
  else
    usage = COGL_BUFFER_UPDATE_HINT_STATIC;
  cogl_buffer_set_update_hint (COGL_BUFFER (cogl_vbo->attribute_buffer), usage);

  if (cogl_vbo->flags & COGL_VERTEX_BUFFER_VBO_FLAG_STRIDED)
    {
      const void *pointer = prep_strided_vbo_for_upload (cogl_vbo);
      cogl_buffer_set_data (COGL_BUFFER (cogl_vbo->attribute_buffer),
                            0, /* offset */
                            pointer,
                            cogl_vbo->buffer_bytes);
    }
  else /* MULTIPACK */
    {
      /* I think it might depend on the specific driver/HW whether its better
       * to use glMapBuffer here or glBufferSubData here. There is even a good
       * thread about this topic here:
       * http://www.mail-archive.com/dri-devel@lists.sourceforge.net/msg35004.html
       * For now I have gone with glMapBuffer, but the jury is still out.
       */

      if (!upload_multipack_vbo_via_map_buffer (cogl_vbo))
	upload_multipack_vbo_via_buffer_sub_data  (cogl_vbo);
    }

  cogl_vbo->flags |= COGL_VERTEX_BUFFER_VBO_FLAG_SUBMITTED;
}

/* Note: although there ends up being quite a few inner loops involved with
 * resolving buffers, the number of attributes will be low so I don't expect
 * them to cause a problem. */
static void
cogl_vertex_buffer_vbo_resolve (CoglVertexBuffer *buffer,
			        CoglVertexBufferVBO *new_cogl_vbo,
			        GList **final_vbos)
{
  GList *conflicts;
  GList *tmp;
  GList *next;
  CoglBool found_target_vbo = FALSE;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  conflicts =
    get_submitted_vbo_conflicts (&buffer->submitted_vbos, new_cogl_vbo);

  for (tmp = conflicts; tmp != NULL; tmp = next)
    {
      CoglVertexBufferVBO *conflict_vbo = tmp->data;

      next = tmp->next;

      disassociate_conflicting_attributes (conflict_vbo, new_cogl_vbo);

      if (!conflict_vbo->attributes)
	{
	  /* See if we can re-use this now empty VBO: */

	  if (!found_target_vbo
	      && conflict_vbo->buffer_bytes == new_cogl_vbo->buffer_bytes)
	    {
	      found_target_vbo = TRUE;
	      new_cogl_vbo->attribute_buffer =
                cogl_object_ref (conflict_vbo->attribute_buffer);
	      cogl_vertex_buffer_vbo_free (conflict_vbo);

	      upload_attributes (new_cogl_vbo);

	      *final_vbos = g_list_prepend (*final_vbos, new_cogl_vbo);
	    }
	  else
	    cogl_vertex_buffer_vbo_free (conflict_vbo);
	}
      else
	{
	  /* Relink the VBO back into buffer->submitted_vbos since it may
	   * be involved in other conflicts later */
	  tmp->next = buffer->submitted_vbos;
	  tmp->prev = NULL;
	  buffer->submitted_vbos = tmp;
	}
    }

  if (!found_target_vbo)
    {
      _COGL_GET_CONTEXT (ctx, NO_RETVAL);

      new_cogl_vbo->attribute_buffer =
        cogl_attribute_buffer_new (ctx, new_cogl_vbo->buffer_bytes, NULL);

      upload_attributes (new_cogl_vbo);
      *final_vbos = g_list_prepend (*final_vbos, new_cogl_vbo);
    }
}

static void
update_primitive_attributes (CoglVertexBuffer *buffer)
{
  GList *l;
  int n_attributes = 0;
  CoglAttribute **attributes;
  int i;

  if (!buffer->dirty_attributes)
    return;

  buffer->dirty_attributes = FALSE;

  for (l = buffer->submitted_vbos; l; l = l->next)
    {
      CoglVertexBufferVBO *cogl_vbo = l->data;
      GList *l2;

      for (l2 = cogl_vbo->attributes; l2; l2 = l2->next, n_attributes++)
        ;
    }

  _COGL_RETURN_IF_FAIL (n_attributes > 0);

  attributes = g_alloca (sizeof (CoglAttribute *) * n_attributes);

  i = 0;
  for (l = buffer->submitted_vbos; l; l = l->next)
    {
      CoglVertexBufferVBO *cogl_vbo = l->data;
      GList *l2;

      for (l2 = cogl_vbo->attributes; l2; l2 = l2->next)
        {
	  CoglVertexBufferAttrib *attribute = l2->data;
	  if (G_LIKELY (attribute->flags &
                        COGL_VERTEX_BUFFER_ATTRIB_FLAG_ENABLED))
            {
              if (G_UNLIKELY (!attribute->attribute))
                {
                  attribute->attribute =
                    cogl_attribute_new (cogl_vbo->attribute_buffer,
                                        attribute->name_without_detail,
                                        attribute->stride,
                                        attribute->u.vbo_offset,
                                        attribute->n_components,
                                        attribute->type);
                }

              attributes[i++] = attribute->attribute;
            }
        }
    }

  cogl_primitive_set_attributes (buffer->primitive, attributes, i);
}

static void
cogl_vertex_buffer_submit_real (CoglVertexBuffer *buffer)
{
  GList *tmp;
  CoglVertexBufferVBO *new_multipack_vbo;
  GList *new_multipack_vbo_link;
  GList *new_vbos = NULL;
  GList *reuse_vbos = NULL;
  GList *final_vbos = NULL;

  if (!buffer->new_attributes)
    goto done;

  /* The objective now is to copy the attribute data supplied by the client
   * into buffer objects, but it's important to minimize the number of
   * redundant data uploads.
   *
   * We obviously aim to group together the attributes that are interleved so
   * that they can be delivered in one go to the driver.
   * All BOs for interleved data are created as STATIC_DRAW_ARB.
   *
   * Non interleved attributes tagged as INFREQUENT_RESUBMIT will be grouped
   * together back to back in a single BO created as STATIC_DRAW_ARB
   *
   * Non interleved attributes tagged as FREQUENT_RESUBMIT will be copied into
   * individual buffer objects, and the BO itself created DYNAMIC_DRAW_ARB
   *
   * If we are modifying a previously submitted CoglVertexBuffer then we are
   * carefull not to needlesly delete OpenGL buffer objects and replace with
   * new ones, instead we upload new data to the existing buffers.
   */

  /* NB: We must forget attribute->pointer after submitting since the user
   * is free to re-use that memory for other purposes now. */

  /* Pseudo code:
   *
   * Broadly speaking we start with a list of unsorted attributes, and filter
   * those into 'new' and 're-use' CoglVertexBufferVBO (CBO) lists. We then
   * take the list of new CBO structs and compare with the CBOs that have
   * already been submitted to the GPU (but ignoring those we already know will
   * be re-used) to determine what other CBOs can be re-used, due to being
   * superseded, and what new GL VBOs need to be created.
   *
   * We have two kinds of CBOs:
   * - Multi Pack CBOs
   *     These contain multiple attributes tightly packed back to back)
   * - Strided CBOs
   *	 These typically contain multiple interleved sets of attributes,
   *	 though they can contain just one attribute with a stride
   *
   * First create a new-CBOs entry "new-multipack-CBO"
   * Tag "new-multipack-CBO" as MULTIPACK + INFREQUENT_RESUBMIT
   * For each unsorted attrib:
   *   if already marked as submitted:
   *	 iterate reuse-CBOs:
   *	   if we find one that contains this attribute:
   *	     free redundant unsorted attrib struct
   *	     remove the UNUSED flag from the attrib found in the reuse-CBO
   *	     continue to next unsorted attrib
   *	 iterate submitted VBOs:
   *	   if we find one that contains this attribute:
   *	     free redundant unsorted attrib struct
   *	     unlink the vbo and move it to the list of reuse-CBOs
   *	     mark all attributes except the one just matched as UNUSED
   *	 assert (found)
   *	 continue to next unsorted attrib
   *   if strided:
   * 	 iterate the new, strided, CBOs, to see if the attribute is
   *	 interleved with one of them, if found:
   *	   add to the matched CBO
   *	 else if not found:
   *	   create a new-CBOs entry tagged STRIDED + INFREQUENT_RESUBMIT
   *   else if unstrided && tagged with FREQUENT_RESUBMIT:
   *     create a new-CBOs entry tagged MULTIPACK + FREQUENT_RESUBMIT
   *   else
   *     add to the new-multipack-CBO
   * free list of unsorted-attribs
   *
   * Next compare the new list of CBOs with the submitted set and try to
   * minimize the memory bandwidth required to upload the attributes and the
   * overhead of creating new GL-BOs.
   *
   * We deal with four sets of CBOs:
   * - The "new" CBOs
   *    (as determined above during filtering)
   * - The "re-use" CBOs
   *    (as determined above during filtering)
   * - The "submitted" CBOs
   *    (I.e. ones currently submitted to the GPU)
   * - The "final" CBOs
   *	(The result of resolving the differences between the above sets)
   *
   * The re-use CBOs are dealt with first, and we simply delete any remaining
   * attributes in these that are still marked as UNUSED, and move them
   * to the list of final CBOs.
   *
   * Next we iterate through the "new" CBOs, searching for conflicts
   * with the "submitted" CBOs and commit our decision to the "final" CBOs
   *
   * When searching for submitted entries we always unlink items from the
   * submitted list once we make matches (before we make descisions
   * based on the matches). If the CBO node is superseded it is freed,
   * if it is modified but may be needed for more descisions later it is
   * relinked back into the submitted list and if it's identical to a new
   * CBO it will be linked into the final list.
   *
   * At the end the list of submitted CBOs represents the attributes that were
   * deleted from the buffer.
   *
   * Iterate re-use-CBOs:
   *   Iterate attribs for each:
   *	 if attrib UNUSED:
   *	   remove the attrib from the CBO + free
   *	   |Note: we could potentially mark this as a re-useable gap
   *	   |if needs be later.
   *   add re-use CBO to the final-CBOs
   * Iterate new-CBOs:
   *   List submitted CBOs conflicting with the this CBO (Unlinked items)
   *   found-target-BO=FALSE
   *   Iterate conflicting CBOs:
   *	 Disassociate conflicting attribs from conflicting CBO struct
   *	 If no attribs remain:
   *	   If found-target-BO!=TRUE
   *	   _AND_ If the total size of the conflicting CBO is compatible:
   *	   |Note: We don't currently consider re-using oversized buffers
   *	     found-target-BO=TRUE
   *	     upload replacement data
   *	     free submitted CBO struct
   *	     add new CBO struct to final-CBOs
   *	   else:
   *	     delete conflict GL-BO
   *	     delete conflict CBO struct
   *	 else:
   *	   relink CBO back into submitted-CBOs
   *
   *   if found-target-BO == FALSE:
   *	 create a new GL-BO
   *	 upload data
   *	 add new CBO struct to final-BOs
   *
   * Iterate through the remaining "submitted" CBOs:
   *   delete the submitted GL-BO
   *   free the submitted CBO struct
   */

  new_multipack_vbo = g_slice_alloc (sizeof (CoglVertexBufferVBO));
  new_multipack_vbo->attribute_buffer = NULL;
  new_multipack_vbo->buffer_bytes = 0;
  new_multipack_vbo->flags =
    COGL_VERTEX_BUFFER_VBO_FLAG_MULTIPACK
    | COGL_VERTEX_BUFFER_VBO_FLAG_INFREQUENT_RESUBMIT;
  new_multipack_vbo->attributes = NULL;
  new_vbos = g_list_prepend (new_vbos, new_multipack_vbo);
  /* We save the link pointer here, just so we can do a fast removal later if
   * no attributes get added to this vbo. */
  new_multipack_vbo_link = new_vbos;

  /* Start with a list of unsorted attributes, and filter those into
   * potential new Cogl BO structs
   */
  for (tmp = buffer->new_attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglVertexBufferAttrib *attribute = tmp->data;

      if (attribute->flags & COGL_VERTEX_BUFFER_ATTRIB_FLAG_SUBMITTED)
	{
	  /* If the attribute is already marked as submitted, then we need
	   * to find the existing VBO that contains it so we dont delete it.
	   *
	   * NB: this also frees the attribute struct since it's implicitly
	   * redundant in this case.
	   */
	  filter_already_submitted_attribute (attribute,
					      &reuse_vbos,
					      &buffer->submitted_vbos);
	}
      else if (attribute->stride)
	{
	  /* look for a CoglVertexBufferVBO that the attribute is
           * interleved with. If one can't be found then a new
           * CoglVertexBufferVBO is allocated and added to the list of
           * new_vbos: */
	  filter_strided_attribute (attribute, &new_vbos);
	}
      else if (attribute->flags &
               COGL_VERTEX_BUFFER_ATTRIB_FLAG_FREQUENT_RESUBMIT)
	{
	  CoglVertexBufferVBO *cogl_vbo =
            g_slice_alloc (sizeof (CoglVertexBufferVBO));

	  /* attributes we expect will be frequently resubmitted are placed
	   * in their own VBO so that updates don't impact other attributes
	   */

	  cogl_vbo->flags =
            COGL_VERTEX_BUFFER_VBO_FLAG_MULTIPACK
	    | COGL_VERTEX_BUFFER_VBO_FLAG_FREQUENT_RESUBMIT;
	  cogl_vbo->attributes = NULL;
	  cogl_vbo->attributes = g_list_prepend (cogl_vbo->attributes,
						 attribute);
	  cogl_vbo->attribute_buffer = NULL;
	  cogl_vbo->buffer_bytes = attribute->span_bytes;
	  new_vbos = g_list_prepend (new_vbos, cogl_vbo);
	}
      else
	{
	  gsize type_size = sizeof_attribute_type (attribute->flags);

	  /* Infrequently updated attributes just get packed back to back
	   * in a single VBO: */
	  new_multipack_vbo->attributes =
	    g_list_prepend (new_multipack_vbo->attributes,
			    attribute);

	  /* Note: we have to ensure that each run of attributes is
	   * naturally aligned according to its data type, which may
	   * require some padding bytes: */

	  /* XXX: We also have to be sure that the attributes aren't
	   * reorderd before being uploaded because the alignment padding
	   * is based on the adjacent attribute.
	   */

	  PAD_FOR_ALIGNMENT (new_multipack_vbo->buffer_bytes, type_size);

	  new_multipack_vbo->buffer_bytes += attribute->span_bytes;
	}
    }

  /* At this point all buffer->new_attributes have been filtered into
   * CoglVertexBufferVBOs... */
  g_list_free (buffer->new_attributes);
  buffer->new_attributes = NULL;

  /* If the multipack vbo wasn't needed: */
  if (new_multipack_vbo->attributes == NULL)
    {
      new_vbos = g_list_delete_link (new_vbos, new_multipack_vbo_link);
      g_slice_free (CoglVertexBufferVBO, new_multipack_vbo);
    }

  for (tmp = reuse_vbos; tmp != NULL; tmp = tmp->next)
    remove_unused_attributes (tmp->data);
  final_vbos = g_list_concat (final_vbos, reuse_vbos);

  for (tmp = new_vbos; tmp != NULL; tmp = tmp->next)
    cogl_vertex_buffer_vbo_resolve (buffer, tmp->data, &final_vbos);

  /* Anything left corresponds to deleted attributes: */
  for (tmp = buffer->submitted_vbos; tmp != NULL; tmp = tmp->next)
    cogl_vertex_buffer_vbo_free (tmp->data);
  g_list_free (buffer->submitted_vbos);
  g_list_free (new_vbos);

  buffer->submitted_vbos = final_vbos;

done:
  update_primitive_attributes (buffer);
}

void
cogl_vertex_buffer_submit (CoglHandle handle)
{
  CoglVertexBuffer *buffer;

  if (!cogl_is_vertex_buffer (handle))
    return;

  buffer = handle;

  cogl_vertex_buffer_submit_real (buffer);
}

typedef struct
{
  /* We have a ref-count on this private structure because we need to
     refer to it both from the private data on a pipeline and any weak
     pipelines that we create from it. If we didn't have the ref count
     then we would depend on the order of destruction of a
     CoglPipeline and the weak materials to avoid a crash */
  unsigned int ref_count;

  CoglPipeline *real_source;
} VertexBufferMaterialPrivate;

static void
unref_pipeline_priv (VertexBufferMaterialPrivate *priv)
{
  if (--priv->ref_count < 1)
    g_slice_free (VertexBufferMaterialPrivate, priv);
}

static void
weak_override_source_destroyed_cb (CoglPipeline *pipeline,
                                   void *user_data)
{
  VertexBufferMaterialPrivate *pipeline_priv = user_data;
  /* Unref the weak pipeline copy since it is no longer valid - probably because
   * one of its ancestors has been changed. */
  cogl_object_unref (pipeline_priv->real_source);
  pipeline_priv->real_source = NULL;
  /* A reference was added when we copied the weak material so we need
     to unref it here */
  unref_pipeline_priv (pipeline_priv);
}

static CoglBool
validate_layer_cb (CoglPipeline *pipeline,
                   int layer_index,
                   void *user_data)
{
  VertexBufferMaterialPrivate *pipeline_priv = user_data;
  CoglPipeline *source = pipeline_priv->real_source;

  if (!cogl_pipeline_get_layer_point_sprite_coords_enabled (source,
                                                            layer_index))
    {
      CoglPipelineWrapMode wrap_s;
      CoglPipelineWrapMode wrap_t;
      CoglPipelineWrapMode wrap_p;
      CoglBool need_override_source = FALSE;

      /* By default COGL_PIPELINE_WRAP_MODE_AUTOMATIC becomes
       * GL_CLAMP_TO_EDGE but we want GL_REPEAT to maintain
       * compatibility with older versions of Cogl so we'll override
       * it. We don't want to do this for point sprites because in
       * that case the whole texture is drawn so you would usually
       * want clamp-to-edge.
       */
      wrap_s = cogl_pipeline_get_layer_wrap_mode_s (source, layer_index);
      if (wrap_s == COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
        {
          need_override_source = TRUE;
          wrap_s = COGL_PIPELINE_WRAP_MODE_REPEAT;
        }
      wrap_t = cogl_pipeline_get_layer_wrap_mode_t (source, layer_index);
      if (wrap_t == COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
        {
          need_override_source = TRUE;
          wrap_t = COGL_PIPELINE_WRAP_MODE_REPEAT;
        }
      wrap_p = cogl_pipeline_get_layer_wrap_mode_p (source, layer_index);
      if (wrap_p == COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
        {
          need_override_source = TRUE;
          wrap_p = COGL_PIPELINE_WRAP_MODE_REPEAT;
        }

      if (need_override_source)
        {
          if (pipeline_priv->real_source == pipeline)
            {
              pipeline_priv->ref_count++;
              pipeline_priv->real_source = source =
                _cogl_pipeline_weak_copy (pipeline,
                                          weak_override_source_destroyed_cb,
                                          pipeline_priv);
            }

          cogl_pipeline_set_layer_wrap_mode_s (source, layer_index, wrap_s);
          cogl_pipeline_set_layer_wrap_mode_t (source, layer_index, wrap_t);
          cogl_pipeline_set_layer_wrap_mode_p (source, layer_index, wrap_p);
        }
    }

  return TRUE;
}

static void
destroy_pipeline_priv_cb (void *user_data)
{
  unref_pipeline_priv (user_data);
}

static void
update_primitive_and_draw (CoglVertexBuffer *buffer,
                           CoglVerticesMode mode,
                           int first,
                           int count,
                           CoglVertexBufferIndices *buffer_indices)
{
  VertexBufferMaterialPrivate *pipeline_priv;
  CoglPipeline *users_source;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_primitive_set_mode (buffer->primitive, mode);
  cogl_primitive_set_first_vertex (buffer->primitive, first);
  cogl_primitive_set_n_vertices (buffer->primitive, count);

  if (buffer_indices)
    cogl_primitive_set_indices (buffer->primitive, buffer_indices->indices, count);
  else
    cogl_primitive_set_indices (buffer->primitive, NULL, count);

  cogl_vertex_buffer_submit_real (buffer);

  users_source = cogl_get_source ();
  pipeline_priv =
    cogl_object_get_user_data (COGL_OBJECT (users_source),
                               &_cogl_vertex_buffer_pipeline_priv_key);
  if (G_UNLIKELY (!pipeline_priv))
    {
      pipeline_priv = g_slice_new0 (VertexBufferMaterialPrivate);
      pipeline_priv->ref_count = 1;
      cogl_object_set_user_data (COGL_OBJECT (users_source),
                                 &_cogl_vertex_buffer_pipeline_priv_key,
                                 pipeline_priv,
                                 destroy_pipeline_priv_cb);
    }

  if (G_UNLIKELY (!pipeline_priv->real_source))
    {
      pipeline_priv->real_source = users_source;
      cogl_pipeline_foreach_layer (pipeline_priv->real_source,
                                   validate_layer_cb,
                                   pipeline_priv);
    }

  /* XXX: although this may seem redundant, we need to do this since
   * CoglVertexBuffers can be used with legacy state and its the source stack
   * which track whether legacy state is enabled.
   *
   * (We only have a CoglDrawFlag to disable legacy state not one
   *  to enable it) */
  cogl_push_source (pipeline_priv->real_source);

  _cogl_primitive_draw (buffer->primitive,
                        cogl_get_draw_framebuffer (),
                        pipeline_priv->real_source,
                        0 /* no draw flags */);

  cogl_pop_source ();
}

void
cogl_vertex_buffer_draw (CoglHandle       handle,
		         CoglVerticesMode mode,
		         int              first,
		         int              count)
{
  CoglVertexBuffer *buffer;

  if (!cogl_is_vertex_buffer (handle))
    return;

  buffer = handle;

  update_primitive_and_draw (buffer, mode, first, count, NULL);
}

static CoglHandle
_cogl_vertex_buffer_indices_new_real (CoglIndices *indices)
{
  CoglVertexBufferIndices *buffer_indices =
    g_slice_alloc (sizeof (CoglVertexBufferIndices));
  buffer_indices->indices = indices;

  return _cogl_vertex_buffer_indices_handle_new (buffer_indices);
}

CoglHandle
cogl_vertex_buffer_indices_new (CoglIndicesType  indices_type,
                                const void      *indices_array,
                                int              indices_len)
{
  CoglIndices *indices;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  indices = cogl_indices_new (ctx, indices_type, indices_array, indices_len);
  return _cogl_vertex_buffer_indices_new_real (indices);
}

CoglIndicesType
cogl_vertex_buffer_indices_get_type (CoglHandle indices_handle)
{
  CoglVertexBufferIndices *buffer_indices = NULL;

  if (!cogl_is_vertex_buffer_indices (indices_handle))
    return COGL_INDICES_TYPE_UNSIGNED_SHORT;

  buffer_indices = indices_handle;

  return cogl_indices_get_type (buffer_indices->indices);
}

void
_cogl_vertex_buffer_indices_free (CoglVertexBufferIndices *buffer_indices)
{
  cogl_object_unref (buffer_indices->indices);
  g_slice_free (CoglVertexBufferIndices, buffer_indices);
}

void
cogl_vertex_buffer_draw_elements (CoglHandle       handle,
			          CoglVerticesMode mode,
                                  CoglHandle       indices_handle,
                                  int              min_index,
                                  int              max_index,
                                  int              indices_offset,
                                  int              count)
{
  CoglVertexBuffer *buffer;
  CoglVertexBufferIndices *buffer_indices;

  if (!cogl_is_vertex_buffer (handle))
    return;

  buffer = handle;

  if (!cogl_is_vertex_buffer_indices (indices_handle))
    return;

  buffer_indices = indices_handle;

  update_primitive_and_draw (buffer, mode, indices_offset, count,
                             buffer_indices);
}

static void
_cogl_vertex_buffer_free (CoglVertexBuffer *buffer)
{
  GList *tmp;

  for (tmp = buffer->submitted_vbos; tmp != NULL; tmp = tmp->next)
    cogl_vertex_buffer_vbo_free (tmp->data);
  g_list_free (buffer->submitted_vbos);

  for (tmp = buffer->new_attributes; tmp != NULL; tmp = tmp->next)
    _cogl_vertex_buffer_attrib_free (tmp->data);
  g_list_free (buffer->new_attributes);

  if (buffer->primitive)
    cogl_object_unref (buffer->primitive);

  g_slice_free (CoglVertexBuffer, buffer);
}

CoglHandle
cogl_vertex_buffer_indices_get_for_quads (unsigned int n_indices)
{
  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  if (n_indices <= 256 / 4 * 6)
    {
      if (ctx->quad_buffer_indices_byte == COGL_INVALID_HANDLE)
        {
          /* NB: cogl_get_quad_indices takes n_quads not n_indices... */
          CoglIndices *indices = cogl_get_rectangle_indices (ctx, 256 / 4);
          cogl_object_ref (indices);
          ctx->quad_buffer_indices_byte =
            _cogl_vertex_buffer_indices_new_real (indices);
        }

      return ctx->quad_buffer_indices_byte;
    }
  else
    {
      if (ctx->quad_buffer_indices &&
          ctx->quad_buffer_indices_len < n_indices)
        {
          cogl_handle_unref (ctx->quad_buffer_indices);
          ctx->quad_buffer_indices = COGL_INVALID_HANDLE;
        }

      if (ctx->quad_buffer_indices == COGL_INVALID_HANDLE)
        {
          /* NB: cogl_get_quad_indices takes n_quads not n_indices... */
          CoglIndices *indices =
            cogl_get_rectangle_indices (ctx, n_indices / 6);
          cogl_object_ref (indices);
          ctx->quad_buffer_indices =
            _cogl_vertex_buffer_indices_new_real (indices);
        }

      ctx->quad_buffer_indices_len = n_indices;

      return ctx->quad_buffer_indices;
    }

  g_return_val_if_reached (NULL);
}

