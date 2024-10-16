#pragma once


#include <st/st.h>

G_BEGIN_DECLS

#define SHELL_TYPE_SQUARE_BIN (shell_square_bin_get_type ())
G_DECLARE_FINAL_TYPE (ShellSquareBin, shell_square_bin, SHELL, SquareBin, StBin)

G_END_DECLS
