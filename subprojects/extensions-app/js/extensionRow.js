import Adw from 'gi://Adw?version=1';
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
        'versionLabel',
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
                    this.get_root().uninstall(extension).catch(logError);
                },
                enabledProp: 'is-user',
            },
        ];
        this._actionGroup.add_action_entries(actionEntries);
        this._bindActionEnabled(actionEntries);

        this._switch.connect('state-set', (sw, state) => {
            const {uuid, enabled} = this._extension;
            if (enabled === state)
                return true;

            if (state)
                this._app.extensionManager.enableExtension(uuid);
            else
                this._app.extensionManager.disableExtension(uuid);
            return true;
        });

        this._extension.bind_property_full('state',
            this._switch, 'state',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, source === ExtensionState.ACTIVE],
            null);

        this._extension.bind_property_full('version',
            this._versionLabel, 'label',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, _('Version %s').format(source)],
            null);
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
});
