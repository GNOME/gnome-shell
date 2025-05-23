import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

import * as FileUtils from 'resource:///org/gnome/shell/misc/fileUtils.js';

const SUBDIR = 'collect-datadirs';

describe('collectFromDatadirs()', () => {
    const {collectFromDatadirs} = FileUtils;

    it('can collect files from all datadirs', () => {
        const files = [...collectFromDatadirs(SUBDIR, true)];
        expect(files.length).toEqual(2);
    });

    it('can collect files from only system datadirs', () => {
        const files = [...collectFromDatadirs(SUBDIR, false)];
        expect(files.length).toEqual(1);
    });

    it('returns objects with file and info properties', () => {
        const [{file, info}] = collectFromDatadirs(SUBDIR, false);
        expect(file instanceof Gio.File).toBeTrue();
        expect(info instanceof Gio.FileInfo).toBeTrue();
    });

    it('returns user files before system files', () => {
        const dataHome = GLib.get_user_data_dir();
        const [{file: first}, {file: last}] = collectFromDatadirs(SUBDIR, true);
        expect(first.get_path().startsWith(dataHome)).toBeTrue();
        expect(last.get_path().startsWith(dataHome)).toBeFalse();
    });

    it('returns file infos with standard::name and standard::type attributes', () => {
        const [{info}] = collectFromDatadirs(SUBDIR, false);

        expect(info.has_attribute('standard::name')).toBeTrue();
        expect(info.has_attribute('standard::type')).toBeTrue();
    });
});

function touchFile(parent, name) {
    const child = parent.get_child(name);
    const stream = child.create(Gio.FileCreateFlags.NONE, null);
    stream.close(null);
}

describe('recursivelyDeleteDir()', () => {
    const {recursivelyDeleteDir} = FileUtils;
    let dir;

    beforeEach(() => {
        dir = Gio.File.new_for_path(
            GLib.dir_make_tmp('gnome-shell.unit-fileUtils.XXXXXX'));
    });

    it('deletes the contents of a dir', () => {
        touchFile(dir, 'file');

        expect(() => recursivelyDeleteDir(dir, false)).not.toThrow();
        expect(dir.query_exists(null)).toBeTrue();
        dir.delete(null);
    });

    it('optionally deletes the dir itself', () => {
        touchFile(dir, 'file');

        expect(() => recursivelyDeleteDir(dir, true)).not.toThrow();
        expect(dir.query_exists(null)).toBeFalse();
    });

    it('recursively deletes subdirs', () => {
        const subdir = dir.get_child('subdir');
        subdir.make_directory(null);
        touchFile(subdir, 'file');

        expect(() => recursivelyDeleteDir(dir, true)).not.toThrow();
        expect(dir.query_exists(null)).toBeFalse();
    });

    it('handles symlinks', () => {
        touchFile(dir, 'file');

        const link = dir.get_child('link');
        link.make_symbolic_link('file', null);

        expect(() => recursivelyDeleteDir(dir, true)).not.toThrow();
        expect(dir.query_exists(null)).toBeFalse();
    });
});

describe('recursivelyMoveDir()', () => {
    const {recursivelyDeleteDir, recursivelyMoveDir} = FileUtils;
    let srcDir, dstDir;

    beforeEach(() => {
        srcDir = Gio.File.new_for_path(
            GLib.dir_make_tmp('gnome-shell.unit-fileUtils.XXXXXX'));
        dstDir = Gio.File.new_for_path(
            GLib.dir_make_tmp('gnome-shell.unit-fileUtils.XXXXXX'));
    });

    afterEach(() => {
        recursivelyDeleteDir(dstDir, true);
        recursivelyDeleteDir(srcDir, true);
    });

    it('moves the contents of a dir', () => {
        touchFile(srcDir, 'file');

        expect(() => recursivelyMoveDir(srcDir, dstDir)).not.toThrow();
        expect(dstDir.query_exists(null)).toBeTrue();
        expect(dstDir.get_child('file').query_exists(null)).toBeTrue();
    });

    it('creates the destination dir if necessary', () => {
        dstDir.delete(null);

        expect(dstDir.query_exists(null)).toBeFalse();
        expect(() => recursivelyMoveDir(srcDir, dstDir)).not.toThrow();
        expect(dstDir.query_exists(null)).toBeTrue();
    });

    it('recursively moves subdirs', () => {
        const subdir = srcDir.get_child('subdir');
        subdir.make_directory(null);
        touchFile(subdir, 'file');

        expect(() => recursivelyMoveDir(srcDir, dstDir)).not.toThrow();
        expect(dstDir.query_exists(null)).toBeTrue();

        const dstSubdir = dstDir.get_child('subdir');
        expect(dstSubdir.query_exists(null)).toBeTrue();
        expect(dstSubdir.get_child('file').query_exists(null)).toBeTrue();
    });

    it('handles symlinks', () => {
        touchFile(srcDir, 'file');

        const link = srcDir.get_child('link');
        link.make_symbolic_link('file', null);

        expect(() => recursivelyMoveDir(srcDir, dstDir)).not.toThrow();
        expect(dstDir.query_exists(null)).toBeTrue();
        expect(dstDir.get_child('link').query_exists(null)).toBeTrue();
    });
});
