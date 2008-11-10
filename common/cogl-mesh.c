/* Mesh API: Handle extensible arrays of vertex attributes
 *
 * Copyright (C) 2008  Intel Corporation.
 *
 * Authored by: Robert Bragg <bob@o-hand.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* XXX: For an overview of the functionality implemented here, please
 * see cogl.h.in, which contains the gtk-doc section overview for the
 * Mesh API.
 */

/* 
 * TODO: We need to do a better job of minimizing when we call glVertexPointer
 * and pals in enable_state_for_drawing_mesh
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
 * Actually hook this up to the cogl shaders infrastructure. The mesh API has
 * been designed to allow adding of arbitrary attributes for use with shaders,
 * but this has yet to be actually plumbed together and tested.
 * The bits we are missing:
 * - cogl_program_use doesn't currently record within ctx-> which program
 *   is currently in use so a.t.m only Clutter knows the current shader.
 * - We don't query the current shader program for the generic vertex indices
 *   (using glGetAttribLocation) so that we can call glEnableVertexAttribArray
 *   with those indices.
 *   (currently we just make up consecutive indices)
 * - some dirty flag meshanims to know when the shader program has changed
 *   so we don't need to re-query it each time we draw a mesh.
 * 
 * TODO:
 * There is currently no API for querying back info about a mesh, E.g.:
 * cogl_mesh_get_n_vertices (mesh_handle);
 * cogl_mesh_attribute_get_n_components (mesh_handle, "attrib_name");
 * cogl_mesh_attribute_get_stride (mesh_handle, "attrib_name");
 * cogl_mesh_attribute_get_normalized (mesh_handle, "attrib_name");
 * cogl_mesh_attribute_map (mesh_handle, "attrib_name");
 * cogl_mesh_attribute_unmap (mesh_handle, "attrib_name");
 * (Realistically I wouldn't expect anyone to use such an API examine the
 *  contents of a mesh for modification, since you'd need to handle too many
 *  possibilities, but never the less there might be other value in these.)

 * TODO:
 * It may be worth exposing the underlying VBOs for some advanced use
 * cases, e.g.:
 * handle = cogl_vbo_new (COGL_VBO_FLAG_STATIC);
 * pointer = cogl_vbo_map (handle, COGL_VBO_FLAG_WRITEONLY);
 * cogl_vbo_unmap (handle);
 * cogl_vbo_set_data (handle, size, data);
 * cogl_vbo_set_sub_data (handle, offset, size, data);
 * cogl_vbo_set_usage_hint (COGL_VBO_FLAG_DYNAMIC);
 *
 * TODO:
 * Experiment with wider use of the mesh API internally to Cogl.
 * - There is potential, I think, for this API to become a work-horse API
 *   within COGL for submitting geometry to the GPU, and could unify some of
 *   the GL/GLES code paths.
 * E.g.:
 * - Try creating a per-context mesh cache for cogl_texture_rectangle to sit
 *   on top of.
 * - Try saving the tesselation of paths/polygons into mesh objects internally.
 *
 * TODO
 * Expose API that lets developers get back a mesh handle for a particular
 * polygon so they may add custom attributes to them.
 * - It should be possible to query/modify a mesh efficiently, in place,
 *   avoiding copies. It would not be acceptable to simply require that
 *   developers must query back the n_vertices of a mesh and then the
 *   n_components, type and stride etc of each component since there
 *   would be too many combinations to realistically handle.
 * 
 * - In practice, some cases might be best solved with a higher level
 *   EditableMesh API, (see futher below) but for many cases I think an
 *   API like this might be appropriate:
 *
 * cogl_mesh_foreach_vertex (mesh_handle, (MeshIteratorFunc)callback,
 *			     "gl_Vertex", "gl_Color", NULL);
 * void callback (CoglMeshVertex *vert)
 * {
 *    GLfloat *pos = vert->attrib[0];
 *    GLubyte *color = vert->attrib[1];
 *    GLfloat *new_attrib = buf[vert->index];
 *    
 *    new_attrib = pos*color;
 * }
 *
 * TODO
 * Think about a higher level EditableMesh API for building/modifying mesh
 * objects. 
 * - E.g. look at Blender for inspiration here. They can build a mesh
 *   from "MVert", "MFace" and "MEdge" primitives. 
 * - It would be possible to bake an EditableMesh into a regular Mesh, and
 *   vica versa 
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib/gprintf.h>

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-mesh-private.h"

#define PAD_FOR_ALIGNMENT(VAR, TYPE_SIZE) \
  (VAR = TYPE_SIZE + ((VAR - 1) & ~(TYPE_SIZE - 1)))


/* 
 * GL/GLES compatability defines for VBO thingies:
 */

#if HAVE_COGL_GL

#define glGenBuffers ctx->pf_glGenBuffersARB
#define glBindBuffer ctx->pf_glBindBufferARB
#define glBufferData ctx->pf_glBufferDataARB
#define glBufferDataSub ctx->pf_glBufferDataSubARB
#define glDeleteBuffers ctx->pf_glDeleteBuffersARB
#define glMapBuffer ctx->pf_glMapBufferARB
#define glUnmapBuffer ctx->pf_glUnmapBufferARB
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER GL_ARRAY_BUFFER_ARB
#endif

#else

/* NB: GLES has had VBOs/GLSL since 1.1, so we don't need any defines in
 * this case except for glBufferSubData which, just for the fun of it, has a
 * different name:
 */
#define glBufferDataSub glBufferSubData

#endif

/* 
 * GL/GLES compatability defines for shader things:
 */

#ifdef HAVE_COGL_GL
#define glVertexAttribPointer ctx->pf_glVertexAttribPointerARB
#define glEnableVertexAttribArray ctx->pf_glEnableVertexAttribArrayARB
#define glDisableVertexAttribArray ctx->pf_glEnableVertexAttribArrayARB
#define MAY_HAVE_PROGRAMABLE_GL
#endif

