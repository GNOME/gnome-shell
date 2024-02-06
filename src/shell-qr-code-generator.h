/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_QR_CODE_GENERATOR_H__
#define __SHELL_QR_CODE_GENERATOR_H__

/**
 * SECTION:shell-qr-code-generator
 * @short_description: Generates a QR code for a given url.
 *
 * The #ShellQrCodeGenerator object is used to generate QR codes for URLs
 *
 */
#define SHELL_TYPE_QR_CODE_GENERATOR (shell_qr_code_generator_get_type ())
G_DECLARE_FINAL_TYPE (ShellQrCodeGenerator, shell_qr_code_generator,
                      SHELL, QR_CODE_GENERATOR, GObject)

ShellQrCodeGenerator *shell_qr_code_generator_new (void);

void    shell_qr_code_generator_generate_qr_code           (ShellQrCodeGenerator   *qr_code_generator,
                                                            const char             *url,
                                                            size_t                  width,
                                                            size_t                  height,
                                                            GAsyncReadyCallback     callback,
                                                            gpointer                user_data);
GIcon *shell_qr_code_generator_generate_qr_code_finish   (ShellQrCodeGenerator   *qr_code_generator,
                                                          GAsyncResult           *result,
                                                          GError                **error);

#endif /* ___SHELL_QR_CODE_GENERATOR_H__ */
