#ifndef __COGL_MATERIAL_PRIVATE_H
#define __COGL_MATERIAL_PRIVATE_H

#include "cogl-material.h"
#include "cogl-matrix.h"

#include <glib.h>

typedef struct _CoglMaterial	      CoglMaterial;
typedef struct _CoglMaterialLayer     CoglMaterialLayer;

/* XXX: I don't think gtk-doc supports having private enums so these aren't
 * bundled in with CoglMaterialLayerFlags */
typedef enum _CoglMaterialLayerPrivFlags
{
  /* Ref: CoglMaterialLayerFlags
  COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX  = 1L<<0
  */
  COGL_MATERIAL_LAYER_FLAG_DIRTY            = 1L<<1,
  COGL_MATERIAL_LAYER_FLAG_DEFAULT_COMBINE  = 1L<<2
} CoglMaterialLayerPrivFlags;

/* For tracking the state of a layer that's been flushed to OpenGL */
typedef struct _CoglLayerInfo
{
  CoglHandle  handle;
  gulong      flags;
  GLenum      gl_target;
  GLuint      gl_texture;
  gboolean    fallback;
  gboolean    disabled;
  gboolean    layer0_overridden;
} CoglLayerInfo;

struct _CoglMaterialLayer
{
  guint	       ref_count;
  guint	       index;	/*!< lowest index is blended first then others
			     on top */
  gulong       flags;
  CoglHandle   texture;	/*!< The texture for this layer, or COGL_INVALID_HANDLE
			     for an empty layer */

  /* Determines how the color of individual texture fragments
   * are calculated. */
  CoglMaterialLayerCombineFunc texture_combine_rgb_func;
  CoglMaterialLayerCombineSrc texture_combine_rgb_src[3];
  CoglMaterialLayerCombineOp texture_combine_rgb_op[3];

  CoglMaterialLayerCombineFunc texture_combine_alpha_func;
  CoglMaterialLayerCombineSrc texture_combine_alpha_src[3];
  CoglMaterialLayerCombineOp texture_combine_alpha_op[3];

  /* TODO: Support purely GLSL based material layers */

  CoglMatrix matrix;
};

typedef enum _CoglMaterialFlags
{
  COGL_MATERIAL_FLAG_ENABLE_BLEND	    = 1L<<0,
  COGL_MATERIAL_FLAG_SHOWN_SAMPLER_WARNING  = 1L<<1,
  COGL_MATERIAL_FLAG_DEFAULT_COLOR          = 1L<<2,
  COGL_MATERIAL_FLAG_DEFAULT_GL_MATERIAL    = 1L<<3,
  COGL_MATERIAL_FLAG_DEFAULT_ALPHA_FUNC     = 1L<<4,
  COGL_MATERIAL_FLAG_DEFAULT_BLEND_FUNC     = 1L<<5
} CoglMaterialFlags;

struct _CoglMaterial
{
  guint     ref_count;

  gulong    flags;

  /* If no lighting is enabled; this is the basic material color */
  GLfloat   unlit[4];

  /* Standard OpenGL lighting model attributes */
  GLfloat   ambient[4];
  GLfloat   diffuse[4];
  GLfloat   specular[4];
  GLfloat   emission[4];
  GLfloat   shininess;

  /* Determines what fragments are discarded based on their alpha */
  CoglMaterialAlphaFunc alpha_func;
  GLfloat		alpha_func_reference;

  /* Determines how this material is blended with other primitives */
  CoglMaterialBlendFactor blend_src_factor;
  CoglMaterialBlendFactor blend_dst_factor;

  GList	   *layers;
};

#endif /* __COGL_MATERIAL_PRIVATE_H */

