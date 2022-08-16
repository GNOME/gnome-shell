'''Accounts Service D-Bus mock template'''

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Marco Trevisan'
__email__ = 'marco.trevisan@canonical.com'
__copyright__ = '(c) 2021 Canonical Ltd.'
__license__ = 'LGPL 3+'

import sys
import time

import dbus
import dbusmock
from dbusmock import MOCK_IFACE, mockobject

BUS_NAME = 'org.freedesktop.Accounts'
MAIN_OBJ = '/org/freedesktop/Accounts'
MAIN_IFACE = 'org.freedesktop.Accounts'
USER_IFACE = MAIN_IFACE + '.User'
SYSTEM_BUS = True

DEFAULT_USER_PASSWORD = 'Pa$$wo0rd'


def get_user_path(uid):
    return f'/org/freedesktop/Accounts/User{uid}'


def load(mock, parameters=None):
    parameters = parameters if parameters else {}
    mock.mock_users = {}
    mock.users_auto_uids = 2000

    mock.AddProperties(MAIN_IFACE, {
        'HasNoUsers': dbus.Boolean(True),
        'HasMultipleUsers': dbus.Boolean(False),
    })

    for uid, name in parameters.get('users', {}):
        mock.AddUser(uid, name, '', {})


@dbus.service.method(MOCK_IFACE, in_signature='xssa{sv}',
                     out_signature='o')
def AddUser(self, uid, username, password, overrides):
    '''Add user via uid and username and optionally overriding properties

    Returns the new object path.
    '''
    path = get_user_path(uid)
    default_props = {
        'Uid': dbus.UInt64(uid),
        'UserName': username,
        'RealName': username[0].upper() + username[1:] + ' Fake',
        'AccountType': dbus.Int32(1),
        'AutomaticLogin': False,
        'BackgroundFile': '',
        'Email': f'{username}@python-dbusmock.org',
        'FormatsLocale': 'C',
        'HomeDirectory': f'/nonexisting/mock-home/{username}',
        'IconFile': '',
        'InputSources': dbus.Array([], signature='a{ss}'),
        'Language': 'C',
        'LocalAccount': True,
        'Location': '',
        'Locked': False,
        'LoginFrequency': dbus.UInt64(0),
        'LoginHistory': dbus.Array([], signature='(xxa{sv})'),
        'LoginTime': dbus.Int64(0),
        'PasswordHint': 'Remember it, come on!',
        'PasswordMode': 0,
        'Session': 'mock-session',
        'SessionType': 'wayland',
        'Shell': '/usr/bin/zsh',
        'SystemAccount': False,
        'XHasMessages': False,
        'XKeyboardLayouts': dbus.Array([], signature='s'),
        'XSession': 'mock-xsession',
    }
    default_props.update(overrides if overrides else {})
    self.AddObject(path, USER_IFACE, default_props, [])
    user_object = mockobject.objects[path]

    user_object.AddProperty(
            'com.endlessm.ParentalControls.AppFilter',
            'AppFilter', dbus.Struct([
                             False, dbus.Array(signature='s')
                         ], variant_level=1),
            )

    user = mockobject.objects[path]
    user.password = password
    user.properties = default_props
    self.mock_users[uid] = default_props

    self.EmitSignal(MAIN_IFACE, 'UserAdded', 'o', [path])

    self.UpdateProperties(MAIN_IFACE, {
        'HasNoUsers': dbus.Boolean(len(self.mock_users) == 0),
        'HasMultipleUsers': dbus.Boolean(len(self.mock_users) > 1),
    })

    return path


@dbus.service.method(MOCK_IFACE, in_signature='', out_signature='ao')
def ListMockUsers(self):
    """ List the mock users that have been created """
    return [get_user_path(uid) for uid in self.mock_users.keys()]


@dbus.service.method(MAIN_IFACE, in_signature='x', out_signature='o')
def FindUserById(_self, uid):
    """ Finds an user by its user id """
    user = mockobject.objects.get(get_user_path(uid), None)
    if not user:
        raise dbus.exceptions.DBusException(
            'No such user exists',
            name='org.freedesktop.Accounts.Error.Failed')
    return get_user_path(uid)


@dbus.service.method(MAIN_IFACE, in_signature='s', out_signature='o')
def FindUserByName(self, username):
    """ Finds an user form its name """
    try:
        [user_id] = [uid for uid, props in self.mock_users.items()
                     if props['UserName'] == username]
    except ValueError as e:
        raise dbus.exceptions.DBusException(f'No such user exists: {e}', name='org.freedesktop.Accounts.Error.Failed')
    return get_user_path(user_id)
