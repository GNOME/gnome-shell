// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Params = imports.misc.params;

function collectFromDatadirs(subdir, includeUserDir, processFile) {
    let dataDirs = GLib.get_system_data_dirs();
    if (includeUserDir)
        dataDirs.unshift(GLib.get_user_data_dir());

    for (let dataDir of dataDirs) {
        let path = GLib.build_filenamev([dataDir, 'gnome-shell', subdir]);
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
                processFile(fileEnum.get_child(info), info);
        }
    }
}

function deleteGFile(file) {
    // Work around 'delete' being a keyword in JS.
    return file['delete'](null);
}

function recursivelyDeleteDir(dir, deleteParent) {
    let children = dir.enumerate_children('standard::name,standard::type',
                                          Gio.FileQueryInfoFlags.NONE, null);

    let info, child;
    while ((info = children.next_file(null)) != null) {
        let type = info.get_file_type();
        let child = dir.get_child(info.get_name());
        if (type == Gio.FileType.REGULAR)
            deleteGFile(child);
        else if (type == Gio.FileType.DIRECTORY)
            recursivelyDeleteDir(child, true);
    }

    if (deleteParent)
        deleteGFile(dir);
}

function recursivelyMoveDir(srcDir, destDir) {
    let children = srcDir.enumerate_children('standard::name,standard::type',
                                             Gio.FileQueryInfoFlags.NONE, null);

    if (!destDir.query_exists(null))
        destDir.make_directory_with_parents(null);

    let info, child;
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
