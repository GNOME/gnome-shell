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
  },
  {
    COGL_FEATURE_ID_GLES2_CONTEXT,
    "GLES2 API integration supported",
    "Support for creating a GLES2 context for using the GLES2 API in a "
      "way that's integrated with Cogl."
  },
  {
    COGL_FEATURE_ID_DEPTH_TEXTURE,
    "Depth Textures",
    "CoglFramebuffers can be configured to render their depth buffer into "
    "a texture"
  }
};

static const char *
get_winsys_name_for_id (CoglWinsysID winsys_id)
{
  switch (winsys_id)
    {
    case COGL_WINSYS_ID_ANY:
      g_return_val_if_reached ("ERROR");
    case COGL_WINSYS_ID_STUB:
      return "Stub";
    case COGL_WINSYS_ID_GLX:
      return "GLX";
    case COGL_WINSYS_ID_EGL_XLIB:
      return "EGL + Xlib platform";
    case COGL_WINSYS_ID_EGL_NULL:
      return "EGL + NULL window system platform";
    case COGL_WINSYS_ID_EGL_GDL:
      return "EGL + GDL platform";
    case COGL_WINSYS_ID_EGL_WAYLAND:
      return "EGL + Wayland platform";
    case COGL_WINSYS_ID_EGL_KMS:
      return "EGL + KMS platform";
    case COGL_WINSYS_ID_EGL_ANDROID:
      return "EGL + Android platform";
    case COGL_WINSYS_ID_WGL:
      return "EGL + Windows WGL platform";
    case COGL_WINSYS_ID_SDL:
      return "EGL + SDL platform";
    }
  g_return_val_if_reached ("Unknown");
}

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

typedef struct _OutputState
{
  int id;
} OutputState;

static void
output_cb (CoglOutput *output, void *user_data)
{
  OutputState *state = user_data;
  const char *order;
  float refresh;

  printf (" Output%d:\n", state->id++);
  printf ("  » position = (%d, %d)\n",
          cogl_output_get_x (output),
          cogl_output_get_y (output));
  printf ("  » resolution = %d x %d\n",
          cogl_output_get_width (output),
          cogl_output_get_height (output));
  printf ("  » physical size = %dmm x %dmm\n",
          cogl_output_get_mm_width (output),
          cogl_output_get_mm_height (output));
  switch (cogl_output_get_subpixel_order (output))
    {
    case COGL_SUBPIXEL_ORDER_UNKNOWN:
      order = "unknown";
      break;
    case COGL_SUBPIXEL_ORDER_NONE:
      order = "non-standard";
      break;
    case COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB:
      order = "horizontal,rgb";
      break;
    case COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR:
      order = "horizontal,bgr";
      break;
    case COGL_SUBPIXEL_ORDER_VERTICAL_RGB:
      order = "vertical,rgb";
      break;
    case COGL_SUBPIXEL_ORDER_VERTICAL_BGR:
      order = "vertical,bgr";
      break;
    }
  printf ("  » sub pixel order = %s\n", order);

  refresh = cogl_output_get_refresh_rate (output);
  if (refresh)
    printf ("  » refresh = %f Hz\n", refresh);
  else
    printf ("  » refresh = unknown\n");
}

int
main (int argc, char **argv)
{
  CoglRenderer *renderer;
  CoglDisplay *display;
  CoglContext *ctx;
  CoglError *error = NULL;
  CoglWinsysID winsys_id;
  const char *winsys_name;
  OutputState output_state;

#ifdef COGL_HAS_EMSCRIPTEN_SUPPORT
  ctx = cogl_sdl_context_new (SDL_USEREVENT, &error);
#else
  ctx = cogl_context_new (NULL, &error);
#endif
  if (!ctx) {
      fprintf (stderr, "Failed to create context: %s\n", error->message);
      return 1;
  }

  display = cogl_context_get_display (ctx);
  renderer = cogl_display_get_renderer (display);
  winsys_id = cogl_renderer_get_winsys_id (renderer);
  winsys_name = get_winsys_name_for_id (winsys_id);
  g_print ("Renderer: %s\n\n", winsys_name);

  g_print ("Features:\n");
  cogl_foreach_feature (ctx, feature_cb, NULL);

  g_print ("Outputs:\n");
  output_state.id = 0;
  cogl_renderer_foreach_output (renderer, output_cb, &output_state);
  if (output_state.id == 0)
    printf (" Unknown\n");

  return 0;
}
