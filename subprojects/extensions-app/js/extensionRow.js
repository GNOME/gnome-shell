import Adw from 'gi://Adw?version=1';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';

import {ExtensionState}  from './misc/extensionUtils.js';
import {Extension} from './extensionManager.js';

export const ExtensionRow = GObject.registerClass({
    GTypeName: 'ExtensionRow',
    Template: 'resource:///org/gnome/Extensions/ui/extension-row.ui',
    Properties: {
        'extension': GObject.ParamSpec.object(
            'extension', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            Extension),
    },
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
    constructor(extension) {
        super({extension});

        this._app = Gio.Application.get_default();

        this._actionGroup = new Gio.SimpleActionGroup();
        this.insert_action_group('row', this._actionGroup);

        const actionEntries = [
            {
                name: 'show-prefs',
                activate: () => {
                    this._detailsPopover.popdown();
                    this.get_root().openPrefs(extension);
                },
                enabledProp: 'has-prefs',
            }, {
                name: 'show-url',
                activate: () => {
                    this._detailsPopover.popdown();
                    Gio.AppInfo.launch_default_for_uri(
                        extension.url, this.get_display().get_app_launch_context());
                },
                enabledProp: 'url',
                enabledTransform: s => s !== '',
            }, {
                name: 'uninstall',
                activate: () => {
                    this._detailsPopover.popdown();
                    this.get_root().uninstall(extension);
                },
                enabledProp: 'is-user',
            }, {
                name: 'enabled',
                state: 'false',
                activate: action => {
                    const state = action.get_state();
                    action.change_state(new GLib.Variant('b', !state.get_boolean()));
                },
                change_state: (a, state) => {
                    const {uuid} = this._extension;
                    if (state.get_boolean())
                        this._app.extensionManager.enableExtension(uuid);
                    else
                        this._app.extensionManager.disableExtension(uuid);
                },
                enabledProp: 'can-change',
            },
        ];
        this._actionGroup.add_action_entries(actionEntries);
        this._bindActionEnabled(actionEntries);

        this.title = extension.name;

        this._descriptionLabel.label = extension.description;

        this.connect('destroy', this._onDestroy.bind(this));

        this._extensionStateChangedId = this._app.extensionManager.connect(
            `extension-changed::${extension.uuid}`, () => this._updateState());
        this._updateState();
    }

    get extension() {
        return this._extension ?? null;
    }

    set extension(ext) {
        this._extension = ext;
    }

    _bindActionEnabled(entries) {
        for (const entry of entries) {
            const {name, enabledProp, enabledTransform} = entry;
            if (!enabledProp)
                continue;

            const action = this._actionGroup.lookup_action(name);
            if (enabledTransform) {
                this._extension.bind_property_full(enabledProp,
                    action, 'enabled',
                    GObject.BindingFlags.SYNC_CREATE,
                    (bind, source) => [true, enabledTransform(source)],
                    null);
            } else {
                this._extension.bind_property(enabledProp,
                    action, 'enabled',
                    GObject.BindingFlags.SYNC_CREATE);
            }
        }
    }

    _updateState() {
        const state = this._extension.state === ExtensionState.ENABLED;

        const action = this._actionGroup.lookup_action('enabled');
        action.set_state(new GLib.Variant('b', state));

        if (!action.enabled)
            this._switch.active = state;

        this._updatesButton.visible = this._extension.hasUpdate;
        this._errorButton.visible = this._extension.hasError;
        this._errorLabel.label = this._extension.error;

        this._versionLabel.label = _('Version %s').format(this._extension.version);
        this._versionLabel.visible = this._extension.version !== '';
    }

    _onDestroy() {
        if (this._extensionStateChangedId)
            this._app.extensionManager.disconnect(this._extensionStateChangedId);
        delete this._extensionStateChangedId;
    }
});
