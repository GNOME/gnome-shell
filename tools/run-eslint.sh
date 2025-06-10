#!/bin/sh
# SPDX-License-Identifier: MIT OR LGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Philip Chimento <philip.chimento@gmail.com>
# SPDX-FileCopyrightText: 2025 Florian Müllner <fmuellner@gnome.org>

cd $(dirname -- "$0")
[ ! -d node_modules ] && npm clean-install
npm run lint -- "$@"