#ifdef HAVE_COGL_GLES2
/* NB: GLES2 had shaders in core since day one so again we don't need
 * defines in this case: */
#define MAY_HAVE_PROGRAMABLE_GL
#endif

#ifndef HAVE_COGL_GL
/* GLES doesn't glDrawRangeElements, so we simply pretend it does
 * but that it makes no use of the start, end constraints: */
#define glDrawRangeElements(mode, start, end, count, type, indices) \
  glDrawElements (mode, count, type, indices)
#endif

static void _cogl_mesh_free (CoglMesh *mesh);

COGL_HANDLE_DEFINE (Mesh, mesh, mesh_handles);

/**
 * cogl_mesh_new:
 * @n_vertices: The number of vertices that will make up your mesh.
 *
 * This creates a Cogl handle for a new mesh that you can then start to add
 * attributes too.
 */
CoglHandle
cogl_mesh_new (guint n_vertices)
{
  CoglMesh *mesh = g_slice_alloc (sizeof (CoglMesh));

  mesh->ref_count = 1;
  COGL_HANDLE_DEBUG_NEW (mesh, mesh);
   
  mesh->n_vertices = n_vertices;
  
  mesh->submitted_vbos = NULL;
  mesh->new_attributes = NULL;

  /* return COGL_INVALID_HANDLE; */
  return _cogl_mesh_handle_new (mesh);
}

/* There are a number of standard OpenGL attributes that we deal with
 * specially. These attributes are all namespaced with a "gl_" prefix
 * so we should catch any typos instead of silently adding a custom
 * attribute.
 */
static CoglMeshAttributeFlags
validate_gl_attribute (const char *gl_attribute,
		       guint8 *n_components,
		       guint8 *texture_unit)
{
  CoglMeshAttributeFlags type;
  char *detail_seperator = NULL;
  int name_len;

  detail_seperator = strstr (gl_attribute, "::");
  if (detail_seperator)
    name_len = detail_seperator - gl_attribute;
  else
    name_len = strlen (gl_attribute);

  if (strncmp (gl_attribute, "Vertex", name_len) == 0)
    {
      type = COGL_MESH_ATTRIBUTE_FLAG_VERTEX_ARRAY;
    }
  else if (strncmp (gl_attribute, "Color", name_len) == 0)
    {
      type = COGL_MESH_ATTRIBUTE_FLAG_COLOR_ARRAY;
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
      type = COGL_MESH_ATTRIBUTE_FLAG_TEXTURE_COORD_ARRAY;
    }
  else if (strncmp (gl_attribute, "Normal", name_len) == 0)
    {
      *n_components = 1;
      type = COGL_MESH_ATTRIBUTE_FLAG_NORMAL_ARRAY;
    }
  else
    {
      g_warning ("Unknown gl_* attribute name gl_%s\n", gl_attribute);
      type = COGL_MESH_ATTRIBUTE_FLAG_INVALID;
    }

  return type;
}

/* This validates that a custom attribute name is a valid GLSL variable name
 *
 * NB: attribute names may have a detail component delimited using '::' E.g.
 * custom_attrib::foo or custom_atrib::bar
 *
 * maybe I should hang a compiled regex somewhere to handle this
 */
static gboolean
validate_custom_attribute_name (const char *attribute_name)
{
  char *detail_seperator = NULL;
  int name_len;
  const char *p;
  int i;

  detail_seperator = strstr (attribute_name, "::");
  if (detail_seperator)
    name_len = detail_seperator - attribute_name;
  else
    name_len = strlen (attribute_name);

  if (name_len == 0 || !g_ascii_isalpha (*p) || *p != '_')
    return FALSE;
  
  for (i = 1; i < name_len; i++)
    if (!g_ascii_isalnum (*p) || *p != '_')
      return FALSE;

  return TRUE;
}

/* Iterates the the CoglMeshVBOs of a mesh and create a flat list of all the
 * submitted attributes
 *
 * Note: The CoglMeshAttribute structs are deep copied.
 */
static GList *
copy_submitted_attributes_list (CoglMesh *mesh)
{
  GList *tmp;
  GList *submitted_attributes = NULL;

  for (tmp = mesh->submitted_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshVBO *cogl_vbo = tmp->data;
      GList *tmp2;
      
      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *attribute = tmp2->data;
	  CoglMeshAttribute *copy = g_slice_alloc (sizeof (CoglMeshAttribute));
	  *copy = *attribute;
	  submitted_attributes = g_list_prepend (submitted_attributes, copy);
	}
    }
  return submitted_attributes;
}

static CoglMeshAttributeFlags
get_attribute_gl_type_flag_from_gl_type (GLenum gl_type)
{
  switch (gl_type)
  {
    case GL_BYTE:
      return COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_BYTE;
    case GL_UNSIGNED_BYTE:
      return COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_UNSIGNED_BYTE;
    case GL_SHORT:
      return COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_SHORT;
    case GL_UNSIGNED_SHORT:
      return COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_UNSIGNED_SHORT;
    case GL_FLOAT:
      return COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_FLOAT;
#if HAVE_COGL_GL
    case GL_INT:
      return COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_INT;
    case GL_UNSIGNED_INT:
      return COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_UNSIGNED_INT;
    case GL_DOUBLE:
      return COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_DOUBLE;
#endif
    default:
      g_warning ("Mesh API: Unrecognised OpenGL type enum 0x%08x\n", gl_type);
      return 0;
  }
}

static gsize
get_gl_type_size (CoglMeshAttributeFlags flags)
{
  CoglMeshAttributeFlags gl_type =
    flags & COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_MASK;

  switch (gl_type)
    {
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_BYTE:
      return sizeof (GLbyte);
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_UNSIGNED_BYTE:
      return sizeof (GLubyte);
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_SHORT:
      return sizeof (GLshort);
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_UNSIGNED_SHORT:
      return sizeof (GLushort);
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_FLOAT:
      return sizeof (GLfloat);
#if HAVE_COGL_GL
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_INT:
      return sizeof (GLint);
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_UNSIGNED_INT:
      return sizeof (GLuint);
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_DOUBLE:
      return sizeof (GLdouble);
#endif
    default:
      g_warning ("Mesh API: Unrecognised OpenGL type enum 0x%08x\n", gl_type);
      return 0;
    }
}

