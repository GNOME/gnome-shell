#pragma once

#include <st/st.h>

G_BEGIN_DECLS

#define SHELL_TYPE_WINDOW_PREVIEW (shell_window_preview_get_type ())
G_DECLARE_FINAL_TYPE (ShellWindowPreview, shell_window_preview,
                      SHELL, WINDOW_PREVIEW, StWidget)

G_END_DECLS
