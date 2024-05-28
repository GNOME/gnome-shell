import Adw from 'gi://Adw';
import GObject from 'gi://GObject';

import {ExtensionBase, GettextWrapper} from './sharedInternals.js';
import {extensionManager} from '../extensionsService.js';

export class ExtensionPreferences extends ExtensionBase {
    static lookupByUUID(uuid) {
        return extensionManager.lookup(uuid)?.stateObj ?? null;
    }

    static defineTranslationFunctions(url) {
        const wrapper = new GettextWrapper(this, url);
        return wrapper.defineTranslationFunctions();
    }

    /**
     * Get the single widget that implements
     * the extension's preferences.
     *
     * @returns {Gtk.Widget|Promise<Gtk.Widget>}
     */
    getPreferencesWidget() {
        throw new GObject.NotImplementedError();
    }

    /**
     * Fill the preferences window with preferences.
     *
     * The default implementation adds the widget
     * returned by getPreferencesWidget().
     *
     * @param {Adw.PreferencesWindow} window - the preferences window
     * @returns {Promise<void>}
     */
    async fillPreferencesWindow(window) {
        const widget = await this.getPreferencesWidget();
        const page = this._wrapWidget(widget);
        window.add(page);
    }

    _wrapWidget(widget) {
        if (widget instanceof Adw.PreferencesPage)
            return widget;

        const page = new Adw.PreferencesPage();
        if (widget instanceof Adw.PreferencesGroup) {
            page.add(widget);
            return page;
        }

        const group = new Adw.PreferencesGroup();
        group.add(widget);
        page.add(group);

        return page;
    }
}

export const {
    gettext, ngettext, pgettext,
} = ExtensionPreferences.defineTranslationFunctions();
