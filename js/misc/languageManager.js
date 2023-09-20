// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import AccountsService from 'gi://AccountsService';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import * as Signals from './signals.js';
import * as Gettext from 'gettext';

export class LanguageManager extends Signals.EventEmitter {
    constructor() {
        super();

        this._userManager = AccountsService.UserManager.get_default();
        this._user = this._userManager.get_user(GLib.get_user_name());

        this._user.connectObject('changed', this._onLanguageUpdated.bind(this));
    }

    _onLanguageUpdated() {
        if (this.language === this._user.language)
            return;

        this.language = this._user.language;
        this.emit('language-changed', this._user.language);
    }

    get language() {
        return Gettext.setlocale(Gettext.LocaleCategory.ALL, null);
    }

    set language(newLanguage) {
        Gettext.setlocale(Gettext.LocaleCategory.ALL, newLanguage)
    }
}
