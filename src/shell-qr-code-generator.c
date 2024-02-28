/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <meta/display.h>
#include <meta/util.h>
#include <meta/meta-plugin.h>
#include <st/st.h>

#include "qrcodegen.h"

#include "shell-qr-code-generator.h"

typedef struct _ShellQrCodeGeneratorPrivate  ShellQrCodeGeneratorPrivate;

#define BYTES_PER_R8G8B8 3

struct _ShellQrCodeGenerator
{
  GObject parent_instance;

  ShellQrCodeGeneratorPrivate *priv;
};

struct _ShellQrCodeGeneratorPrivate
{
  char *url;
  size_t width;
  size_t height;
  GTask *image_task;
  GTask *icon_task;
};

G_DEFINE_TYPE_WITH_PRIVATE (ShellQrCodeGenerator, shell_qr_code_generator, G_TYPE_OBJECT);

static void
shell_qr_code_generator_dispose (GObject *object)
{
  ShellQrCodeGenerator *self = SHELL_QR_CODE_GENERATOR (object);
  g_clear_pointer (&self->priv->url, g_free);
}

static void
shell_qr_code_generator_class_init (ShellQrCodeGeneratorClass *qr_code_generator_class)
{
  GObjectClass *gobject_class = (GObjectClass *) qr_code_generator_class;
  gobject_class->dispose = shell_qr_code_generator_dispose;
}

static void
shell_qr_code_generator_init (ShellQrCodeGenerator *qr_code_generator)
{
  qr_code_generator->priv = shell_qr_code_generator_get_instance_private (qr_code_generator);
}

static void
fill_pixel (GByteArray *array,
            guint8     value,
            int        pixel_size)
{
  guint i;

  for (i = 0; i < pixel_size; i++)
    {
      g_byte_array_append (array, &value, 1); /* R */
      g_byte_array_append (array, &value, 1); /* G */
      g_byte_array_append (array, &value, 1); /* B */
    }
}


static guint8 *
generate_icon (const char   *url,
               size_t        width,
               size_t        height,
               GError      **error)
{
  uint8_t qr_code[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
  uint8_t temp_buf[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
  GByteArray *qr_matrix;
  gint pixel_size, qr_size, total_size;
  gint column, row, i;

  if (!qrcodegen_encodeText (url,
                             temp_buf,
                             qr_code,
                             qrcodegen_Ecc_LOW,
                             qrcodegen_VERSION_MIN,
                             qrcodegen_VERSION_MAX,
                             qrcodegen_Mask_AUTO,
                             FALSE))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "QRCode generation failed for url %s",
                   url);
      return NULL;
    }

  qr_size = qrcodegen_getSize (qr_code);
  pixel_size = MAX (1, width / (qr_size));
  total_size = qr_size * pixel_size;
  qr_matrix = g_byte_array_sized_new (total_size * total_size * pixel_size * BYTES_PER_R8G8B8);

  for (column = 0; column < total_size; column++)
    {
      for (i = 0; i < pixel_size; i++)
        {
          for (row = 0; row < total_size / pixel_size; row++)
            {
              if (qrcodegen_getModule (qr_code, column, row))
                fill_pixel (qr_matrix, 0x00, pixel_size);
              else
                fill_pixel (qr_matrix, 0xff, pixel_size);
            }
        }
    }

  return g_byte_array_free (qr_matrix, FALSE);
}

static void
qr_code_generator_thread (GTask        *task,
                          gpointer      source,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  GError *error = NULL;
  ShellQrCodeGenerator *self = task_data;
  g_autofree guint8 *pixel_data = NULL;

  pixel_data = generate_icon (self->priv->url, self->priv->width, self->priv->height, &error);

  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, g_steal_pointer (&pixel_data), NULL);
}

