// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;

const Config = imports.misc.config;
const Main = imports.ui.main;
const Params = imports.misc.params;


const STANDARD_STATUS_AREA_SHELL_IMPLEMENTATION = {
    'a11y': imports.ui.status.accessibility.ATIndicator,
    'volume': imports.ui.status.volume.Indicator,
    'battery': imports.ui.status.power.Indicator,
    'keyboard': imports.ui.status.keyboard.InputSourceIndicator,
    'userMenu': imports.ui.userMenu.UserMenuButton
};

if (Config.HAVE_BLUETOOTH)
    STANDARD_STATUS_AREA_SHELL_IMPLEMENTATION['bluetooth'] =
        imports.ui.status.bluetooth.Indicator;

try {
    STANDARD_STATUS_AREA_SHELL_IMPLEMENTATION['network'] =
        imports.ui.status.network.NMApplet;
} catch(e) {
    log('NMApplet is not supported. It is possible that your NetworkManager version is too old');
}


const DEFAULT_MODE = 'user';

const _modes = {
    'gdm': { hasOverview: false,
             hasAppMenu: false,
             showCalendarEvents: false,
             allowSettings: false,
             allowExtensions: false,
             allowKeybindingsWhenModal: true,
             hasRunDialog: false,
             hasWorkspaces: false,
             createSession: Main.createGDMSession,
             createUnlockDialog: Main.createGDMLoginDialog,
             extraStylesheet: null,
             statusArea: {
                 order: [
                     'a11y', 'display', 'keyboard',
                     'volume', 'battery', 'powerMenu'
                 ],
                 implementation: {
                     'a11y': imports.ui.status.accessibility.ATIndicator,
                     'volume': imports.ui.status.volume.Indicator,
                     'battery': imports.ui.status.power.Indicator,
                     'keyboard': imports.ui.status.keyboard.InputSourceIndicator,
                     'powerMenu': imports.gdm.powerMenu.PowerMenuButton
                 }
             }
           },

    'initial-setup': { hasOverview: false,
                       hasAppMenu: false,
                       showCalendarEvents: false,
                       allowSettings: false,
                       allowExtensions: false,
                       allowKeybindingsWhenModal: false,
                       hasRunDialog: false,
                       hasWorkspaces: false,
                       createSession: Main.createInitialSetupSession,
                       extraStylesheet: null,
                       statusArea: {
                           order: [
                               'a11y', 'keyboard', 'volume'
                           ],
                           implementation: {
                               'a11y': imports.ui.status.accessibility.ATIndicator,
                               'keyboard': imports.ui.status.keyboard.XKBIndicator,
                               'volume': imports.ui.status.volume.Indicator
                        }
                }
           },

    'user': { hasOverview: true,
              hasAppMenu: true,
              showCalendarEvents: true,
              allowSettings: true,
              allowExtensions: true,
              allowKeybindingsWhenModal: false,
              hasRunDialog: true,
              hasWorkspaces: true,
              createSession: Main.createUserSession,
              createUnlockDialog: Main.createSessionUnlockDialog,
              extraStylesheet: null,
              statusArea: {
                  order: [
                      'input-method', 'a11y', 'keyboard', 'volume', 'bluetooth',
                      'network', 'battery', 'userMenu'
                  ],
                  implementation: STANDARD_STATUS_AREA_SHELL_IMPLEMENTATION
              }
            }
};

function listModes() {
    let modes = Object.getOwnPropertyNames(_modes);
    for (let i = 0; i < modes.length; i++)
        print(modes[i]);
}

const SessionMode = new Lang.Class({
    Name: 'SessionMode',

    _init: function() {
        let params = _modes[global.session_mode];

        params = Params.parse(params, _modes[DEFAULT_MODE]);

        this._createSession = params.createSession;
        delete params.createSession;
        this._createUnlockDialog = params.createUnlockDialog;
        delete params.createUnlockDialog;

        Lang.copyProperties(params, this);
    },

    createSession: function() {
        if (this._createSession)
            this._createSession();
    },

    createUnlockDialog: function() {
        if (this._createUnlockDialog)
            return this._createUnlockDialog.apply(this, arguments);
        else
            return null;
    },
});
