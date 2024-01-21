import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

import 'resource:///org/gnome/shell/ui/environment.js';
import * as ExtensionUtils from 'resource:///org/gnome/shell/misc/extensionUtils.js';

const fixturesDir = Gio.File.new_for_uri(`${import.meta.url}/../fixtures/extensions`);

describe('loadExtensionMetadata()', () => {
    const {loadExtensionMetadata} = ExtensionUtils;

    it('fails if directory name does not match requested UUID', () => {
        const dir = fixturesDir.get_child('valid');
        expect(() => loadExtensionMetadata('invalid', dir))
            .toThrowError(/does not match UUID/);
    });

    it('fails if metadata.json is missing', () => {
        const dir = fixturesDir.get_child('empty');
        expect(() => loadExtensionMetadata('empty', dir))
            .toThrowError(/Missing metadata/);
    });

    it('fails if metadata.json is not valid JSON', () => {
        const dir = fixturesDir.get_child('invalid');
        expect(() => loadExtensionMetadata('invalid', dir))
            .toThrowError(/parse metadata/);
    });

    it('fails if metadata.json misses "uuid" property', () => {
        const dir = fixturesDir.get_child('missing-uuid');
        expect(() => loadExtensionMetadata('missing-uuid', dir))
            .toThrowError(/missing "uuid"/);
    });

    it('fails if metadata.json misses "name" property', () => {
        const dir = fixturesDir.get_child('missing-name');
        expect(() => loadExtensionMetadata('missing-name', dir))
            .toThrowError(/missing "name"/);
    });

    it('fails if metadata.json misses "description" property', () => {
        const dir = fixturesDir.get_child('missing-description');
        expect(() => loadExtensionMetadata('missing-description', dir))
            .toThrowError(/missing "description"/);
    });

    it('fails if metadata.json misses "shell-version" property', () => {
        const dir = fixturesDir.get_child('missing-shell-version');
        expect(() => loadExtensionMetadata('missing-shell-version', dir))
            .toThrowError(/missing "shell-version"/);
    });

    it('fails if metadata.json "uuid" property is not a string', () => {
        const dir = fixturesDir.get_child('invalid-uuid');
        expect(() => loadExtensionMetadata('invalid-uuid', dir))
            .toThrowError(/"uuid" is not of type/);
    });

    it('fails if metadata.json "shell-version" property is not an array', () => {
        const dir = fixturesDir.get_child('invalid-shell-version1');
        expect(() => loadExtensionMetadata('invalid-shell-version1', dir))
            .toThrowError(/"shell-version" is not of type/);
    });

    it('fails if metadata.json "shell-version" property does not contain strings', () => {
        const dir = fixturesDir.get_child('invalid-shell-version2');
        expect(() => loadExtensionMetadata('invalid-shell-version2', dir))
            .toThrowError(/"shell-version" is not of type/);
    });

    it('fails if metadata.json "uuid" property does not match directory name', () => {
        const dir = fixturesDir.get_child('wrong-uuid');
        expect(() => loadExtensionMetadata('wrong-uuid', dir))
            .toThrowError(/does not match directory/);
    });

    it('loads valid metadata.json', () => {
        const dir = fixturesDir.get_child('valid');
        expect(() => loadExtensionMetadata('valid', dir)).not.toThrow();
    });
});

describe('serializeExtension()', () => {
    const {
        loadExtensionMetadata, serializeExtension, deserializeExtension, ExtensionType,
    } = ExtensionUtils;

    // based on ExtensionManager
    function createExtensionObject(uuid, dir, type) {
        const metadata = loadExtensionMetadata(uuid, dir);
        const extension = {
            metadata,
            uuid,
            type,
            dir,
            path: dir.get_path(),
            error: '',
            hasPrefs: dir.get_child('prefs.js').query_exists(null),
            enabled: false,
            hasUpdate: false,
            canChange: false,
            sessionModes: metadata['session-modes'] ?? ['user'],
        };
        return extension;
    }
    const uuid = 'valid';
    const ext = createExtensionObject(uuid,
        fixturesDir.get_child(uuid), ExtensionType.PER_USER);
    let serialized;

    beforeAll(() => {
        jasmine.addCustomEqualityTester((file1, file2) => {
            if (file1 instanceof Gio.File && file2 instanceof Gio.File)
                return file1.equal(file2);
            return undefined;
        });
    });

    it('produces output that can be used as variant of the expected type', () => {
        expect(() => {
            serialized = serializeExtension(ext);
        }).not.toThrow();

        let v;
        expect(() => {
            v = new GLib.Variant('a{sv}', serialized);
        }).not.toThrow();

        expect(v.is_of_type(new GLib.VariantType('a{sv}'))).toBeTrue();
    });

    it('produces output that can be deserialized', () => {
        let deserialized;
        expect(() => {
            deserialized = deserializeExtension(serialized);
        }).not.toThrow();

        expect(deserialized).toEqual(ext);
    });
});
