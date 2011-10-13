#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>

struct {
  CoglFeatureID feature;
  const char *short_description;
  const char *long_description;
} features[] =
{
  {
    COGL_FEATURE_ID_TEXTURE_NPOT_BASIC,
    "Non power of two textures (basic)",
    "The hardware supports non power of two textures, but you also "
    "need to check the COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP and "
    "COGL_FEATURE_ID_TEXTURE_NPOT_REPEAT features to know if the "
    "hardware supports npot texture mipmaps or repeat modes other "
    "than COGL_RENDERER_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE respectively."
  },
  {
    COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP,
    "Non power of two textures (+ mipmap)",
    "Mipmapping is supported in conjuntion with non power of two "
    "textures."
  },
  {
    COGL_FEATURE_ID_TEXTURE_NPOT_REPEAT,
    "Non power of two textures (+ repeat modes)",
    "Repeat modes other than "
    "COGL_RENDERER_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE are supported by "
    "the hardware in conjunction with non power of two textures."
  },
  {
    COGL_FEATURE_ID_TEXTURE_NPOT,
    "Non power of two textures (fully featured)",
    "Non power of two textures are supported by the hardware. This "
    "is a equivalent to the COGL_FEATURE_ID_TEXTURE_NPOT_BASIC, "
    "COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP and "
    "COGL_FEATURE_ID_TEXTURE_NPOT_REPEAT features combined."
  },
  {
    COGL_FEATURE_ID_TEXTURE_RECTANGLE,
    "Unnormalized coordinate, rectangle textures",
    "Support for rectangular textures with non-normalized texture "
    "coordinates."
  },
  {
    COGL_FEATURE_ID_TEXTURE_3D,
    "3D texture support",
    "3D texture support"
  },
  {
    COGL_FEATURE_ID_OFFSCREEN,
    "Offscreen rendering support",
    "Offscreen rendering support"
  },
  {
    COGL_FEATURE_ID_OFFSCREEN_MULTISAMPLE,
    "Offscreen rendering with multisampling support",
    "Offscreen rendering with multisampling support"
  },
  {
    COGL_FEATURE_ID_ONSCREEN_MULTIPLE,
    "Multiple onscreen framebuffers supported",
    "Multiple onscreen framebuffers supported"
  },
  {
    COGL_FEATURE_ID_GLSL,
    "GLSL support",
    "GLSL support"
  },
  {
    COGL_FEATURE_ID_ARBFP,
    "ARBFP support",
    "ARBFP support"
  },
  {
    COGL_FEATURE_ID_UNSIGNED_INT_INDICES,
    "Unsigned integer indices",
    "COGL_RENDERER_INDICES_TYPE_UNSIGNED_INT is supported in cogl_indices_new()."
  },
  {
    COGL_FEATURE_ID_DEPTH_RANGE,
    "cogl_pipeline_set_depth_range() support",
    "cogl_pipeline_set_depth_range() support",
  },
  {
    COGL_FEATURE_ID_POINT_SPRITE,
    "Point sprite coordinates",
    "cogl_pipeline_set_layer_point_sprite_coords_enabled() is supported"
  },
  {
    COGL_FEATURE_ID_MAP_BUFFER_FOR_READ,
    "Mapping buffers for reading",
    "Mapping buffers for reading"
  },
  {
    COGL_FEATURE_ID_MAP_BUFFER_FOR_WRITE,
    "Mapping buffers for writing",
    "Mapping buffers for writing"
  },
  {
    COGL_FEATURE_ID_MIRRORED_REPEAT,
    "Mirrored repeat wrap modes",
    "Mirrored repeat wrap modes"
  }
};

static void
feature_cb (CoglFeatureID feature, void *user_data)
{
  int i;
  for (i = 0; i < sizeof(features) / sizeof(features[0]); i++)
    {
      if (features[i].feature == feature)
        {
          printf (" » %s\n", features[i].short_description);
          return;
        }
    }
  printf (" » Unknown feature %d\n", feature);
}

int
main (int argc, char **argv)
{
    CoglContext *ctx;
    GError *error = NULL;

    ctx = cogl_context_new (NULL, &error);
    if (!ctx) {
        fprintf (stderr, "Failed to create context: %s\n", error->message);
        return 1;
    }

    g_print ("Features:\n");
    cogl_foreach_feature (ctx, feature_cb, NULL);

    return 0;
}
