// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported ForceAppExitDialog */

const { Clutter, GObject, Gtk, Shell, St } = imports.gi;

const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;

const GNOME_SYSTEM_MONITOR_DESKTOP_ID = 'gnome-system-monitor.desktop';
const ICON_SIZE = 32;

const ForceAppExitDialogItem = GObject.registerClass({
    Signals: { 'selected': {} },
}, class ForceAppExitDialogItem extends St.BoxLayout {
    _init(app) {
        super._init({
            style_class: 'force-app-exit-dialog-item',
            can_focus: true,
            reactive: true,
            track_hover: true,
        });
        this.app = app;

        this.connect('key-focus-in', () => this.emit('selected'));
        let action = new Clutter.ClickAction();
        action.connect('clicked', this.grab_key_focus.bind(this));
        this.add_action(action);

        this._icon = this.app.create_icon_texture(ICON_SIZE);
        this.add(this._icon);

        this._label = new St.Label({
            text: this.app.get_name(),
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.label_actor = this._label;
        this.add(this._label);
    }
});

var ForceAppExitDialog = GObject.registerClass(
class ForceAppExitDialog extends ModalDialog.ModalDialog {
    _init() {
        super._init({ styleClass: 'force-app-exit-dialog' });

        let title = new St.Label({
            style_class: 'force-app-exit-dialog-header',
            text: _('Quit applications'),
        });
        this.contentLayout.add(title);

        let subtitle = new St.Label({
            style_class: 'force-app-exit-dialog-subtitle',
            text: _("If an application doesn't respond for a while, select its name and click Quit Application."),
        });
        subtitle.clutter_text.line_wrap = true;
        this.contentLayout.add(subtitle, {
            x_fill: false,
            x_align: St.Align.START,
        });

        this._itemBox = new St.BoxLayout({ vertical: true });
        this._scrollView = new St.ScrollView({
            style_class: 'force-app-exit-dialog-scroll-view',
            hscrollbar_policy: Gtk.PolicyType.NEVER,
            vscrollbar_policy: Gtk.PolicyType.AUTOMATIC,
            overlay_scrollbars: true,
            x_expand: true,
            y_expand: true,
        });
        this._scrollView.add_actor(this._itemBox);

        this.contentLayout.add(this._scrollView, { expand: true });

        this._cancelButton = this.addButton({
            action: this.close.bind(this),
            label: _('Cancel'),
            key: Clutter.KEY_Escape,
        });

        let appSystem = Shell.AppSystem.get_default();
        if (appSystem.lookup_app(GNOME_SYSTEM_MONITOR_DESKTOP_ID)) {
            this.addButton({
                action: this._launchSystemMonitor.bind(this),
                label: _('System Monitor'),
            }, {
                x_align: St.Align.END,
            });
        }

        this._quitButton = this.addButton({
            action: this._quitApp.bind(this),
            label: _('Quit Application'),
            key: Clutter.Return,
        }, {
            expand: true,
            x_fill: false,
            x_align: St.Align.END,
        });

        appSystem.get_running().forEach(app => {
            let item = new ForceAppExitDialogItem(app);
            item.connect('selected', this._selectApp.bind(this));
            this._itemBox.add_child(item);
        });

        this._selectedAppItem = null;
        this._updateSensitivity();
    }

    _updateSensitivity() {
        let quitSensitive = this._selectedAppItem !== null;
        this._quitButton.reactive = quitSensitive;
        this._quitButton.can_focus = quitSensitive;
    }

    _launchSystemMonitor() {
        let appSystem = Shell.AppSystem.get_default();
        let systemMonitor = appSystem.lookup_app(GNOME_SYSTEM_MONITOR_DESKTOP_ID);
        systemMonitor.activate();

        this.close();
        Main.overview.hide();
    }

    _quitApp() {
        let app = this._selectedAppItem.app;
        app.request_quit();
        this.close();
    }

    _selectApp(appItem) {
        if (this._selectedAppItem)
            this._selectedAppItem.remove_style_pseudo_class('selected');

        this._selectedAppItem = appItem;
        this._updateSensitivity();

        if (this._selectedAppItem)
            this._selectedAppItem.add_style_pseudo_class('selected');
    }
});