void
cogl_mesh_add_attribute (CoglHandle handle,
		         const char *attribute_name,
			 guint8 n_components,
			 GLenum gl_type,
			 gboolean normalized,
			 guint16 stride,
			 const void *pointer)
{
  CoglMesh *mesh;
  GQuark name_quark = g_quark_from_string (attribute_name);
  gboolean modifying_an_attrib = FALSE;
  CoglMeshAttribute *attribute;
  CoglMeshAttributeFlags flags = 0;
  guint8 texture_unit = 0;
  GList *tmp;

  if (!cogl_is_mesh (handle))
    return;

  mesh = _cogl_mesh_pointer_from_handle (handle);

  /* The submit function works by diffing between submitted_attributes
   * and new_attributes to minimize the upload bandwidth + cost of
   * allocating new VBOs, so if there isn't already a list of new_attributes
   * we create one: */
  if (!mesh->new_attributes)
    mesh->new_attributes = copy_submitted_attributes_list (mesh);
  
  /* Note: we first look for an existing attribute that we are modifying
   * so we may skip needing to validate the name */
  for (tmp = mesh->new_attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshAttribute *submitted_attribute = tmp->data;
      if (submitted_attribute->name == name_quark)
	{
	  modifying_an_attrib = TRUE;

	  attribute = submitted_attribute;

	  /* since we will skip validate_gl_attribute in this case, we need
	   * to pluck out the attribute type before overwriting the flags: */
	  flags |= attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_TYPE_MASK;
	  break;
	}
    }

  if (!modifying_an_attrib)
    {
      /* Validate the attribute name, is suitable as a variable name */
      if (strncmp (attribute_name, "gl_", 3) == 0)
	{
	  flags |= validate_gl_attribute (attribute_name + 3,
					 &n_components,
					 &texture_unit);
	  if (flags & COGL_MESH_ATTRIBUTE_FLAG_INVALID)
	    return;
	}
      else
	{
	  flags |= COGL_MESH_ATTRIBUTE_FLAG_CUSTOM_ARRAY;
	  if (validate_custom_attribute_name (attribute_name))
	    return;
	}

      attribute = g_slice_alloc (sizeof (CoglMeshAttribute));
    }

  attribute->name = g_quark_from_string (attribute_name);
  attribute->n_components = n_components;
  attribute->stride = mesh->n_vertices > 1 ? stride : 0;
  attribute->u.pointer = pointer;
  attribute->texture_unit = texture_unit;

  flags |= get_attribute_gl_type_flag_from_gl_type (gl_type);
  flags |= COGL_MESH_ATTRIBUTE_FLAG_ENABLED;

  /* Note: We currently just assume, if an attribute is *ever* updated
   * then it should be taged as frequently changing. */
  if (modifying_an_attrib)
    flags |= COGL_MESH_ATTRIBUTE_FLAG_FREQUENT_RESUBMIT;
  else
    flags |= COGL_MESH_ATTRIBUTE_FLAG_INFREQUENT_RESUBMIT;

  if (normalized)
    flags |= COGL_MESH_ATTRIBUTE_FLAG_NORMALIZED;
  attribute->flags = flags;
  
  /* NB: get_gl_type_size must be called after setting the type
   * flags, above. */
  if (attribute->stride)
    attribute->span_bytes = mesh->n_vertices * attribute->stride;
  else
    attribute->span_bytes = mesh->n_vertices
			    * attribute->n_components
			    * get_gl_type_size (attribute->flags);

  if (!modifying_an_attrib)
    mesh->new_attributes =
      g_list_prepend (mesh->new_attributes, attribute);
}

void
cogl_mesh_delete_attribute (CoglHandle handle,
			    const char *attribute_name)
{
  CoglMesh *mesh;
  GQuark name = g_quark_from_string (attribute_name);
  GList *tmp;

  if (!cogl_is_mesh (handle))
    return;

  mesh = _cogl_mesh_pointer_from_handle (handle);

  /* The submit function works by diffing between submitted_attributes
   * and new_attributes to minimize the upload bandwidth + cost of
   * allocating new VBOs, so if there isn't already a list of new_attributes
   * we create one: */
  if (!mesh->new_attributes)
    mesh->new_attributes = copy_submitted_attributes_list (mesh);
  
  for (tmp = mesh->new_attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshAttribute *submitted_attribute = tmp->data;
      if (submitted_attribute->name == name)
	{
	  mesh->new_attributes =
	    g_list_delete_link (mesh->new_attributes, tmp);
	  g_slice_free (CoglMeshAttribute, submitted_attribute);
	  return;
	}
    }

  g_warning ("Failed to find an attribute named %s to delete\n",
	     attribute_name);
}

static void
set_attribute_enable (CoglHandle handle,
		      const char *attribute_name,
		      gboolean state)
{
  CoglMesh *mesh;
  GQuark name_quark = g_quark_from_string (attribute_name);
  GList *tmp;

  if (!cogl_is_mesh (handle))
    return;

  mesh = _cogl_mesh_pointer_from_handle (handle);
  
  /* NB: If a mesh is currently being edited, then there can be two seperate
   * lists of attributes; those that are currently submitted and a new
   * list yet to be submitted, we need to modify both. */

  for (tmp = mesh->new_attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshAttribute *attribute = tmp->data;
      if (attribute->name == name_quark)
	{
	  if (state)
	    attribute->flags |= COGL_MESH_ATTRIBUTE_FLAG_ENABLED;
	  else
	    attribute->flags &= ~COGL_MESH_ATTRIBUTE_FLAG_ENABLED;
	  break;
	}
    }
 
  for (tmp = mesh->submitted_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshVBO *cogl_vbo = tmp->data;
      GList *tmp2;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *attribute = tmp2->data;
	  if (attribute->name == name_quark)
	    {
	      if (state)
		attribute->flags |= COGL_MESH_ATTRIBUTE_FLAG_ENABLED;
	      else
		attribute->flags &= ~COGL_MESH_ATTRIBUTE_FLAG_ENABLED;
	      return;
	    }
	}
    }

  g_warning ("Failed to find an attribute named %s to %s\n",
	     attribute_name,
	     state == TRUE ? "enable" : "disable");
}

