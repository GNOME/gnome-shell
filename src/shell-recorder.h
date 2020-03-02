/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_RECORDER_H__
#define __SHELL_RECORDER_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

/**
 * SECTION:shell-recorder
 * @short_description: Record from a #ClutterStage
 *
 * The #ShellRecorder object is used to make recordings ("screencasts")
 * of a #ClutterStage. Recording is done via #GStreamer. The default is
 * to encode as a Theora movie and write it to a file in the current
 * directory named after the date, but the encoding and output can
 * be configured.
 */
#define SHELL_TYPE_RECORDER (shell_recorder_get_type ())
G_DECLARE_FINAL_TYPE (ShellRecorder, shell_recorder, SHELL, RECORDER, GObject)

ShellRecorder     *shell_recorder_new (ClutterStage  *stage);

void               shell_recorder_set_framerate (ShellRecorder *recorder,
                                                 int framerate);
void               shell_recorder_set_file_template (ShellRecorder *recorder,
                                                     const char    *file_template);
void               shell_recorder_set_pipeline (ShellRecorder *recorder,
						const char    *pipeline);
void               shell_recorder_set_draw_cursor (ShellRecorder *recorder,
                                                   gboolean       draw_cursor);
void               shell_recorder_set_area     (ShellRecorder *recorder,
                                                int            x,
                                                int            y,
                                                int            width,
                                                int            height);
gboolean           shell_recorder_record       (ShellRecorder  *recorder,
                                                char          **filename_used);
void               shell_recorder_close        (ShellRecorder *recorder);
void               shell_recorder_pause        (ShellRecorder *recorder);
gboolean           shell_recorder_is_recording (ShellRecorder *recorder);

G_END_DECLS

#endif /* __SHELL_RECORDER_H__ */
