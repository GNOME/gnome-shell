/* exported KbdA11yDialog */
const { Clutter, Gio, GObject, Meta } = imports.gi;

const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;

const KEYBOARD_A11Y_SCHEMA    = 'org.gnome.desktop.a11y.keyboard';
const KEY_STICKY_KEYS_ENABLED = 'stickykeys-enable';
const KEY_SLOW_KEYS_ENABLED   = 'slowkeys-enable';

var KbdA11yDialog = GObject.registerClass(
class KbdA11yDialog extends GObject.Object {
    _init() {
        super._init();

        this._a11ySettings = new Gio.Settings({ schema_id: KEYBOARD_A11Y_SCHEMA });

        let seat = Clutter.get_default_backend().get_default_seat();
        seat.connect('kbd-a11y-flags-changed',
                     this._showKbdA11yDialog.bind(this));
    }

    _showKbdA11yDialog(seat, newFlags, whatChanged) {
        let dialog = new ModalDialog.ModalDialog();
        let title, description;
        let key, enabled;

        if (whatChanged & Meta.KeyboardA11yFlags.SLOW_KEYS_ENABLED) {
            key = KEY_SLOW_KEYS_ENABLED;
            enabled = (newFlags & Meta.KeyboardA11yFlags.SLOW_KEYS_ENABLED) > 0;
            title = enabled
                ? _("Slow Keys Turned On")
                : _("Slow Keys Turned Off");
            description = _('You just held down the Shift key for 8 seconds. This is the shortcut ' +
                            'for the Slow Keys feature, which affects the way your keyboard works.');
        } else if (whatChanged & Meta.KeyboardA11yFlags.STICKY_KEYS_ENABLED) {
            key = KEY_STICKY_KEYS_ENABLED;
            enabled = (newFlags & Meta.KeyboardA11yFlags.STICKY_KEYS_ENABLED) > 0;
            title = enabled
                ? _("Sticky Keys Turned On")
                : _("Sticky Keys Turned Off");
            description = enabled
                ? _("You just pressed the Shift key 5 times in a row. This is the shortcut " +
                  "for the Sticky Keys feature, which affects the way your keyboard works.")
                : _("You just pressed two keys at once, or pressed the Shift key 5 times in a row. " +
                  "This turns off the Sticky Keys feature, which affects the way your keyboard works.");
        } else {
            return;
        }

        let content = new Dialog.MessageDialogContent({ title, description });
        dialog.contentLayout.add_child(content);

        dialog.addButton({
            label: enabled ? _('Leave On') : _('Turn On'),
            action: () => {
                this._a11ySettings.set_boolean(key, true);
                dialog.close();
            },
            default: enabled,
            key: !enabled ? Clutter.KEY_Escape : null,
        });

        dialog.addButton({
            label: enabled ? _('Turn Off') : _('Leave Off'),
            action: () => {
                this._a11ySettings.set_boolean(key, false);
                dialog.close();
            },
            default: !enabled,
            key: enabled ? Clutter.KEY_Escape : null,
        });

        dialog.open();
    }
});