void
cogl_mesh_enable_attribute (CoglHandle handle,
			    const char *attribute_name)
{
  set_attribute_enable (handle, attribute_name, TRUE);
}

void
cogl_mesh_disable_attribute (CoglHandle handle,
			     const char *attribute_name)
{
  set_attribute_enable (handle, attribute_name, FALSE);
}

static void
free_mesh_attribute (CoglMeshAttribute *attribute)
{
  g_slice_free (CoglMeshAttribute, attribute);
}

/* Given an attribute that we know has already been submitted before, this
 * function looks for the existing VBO that contains it.
 *
 * Note: It will free redundant attribute struct once the corresponding
 * VBO has been found.
 */
static void
filter_already_submitted_attribute (CoglMeshAttribute *attribute,
				    GList **reuse_vbos,
				    GList **submitted_vbos)
{
  GList *tmp;

  /* First check the cogl_vbos we already know are being reused since we
   * are more likley to get a match here */
  for (tmp = *reuse_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshVBO *cogl_vbo = tmp->data;
      GList *tmp2;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *vbo_attribute = tmp2->data;

	  if (vbo_attribute->name == attribute->name)
	    {
	      vbo_attribute->flags &= ~COGL_MESH_ATTRIBUTE_FLAG_UNUSED;
	      /* Note: we don't free the redundant attribute here, since it
	       * will be freed after all filtering in cogl_mesh_submit */
	      return;
	    }
	}
    }
  
  for (tmp = *submitted_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshVBO *cogl_vbo = tmp->data;
      CoglMeshAttribute *reuse_attribute = NULL;
      GList *tmp2;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *vbo_attribute = tmp2->data;
	  if (vbo_attribute->name == attribute->name)
	    {
	      reuse_attribute = vbo_attribute;
	      /* Note: we don't free the redundant attribute here, since it
	       * will be freed after all filtering in cogl_mesh_submit */
	      
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
       * marked as UNUSED can be removed from the their cogl_vbo */
      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *vbo_attribute = tmp2->data;
	  if (vbo_attribute != reuse_attribute)  
	    vbo_attribute->flags |= COGL_MESH_ATTRIBUTE_FLAG_UNUSED;
	}

      return;
    }

  g_critical ("Failed to find the cogl vbo that corresponds to an\n"
	      "attribute that had apparently already been submitted!");
}

/* When we first mark a CoglMeshVBO to be reused, we mark the attributes
 * as unsed, so that when filtering of attributes into VBOs is done
 * we can then prune the now unsed attributes. */
static void
remove_unused_attributes (CoglMeshVBO *cogl_vbo)
{
  GList *tmp;
  GList *next;

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = next)
    {
      CoglMeshAttribute *attribute = tmp->data;
      next = tmp->next;

      if (attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_UNUSED)
	{
	  cogl_vbo->attributes =
	    g_list_delete_link (cogl_vbo->attributes, tmp);
	  g_slice_free (CoglMeshAttribute, attribute);
	}
    }
}

/* Give a newly added, strided, attribute, this function looks for a
 * CoglMeshVBO that the attribute is interleved with. If it can't find
 * one then a new CoglMeshVBO is allocated and added to the list of
 * new_strided_vbos
 */
