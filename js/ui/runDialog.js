// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const St = imports.gi.St;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const FileUtils = imports.misc.fileUtils;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;
const Tweener = imports.ui.tweener;
const Util = imports.misc.util;
const History = imports.misc.history;

const MAX_FILE_DELETED_BEFORE_INVALID = 10;

const HISTORY_KEY = 'command-history';

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const DISABLE_COMMAND_LINE_KEY = 'disable-command-line';

const TERMINAL_SCHEMA = 'org.gnome.desktop.default-applications.terminal';
const EXEC_KEY = 'exec';
const EXEC_ARG_KEY = 'exec-arg';

const DIALOG_GROW_TIME = 0.1;

const RunDialog = new Lang.Class({
    Name: 'RunDialog',
    Extends: ModalDialog.ModalDialog,

    _init : function() {
        this.parent({ styleClass: 'run-dialog',
                      destroyOnClose: false });

        this._lockdownSettings = new Gio.Settings({ schema_id: LOCKDOWN_SCHEMA });
        this._terminalSettings = new Gio.Settings({ schema_id: TERMINAL_SCHEMA });
        global.settings.connect('changed::development-tools', Lang.bind(this, function () {
            this._enableInternalCommands = global.settings.get_boolean('development-tools');
        }));
        this._enableInternalCommands = global.settings.get_boolean('development-tools');

        this._internalCommands = { 'lg':
                                   Lang.bind(this, function() {
                                       Main.createLookingGlass().open();
                                   }),

                                   'r': Lang.bind(this, function() {
                                       global.reexec_self();
                                   }),

                                   // Developer brain backwards compatibility
                                   'restart': Lang.bind(this, function() {
                                       global.reexec_self();
                                   }),

                                   'debugexit': Lang.bind(this, function() {
                                       Meta.quit(Meta.ExitCode.ERROR);
                                   }),

                                   // rt is short for "reload theme"
                                   'rt': Lang.bind(this, function() {
                                       Main.loadTheme();
                                   })
                                 };


        let label = new St.Label({ style_class: 'run-dialog-label',
                                   text: _("Enter a Command") });

        this.contentLayout.add(label, { x_fill: false,
                                        x_align: St.Align.START,
                                        y_align: St.Align.START });

        let entry = new St.Entry({ style_class: 'run-dialog-entry',
                                   can_focus: true });
        ShellEntry.addContextMenu(entry);

        entry.label_actor = label;

        this._entryText = entry.clutter_text;
        this.contentLayout.add(entry, { y_align: St.Align.START });
        this.setInitialKeyFocus(this._entryText);

        this._errorBox = new St.BoxLayout({ style_class: 'run-dialog-error-box' });

        this.contentLayout.add(this._errorBox, { expand: true });

        let errorIcon = new St.Icon({ icon_name: 'dialog-error-symbolic',
                                      icon_size: 24,
                                      style_class: 'run-dialog-error-icon' });

        this._errorBox.add(errorIcon, { y_align: St.Align.MIDDLE });

        this._commandError = false;

        this._errorMessage = new St.Label({ style_class: 'run-dialog-error-label' });
        this._errorMessage.clutter_text.line_wrap = true;

        this._errorBox.add(this._errorMessage, { expand: true,
                                                 x_align: St.Align.START,
                                                 x_fill: false,
                                                 y_align: St.Align.MIDDLE,
                                                 y_fill: false });

        this._errorBox.hide();

        this.setButtons([{ action: Lang.bind(this, this.close),
                           label: _("Close"),
                           key: Clutter.Escape }]);

        this._pathCompleter = new Gio.FilenameCompleter();

        this._history = new History.HistoryManager({ gsettingsKey: HISTORY_KEY,
                                                     entry: this._entryText });
        this._entryText.connect('key-press-event', Lang.bind(this, function(o, e) {
            let symbol = e.get_key_symbol();
            if (symbol == Clutter.Return || symbol == Clutter.KP_Enter) {
                this.popModal();
                this._run(o.get_text(),
                          e.get_state() & Clutter.ModifierType.CONTROL_MASK);
                if (!this._commandError ||
                    !this.pushModal())
                    this.close();

                return Clutter.EVENT_STOP;
            }
            if (symbol == Clutter.Tab) {
                let text = o.get_text();
                let prefix;
                if (text.lastIndexOf(' ') == -1)
                    prefix = text;
                else
                    prefix = text.substr(text.lastIndexOf(' ') + 1);
                let postfix = this._getCompletion(prefix);
                if (postfix != null && postfix.length > 0) {
                    o.insert_text(postfix, -1);
                    o.set_cursor_position(text.length + postfix.length);
                }
                return Clutter.EVENT_STOP;
            }
            return Clutter.EVENT_PROPAGATE;
        }));
    },

    _getCommandCompletion: function(text) {
        function _getCommon(s1, s2) {
            if (s1 == null)
                return s2;

            let k = 0;
            for (; k < s1.length && k < s2.length; k++) {
                if (s1[k] != s2[k])
                    break;
            }
            if (k == 0)
                return '';
            return s1.substr(0, k);
        }

        let paths = GLib.getenv('PATH').split(':');
        paths.push(GLib.get_home_dir());
        let someResults = paths.map(function(path) {
            let results = [];
            try {
                let file = Gio.File.new_for_path(path);
                let fileEnum = file.enumerate_children('standard::name', Gio.FileQueryInfoFlags.NONE, null);
                let info;
                while ((info = fileEnum.next_file(null))) {
                    let name = info.get_name();
                    if (name.slice(0, text.length) == text)
                        results.push(name);
                }
            } catch (e if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND) &&
                           !e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_DIRECTORY))) {
                log(e);
            } finally {
                return results;
            }
        });
        let results = someResults.reduce(function(a, b) {
            return a.concat(b);
        }, []);
        let common = results.reduce(_getCommon, null);
        return common.substr(text.length);
    },

    _getCompletion : function(text) {
        if (text.indexOf('/') != -1) {
            return this._pathCompleter.get_completion_suffix(text);
        } else {
            return this._getCommandCompletion(text);
        }
    },

    _run : function(input, inTerminal) {
        let command = input;

        this._history.addItem(input);
        this._commandError = false;
        let f;
        if (this._enableInternalCommands)
            f = this._internalCommands[input];
        else
            f = null;
        if (f) {
            f();
        } else if (input) {
            try {
                if (inTerminal) {
                    let exec = this._terminalSettings.get_string(EXEC_KEY);
                    let exec_arg = this._terminalSettings.get_string(EXEC_ARG_KEY);
                    command = exec + ' ' + exec_arg + ' ' + input;
                }
                Util.trySpawnCommandLine(command);
            } catch (e) {
                // Mmmh, that failed - see if @input matches an existing file
                let path = null;
                if (input.charAt(0) == '/') {
                    path = input;
                } else {
                    if (input.charAt(0) == '~')
                        input = input.slice(1);
                    path = GLib.get_home_dir() + '/' + input;
                }

                if (GLib.file_test(path, GLib.FileTest.EXISTS)) {
                    let file = Gio.file_new_for_path(path);
                    try {
                        Gio.app_info_launch_default_for_uri(file.get_uri(),
                                                            global.create_app_launch_context(0, -1));
                    } catch (e) {
                        // The exception from gjs contains an error string like:
                        //     Error invoking Gio.app_info_launch_default_for_uri: No application
                        //     is registered as handling this file
                        // We are only interested in the part after the first colon.
                        let message = e.message.replace(/[^:]*: *(.+)/, '$1');
                        this._showError(message);
                    }
                } else {
                    this._showError(e.message);
                }
            }
        }
    },

    _showError : function(message) {
        this._commandError = true;

        this._errorMessage.set_text(message);

        if (!this._errorBox.visible) {
            let [errorBoxMinHeight, errorBoxNaturalHeight] = this._errorBox.get_preferred_height(-1);

            let parentActor = this._errorBox.get_parent();
            Tweener.addTween(parentActor,
                             { height: parentActor.height + errorBoxNaturalHeight,
                               time: DIALOG_GROW_TIME,
                               transition: 'easeOutQuad',
                               onComplete: Lang.bind(this,
                                                     function() {
                                                         parentActor.set_height(-1);
                                                         this._errorBox.show();
                                                     })
                             });
        }
    },

    open: function() {
        this._history.lastItem();
        this._errorBox.hide();
        this._entryText.set_text('');
        this._commandError = false;

        if (this._lockdownSettings.get_boolean(DISABLE_COMMAND_LINE_KEY))
            return;

        this.parent();
    },
});
Signals.addSignalMethods(RunDialog.prototype);
