// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported getDefault */

const { EosMetrics, Gio, GLib, GObject, Json, Shell } = imports.gi;

const Config = imports.misc.config;
const Main = imports.ui.main;
const ParentalControlsManager = imports.misc.parentalControlsManager;

var DESKTOP_GRID_ID = 'desktop';

const SCHEMA_KEY = 'icon-grid-layout';
const DIRECTORY_EXT = '.directory';
const FOLDER_DIR_NAME = 'desktop-directories';

const DEFAULT_CONFIGS_DIR = '%s/eos-shell-content/icon-grid-defaults'.format(Config.DATADIR);
const DEFAULT_CONFIG_NAME_BASE = 'icon-grid';

const OVERRIDE_CONFIGS_DIR = '%s/lib/eos-image-defaults/icon-grid'.format(Config.LOCALSTATEDIR);
const OVERRIDE_CONFIG_NAME_BASE = 'icon-grid';
const PREPEND_CONFIG_NAME_BASE = 'icon-grid-prepend';
const APPEND_CONFIG_NAME_BASE = 'icon-grid-append';

/* Occurs when an application is uninstalled, meaning removed from the desktop's
 * app grid. Applications can be uninstalled in the app store or via dragging
 * and dropping to the trash.
 */
const SHELL_APP_REMOVED_EVENT = '683b40a7-cac0-4f9a-994c-4b274693a0a0';

function findInArray(array, func) {
    for (let item of array) {
        if (func(item))
            return item;
    }

    return null;
}

let _singleton = null;

function getDefault() {
    if (_singleton === null)
        _singleton = new IconGridLayout();

    return _singleton;
}