static void
filter_strided_attribute (CoglMeshAttribute *attribute,
			  GList **new_vbos)
{
  GList *tmp;
  CoglMeshVBO *new_cogl_vbo;

  for (tmp = *new_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshVBO *cogl_vbo = tmp->data;
      GList *tmp2;
      
      if (!cogl_vbo->flags & COGL_MESH_VBO_FLAG_STRIDED)
	continue;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *vbo_attribute = tmp2->data;
	  const char *attribute_start = attribute->u.pointer;
	  const char *vbo_attribute_start = vbo_attribute->u.pointer;

	  /* NB: All attributes have mesh->n_vertices values which
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

	  if (attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_FREQUENT_RESUBMIT)
	    {
	      cogl_vbo->flags &= ~COGL_MESH_VBO_FLAG_INFREQUENT_RESUBMIT;
	      cogl_vbo->flags |= COGL_MESH_VBO_FLAG_FREQUENT_RESUBMIT;
	    }
	      return;
	}
    }
  new_cogl_vbo = g_slice_alloc (sizeof (CoglMeshVBO));
  new_cogl_vbo->vbo_name = 0;
  new_cogl_vbo->attributes = NULL;
  new_cogl_vbo->attributes =
    g_list_prepend (new_cogl_vbo->attributes, attribute);
  /* Any one of the interleved attributes will have the same span_bytes */
  new_cogl_vbo->vbo_bytes = attribute->span_bytes;
  new_cogl_vbo->flags = COGL_MESH_VBO_FLAG_STRIDED;
  
  if (attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_INFREQUENT_RESUBMIT)
    new_cogl_vbo->flags |= COGL_MESH_VBO_FLAG_INFREQUENT_RESUBMIT;
  else
    new_cogl_vbo->flags |= COGL_MESH_VBO_FLAG_FREQUENT_RESUBMIT;

  *new_vbos = g_list_prepend (*new_vbos, new_cogl_vbo);
  return;
}

/* This iterates through the list of submitted VBOs looking for one that
 * contains attribute. If found the list *link* is removed and returned */
static GList *
unlink_submitted_vbo_containing_attribute (GList **submitted_vbos,
					   CoglMeshAttribute *attribute)
{
  GList *tmp;
  GList *next = NULL;

  for (tmp = *submitted_vbos; tmp != NULL; tmp = next)
    {
      CoglMeshVBO *submitted_vbo = tmp->data;
      GList *tmp2;

      next = tmp->next;

      for (tmp2 = submitted_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *submitted_attribute = tmp2->data;

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
get_submitted_vbo_conflicts (GList **submitted_vbos, CoglMeshVBO *cogl_vbo)
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
disassociate_conflicting_attributes (CoglMeshVBO *conflict_vbo,
				     CoglMeshVBO *cogl_vbo)
{
  GList *tmp;
  
  /* NB: The attributes list in conflict_vbo will be shrinking so
   * we iterate those in the inner loop. */

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshAttribute *attribute = tmp->data;
      GList *tmp2;
      for (tmp2 = conflict_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *conflict_attribute = tmp2->data;

	  if (conflict_attribute->name == attribute->name)
	    {
	      free_mesh_attribute (conflict_attribute);
	      conflict_vbo->attributes =
		g_list_delete_link (conflict_vbo->attributes, tmp2);
	      break;
	    }
	}
    }
}

static void
free_cogl_mesh_vbo (CoglMeshVBO *cogl_vbo, gboolean delete_gl_vbo)
{
  GList *tmp;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      free_mesh_attribute (tmp->data);
    }
  g_list_free (cogl_vbo->attributes);

  if (delete_gl_vbo && cogl_vbo->flags & COGL_MESH_VBO_FLAG_SUBMITTED)
    GE (glDeleteBuffers (1, &cogl_vbo->vbo_name));

  g_slice_free (CoglMeshVBO, cogl_vbo);
}

/* This figures out the lowest attribute client pointer. (This pointer is used
 * to upload all the interleved attributes).
 *
 * In the process it also replaces the client pointer with the attributes
 * offset, and marks the attribute as submitted.
 */
static const void *
prep_strided_vbo_for_upload (CoglMeshVBO *cogl_vbo)
{
  GList *tmp;
  const char *lowest_pointer = NULL;

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshAttribute *attribute = tmp->data;
      const char *client_pointer = attribute->u.pointer;

      if (!lowest_pointer || client_pointer < lowest_pointer)
	lowest_pointer = client_pointer;
    }
  
  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshAttribute *attribute = tmp->data;
      const char *client_pointer = attribute->u.pointer;
      attribute->u.vbo_offset = client_pointer - lowest_pointer;
      attribute->flags |= COGL_MESH_ATTRIBUTE_FLAG_SUBMITTED;
    }

  return lowest_pointer;
}

static gboolean
upload_multipack_vbo_via_map_buffer (CoglMeshVBO *cogl_vbo)
{
#if HAVE_COGL_GL
  GList *tmp;
  guint offset = 0;
  char *buf;

  _COGL_GET_CONTEXT (ctx, FALSE);

  buf = glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
  glGetError();
  if (!buf)
    return FALSE;

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshAttribute *attribute = tmp->data;
      gsize attribute_size = attribute->span_bytes;
      gsize gl_type_size = get_gl_type_size (attribute->flags);

      PAD_FOR_ALIGNMENT (offset, gl_type_size);

      memcpy (buf + offset, attribute->u.pointer, attribute_size);

      attribute->u.vbo_offset = offset;
      attribute->flags |= COGL_MESH_ATTRIBUTE_FLAG_SUBMITTED;
      offset += attribute_size;
    }
  glUnmapBuffer (GL_ARRAY_BUFFER);

  return TRUE;
#else
  return FALSE;
#endif
}

static void
upload_multipack_vbo_via_buffer_sub_data (CoglMeshVBO *cogl_vbo)
{
  GList *tmp;
  guint offset = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (tmp = cogl_vbo->attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshAttribute *attribute = tmp->data;
      gsize attribute_size = attribute->span_bytes;
      gsize gl_type_size = get_gl_type_size (attribute->flags);

      PAD_FOR_ALIGNMENT (offset, gl_type_size);

      GE (glBufferDataSub (GL_ARRAY_BUFFER,
			   offset,
			   attribute_size,
			   attribute->u.pointer));
      attribute->u.vbo_offset = offset;
      attribute->flags |= COGL_MESH_ATTRIBUTE_FLAG_SUBMITTED;
      offset += attribute_size;
    }
}

static void
upload_gl_vbo (CoglMeshVBO *cogl_vbo)
{
  GLenum usage;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_vbo->vbo_name != 0);

  if (cogl_vbo->flags & COGL_MESH_VBO_FLAG_FREQUENT_RESUBMIT)
    usage = GL_DYNAMIC_DRAW;
  else
    usage = GL_STATIC_DRAW;

  GE (glBindBuffer (GL_ARRAY_BUFFER, cogl_vbo->vbo_name));
  
  if (cogl_vbo->flags & COGL_MESH_VBO_FLAG_STRIDED)
    {
      const void *pointer =
	prep_strided_vbo_for_upload (cogl_vbo);
      GE (glBufferData (GL_ARRAY_BUFFER,
			cogl_vbo->vbo_bytes,
			pointer,
			usage));
    }
  else if (cogl_vbo->flags & COGL_MESH_VBO_FLAG_MULTIPACK)
    {
      /* First we make it obvious to the driver that we want to update the
       * whole buffer (without this, the driver is more likley to block
       * if the GPU is busy using the buffer) */
      GE (glBufferData (GL_ARRAY_BUFFER,
			cogl_vbo->vbo_bytes,
			NULL,
			usage));
      
      /* I think it might depend on the specific driver/HW whether its better to
       * use glMapBuffer here or glBufferSubData here. There is even a good
       * thread about this topic here:
       * http://www.mail-archive.com/dri-devel@lists.sourceforge.net/msg35004.html
       * For now I have gone with glMapBuffer, but the jury is still out.
       */

      if (!upload_multipack_vbo_via_map_buffer (cogl_vbo))
	upload_multipack_vbo_via_buffer_sub_data  (cogl_vbo);
    }
  else
    {
      CoglMeshAttribute *attribute = cogl_vbo->attributes->data;
      GE (glBufferData (GL_ARRAY_BUFFER,
			cogl_vbo->vbo_bytes,
			attribute->u.pointer,
			usage));
      /* We forget this pointer now since the client will be free
       * to re-use this memory */
      attribute->u.pointer = NULL;
      attribute->flags |= COGL_MESH_ATTRIBUTE_FLAG_SUBMITTED;
    }

  cogl_vbo->flags |= COGL_MESH_VBO_FLAG_SUBMITTED;

  GE (glBindBuffer (GL_ARRAY_BUFFER, 0));
}

