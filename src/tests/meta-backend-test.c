/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "tests/meta-backend-test.h"

#include "tests/meta-monitor-manager-test.h"

struct _MetaBackendTest
{
  MetaBackendX11Nested parent;
};

G_DEFINE_TYPE (MetaBackendTest, meta_backend_test, META_TYPE_BACKEND_X11_NESTED)

static void
meta_backend_test_init (MetaBackendTest *backend_test)
{
}

static MetaMonitorManager *
meta_backend_test_create_monitor_manager (MetaBackend *backend)
{
  return g_object_new (META_TYPE_MONITOR_MANAGER_TEST, NULL);
}

static void
meta_backend_test_class_init (MetaBackendTestClass *klass)
{
  MetaBackendClass *backend_class = META_BACKEND_CLASS (klass);

  backend_class->create_monitor_manager = meta_backend_test_create_monitor_manager;
}
