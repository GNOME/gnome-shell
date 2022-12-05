// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported collectFromDatadirs, recursivelyDeleteDir,
            recursivelyMoveDir, loadInterfaceXML, loadSubInterfaceXML */

const { Gio, GLib } = imports.gi;

var { loadInterfaceXML } = imports.misc.dbusUtils;

/**
 * @typedef {object} SubdirInfo
 * @property {Gio.File} dir the file object for the subdir
 * @property {Gio.FileInfo} info the file descriptor for the subdir
 */

/**
 * @param {string} subdir the subdirectory to search within the data directories
 * @param {boolean} includeUserDir whether the user's data directory should also be searched in addition
 *                                 to the system data directories
 * @returns {Generator<SubdirInfo, void, void>} a generator which yields file info for subdirectories named
 *                                              `subdir` within data directories
 */
function* collectFromDatadirs(subdir, includeUserDir) {
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
        } catch (e) {
            fileEnum = null;
        }
        if (fileEnum != null) {
            let info;
            while ((info = fileEnum.next_file(null)))
                yield {dir: fileEnum.get_child(info), info};
        }
    }
}

function recursivelyDeleteDir(dir, deleteParent) {
    let children = dir.enumerate_children('standard::name,standard::type',
        Gio.FileQueryInfoFlags.NOFOLLOW_SYMLINKS, null);

    let info;
    while ((info = children.next_file(null)) != null) {
        let type = info.get_file_type();
        let child = dir.get_child(info.get_name());
        if (type == Gio.FileType.REGULAR)
            child.delete(null);
        else if (type == Gio.FileType.DIRECTORY)
            recursivelyDeleteDir(child, true);
    }

    if (deleteParent)
        dir.delete(null);
}

function recursivelyMoveDir(srcDir, destDir) {
    let children = srcDir.enumerate_children('standard::name,standard::type',
        Gio.FileQueryInfoFlags.NOFOLLOW_SYMLINKS, null);

    if (!destDir.query_exists(null))
        destDir.make_directory_with_parents(null);

    let info;
    while ((info = children.next_file(null)) != null) {
        let type = info.get_file_type();
        let srcChild = srcDir.get_child(info.get_name());
        let destChild = destDir.get_child(info.get_name());
        if (type == Gio.FileType.REGULAR)
            srcChild.move(destChild, Gio.FileCopyFlags.NONE, null, null);
        else if (type == Gio.FileType.DIRECTORY)
            recursivelyMoveDir(srcChild, destChild);
    }
}
