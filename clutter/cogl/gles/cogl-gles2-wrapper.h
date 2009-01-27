/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __COGL_GLES2_WRAPPER_H__
#define __COGL_GLES2_WRAPPER_H__

#include "cogl-internal.h"

G_BEGIN_DECLS

#ifdef HAVE_COGL_GLES2

typedef struct _CoglGles2Wrapper	  CoglGles2Wrapper;
typedef struct _CoglGles2WrapperTextureUnit
					  CoglGles2WrapperTextureUnit;

typedef struct _CoglGles2WrapperAttributes  CoglGles2WrapperAttributes;
typedef struct _CoglGles2WrapperUniforms  CoglGles2WrapperUniforms;
typedef struct _CoglGles2WrapperTextureUnitSettings
					  CoglGles2WrapperTextureUnitSettings;
typedef struct _CoglGles2WrapperSettings  CoglGles2WrapperSettings;
typedef struct _CoglGles2WrapperProgram	  CoglGles2WrapperProgram;
typedef struct _CoglGles2WrapperShader	  CoglGles2WrapperShader;

#define COGL_GLES2_NUM_CUSTOM_UNIFORMS    16
#define COGL_GLES2_UNBOUND_CUSTOM_UNIFORM -2

/* Must be a power of two */
#define COGL_GLES2_MODELVIEW_STACK_SIZE   32
#define COGL_GLES2_PROJECTION_STACK_SIZE  2
#define COGL_GLES2_TEXTURE_STACK_SIZE     2

/* Dirty flags for shader uniforms */
enum
  {
    COGL_GLES2_DIRTY_MVP_MATRIX       = 1 << 0,
    COGL_GLES2_DIRTY_MODELVIEW_MATRIX = 1 << 1,
    COGL_GLES2_DIRTY_TEXTURE_MATRICES = 1 << 2,
    COGL_GLES2_DIRTY_FOG_DENSITY      = 1 << 3,
    COGL_GLES2_DIRTY_FOG_START        = 1 << 4,
    COGL_GLES2_DIRTY_FOG_END          = 1 << 5,
    COGL_GLES2_DIRTY_FOG_COLOR        = 1 << 6,
    COGL_GLES2_DIRTY_ALPHA_TEST_REF   = 1 << 7,
    COGL_GLES2_DIRTY_TEXTURE_UNITS    = 1 << 8,

    COGL_GLES2_DIRTY_ALL              = (1 << 9) - 1
  };

/* Dirty flags for shader vertex attribute pointers */
enum
  {
    COGL_GLES2_DIRTY_TEX_COORD_VERTEX_ATTRIB  = 1 << 0
  };

/* Dirty flags for shader vertex attributes enabled status */
enum
  {
    COGL_GLES2_DIRTY_TEX_COORD_ATTRIB_ENABLES = 1 << 0
  };

struct _CoglGles2WrapperAttributes
{
  GArray    *multi_texture_coords;
};

struct _CoglGles2WrapperUniforms
{
  GLint      mvp_matrix_uniform;
  GLint      modelview_matrix_uniform;
  GArray    *texture_matrix_uniforms;

  GArray    *texture_sampler_uniforms;

  GLint      fog_density_uniform;
  GLint      fog_start_uniform;
  GLint      fog_end_uniform;
  GLint      fog_color_uniform;

  GLint      alpha_test_ref_uniform;

  GLint     texture_unit_uniform;
};

struct _CoglGles2WrapperTextureUnitSettings
{
  guint	enabled:1;
  guint alpha_only:1;
  /* TODO: blending state */
};

/* NB: We get a copy of this for each fragment/vertex
 * program varient we generate so we try to keep it
 * fairly lean */
struct _CoglGles2WrapperSettings
{
  CoglGles2WrapperTextureUnitSettings *texture_units;
  guint	   n_texture_units;

  GLint    alpha_test_func;
  GLint    fog_mode;

  /* The current in-use user program */
  CoglHandle user_program;

  guint	   alpha_test_enabled:1;
  guint	   fog_enabled:1;
};

struct _CoglGles2WrapperTextureUnit
{
  GLfloat  texture_stack[COGL_GLES2_TEXTURE_STACK_SIZE * 16];
  GLuint   texture_stack_pos;

  GLenum       texture_coords_type;
  GLint	       texture_coords_size;
  GLsizei      texture_coords_stride;
  const void  *texture_coords_pointer;

  guint	   texture_coords_enabled:1;
  guint	   dirty_matrix:1; /*!< shader uniform needs updating */
};

struct _CoglGles2Wrapper
{
  GLuint     matrix_mode;
  GLfloat    modelview_stack[COGL_GLES2_MODELVIEW_STACK_SIZE * 16];
  GLuint     modelview_stack_pos;
  GLfloat    projection_stack[COGL_GLES2_PROJECTION_STACK_SIZE * 16];
  GLuint     projection_stack_pos;
  GArray    *texture_units;
  guint	     active_texture_unit;
  guint	     active_client_texture_unit;

