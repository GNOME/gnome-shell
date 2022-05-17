/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 20011 Red Hat, Inc.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#pragma once

#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkitagent/polkitagent.h>
#include <glib-object.h>

G_BEGIN_DECLS

#ifndef HAVE_POLKIT_AUTOCLEANUP
/* Polkit doesn't have g_autoptr support, thus we have to manually set the autoptr function here */
G_DEFINE_AUTOPTR_CLEANUP_FUNC (PolkitAgentListener, g_object_unref)
#endif

#define SHELL_TYPE_POLKIT_AUTHENTICATION_AGENT (shell_polkit_authentication_agent_get_type())

G_DECLARE_FINAL_TYPE (ShellPolkitAuthenticationAgent, shell_polkit_authentication_agent, SHELL, POLKIT_AUTHENTICATION_AGENT, PolkitAgentListener)

ShellPolkitAuthenticationAgent *shell_polkit_authentication_agent_new (void);

void                            shell_polkit_authentication_agent_complete (ShellPolkitAuthenticationAgent *agent,
                                                                            gboolean                        dismissed);
void                            shell_polkit_authentication_agent_register (ShellPolkitAuthenticationAgent *agent,
                                                                            GError                        **error_out);
void                            shell_polkit_authentication_agent_unregister (ShellPolkitAuthenticationAgent *agent);

G_END_DECLS