/* Note: although there ends up being quite a few inner loops involved
 * with resolving buffers, the number of attributes will be low so I
 * don't expect them to cause a problem. */
static void
resolve_new_cogl_mesh_vbo (CoglMesh *mesh,
			   CoglMeshVBO *new_cogl_vbo,
			   GList **final_vbos)
{
  GList *conflicts;
  GList *tmp;
  GList *next;
  gboolean found_target_vbo = FALSE;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  conflicts =
    get_submitted_vbo_conflicts (&mesh->submitted_vbos, new_cogl_vbo);

  for (tmp = conflicts; tmp != NULL; tmp = next)
    {
      CoglMeshVBO *conflict_vbo = tmp->data;

      next = tmp->next;
      
      disassociate_conflicting_attributes (conflict_vbo, new_cogl_vbo);

      if (!conflict_vbo->attributes)
	{
	  /* See if we can re-use this now empty VBO: */

	  if (!found_target_vbo
	      && conflict_vbo->vbo_bytes == new_cogl_vbo->vbo_bytes)
	    {
	      found_target_vbo = TRUE;
	      new_cogl_vbo->vbo_name = conflict_vbo->vbo_name;
	      free_cogl_mesh_vbo (conflict_vbo, FALSE);
	      
	      upload_gl_vbo (new_cogl_vbo);

	      *final_vbos = g_list_prepend (*final_vbos, new_cogl_vbo);
	    }
	  else
	    free_cogl_mesh_vbo (conflict_vbo, TRUE);
	}
      else
	{
	  /* Relink the VBO back into mesh->submitted_vbos since it may
	   * be involved in other conflicts later */
	  tmp->next = mesh->submitted_vbos;
	  tmp->prev = NULL;
	  mesh->submitted_vbos = tmp;
	}
    }

  if (!found_target_vbo)
    {
      GE (glGenBuffers (1, &new_cogl_vbo->vbo_name));
      /* FIXME: debug */
      g_assert (glGetError() == GL_NO_ERROR);
	
      upload_gl_vbo (new_cogl_vbo);
      *final_vbos = g_list_prepend (*final_vbos, new_cogl_vbo);
    }
}

/**
 * cogl_mesh_submit:
 * @handle: A Cogl mesh handle
 * 
 * This function copies all the user added attributes into a buffer object
 * managed by the OpenGL driver.
 *
 * After the attributes have been submitted, then you may no longer add or
 * remove attributes from a mesh, though you can enable or disable them.
 */