  /* The combined modelview and projection matrix is only updated at
     the last minute in glDrawArrays to avoid recalculating it for
     every change to the modelview matrix */
  GLboolean  mvp_uptodate;

  /* The currently bound program */
  CoglGles2WrapperProgram *current_program;

  /* The current settings. Effectively these represent anything that
   * will require a modified fixed function shader */
  CoglGles2WrapperSettings settings;
  /* Whether the settings have changed since the last draw */
  gboolean settings_dirty;
  /* Uniforms that have changed since the last draw */
  int dirty_uniforms, dirty_custom_uniforms;

  /* Attribute pointers that have changed since the last draw */
  int dirty_attribute_pointers;

  /* Vertex attribute pointer enables that have changed since the last draw */
  int dirty_vertex_attrib_enables;

  /* List of all compiled program combinations */
  GSList *compiled_programs;

  /* List of all compiled vertex shaders */
  GSList *compiled_vertex_shaders;

  /* List of all compiled fragment shaders */
  GSList *compiled_fragment_shaders;

  /* Values for the uniforms */
  GLfloat alpha_test_ref;
  GLfloat fog_density;
  GLfloat fog_start;
  GLfloat fog_end;
  GLfloat fog_color[4];
  CoglBoxedValue custom_uniforms[COGL_GLES2_NUM_CUSTOM_UNIFORMS];
};

struct _CoglGles2WrapperProgram
{
  GLuint    program;

  /* The settings that were used to generate this combination */
  CoglGles2WrapperSettings settings;

  /* The attributes for this program that are not bound up-front
   * with constant indices */
  CoglGles2WrapperAttributes attributes;

  /* The uniforms for this program */
  CoglGles2WrapperUniforms uniforms;
  GLint custom_uniforms[COGL_GLES2_NUM_CUSTOM_UNIFORMS];
};

struct _CoglGles2WrapperShader
{
  GLuint shader;

  /* The settings that were used to generate this shader */
  CoglGles2WrapperSettings settings;
};

/* These defines are missing from GL ES 2 but we can still use them
   with the wrapper functions */

#ifndef GL_MODELVIEW

#define GL_MODELVIEW           0x1700
#define GL_PROJECTION          0x1701

#define GL_VERTEX_ARRAY        0x8074
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_COLOR_ARRAY         0x8076
#define GL_NORMAL_ARRAY        0x8075

#define GL_LIGHTING            0x0B50
#define GL_ALPHA_TEST          0x0BC0

#define GL_FOG                 0x0B60
#define GL_FOG_COLOR           0x0B66
#define GL_FOG_MODE            0x0B65
#define GL_FOG_HINT            0x0C54
#define GL_FOG_DENSITY         0x0B62
#define GL_FOG_START           0x0B63
#define GL_FOG_END             0x0B64

#define GL_CLIP_PLANE0         0x3000
#define GL_CLIP_PLANE1         0x3001
#define GL_CLIP_PLANE2         0x3002
#define GL_CLIP_PLANE3         0x3003
#define GL_MAX_CLIP_PLANES     0x0D32

#define GL_MODELVIEW_MATRIX    0x0BA6
#define GL_PROJECTION_MATRIX   0x0BA7

#define GL_GENERATE_MIPMAP     0x8191

#define GL_TEXTURE_ENV         0x2300
#define GL_TEXTURE_ENV_MODE    0x2200
#define GL_MODULATE            0x2100

#define GL_EXP                 0x8000
#define GL_EXP2                0x8001

#endif /* GL_MODELVIEW */

void cogl_gles2_wrapper_init (CoglGles2Wrapper *wrapper);
void cogl_gles2_wrapper_deinit (CoglGles2Wrapper *wrapper);

void cogl_wrap_glPushMatrix ();
void cogl_wrap_glPopMatrix ();
void cogl_wrap_glMatrixMode (GLenum mode);
void cogl_wrap_glLoadIdentity ();
void cogl_wrap_glMultMatrixf (const GLfloat *m);
void cogl_wrap_glFrustumf (GLfloat left, GLfloat right,
			   GLfloat bottom, GLfloat top,
			   GLfloat z_near, GLfloat z_far);
void cogl_wrap_glScalef (GLfloat x, GLfloat y, GLfloat z);
void cogl_wrap_glTranslatef (GLfloat x, GLfloat y, GLfloat z);
void cogl_wrap_glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void cogl_wrap_glOrthof (GLfloat left, GLfloat right,
			 GLfloat bottom, GLfloat top,
			 GLfloat near, GLfloat far);

void cogl_wrap_glEnable (GLenum cap);
void cogl_wrap_glDisable (GLenum cap);

