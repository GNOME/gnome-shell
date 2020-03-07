// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported collectFromDatadirs, recursivelyDeleteDir,
            recursivelyMoveDir, loadInterfaceXML */

const { Gio, GLib } = imports.gi;
const Config = imports.misc.config;

function collectFromDatadirs(subdir, includeUserDir, processFile) {
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
                processFile(fileEnum.get_child(info), info);
        }
    }
}

function recursivelyDeleteDir(dir, deleteParent) {
    let children = dir.enumerate_children('standard::name,standard::type',
                                          Gio.FileQueryInfoFlags.NONE, null);

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
                                             Gio.FileQueryInfoFlags.NONE, null);

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

let _ifaceResource = null;
function loadInterfaceXML(iface) {
    if (!_ifaceResource) {
        // don't use global.datadir so the method is usable from tests/tools
        let dir = GLib.getenv('GNOME_SHELL_DATADIR') || Config.PKGDATADIR;
        let path = `${dir}/gnome-shell-dbus-interfaces.gresource`;
        _ifaceResource = Gio.Resource.load(path);
        _ifaceResource._register();
    }

    let uri = `resource:///org/gnome/shell/dbus-interfaces/${iface}.xml`;
    let f = Gio.File.new_for_uri(uri);

    try {
        let [ok_, bytes] = f.load_contents(null);
        return imports.byteArray.toString(bytes);
    } catch (e) {
        log(`Failed to load D-Bus interface ${iface}`);
    }

    return null;
}
