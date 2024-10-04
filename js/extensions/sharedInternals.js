import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';

import {bindtextdomain} from 'gettext';

import * as Config from '../misc/config.js';

export class ExtensionBase {
    #gettextDomain;
    #console;

    /**
     * Look up an extension by URL (usually 'import.meta.url')
     *
     * @param {string} url - a file:// URL
     */
    static lookupByURL(url) {
        if (!url.startsWith('file://'))
            return null;

        // Keep the last '/' from 'file://' to force an absolute path
        let path = url.slice(6);

        // Walk up the directory tree, looking for an extension with
        // the same UUID as a directory name.
        do {
            path = GLib.path_get_dirname(path);

            const dirName = GLib.path_get_basename(path);
            const extension = this.lookupByUUID(dirName);
            if (extension !== null)
                return extension;
        } while (path !== '/');

        return null;
    }

    /**
     * Look up an extension by UUID
     *
     * @param {string} _uuid
     */
    static lookupByUUID(_uuid) {
        throw new GObject.NotImplementedError();
    }

    /**
     * @param {object} metadata - metadata passed in when loading the extension
     */
    constructor(metadata) {
        if (this.constructor === ExtensionBase)
            throw new Error('ExtensionBase cannot be used directly.');

        if (!metadata)
            throw new Error(`${this.constructor.name} did not pass metadata to parent`);

        this.metadata = metadata;
        this.initTranslations();
    }

    /**
     * @type {string}
     */
    get uuid() {
        return this.metadata['uuid'];
    }

    /**
     * @type {Gio.File}
     */
    get dir() {
        return this.metadata['dir'];
    }

    /**
     * @type {string}
     */
    get path() {
        return this.metadata['path'];
    }

    /**
     * Get a GSettings object for schema, using schema files in
     * extensionsdir/schemas. If schema is omitted, it is taken
     * from metadata['settings-schema'].
     *
     * @param {string=} schema - the GSettings schema id
     *
     * @returns {Gio.Settings}
     */
    getSettings(schema) {
        schema ||= this.metadata['settings-schema'];

        // Expect USER extensions to have a schemas/ subfolder, otherwise assume a
        // SYSTEM extension that has been installed in the same prefix as the shell
        const schemaDir = this.dir.get_child('schemas');
        const defaultSource = Gio.SettingsSchemaSource.get_default();
        let schemaSource;
        if (schemaDir.query_exists(null)) {
            schemaSource = Gio.SettingsSchemaSource.new_from_directory(
                schemaDir.get_path(), defaultSource, false);
        } else {
            schemaSource = defaultSource;
        }

        const schemaObj = schemaSource.lookup(schema, true);
        if (!schemaObj)
            throw new Error(`Schema ${schema} could not be found for extension ${this.uuid}. Please check your installation`);

        return new Gio.Settings({settings_schema: schemaObj});
    }