void cogl_wrap_glTexCoordPointer (GLint size, GLenum type, GLsizei stride,
				  const GLvoid *pointer);
void cogl_wrap_glVertexPointer (GLint size, GLenum type, GLsizei stride,
				const GLvoid *pointer);
void cogl_wrap_glColorPointer (GLint size, GLenum type, GLsizei stride,
			       const GLvoid *pointer);
void cogl_wrap_glNormalPointer (GLenum type, GLsizei stride,
				const GLvoid *pointer);

void cogl_wrap_glTexEnvf (GLenum target, GLenum pname, GLfloat param);

void cogl_wrap_glClientActiveTexture (GLenum texture);
void cogl_wrap_glActiveTexture (GLenum texture);

void cogl_wrap_glEnableClientState (GLenum array);
void cogl_wrap_glDisableClientState (GLenum array);

void cogl_wrap_glAlphaFunc (GLenum func, GLclampf ref);

void cogl_wrap_glColor4f (GLclampf r, GLclampf g, GLclampf b, GLclampf a);

void cogl_wrap_glClipPlanef (GLenum plane, GLfloat *equation);

void cogl_wrap_glGetIntegerv (GLenum pname, GLint *params);
void cogl_wrap_glGetFloatv (GLenum pname, GLfloat *params);

void cogl_wrap_glFogf (GLenum pname, GLfloat param);
void cogl_wrap_glFogfv (GLenum pname, const GLfloat *params);

void cogl_wrap_glDrawArrays (GLenum mode, GLint first, GLsizei count);
void cogl_wrap_glDrawElements (GLenum mode, GLsizei count, GLenum type,
                               const GLvoid *indices);
void cogl_wrap_glTexParameteri (GLenum target, GLenum pname, GLfloat param);

void cogl_gles2_wrapper_bind_texture (GLenum target, GLuint texture,
				      GLenum internal_format);

/* This function is only available on GLES 2 */
#define cogl_wrap_glGenerateMipmap glGenerateMipmap

void _cogl_gles2_clear_cache_for_program (CoglHandle program);

#else /* HAVE_COGL_GLES2 */

/* If we're not using GL ES 2 then just use the GL functions
   directly */

#define cogl_wrap_glDrawArrays         glDrawArrays
#define cogl_wrap_glDrawElements       glDrawElements
#define cogl_wrap_glPushMatrix         glPushMatrix
#define cogl_wrap_glPopMatrix          glPopMatrix
#define cogl_wrap_glMatrixMode         glMatrixMode
#define cogl_wrap_glLoadIdentity       glLoadIdentity
#define cogl_wrap_glMultMatrixf        glMultMatrixf
#define cogl_wrap_glFrustumf           glFrustumf
#define cogl_wrap_glScalef             glScalef
#define cogl_wrap_glTranslatef         glTranslatef
#define cogl_wrap_glRotatef            glRotatef
#define cogl_wrap_glOrthof             glOrthof
#define cogl_wrap_glEnable             glEnable
#define cogl_wrap_glDisable            glDisable
#define cogl_wrap_glTexCoordPointer    glTexCoordPointer
#define cogl_wrap_glVertexPointer      glVertexPointer
#define cogl_wrap_glColorPointer       glColorPointer
#define cogl_wrap_glNormalPointer      glNormalPointer
#define cogl_wrap_glTexEnvf            glTexEnvf
#define cogl_wrap_glActiveTexture      glActiveTexture
#define cogl_wrap_glEnableClientState  glEnableClientState
#define cogl_wrap_glDisableClientState glDisableClientState
#define cogl_wrap_glAlphaFunc          glAlphaFunc
#define cogl_wrap_glColor4f            glColor4f
#define cogl_wrap_glClipPlanef         glClipPlanef
#define cogl_wrap_glGetIntegerv        glGetIntegerv
#define cogl_wrap_glGetFloatv          glGetFloatv
#define cogl_wrap_glFogf               glFogf
#define cogl_wrap_glFogfv              glFogfv
#define cogl_wrap_glTexParameteri      glTexParameteri

/* The extra third parameter of the bind texture wrapper isn't needed
   so we can just directly call glBindTexture */
#define cogl_gles2_wrapper_bind_texture(target, texture, internal_format) \
  glBindTexture ((target), (texture))

/* COGL uses the automatic mipmap generation for GLES 1 so
   glGenerateMipmap doesn't need to do anything */
#define cogl_wrap_glGenerateMipmap(x) ((void) 0)

/* GLES doesn't have glDrawRangeElements, so we simply pretend it does
 * but that it makes no use of the start, end constraints: */
#define glDrawRangeElements(mode, start, end, count, type, indices) \
  glDrawElements (mode, count, type, indices)

#endif /* HAVE_COGL_GLES2 */

G_END_DECLS

#endif /* __COGL_GLES2_WRAPPER_H__ */