var IconGridLayout = GObject.registerClass({
    Signals: { 'layout-changed': {} },
}, class IconGridLayout extends GObject.Object {
    _init() {
        super._init();

        this._parentalControlsManager = ParentalControlsManager.getDefault();
        this._parentalControlsManager.connect('app-filter-changed', () => {
            this._updateIconTree();
            this.emit('layout-changed');
        });

        this._updateIconTree();

        this._removeUndone = false;

        global.settings.connect('changed::%s'.format(SCHEMA_KEY), () => {
            this._updateIconTree();
            this.emit('layout-changed');
        });
    }

    _getIconTreeFromVariant(allIcons) {
        let iconTree = {};
        let appSys = Shell.AppSystem.get_default();

        for (let i = 0; i < allIcons.n_children(); i++) {
            let context = allIcons.get_child_value(i);
            let [folder] = context.get_child_value(0).get_string();
            let children = context.get_child_value(1).get_strv();

            children = children.filter(appId => {
                let app = appSys.lookup_alias(appId);
                if (!app)
                    return true;

                // Ensure the app is not blacklisted.
                return this._parentalControlsManager.shouldShowApp(app.get_app_info());
            });

            iconTree[folder] = children.map(appId => {
                // Some older versions of eos-app-store incorrectly added eos-app-*.desktop
                // files to the icon grid layout, instead of the proper unprefixed .desktop
                // files, which should never leak out of the Shell. Take these out of the
                // icon layout.
                if (appId.startsWith('eos-app-'))
                    return appId.slice('eos-app-'.length);

                // Some apps have their name superceded, for instance gedit -> org.gnome.gedit.
                // We want the new name, not the old one.
                let app = appSys.lookup_alias(appId);
                if (app)
                    return app.get_id();

                return appId;
            });
        }

        return iconTree;
    }

    _updateIconTree() {
        let allIcons = global.settings.get_value(SCHEMA_KEY);
        let nIcons = allIcons.n_children();
        let iconTree = this._getIconTreeFromVariant(allIcons);

        if (nIcons > 0 && !iconTree[DESKTOP_GRID_ID]) {
            // Missing toplevel desktop ID indicates we are reading a
            // corrupted setting. Reset grid to defaults, and let the logic
            // below run after the GSettings notification
            log('Corrupted icon-grid-layout detected, resetting to defaults');
            global.settings.reset(SCHEMA_KEY);
            return;
        }

        if (nIcons === 0) {
            // Entirely empty indicates that we need to read in the defaults
            allIcons = this._getDefaultIcons();
            iconTree = this._getIconTreeFromVariant(allIcons);
        }

        this._iconTree = iconTree;
    }

    _loadConfigJsonString(dir, base) {
        let jsonString = null;
        GLib.get_language_names()
            .filter(name => name.indexOf('.') === -1)
            .map(name => {
                let path = GLib.build_filenamev([dir, '%s-%s.json'.format(base, name)]);
                return Gio.File.new_for_path(path);
            })
            .some(defaultsFile => {
                try {
                    let [, data] = defaultsFile.load_contents(null);
                    jsonString = data.toString();
                    return true;
                } catch (e) {
                    // Ignore errors, as we always have a fallback
                }
                return false;
            });
        return jsonString;
    }

    _mergeJsonStrings(base, override, prepend, append) {
        let baseNode = {};
        let prependNode = null;
        let appendNode = null;
        // If any image default override matches the user's locale,
        // give that priority over the default from the base OS
        if (override)
            baseNode = JSON.parse(override);
        else if (base)
            baseNode = JSON.parse(base);

        if (prepend)
            prependNode = JSON.parse(prepend);

        if (append)
            appendNode = JSON.parse(append);

        for (let key in baseNode) {
            if (prependNode && prependNode[key])
                baseNode[key] = prependNode[key].concat(baseNode[key]);

            if (appendNode && appendNode[key])
                baseNode[key] = baseNode[key].concat(appendNode[key]);
        }
        return JSON.stringify(baseNode);
    }

    _getDefaultIcons() {
        let iconTree = null;

        try {
            let mergedJson = this._mergeJsonStrings(
                this._loadConfigJsonString(DEFAULT_CONFIGS_DIR, DEFAULT_CONFIG_NAME_BASE),
                this._loadConfigJsonString(OVERRIDE_CONFIGS_DIR, OVERRIDE_CONFIG_NAME_BASE),
                this._loadConfigJsonString(OVERRIDE_CONFIGS_DIR, PREPEND_CONFIG_NAME_BASE),
                this._loadConfigJsonString(OVERRIDE_CONFIGS_DIR, APPEND_CONFIG_NAME_BASE));
            iconTree = Json.gvariant_deserialize_data(mergedJson, -1, 'a{sas}');
        } catch (e) {
            logError(e, 'Failed to read JSON config');
        }

        if (iconTree === null || iconTree.n_children() === 0) {
            log('No icon grid defaults found!');
            // At the minimum, put in something that avoids exceptions later
            let fallback = {};
            fallback[DESKTOP_GRID_ID] = [];
            iconTree = GLib.Variant.new('a{sas}', fallback);
        }

        return iconTree;
    }

    hasIcon(id) {
        for (let folderId in this._iconTree) {
            let folder = this._iconTree[folderId];
            if (folder.indexOf(id) !== -1)
                return true;
        }

        return false;
    }

    _getIconLocation(id) {
        for (let folderId in this._iconTree) {
            let folder = this._iconTree[folderId];
            let nIcons = folder.length;

            let itemIdx = folder.indexOf(id);
            let nextId;

            if (itemIdx < nIcons) {
                nextId = folder[itemIdx + 1];
            } else {
                // append to the folder
                nextId = null;
            }

            if (itemIdx !== -1)
                return [folderId, nextId];
        }
        return null;
    }

    getIcons(folder) {
        if (this._iconTree && this._iconTree[folder])
            return this._iconTree[folder];

        return [];
    }

    iconIsFolder(id) {
        return id && id.endsWith(DIRECTORY_EXT);
    }

    appendIcon(id, folderId) {
        this.repositionIcon(id, null, folderId);
    }

    // Two operations, first insert the new icon
    // to the left of the old one, then remove
    // the old one
    //
    // defaultFolderId here refers to the folder id
    // to insert the icon into if the icon is not already
    // in a folder. Otherwise, we use the folder that
    // the icon is in already.
    replaceIcon(originalId, replacementId, defaultFolderId) {
        let folderId = findInArray(Object.keys(this._iconTree), key => {
            return this._iconTree[key].indexOf(originalId) !== -1;
        }) || defaultFolderId;

        this.repositionIcon(replacementId, originalId, folderId);
        this.removeIcon(originalId, false);
    }

    removeIcon(id, interactive) {
        if (!this.hasIcon(id))
            return;

        this._removeUndone = false;

        let undoInfo = null;
        let currentLocation = this._getIconLocation(id);
        if (currentLocation) {
            undoInfo = {
                id,
                folderId: currentLocation[0],
                insertId: currentLocation[1],
            };
        }

        this.repositionIcon(id, null, null);

        let info = null;
        if (this.iconIsFolder(id)) {
            info = Shell.DesktopDirInfo.new(id);
        } else {
            let appSystem = Shell.AppSystem.get_default();
            let app = appSystem.lookup_alias(id);
            if (app)
                info = app.get_app_info();
        }

        if (!info)
            return;

        if (interactive) {
            const options = {
                forFeedback: true,
                destroyCallback: () => this._onMessageDestroy(info),
                undoCallback: null,
            };

            // FIXME: re-enable Undo action for folders when support is implemented
            if (!this.iconIsFolder(id))
                options.undoCallback = () => this._undoRemoveItem(undoInfo);

            Main.overview.setMessage(
                _('%s has been removed').format(info.get_name()),
                options);
        } else {
            this._onMessageDestroy(info);
        }
    }

    _onMessageDestroy(info) {
        if (this._removeUndone) {
            this._removeUndone = false;
            return;
        }

        if (!this.iconIsFolder(info.get_id())) {
            let eventRecorder = EosMetrics.EventRecorder.get_default();
            let appId = new GLib.Variant('s', info.get_id());
            eventRecorder.record_event(SHELL_APP_REMOVED_EVENT, appId);
        }

        let filename = info.get_filename();
        let userDir = GLib.get_user_data_dir();
        if (filename && userDir && GLib.str_has_prefix(filename, userDir) &&
            info.get_string('X-Endless-CreatedBy') === 'eos-desktop') {
            // only delete .desktop files if they are in the user's local data
            // folder and they were created by eos-desktop
            info.delete();
        }
    }

    _undoRemoveItem(undoInfo) {
        if (undoInfo !== null)
            this.repositionIcon(undoInfo.id, undoInfo.insertId, undoInfo.folderId);

        this._removeUndone = true;
    }

    listApplications() {
        let allApplications = [];

        for (let folderId in this._iconTree) {
            let folder = this._iconTree[folderId];
            for (let iconIdx in folder) {
                let icon = folder[iconIdx];
                if (!this.iconIsFolder(icon))
                    allApplications.push(icon);
            }
        }

        return allApplications;
    }

    repositionIcon(id, insertId, newFolderId) {
        let icons;
        let existing = false;
        let isFolder = this.iconIsFolder(id);

        for (let i in this._iconTree) {
            icons = this._iconTree[i];
            let oldPos = icons.indexOf(id);
            if (oldPos !== -1) {
                icons.splice(oldPos, 1);
                existing = true;
                break;
            }
        }

        if (newFolderId !== null) {
            // We're adding or repositioning an icon
            icons = this._iconTree[newFolderId];

            // Invalid destination folder
            if (!icons)
                return;

            this._insertIcon(icons, id, insertId);

            if (isFolder && !existing) {
                // We're adding a folder, need to initialize an
                // array for its contents
                this._iconTree[id] = [];
            }
        } else if (isFolder && existing) {
            // We're removing a folder, need to delete the array
            // for its contents as well
            delete this._iconTree[id];
        }

        // Recreate GVariant from iconTree
        let newLayout = GLib.Variant.new('a{sas}', this._iconTree);

        // Store gsetting
        global.settings.set_value(SCHEMA_KEY, newLayout);
    }

    resetDesktop() {
        // Reset the gsetting to restore the default layout
        global.settings.reset(SCHEMA_KEY);

        let userPath = GLib.get_user_data_dir();
        let userDir = Gio.File.new_for_path(userPath);

        if (!userDir)
            return;

        // Remove any user-specified desktop files as consequence of
        // renaming folders (only folders), to restore all default names
        // and clean up any unused resources
        let folderDir = userDir.get_child(FOLDER_DIR_NAME);
        if (folderDir) {
            folderDir.enumerate_children_async(
                Gio.FILE_ATTRIBUTE_STANDARD_NAME,
                Gio.FileQueryInfoFlags.NONE,
                GLib.PRIORITY_DEFAULT,
                null,
                this._enumerateDirectoryFiles.bind(this));
        }
    }

    _enumerateDirectoryFiles(file, result) {
        let enumerator = file.enumerate_children_finish(result);
        enumerator.next_files_async(
            GLib.MAXINT32, GLib.PRIORITY_DEFAULT, null, this._removeDirectoryFiles);
    }

    _removeDirectoryFiles(enumerator, result) {
        let fileInfos = enumerator.next_files_finish(result);
        for (let i = 0; i < fileInfos.length; i++) {
            let fileInfo = fileInfos[i];
            let fileName = fileInfo.get_name();
            if (fileName.endsWith(DIRECTORY_EXT)) {
                let file = enumerator.get_child(fileInfo);
                file.delete_async(GLib.PRIORITY_DEFAULT, null, null);
            }
        }
    }

    // We use the insert Id instead of the index here since gsettings
    // includes the full application list that the desktop may have.
    // Relying on the position leads to faulty behaviour if some
    // apps are not present on the system
    _insertIcon(icons, id, insertId) {
        let insertIdx = -1;

        if (insertId !== null)
            insertIdx = icons.indexOf(insertId);

        // We were dropped to the left of the trashcan,
        // or we were asked to append
        if (insertIdx === -1)
            insertIdx = icons.length;

        icons.splice(insertIdx, 0, id);
    }

    _createFolderFile(name) {
        let keyFile = new GLib.KeyFile();

        keyFile.set_value(
            GLib.KEY_FILE_DESKTOP_GROUP,
            GLib.KEY_FILE_DESKTOP_KEY_NAME,
            name);

        keyFile.set_value(
            GLib.KEY_FILE_DESKTOP_GROUP,
            GLib.KEY_FILE_DESKTOP_KEY_TYPE,
            GLib.KEY_FILE_DESKTOP_TYPE_DIRECTORY);

        let dir = Gio.File.new_for_path('%s/desktop-directories'.format(GLib.get_user_data_dir()));
        try {
            dir.make_directory_with_parents(null);
        } catch (err) {
            if (!err.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.EXISTS)) {
                logError(err, 'Error creating %s'.format(dir.get_path()));
                return null;
            }
        }

        let enumerator = null;
        try {
            enumerator = dir.enumerate_children('standard::name', Gio.FileQueryInfoFlags.NONE, null);
        } catch (err) {
            logError(err, 'Error trying to traverse %s'.format(dir.get_path()));
            return null;
        }

        let prefix = 'eos-folder-user-';
        let suffix = '.directory';
        let re = new RegExp('%s([0-9]+)%s'.format(prefix, suffix));
        let folderIndex = -1;

        try {
            for (let f = enumerator.next_file(null);
                f !== null;
                f = enumerator.next_file(null)) {
                let result = re.exec(f.get_name());
                if (result) {
                    let newFolderIndex = result[1];
                    folderIndex = Math.max(folderIndex, parseInt(newFolderIndex));
                }
            }
        } catch (err) {
            logError(err, 'Error traversing %s'.format(dir.get_path()));
            return null;
        }

        try {
            enumerator.close(null);
        } catch (err) {
            logError(err, 'Error closing file enumerator for %s'.format(dir.get_path()));
            return null;
        }

        ++folderIndex;
        let filename = prefix + folderIndex + suffix;
        let absFilename = GLib.build_filenamev([dir.get_path(), filename]);

        try {
            keyFile.save_to_file(absFilename);
        } catch (err) {
            logError(err, 'Failed to save key file for directory %s'.format(absFilename));
            return null;
        }

        return filename;
    }

    addFolder(folderName, positionId = null) {
        if (!folderName)
            folderName = _('New Folder');

        let id = this._createFolderFile(folderName);
        if (!id)
            return null;

        this.repositionIcon(id, positionId, DESKTOP_GRID_ID);

        return id;
    }
});
