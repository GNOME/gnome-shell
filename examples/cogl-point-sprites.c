#include <cogl/cogl.h>
#include <glib.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define N_FIREWORKS 32
/* Units per second per second */
#define GRAVITY -1.5f

#define N_SPARKS (N_FIREWORKS * 32) /* Must be a power of two */
#define TIME_PER_SPARK 0.01f /* in seconds */

#define TEXTURE_SIZE 32

typedef struct
{
  uint8_t red, green, blue, alpha;
} Color;

typedef struct
{
  float size;
  float x, y;
  float start_x, start_y;
  Color color;

  /* Velocities are in units per second */
  float initial_x_velocity;
  float initial_y_velocity;

  GTimer *timer;
} Firework;

typedef struct
{
  float x, y;
  Color color;
  Color base_color;
} Spark;

typedef struct
{
  Firework fireworks[N_FIREWORKS];

  int next_spark_num;
  Spark sparks[N_SPARKS];
  GTimer *last_spark_time;

  CoglContext *context;
  CoglFramebuffer *fb;
  CoglPipeline *pipeline;
  CoglPrimitive *primitive;
  CoglAttributeBuffer *attribute_buffer;
} Data;

static CoglTexture *
generate_round_texture (CoglContext *context)
{
  uint8_t *p, *data;
  int x, y;
  CoglTexture2D *tex;

  p = data = g_malloc (TEXTURE_SIZE * TEXTURE_SIZE * 4);

  /* Generate a white circle which gets transparent towards the edges */
  for (y = 0; y < TEXTURE_SIZE; y++)
    for (x = 0; x < TEXTURE_SIZE; x++)
      {
        int dx = x - TEXTURE_SIZE / 2;
        int dy = y - TEXTURE_SIZE / 2;
        float value = sqrtf (dx * dx + dy * dy) * 255.0 / (TEXTURE_SIZE / 2);
        if (value > 255.0f)
          value = 255.0f;
        value = 255.0f - value;
        *(p++) = value;
        *(p++) = value;
        *(p++) = value;
        *(p++) = value;
      }

  tex = cogl_texture_2d_new_from_data (context,
                                       TEXTURE_SIZE, TEXTURE_SIZE,
                                       COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                       TEXTURE_SIZE * 4,
                                       data,
                                       NULL /* error */);

  g_free (data);

  return tex;
}

