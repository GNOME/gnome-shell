#pragma once

#include <st/st.h>

G_BEGIN_DECLS

#define SHELL_TYPE_WORKSPACE_BACKGROUND (shell_workspace_background_get_type ())
G_DECLARE_FINAL_TYPE (ShellWorkspaceBackground, shell_workspace_background,
                      SHELL, WORKSPACE_BACKGROUND, StWidget)

int shell_workspace_background_get_monitor_index (ShellWorkspaceBackground *bg);

double shell_workspace_background_get_state_adjustment_value (ShellWorkspaceBackground *bg);
void   shell_workspace_background_set_state_adjustment_value (ShellWorkspaceBackground *bg,
                                                              double                    value);

G_END_DECLS
