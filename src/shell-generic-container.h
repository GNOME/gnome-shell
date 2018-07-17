/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_GENERIC_CONTAINER_H__
#define __SHELL_GENERIC_CONTAINER_H__

#include "st.h"

#define SHELL_TYPE_GENERIC_CONTAINER (shell_generic_container_get_type ())
G_DECLARE_FINAL_TYPE (ShellGenericContainer, shell_generic_container,
                      SHELL, GENERIC_CONTAINER, StWidget)

typedef struct {
  float min_size;
  float natural_size;

  /* <private> */
  guint _refcount;
} ShellGenericContainerAllocation;

#define SHELL_TYPE_GENERIC_CONTAINER_ALLOCATION (shell_generic_container_allocation_get_type ())
GType shell_generic_container_allocation_get_type (void);

guint    shell_generic_container_get_n_skip_paint (ShellGenericContainer *self);

gboolean shell_generic_container_get_skip_paint   (ShellGenericContainer *self,
                                                   ClutterActor          *child);
void     shell_generic_container_set_skip_paint   (ShellGenericContainer *self,
                                                   ClutterActor          *child,
                                                   gboolean               skip);

#endif /* __SHELL_GENERIC_CONTAINER_H__ */
