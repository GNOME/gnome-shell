// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/*
 * Copyright 2024 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

import GObject from 'gi://GObject';
import Gio from 'gi://Gio';
import * as Main from '../ui/main.js';
import * as MessageTray from '../ui/messageTray.js';
import * as Params from '../misc/params.js';

export const AuthNotificationSource = GObject.registerClass({
}, class AuthNotificationSource extends MessageTray.Source {
    constructor(params) {
        params = Params.parse(params, {
            title: _('Login Failed'),
            iconName: 'dialog-password-symbolic',
        });

        super(params);
    }
    createBanner(notification) {
        return new AuthNotificationBanner(notification);
    }
});

const AuthNotificationBanner = GObject.registerClass({
}, class AuthNotificationBanner extends MessageTray.NotificationBanner {
    _init(notification) {
        super._init(notification);
        this.add_style_class_name('auth-notification-banner');
    }

    createBanner(notification) {
        return new AuthNotificationBanner(notification);
    }
});
