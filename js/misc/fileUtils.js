import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

export {loadInterfaceXML} from './dbusUtils.js';

/**
 * @typedef {object} ChildInfo
 * @property {Gio.File} file the file object for the child
 * @property {Gio.FileInfo} info the file descriptor for the child
 */

/**
 * @param {string} subdir the subdirectory to search within the data directories
 * @param {boolean} includeUserDir whether the user's data directory should also be searched in addition
 *                                 to the system data directories
 * @returns {Generator<ChildInfo, void, void>} a generator which yields child infos for subdirectories named
 *                                              `subdir` within data directories
 */
export function* collectFromDatadirs(subdir, includeUserDir) {
    let dataDirs = GLib.get_system_data_dirs();
    if (includeUserDir)
        dataDirs.unshift(GLib.get_user_data_dir());

    for (let i = 0; i < dataDirs.length; i++) {
        let path = GLib.build_filenamev([dataDirs[i], 'gnome-shell', subdir]);
        let dir = Gio.File.new_for_path(path);

        let fileEnum;
        try {
            fileEnum = dir.enumerate_children('standard::name,standard::type',
                Gio.FileQueryInfoFlags.NONE, null);
        } catch {
            fileEnum = null;
        }
        if (fileEnum != null) {
            let info;
            while ((info = fileEnum.next_file(null)))
                yield {file: fileEnum.get_child(info), info};
        }
    }
}

/**
 * @param {Gio.File} dir
 * @param {boolean} deleteParent
 */
export function recursivelyDeleteDir(dir, deleteParent) {
    let children = dir.enumerate_children('standard::name,standard::type',
        Gio.FileQueryInfoFlags.NOFOLLOW_SYMLINKS, null);

    let info;
    while ((info = children.next_file(null)) != null) {
        let type = info.get_file_type();
        let child = dir.get_child(info.get_name());
        if (type === Gio.FileType.REGULAR || type === Gio.FileType.SYMBOLIC_LINK)
            child.delete(null);
        else if (type === Gio.FileType.DIRECTORY)
            recursivelyDeleteDir(child, true);
    }

    if (deleteParent)
        dir.delete(null);
}

/**
 * @param {Gio.File} srcDir
 * @param {Gio.File} destDir
 */
export function recursivelyMoveDir(srcDir, destDir) {
    let children = srcDir.enumerate_children('standard::name,standard::type',
        Gio.FileQueryInfoFlags.NOFOLLOW_SYMLINKS, null);

    if (!destDir.query_exists(null))
        destDir.make_directory_with_parents(null);

    let info;
    while ((info = children.next_file(null)) != null) {
        let type = info.get_file_type();
        let srcChild = srcDir.get_child(info.get_name());
        let destChild = destDir.get_child(info.get_name());
        if (type === Gio.FileType.REGULAR || type === Gio.FileType.SYMBOLIC_LINK)
            srcChild.move(destChild, Gio.FileCopyFlags.NONE, null, null);
        else if (type === Gio.FileType.DIRECTORY)
            recursivelyMoveDir(srcChild, destChild);
    }
}
