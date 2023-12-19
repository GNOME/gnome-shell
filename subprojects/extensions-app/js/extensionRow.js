import Adw from 'gi://Adw?version=1';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';

import {ExtensionState, ExtensionType, deserializeExtension}  from './misc/extensionUtils.js';

export const ExtensionRow = GObject.registerClass({
    GTypeName: 'ExtensionRow',
    Template: 'resource:///org/gnome/Extensions/ui/extension-row.ui',
    InternalChildren: [
        'detailsPopover',
        'descriptionLabel',
        'versionLabel',
        'errorLabel',
        'errorButton',
        'updatesButton',
        'switch',
        'actionsBox',
    ],
}, class ExtensionRow extends Adw.ActionRow {
    _init(extension) {
        super._init();

        this._app = Gio.Application.get_default();
        this._extension = extension;

        [this._keywords] = GLib.str_tokenize_and_fold(this.name, null);

        this._actionGroup = new Gio.SimpleActionGroup();
        this.insert_action_group('row', this._actionGroup);

        let action;
        action = new Gio.SimpleAction({
            name: 'show-prefs',
            enabled: this.hasPrefs,
        });
        action.connect('activate', () => {
            this._detailsPopover.popdown();
            this.get_root().openPrefs(this.uuid);
        });
        this._actionGroup.add_action(action);

        action = new Gio.SimpleAction({
            name: 'show-url',
            enabled: this.url !== '',
        });
        action.connect('activate', () => {
            this._detailsPopover.popdown();
            Gio.AppInfo.launch_default_for_uri(
                this.url, this.get_display().get_app_launch_context());
        });
        this._actionGroup.add_action(action);

        action = new Gio.SimpleAction({
            name: 'uninstall',
            enabled: this.type === ExtensionType.PER_USER,
        });
        action.connect('activate', () => {
            this._detailsPopover.popdown();
            this.get_root().uninstall(this.uuid);
        });
        this._actionGroup.add_action(action);

        action = new Gio.SimpleAction({
            name: 'enabled',
            state: new GLib.Variant('b', false),
        });
        action.connect('activate', () => {
            const state = action.get_state();
            action.change_state(new GLib.Variant('b', !state.get_boolean()));
        });

        action.connect('change-state', (a, state) => {
            if (state.get_boolean())
                this._app.shellProxy.EnableExtensionAsync(this.uuid).catch(console.error);
            else
                this._app.shellProxy.DisableExtensionAsync(this.uuid).catch(console.error);
        });
        this._actionGroup.add_action(action);

        this.title = this.name;

        const desc = this._extension.metadata.description.split('\n')[0];
        this._descriptionLabel.label = desc;

        this.connect('destroy', this._onDestroy.bind(this));

        this._extensionStateChangedId = this._app.shellProxy.connectSignal(
            'ExtensionStateChanged', (p, sender, [uuid, newState]) => {
                if (this.uuid !== uuid)
                    return;

                this._extension = deserializeExtension(newState);
                this._updateState();
            });
        this._updateState();
    }

    get uuid() {
        return this._extension.uuid;
    }

    get name() {
        return this._extension.metadata.name;
    }

    get hasPrefs() {
        return this._extension.hasPrefs;
    }

    get hasUpdate() {
        return this._extension.hasUpdate || false;
    }

    get hasError() {
        const {state} = this._extension;
        return state === ExtensionState.OUT_OF_DATE ||
               state === ExtensionState.ERROR;
    }

    get type() {
        return this._extension.type;
    }

    get creator() {
        return this._extension.metadata.creator || '';
    }

    get url() {
        return this._extension.metadata.url || '';
    }

    get version() {
        return this._extension.metadata['version-name'] || this._extension.metadata.version || '';
    }

    get error() {
        if (!this.hasError)
            return '';

        if (this._extension.state === ExtensionState.OUT_OF_DATE) {
            const {ShellVersion: shellVersion} = this._app.shellProxy;
            return this.version !== ''
                ? _('The installed version of this extension (%s) is incompatible with the current version of GNOME (%s). The extension has been disabled.').format(this.version, shellVersion)
                : _('The installed version of this extension is incompatible with the current version of GNOME (%s). The extension has been disabled.').format(shellVersion);
        }

        const message = [
            _('An error has occurred in this extension. This could cause issues elsewhere in the system. It is recommended to turn the extension off until the error is resolved.'),
        ];

        if (this._extension.error) {
            message.push(
                // translators: Details for an extension error
                _('Error details:'), this._extension.error);
        }

        return message.join('\n\n');
    }

    get keywords() {
        return this._keywords;
    }

    _updateState() {
        const state = this._extension.state === ExtensionState.ENABLED;

        const action = this._actionGroup.lookup_action('enabled');
        action.set_state(new GLib.Variant('b', state));
        action.enabled = this._extension.canChange;

        if (!action.enabled)
            this._switch.active = state;

        this._updatesButton.visible = this.hasUpdate;
        this._errorButton.visible = this.hasError;
        this._errorLabel.label = this.error;

        this._versionLabel.label = _('Version %s').format(this.version.toString());
        this._versionLabel.visible = this.version !== '';
    }

    _onDestroy() {
        if (!this._app.shellProxy)
            return;

        if (this._extensionStateChangedId)
            this._app.shellProxy.disconnectSignal(this._extensionStateChangedId);
        this._extensionStateChangedId = 0;
    }
});

