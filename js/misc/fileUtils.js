// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Params = imports.misc.params;

function listDirAsync(file, callback) {
    let allFiles = [];
    file.enumerate_children_async('standard::name,standard::type',
                                  Gio.FileQueryInfoFlags.NONE,
                                  GLib.PRIORITY_LOW, null, function (obj, res) {
        let enumerator = obj.enumerate_children_finish(res);
        function onNextFileComplete(obj, res) {
            let files = obj.next_files_finish(res);
            if (files.length) {
                allFiles = allFiles.concat(files);
                enumerator.next_files_async(100, GLib.PRIORITY_LOW, null, onNextFileComplete);
            } else {
                enumerator.close(null);
                callback(allFiles);
            }
        }
        enumerator.next_files_async(100, GLib.PRIORITY_LOW, null, onNextFileComplete);
    });
}

function _collectFromDirectoryAsync(dir, loadState) {
    function done() {
        loadState.numLoading--;
        if (loadState.loadedCallback &&
            loadState.numLoading == 0)
            loadState.loadedCallback(loadState.data);
    }

    dir.query_info_async('standard::type', Gio.FileQueryInfoFlags.NONE,
        GLib.PRIORITY_DEFAULT, null, function(object, res) {
            try {
                object.query_info_finish(res);
            } catch (e) {
                if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND))
                    log(e.message);
                done();
                return;
            }

            listDirAsync(dir, Lang.bind(this, function(infos) {
                for (let i = 0; i < infos.length; i++)
                    loadState.processFile(dir.get_child(infos[i].get_name()),
                                          infos[i], loadState.data);
                done();
            }));
        });
}

function collectFromDatadirsAsync(subdir, params) {
    params = Params.parse(params, { includeUserDir: false,
                                    processFile: null,
                                    loadedCallback: null,
                                    data: null });
    let loadState = { data: params.data,
                      numLoading: 0,
                      loadedCallback: params.loadedCallback,
                      processFile: params.processFile };

    if (params.processFile == null) {
        if (params.loadedCallback)
            params.loadedCallback(params.data);
        return;
    }

    let dataDirs = GLib.get_system_data_dirs();
    if (params.includeUserDir)
        dataDirs.unshift(GLib.get_user_data_dir());
    loadState.numLoading = dataDirs.length;

    for (let i = 0; i < dataDirs.length; i++) {
        let path = GLib.build_filenamev([dataDirs[i], 'gnome-shell', subdir]);
        let dir = Gio.File.new_for_path(path);

        _collectFromDirectoryAsync(dir, loadState);
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
