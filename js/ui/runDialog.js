import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Dialog from './dialog.js';
import * as Main from './main.js';
import * as ModalDialog from './modalDialog.js';
import * as ShellEntry from './shellEntry.js';
import * as Util from '../misc/util.js';
import * as History from '../misc/history.js';

const HISTORY_KEY = 'command-history';

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const DISABLE_COMMAND_LINE_KEY = 'disable-command-line';

const TERMINAL_SCHEMA = 'org.gnome.desktop.default-applications.terminal';
const EXEC_KEY = 'exec';
const EXEC_ARG_KEY = 'exec-arg';

export const RunDialog = GObject.registerClass(
class RunDialog extends ModalDialog.ModalDialog {
    _init() {
        super._init({
            styleClass: 'run-dialog',
            destroyOnClose: false,
        });

        this._lockdownSettings = new Gio.Settings({schema_id: LOCKDOWN_SCHEMA});
        this._terminalSettings = new Gio.Settings({schema_id: TERMINAL_SCHEMA});
        global.settings.connect('changed::development-tools', () => {
            this._enableInternalCommands = global.settings.get_boolean('development-tools');
        });
        this._enableInternalCommands = global.settings.get_boolean('development-tools');

        this._internalCommands = {
            'lg': () => Main.createLookingGlass().open(),

            'r': this._restart.bind(this),

            // Developer brain backwards compatibility
            'restart': this._restart.bind(this),

            'debugexit': () => global.context.terminate(),

            // rt is short for "reload theme"
            'rt': () => {
                Main.reloadThemeResource();
                Main.loadTheme();
            },

            'check_cloexec_fds': () => {
                Shell.util_check_cloexec_fds();
            },
        };

        let title = _('Run a Command');

        let content = new Dialog.MessageDialogContent({title});
        this.contentLayout.add_child(content);
        const [labelActor] = content;

        let entry = new St.Entry({
            style_class: 'run-dialog-entry',
            labelActor,
            can_focus: true,
        });
        ShellEntry.addContextMenu(entry);

        this._entryText = entry.clutter_text;
        this._entryText.activatable = false;
        content.add_child(entry);
        this.setInitialKeyFocus(this._entryText);

        let defaultDescriptionText = _('Press ESC to close');

        this._descriptionLabel = new St.Label({
            style_class: 'run-dialog-description',
            text: defaultDescriptionText,
        });
        content.add_child(this._descriptionLabel);

        this._commandError = false;
        this._pressedKey = null;

        this._pathCompleter = new Gio.FilenameCompleter();

        this._history = new History.HistoryManager({
            gsettingsKey: HISTORY_KEY,
            entry: this._entryText,
        });
        this._entryText.connect('key-press-event', (o, e) => {
            let symbol = e.get_key_symbol();
            if (symbol === Clutter.KEY_Tab) {
                let text = o.get_text();
                let prefix;
                if (text.lastIndexOf(' ') === -1)
                    prefix = text;
                else
                    prefix = text.substring(text.lastIndexOf(' ') + 1);
                let postfix = this._getCompletion(prefix);
                if (postfix != null && postfix.length > 0) {
                    o.insert_text(postfix, -1);
                    o.set_cursor_position(text.length + postfix.length);
                }
                return Clutter.EVENT_STOP;
            } else if ([Clutter.KEY_Return, Clutter.KEY_KP_Enter, Clutter.KEY_ISO_Enter].includes(symbol)) {
                this.popModal();
                this._run(o.get_text(),
                    Clutter.get_current_event().get_state() & Clutter.ModifierType.CONTROL_MASK);
                if (!this._commandError ||
                    !this.pushModal())
                    this.close();
                return Clutter.EVENT_STOP;
            }
            return Clutter.EVENT_PROPAGATE;
        });
        this._entryText.connect('text-changed', () => {
            this._descriptionLabel.set_text(defaultDescriptionText);
        });
    }

    vfunc_key_press_event(event) {
        this._pressedKey = event.get_key_symbol();
    }

    vfunc_key_release_event(event) {
        const pressedKey = this._pressedKey;
        this._pressedKey = null;

        const key = event.get_key_symbol();
        if (key !== pressedKey)
            return Clutter.EVENT_PROPAGATE;

        if (key === Clutter.KEY_Escape) {
            this.close();
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _getCommandCompletion(text) {
        function _getCommon(s1, s2) {
            if (s1 == null)
                return s2;

            let k = 0;
            for (; k < s1.length && k < s2.length; k++) {
                if (s1[k] !== s2[k])
                    break;
            }
            if (k === 0)
                return '';
            return s1.substring(0, k);
        }

        let paths = GLib.getenv('PATH').split(':');
        paths.push(GLib.get_home_dir());
        let someResults = paths.map(path => {
            let results = [];
            try {
                let file = Gio.File.new_for_path(path);
                let fileEnum = file.enumerate_children('standard::name', Gio.FileQueryInfoFlags.NONE, null);
                let info;
                while ((info = fileEnum.next_file(null))) {
                    let name = info.get_name();
                    if (name.slice(0, text.length) === text)
                        results.push(name);
                }
            } catch (e) {
                if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND) &&
                    !e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_DIRECTORY))
                    log(e);
            }
            return results;
        });
        let results = someResults.reduce((a, b) => a.concat(b), []);

        if (!results.length)
            return null;

        let common = results.reduce(_getCommon, null);
        return common.substring(text.length);
    }

    _getCompletion(text) {
        if (text.includes('/'))
            return this._pathCompleter.get_completion_suffix(text);
        else
            return this._getCommandCompletion(text);
    }

    _run(input, inTerminal) {
        input = this._history.addItem(input); // trims input
        let command = input;

        this._commandError = false;
        let f;
        if (this._enableInternalCommands)
            f = this._internalCommands[input];
        else
            f = null;
        if (f) {
            f();
        } else {
            try {
                if (inTerminal) {
                    let exec = this._terminalSettings.get_string(EXEC_KEY);
                    let execArg = this._terminalSettings.get_string(EXEC_ARG_KEY);
                    command = `${exec} ${execArg} ${input}`;
                }
                Util.trySpawnCommandLine(command);
            } catch (e) {
                // Mmmh, that failed - see if @input matches an existing file
                let path = null;
                if (input.charAt(0) === '/') {
                    path = input;
                } else if (input) {
                    if (input.charAt(0) === '~')
                        input = input.slice(1);
                    path = `${GLib.get_home_dir()}/${input}`;
                }

                if (path && GLib.file_test(path, GLib.FileTest.EXISTS)) {
                    let file = Gio.file_new_for_path(path);
                    try {
                        Gio.app_info_launch_default_for_uri(file.get_uri(),
                            global.create_app_launch_context(0, -1));
                    } catch (err) {
                        // The exception from gjs contains an error string like:
                        //     Error invoking Gio.app_info_launch_default_for_uri: No application
                        //     is registered as handling this file
                        // We are only interested in the part after the first colon.
                        let message = err.message.replace(/[^:]*: *(.+)/, '$1');
                        this._showError(message);
                    }
                } else {
                    this._showError(e.message);
                }
            }
        }
    }

    _showError(message) {
        this._commandError = true;
        this._descriptionLabel.set_text(message);
    }

    _restart() {
        if (Meta.is_wayland_compositor()) {
            this._showError(_('Restart is not available on Wayland'));
            return;
        }
        this._shouldFadeOut = false;
        this.close();
        Meta.restart(_('Restarting…'), global.context);
    }

    open() {
        this._history.lastItem();
        this._entryText.set_text('');
        this._commandError = false;

        if (this._lockdownSettings.get_boolean(DISABLE_COMMAND_LINE_KEY))
            return false;

        return super.open();
    }
});