void
cogl_mesh_submit (CoglHandle handle)
{
  CoglMesh *mesh;
  GList *tmp;
  CoglMeshVBO *new_multipack_vbo;
  GList *new_multipack_vbo_link;
  GList *new_vbos = NULL;
  GList *reuse_vbos = NULL;
  GList *final_vbos = NULL;
  
  if (!cogl_is_mesh (handle))
    return;
  
  mesh = _cogl_mesh_pointer_from_handle (handle);
  
  /* The objective now is to copy the attribute data supplied by the client
   * into buffer objects, but it's important to minimize the amount of memory
   * bandwidth we waste here.
   *
   * We need to group together the attributes that are interleved so that the
   * driver can use a single continguous memcpy for these. All BOs for
   * interleved data are created as STATIC_DRAW_ARB.
   *
   * Non interleved attributes tagged as INFREQUENT_RESUBMIT will be grouped
   * together back to back in a single BO created as STATIC_DRAW_ARB
   *
   * Non interleved attributes tagged as FREQUENT_RESUBMIT will be copied into
   * individual buffer objects, and the BO itself created DYNAMIC_DRAW_ARB
   *
   * If we are modifying an submitted mesh object then we are carefull not
   * to needlesly delete submitted buffer objects and replace with new ones,
   * instead we upload new data to the submitted buffers.
   */
  
  /* NB: We must forget attribute->pointer after submitting since the user
   * is free to re-use that memory for other purposes now. */

  /* Pseudo code:
   * 
   * Broadly speaking we start with a list of unsorted attributes, and filter
   * those into 'new' and 're-use' CoglMeshVBO (CBO) lists. We then take the
   * list of new CBO structs and compare with the CBOs that have already been
   * submitted to the GPU (but ignoring those we already know will be re-used)
   * to determine what other CBOs can be re-used, due to being superseded, 
   * and what new GL VBOs need to be created.
   * 
   * We have three kinds of CBOs:
   * - Unstrided CBOs
   *    These contain a single tightly packed attribute
   *    These are currently the only ones ever marked as FREQUENT_SUBMIT
   * - Strided CBOs
   *	 These typically contain multiple interleved sets of attributes,
   *	 though they can contain just one attribute with a stride
   * - Multi Pack CBOs
   *     These contain multiple attributes tightly packed back to back)
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
   *     create a new-CBOs entry tagged UNSTRIDED + FREQUENT_RESUBMIT
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
   * deleted from the mesh.
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
  
  new_multipack_vbo = g_slice_alloc (sizeof (CoglMeshVBO));
  new_multipack_vbo->vbo_name = 0;
  new_multipack_vbo->flags = COGL_MESH_VBO_FLAG_MULTIPACK
			     | COGL_MESH_VBO_FLAG_INFREQUENT_RESUBMIT;
  new_multipack_vbo->vbo_bytes = 0;
  new_multipack_vbo->attributes = NULL;
  new_vbos = g_list_prepend (new_vbos, new_multipack_vbo);
  /* We save the link pointer here, just so we can do a fast removal later if
   * no attributes get added to this vbo. */
  new_multipack_vbo_link = new_vbos;

  /* Start with a list of unsorted attributes, and filter those into
   * potential new Cogl BO structs
   */
  for (tmp = mesh->new_attributes; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshAttribute *attribute = tmp->data;
      
      if (attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_SUBMITTED)
	{
	  /* If the attribute is already marked as submitted, then we need
	   * to find the existing VBO that contains it so we dont delete it.
	   *
	   * NB: this also frees the attribute struct since it's implicitly
	   * redundant in this case.
	   */
	  filter_already_submitted_attribute (attribute,
					      &reuse_vbos,
					      &mesh->submitted_vbos);
	}
      else if (attribute->stride)
	{
	  /* look for a CoglMeshVBO that the attribute is interleved with. If
	   * one can't be found then a new CoglMeshVBO is allocated and added
	   * to the list of new_vbos: */
	  filter_strided_attribute (attribute, &new_vbos);
	}
      else if (attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_FREQUENT_RESUBMIT)
	{
	  CoglMeshVBO *cogl_vbo = g_slice_alloc (sizeof (CoglMeshVBO));
	  
	  /* attributes we expect will be frequently resubmitted are placed
	   * in their own VBO so that updates don't impact other attributes
	   */

	  cogl_vbo->vbo_name = 0;
	  cogl_vbo->flags = COGL_MESH_VBO_FLAG_UNSTRIDED
			    | COGL_MESH_VBO_FLAG_FREQUENT_RESUBMIT;
	  cogl_vbo->attributes = NULL;
	  cogl_vbo->attributes = g_list_prepend (cogl_vbo->attributes,
						 attribute);
	  cogl_vbo->vbo_bytes = attribute->span_bytes;
	  new_vbos = g_list_prepend (new_vbos, cogl_vbo);
	}
      else
	{
	  gsize gl_type_size = get_gl_type_size (attribute->flags);

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

	  PAD_FOR_ALIGNMENT (new_multipack_vbo->vbo_bytes, gl_type_size);

	  new_multipack_vbo->vbo_bytes += attribute->span_bytes;
	}
    }

  /* At this point all mesh->new_attributes have been filtered into
   * CoglMeshVBOs... */
  g_list_free (mesh->new_attributes);
  mesh->new_attributes = NULL;
  
  /* If the multipack vbo wasn't needed: */
  if (new_multipack_vbo->attributes == NULL)
    {
      new_vbos = g_list_delete_link (new_vbos, new_multipack_vbo_link);
      g_slice_free (CoglMeshVBO, new_multipack_vbo);
    }

  for (tmp = reuse_vbos; tmp != NULL; tmp = tmp->next)
    remove_unused_attributes (tmp->data);
  final_vbos = g_list_concat (final_vbos, reuse_vbos);

  for (tmp = new_vbos; tmp != NULL; tmp = tmp->next)
    resolve_new_cogl_mesh_vbo (mesh, tmp->data, &final_vbos);
  
  /* Anything left corresponds to deleted attributes: */
  for (tmp = mesh->submitted_vbos; tmp != NULL; tmp = tmp->next)
    free_cogl_mesh_vbo (tmp->data, TRUE);
  g_list_free (mesh->submitted_vbos);

  mesh->submitted_vbos = final_vbos;
}

static GLenum
get_gl_type_from_attribute_flags (CoglMeshAttributeFlags flags)
{
  CoglMeshAttributeFlags gl_type =
    flags & COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_MASK;

  switch (gl_type)
    {
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_BYTE:
      return GL_BYTE;
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_UNSIGNED_BYTE:
      return GL_UNSIGNED_BYTE;
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_SHORT:
      return GL_SHORT;
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_UNSIGNED_SHORT:
      return GL_UNSIGNED_SHORT;
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_FLOAT:
      return GL_FLOAT;
#if HAVE_COGL_GL
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_INT:
      return GL_INT;
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_UNSIGNED_INT:
      return GL_UNSIGNED_INT;
    case COGL_MESH_ATTRIBUTE_FLAG_GL_TYPE_DOUBLE:
      return GL_DOUBLE;
#endif
    default:
      g_warning ("Couldn't convert from attribute flags (0x%08x) "
		 "to gl type enum\n", flags);
      return 0;
    }
}

