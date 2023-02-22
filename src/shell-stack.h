/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_STACK_H__
#define __SHELL_STACK_H__

#include "st.h"

#define SHELL_TYPE_STACK (shell_stack_get_type ())
G_DECLARE_FINAL_TYPE (ShellStack, shell_stack, SHELL, STACK, StWidget)

#endif /* __SHELL_STACK_H__ */