    /**
     * @returns {Console}
     */
    getLogger() {
        if (!this.#console)
            this.#console = new Console(this);
        return this.#console;
    }

    /**
     * Initialize Gettext to load translations from extensionsdir/locale. If
     * domain is not provided, it will be taken from metadata['gettext-domain']
     * if provided, or use the UUID
     *
     * @param {string=} domain - the gettext domain to use
     */
    initTranslations(domain) {
        domain ||= this.metadata['gettext-domain'] ?? this.uuid;

        // Expect USER extensions to have a locale/ subfolder, otherwise assume a
        // SYSTEM extension that has been installed in the same prefix as the shell
        const localeDir = this.dir.get_child('locale');
        if (localeDir.query_exists(null))
            bindtextdomain(domain, localeDir.get_path());
        else
            bindtextdomain(domain, Config.LOCALEDIR);

        this.#gettextDomain = domain;
    }

    /**
     * Translate `str` using the extension's gettext domain
     *
     * @param {string} str - the string to translate
     *
     * @returns {string} the translated string
     */
    gettext(str) {
        this.#checkGettextDomain('gettext');
        return GLib.dgettext(this.#gettextDomain, str);
    }

    /**
     * Translate `str` and choose plural form using the extension's
     * gettext domain
     *
     * @param {string} str - the string to translate
     * @param {string} strPlural - the plural form of the string
     * @param {number} n - the quantity for which translation is needed
     *
     * @returns {string} the translated string
     */
    ngettext(str, strPlural, n) {
        this.#checkGettextDomain('ngettext');
        return GLib.dngettext(this.#gettextDomain, str, strPlural, n);
    }

    /**
     * Translate `str` in the context of `context` using the extension's
     * gettext domain
     *
     * @param {string} context - context to disambiguate `str`
     * @param {string} str - the string to translate
     *
     * @returns {string} the translated string
     */
    pgettext(context, str) {
        this.#checkGettextDomain('pgettext');
        return GLib.dpgettext2(this.#gettextDomain, context, str);
    }

    /**
     * @param {string} func
     */
    #checkGettextDomain(func) {
        if (!this.#gettextDomain)
            throw new Error(`${func}() is used without calling initTranslations() first`);
    }
}

export class GettextWrapper {
    #url;
    #extensionClass;

    constructor(extensionClass, url) {
        this.#url = url;
        this.#extensionClass = extensionClass;
    }

    #detectUrl() {
        const basePath = '/gnome-shell/extensions/';

        // Search for an occurrence of an extension stack frame
        // Start at 1 because 0 is the stack frame of this function
        const [, ...stack] = new Error().stack.split('\n');
        const extensionLine = stack.find(
            line => line.includes(basePath));

        if (!extensionLine)
            return null;

        // The exact stack line differs depending on where the function
        // was called (function or module scope), and whether it's called
        // from a module or legacy import (file:// URI vs. plain path).
        //
        // We don't have to care about the exact composition, all we need
        // is a string that can be traversed as path and contains the UUID
        const path = extensionLine.slice(extensionLine.indexOf(basePath));
        return `file://${path}`;
    }

    #lookupExtension(funcName) {
        const url = this.#url ?? this.#detectUrl();
        const extension = this.#extensionClass.lookupByURL(url);
        if (!extension)
            throw new Error(`${funcName} can only be called from extensions`);
        return extension;
    }

    #gettext(str) {
        const extension = this.#lookupExtension('gettext');
        return extension.gettext(str);
    }

    #ngettext(str, strPlural, n) {
        const extension = this.#lookupExtension('ngettext');
        return extension.ngettext(str, strPlural, n);
    }

    #pgettext(context, str) {
        const extension = this.#lookupExtension('pgettext');
        return extension.pgettext(context, str);
    }

    defineTranslationFunctions() {
        return {
            /**
             * Translate `str` using the extension's gettext domain
             *
             * @param {string} str - the string to translate
             *
             * @returns {string} the translated string
             */
            gettext: this.#gettext.bind(this),

            /**
             * Translate `str` and choose plural form using the extension's
             * gettext domain
             *
             * @param {string} str - the string to translate
             * @param {string} strPlural - the plural form of the string
             * @param {number} n - the quantity for which translation is needed
             *
             * @returns {string} the translated string
             */
            ngettext: this.#ngettext.bind(this),

            /**
             * Translate `str` in the context of `context` using the extension's
             * gettext domain
             *
             * @param {string} context - context to disambiguate `str`
             * @param {string} str - the string to translate
             *
             * @returns {string} the translated string
             */
            pgettext: this.#pgettext.bind(this),
        };
    }
}

class Console {
    #extension;

    constructor(ext) {
        this.#extension = ext;
    }

    #prefixArgs(first, ...args) {
        return [`[${this.#extension.metadata.name}] ${first}`, ...args];
    }

    log(...args) {
        globalThis.console.log(...this.#prefixArgs(...args));
    }

    warn(...args) {
        globalThis.console.warn(...this.#prefixArgs(...args));
    }

    error(...args) {
        globalThis.console.error(...this.#prefixArgs(...args));
    }

    info(...args) {
        globalThis.console.info(...this.#prefixArgs(...args));
    }

    debug(...args) {
        globalThis.console.debug(...this.#prefixArgs(...args));
    }

    assert(condition, ...args) {
        if (condition)
            return;

        const message = 'Assertion failed';

        if (args.length === 0)
            args.push(message);

        if (typeof args[0] !== 'string') {
            args.unshift(message);
        } else {
            const first = args.shift();
            args.unshift(`${message}: ${first}`);
        }
        globalThis.console.error(...this.#prefixArgs(...args));
    }

    trace(...args) {
        if (args.length === 0)
            args = ['Trace'];

        globalThis.console.trace(...this.#prefixArgs(...args));
    }

    group(...args) {
        globalThis.console.group(...this.#prefixArgs(...args));
    }

    groupEnd() {
        globalThis.console.groupEnd();
    }
}