static void
enable_state_for_drawing_mesh (CoglMesh *mesh)
{
  GList *tmp;
  GLenum gl_type;
  GLuint generic_index = 0;
  gulong enable_flags = COGL_ENABLE_BLEND;
  /* FIXME: I don't think it's appropriate to force enable
   * GL_BLEND here. */

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (tmp = mesh->submitted_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshVBO *cogl_vbo = tmp->data;
      GList *tmp2;

      GE (glBindBuffer (GL_ARRAY_BUFFER, cogl_vbo->vbo_name));

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *attribute = tmp2->data;
	  CoglMeshAttributeFlags type =
	    attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_TYPE_MASK;
	  
	  if (!(attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_ENABLED))
	    continue;
	  
	  gl_type = get_gl_type_from_attribute_flags (attribute->flags);
	  switch (type)
	    {
	    case COGL_MESH_ATTRIBUTE_FLAG_COLOR_ARRAY:
	      /* FIXME: go through cogl cache to enable color array */
	      GE (glEnableClientState (GL_COLOR_ARRAY));
	      GE (glColorPointer (attribute->n_components,
				  gl_type,
				  attribute->stride,
				  (const GLvoid *)attribute->u.vbo_offset));
	      break;
	    case COGL_MESH_ATTRIBUTE_FLAG_NORMAL_ARRAY:
	      /* FIXME: go through cogl cache to enable normal array */
	      GE (glEnableClientState (GL_NORMAL_ARRAY));
	      GE (glNormalPointer (gl_type,
				   attribute->stride,
				   (const GLvoid *)attribute->u.vbo_offset));
	      break;
	    case COGL_MESH_ATTRIBUTE_FLAG_TEXTURE_COORD_ARRAY:
	      /* FIXME: set the active texture unit */
	      /* NB: Cogl currently manages unit 0 */
	      enable_flags |= (COGL_ENABLE_TEXCOORD_ARRAY
			       | COGL_ENABLE_TEXTURE_2D);
	      /* FIXME: I don't think it's appropriate to force enable
	       * GL_TEXTURE_2D here. */
	      /* GE (glEnableClientState (GL_VERTEX_ARRAY)); */
	      GE (glTexCoordPointer (attribute->n_components,
				     gl_type,
				     attribute->stride,
				     (const GLvoid *)attribute->u.vbo_offset));
	      break;
	    case COGL_MESH_ATTRIBUTE_FLAG_VERTEX_ARRAY:
	      enable_flags |= COGL_ENABLE_VERTEX_ARRAY;
	      /* GE (glEnableClientState (GL_VERTEX_ARRAY)); */
	      GE (glVertexPointer (attribute->n_components,
				   gl_type,
				   attribute->stride,
				   (const GLvoid *)attribute->u.vbo_offset));
	      break;
	    case COGL_MESH_ATTRIBUTE_FLAG_CUSTOM_ARRAY:
	      {
#ifdef MAY_HAVE_PROGRAMABLE_GL
		GLboolean normalized = GL_FALSE;
		if (attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_NORMALIZED)
		  normalized = GL_TRUE;
		/* FIXME: go through cogl cache to enable generic array */
		GE (glEnableVertexAttribArray (generic_index++));
		GE (glVertexAttribPointer (generic_index, 
					   attribute->n_components,
					   gl_type,
					   normalized,
					   attribute->stride,
					   (const GLvoid *)
					    attribute->u.vbo_offset));
#endif
	      }
	      break;
	    default:
	      g_warning ("Unrecognised attribute type 0x%08x", type);
	    }
	}
    }
  
  cogl_enable (enable_flags);

}

static void
disable_state_for_drawing_mesh (CoglMesh *mesh)
{
  GList *tmp;
  GLenum gl_type;
  GLuint generic_index = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Disable all the client state that cogl doesn't currently know
   * about:
   */
  GE (glBindBuffer (GL_ARRAY_BUFFER, 0));

  generic_index = 0;
  for (tmp = mesh->submitted_vbos; tmp != NULL; tmp = tmp->next)
    {
      CoglMeshVBO *cogl_vbo = tmp->data;
      GList *tmp2;

      for (tmp2 = cogl_vbo->attributes; tmp2 != NULL; tmp2 = tmp2->next)
	{
	  CoglMeshAttribute *attribute = tmp2->data;
	  CoglMeshAttributeFlags type =
	    attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_TYPE_MASK;

	  if (!(attribute->flags & COGL_MESH_ATTRIBUTE_FLAG_ENABLED))
	    continue;

	  gl_type = get_gl_type_from_attribute_flags(attribute->flags);
	  switch (type)
	    {
	    case COGL_MESH_ATTRIBUTE_FLAG_COLOR_ARRAY:
	      /* FIXME: go through cogl cache to enable color array */
	      GE (glDisableClientState (GL_COLOR_ARRAY));
	      break;
	    case COGL_MESH_ATTRIBUTE_FLAG_NORMAL_ARRAY:
	      /* FIXME: go through cogl cache to enable normal array */
	      GE (glDisableClientState (GL_NORMAL_ARRAY));
	      break;
	    case COGL_MESH_ATTRIBUTE_FLAG_TEXTURE_COORD_ARRAY:
	      /* FIXME: set the active texture unit */
	      /* NB: Cogl currently manages unit 0 */
	      /* GE (glDisableClientState (GL_VERTEX_ARRAY)); */
	      break;
	    case COGL_MESH_ATTRIBUTE_FLAG_VERTEX_ARRAY:
	      /* GE (glDisableClientState (GL_VERTEX_ARRAY)); */
	      break;
	    case COGL_MESH_ATTRIBUTE_FLAG_CUSTOM_ARRAY:
#ifdef MAY_HAVE_PROGRAMABLE_GL
	      /* FIXME: go through cogl cache to enable generic array */
	      GE (glDisableVertexAttribArray (generic_index++));
#endif
	      break;
	    default:
	      g_warning ("Unrecognised attribute type 0x%08x", type);
	    }
	}
    }
}

void
cogl_mesh_draw_arrays (CoglHandle handle,
		       GLenum mode,
		       GLint first,
		       GLsizei count)
{
  CoglMesh *mesh;
  
  if (!cogl_is_mesh (handle))
    return;
  
  mesh = _cogl_mesh_pointer_from_handle (handle);
  
  enable_state_for_drawing_mesh (mesh);

  /* FIXME: flush cogl cache */
  GE (glDrawArrays (mode, first, count));
  
  disable_state_for_drawing_mesh (mesh);
}

void
cogl_mesh_draw_range_elements (CoglHandle handle,
			       GLenum mode,
			       GLuint start,
			       GLuint end,
			       GLsizei count,
			       GLenum type,
			       const GLvoid *indices)
{
  CoglMesh *mesh;
  
  if (!cogl_is_mesh (handle))
    return;
  
  mesh = _cogl_mesh_pointer_from_handle (handle);
  
  enable_state_for_drawing_mesh (mesh);

  /* FIXME: flush cogl cache */
  GE (glDrawRangeElements (mode, start, end, count, type, indices));

  disable_state_for_drawing_mesh (mesh);
}

static void
_cogl_mesh_free (CoglMesh *mesh)
{
  GList *tmp;

  for (tmp = mesh->submitted_vbos; tmp != NULL; tmp = tmp->next)
    free_cogl_mesh_vbo (tmp->data, TRUE);
  for (tmp = mesh->new_attributes; tmp != NULL; tmp = tmp->next)
    free_mesh_attribute (tmp->data);

  g_slice_free (CoglMesh, mesh);
}