static void
paint (Data *data)
{
  int i;
  float diff_time;

  /* Update all of the firework's positions */
  for (i = 0; i < N_FIREWORKS; i++)
    {
      Firework *firework = data->fireworks + i;

      if ((fabsf (firework->x - firework->start_x) > 2.0f) ||
          firework->y < -1.0f)
        {
          firework->size = g_random_double_range (0.001f, 0.1f);
          firework->start_x = 1.0f + firework->size;
          firework->start_y = -1.0f;
          firework->initial_x_velocity = g_random_double_range (-0.1f, -2.0f);
          firework->initial_y_velocity = g_random_double_range (0.1f, 4.0f);
          g_timer_reset (firework->timer);

          /* Pick a random color out of six */
          if (g_random_boolean ())
            {
              memset (&firework->color, 0, sizeof (Color));
              ((uint8_t *) &firework->color)[g_random_int_range (0, 3)] = 255;
            }
          else
            {
              memset (&firework->color, 255, sizeof (Color));
              ((uint8_t *) &firework->color)[g_random_int_range (0, 3)] = 0;
            }
          firework->color.alpha = 255;

          /* Fire some of the fireworks from the other side */
          if (g_random_boolean ())
            {
              firework->start_x = -firework->start_x;
              firework->initial_x_velocity = -firework->initial_x_velocity;
            }
        }

      diff_time = g_timer_elapsed (firework->timer, NULL);

      firework->x = (firework->start_x +
                     firework->initial_x_velocity * diff_time);

      firework->y = ((firework->initial_y_velocity * diff_time +
                      0.5f * GRAVITY * diff_time * diff_time) +
                     firework->start_y);
    }

  diff_time = g_timer_elapsed (data->last_spark_time, NULL);
  if (diff_time < 0.0f || diff_time >= TIME_PER_SPARK)
    {
      /* Add a new spark for each firework, overwriting the oldest ones */
      for (i = 0; i < N_FIREWORKS; i++)
        {
          Spark *spark = data->sparks + data->next_spark_num;
          Firework *firework = data->fireworks + i;

          spark->x = (firework->x +
                      g_random_double_range (-firework->size / 2.0f,
                                             firework->size / 2.0f));
          spark->y = (firework->y +
                      g_random_double_range (-firework->size / 2.0f,
                                             firework->size / 2.0f));
          spark->base_color = firework->color;

          data->next_spark_num = (data->next_spark_num + 1) & (N_SPARKS - 1);
        }

      /* Update the colour of each spark */
      for (i = 0; i < N_SPARKS; i++)
        {
          float color_value;

          /* First spark is the oldest */
          Spark *spark = data->sparks + ((data->next_spark_num + i)
                                         & (N_SPARKS - 1));

          color_value = i / (N_SPARKS - 1.0f);
          spark->color.red = spark->base_color.red * color_value;
          spark->color.green = spark->base_color.green * color_value;
          spark->color.blue = spark->base_color.blue * color_value;
          spark->color.alpha = 255.0f * color_value;
        }

      g_timer_reset (data->last_spark_time);
    }

  cogl_buffer_set_data (data->attribute_buffer,
                        0, /* offset */
                        data->sparks,
                        sizeof (data->sparks));

  cogl_framebuffer_clear4f (data->fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  cogl_primitive_draw (data->primitive,
                       data->fb,
                       data->pipeline);

  cogl_onscreen_swap_buffers (data->fb);
}

static void
create_primitive (Data *data)
{
  CoglAttribute *attributes[2];
  int i;

  data->attribute_buffer =
    cogl_attribute_buffer_new_with_size (data->context,
                                         sizeof (data->sparks));
  cogl_buffer_set_update_hint (data->attribute_buffer,
                               COGL_BUFFER_UPDATE_HINT_DYNAMIC);

  attributes[0] = cogl_attribute_new (data->attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (Spark),
                                      G_STRUCT_OFFSET (Spark, x),
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cogl_attribute_new (data->attribute_buffer,
                                      "cogl_color_in",
                                      sizeof (Spark),
                                      G_STRUCT_OFFSET (Spark, color),
                                      4, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

  data->primitive =
    cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_POINTS,
                                        N_SPARKS,
                                        attributes,
                                        G_N_ELEMENTS (attributes));

  for (i = 0; i < G_N_ELEMENTS (attributes); i++)
    cogl_object_unref (attributes[i]);
}

static void
frame_event_cb (CoglOnscreen *onscreen,
                CoglFrameEvent event,
                CoglFrameInfo *info,
                void *user_data)
{
  Data *data = user_data;

  if (event == COGL_FRAME_EVENT_SYNC)
    paint (data);
}

int
main (int argc, char *argv[])
{
  CoglTexture *tex;
  CoglOnscreen *onscreen;
  GSource *cogl_source;
  GMainLoop *loop;
  Data data;
  int i;

  data.context = cogl_context_new (NULL, NULL);

  create_primitive (&data);

  data.pipeline = cogl_pipeline_new (data.context);
  data.last_spark_time = g_timer_new ();
  data.next_spark_num = 0;
  cogl_pipeline_set_point_size (data.pipeline, TEXTURE_SIZE);

  tex = generate_round_texture (data.context);
  cogl_pipeline_set_layer_texture (data.pipeline, 0, tex);
  cogl_object_unref (tex);

  cogl_pipeline_set_layer_point_sprite_coords_enabled (data.pipeline,
                                                       0, /* layer */
                                                       TRUE,
                                                       NULL /* error */);

  for (i = 0; i < N_FIREWORKS; i++)
    {
      data.fireworks[i].x = -FLT_MAX;
      data.fireworks[i].y = FLT_MAX;
      data.fireworks[i].size = 0.0f;
      data.fireworks[i].timer = g_timer_new ();
    }

  for (i = 0; i < N_SPARKS; i++)
    {
      data.sparks[i].x = 2.0f;
      data.sparks[i].y = 2.0f;
    }

  onscreen = cogl_onscreen_new (data.context, 800, 600);
  cogl_onscreen_show (onscreen);
  data.fb = onscreen;

  cogl_source = cogl_glib_source_new (data.context, G_PRIORITY_DEFAULT);

  g_source_attach (cogl_source, NULL);

  cogl_onscreen_add_frame_callback (onscreen,
                                    frame_event_cb,
                                    &data,
                                    NULL /* destroy notify */);

  loop = g_main_loop_new (NULL, TRUE);

  paint (&data);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);

  g_source_destroy (cogl_source);

  cogl_object_unref (data.pipeline);
  cogl_object_unref (data.attribute_buffer);
  cogl_object_unref (data.primitive);
  cogl_object_unref (onscreen);
  cogl_object_unref (data.context);

  g_timer_destroy (data.last_spark_time);

  for (i = 0; i < N_FIREWORKS; i++)
    g_timer_destroy (data.fireworks[i].timer);

  return 0;
}