static void
on_image_task_complete (ShellQrCodeGenerator *self,
                        GAsyncResult         *result,
                        gpointer              user_data)
{
  guint8 *pixel_data;
  g_autoptr (ClutterContent) content = NULL;
  g_autoptr (GError) error = NULL;
  gboolean data_set;

  pixel_data = g_task_propagate_pointer (G_TASK (result), &error);

  if (error != NULL)
    {
      g_task_return_error (self->priv->icon_task, error);
      return;
    }

  content = st_image_content_new_with_preferred_size (self->priv->width, self->priv->height);
  data_set = clutter_image_set_data (CLUTTER_IMAGE (content),
                                     pixel_data,
                                     COGL_PIXEL_FORMAT_RGB_888,
                                     self->priv->width,
                                     self->priv->height,
                                     self->priv->width * BYTES_PER_R8G8B8,
                                     &error);

  if (!data_set)
    {
      g_task_return_error (self->priv->icon_task, error);
      return;
    }

  g_task_return_pointer (self->priv->icon_task, g_steal_pointer (&content), g_object_unref);
  g_clear_object (&self->priv->image_task);
}

/**
 * shell_qr_code_generator_generator_qr_code:
 * @qr_code_generator: the #ShellQrCodeGenerator
 * @stream: The stream for the QR code generator
 * @callback: (scope async): function to call returning success or failure
 * of the async grabbing
 * @user_data: the data to pass to callback function
 *
 */
void
shell_qr_code_generator_generate_qr_code (ShellQrCodeGenerator *self,
                                          const char           *url,
                                          size_t                width,
                                          size_t                height,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data)
{
  ShellQrCodeGeneratorPrivate *priv;

  g_return_if_fail (SHELL_IS_QR_CODE_GENERATOR (self));

  priv = self->priv;

  if (!url || *url == '\0')
    {
      if (callback)
        g_task_report_new_error (self,
                                 callback,
                                 user_data,
                                 shell_qr_code_generator_generate_qr_code,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "No valid QR code uri is provided");
      return;
    }

  if (width != height)
    {
      if (callback)
        g_task_report_new_error (self,
                                 callback,
                                 user_data,
                                 shell_qr_code_generator_generate_qr_code,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Qr code size mismatch");
      return;
    }

  if (priv->url != NULL)
    {
      if (callback)
        g_task_report_new_error (self,
                                 callback,
                                 user_data,
                                 shell_qr_code_generator_generate_qr_code,
                                 G_IO_ERROR,
                                 G_IO_ERROR_PENDING,
                                 "Only one QR code generator operation at a time "
                                 "is permitted");
      return;
    }

  priv->url = g_strdup (url);
  priv->width = width;
  priv->height = height;

  priv->icon_task = g_task_new (self, NULL, callback, user_data);
  g_task_set_source_tag (priv->icon_task, shell_qr_code_generator_generate_qr_code);
  g_task_set_task_data (priv->icon_task, self, NULL);

  priv->image_task = g_task_new (self, NULL, (GAsyncReadyCallback) on_image_task_complete, NULL);
  g_task_set_source_tag (priv->image_task, on_image_task_complete);
  g_task_set_task_data (priv->image_task, self, NULL);
  g_task_run_in_thread (priv->image_task, qr_code_generator_thread);
}

/**
 * shell_qr_code_generator_generate_qr_code_finish:
 * @result: the #GAsyncResult that was provided to the callback
 * @error: #GError for error reporting
 *
 * Finish the asynchronous operation started by shell_qr_code_generator_generate_qr_code()
 * and obtain its result.
 *
 * Returns: (transfer full):  a #GIcon of the QR code
 *
 */
GIcon *
shell_qr_code_generator_generate_qr_code_finish (ShellQrCodeGenerator   *self,
                                                 GAsyncResult           *result,
                                                 GError                **error)
{
  g_return_val_if_fail (SHELL_IS_QR_CODE_GENERATOR (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result,
                                                  shell_qr_code_generator_generate_qr_code),
                        NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

ShellQrCodeGenerator *
shell_qr_code_generator_new (void)
{
  return g_object_new (SHELL_TYPE_QR_CODE_GENERATOR, NULL);
}
