// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ScreenTransitionService */

const { Clutter, Gio, GObject, St } = imports.gi;

const Main = imports.ui.main;

const { loadInterfaceXML } = imports.misc.fileUtils;
const { DBusSenderChecker } = imports.misc.util;

const ScreenTransitionIface = loadInterfaceXML('org.gnome.Shell.ScreenTransition');

const SCREEN_TRANSITION_DURATION = 500; // milliseconds

var ScreenTransitionService = class {
    constructor() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(ScreenTransitionIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell/ScreenTransition');

        this._senderChecker = new DBusSenderChecker([
            'org.gnome.ControlCenter',
        ]);

        Gio.DBus.session.own_name('org.gnome.Shell.ScreenTransition', Gio.BusNameOwnerFlags.REPLACE, null, null);
    }

    TransitionAsync(params, invocation) {
        try {
            this._senderChecker.checkInvocation(invocation);
        } catch (e) {
            invocation.return_gerror(e);
            return;
        }

        if (this._transition)
            this._transition.destroy();

        this._transition = new ScreenTransition(() => {
            this._transition = null;
        });
        invocation.return_value(null);
    }
};

var ScreenTransition = GObject.registerClass(
class ScreenTransition extends St.Bin {
    _init(doneCallback) {
        super._init();

        Main.uiGroup.add_actor(this);
        Main.uiGroup.set_child_above_sibling(this, null);

        const rect = new imports.gi.cairo.RectangleInt({
            x: 0,
            y: 0,
            width: global.screen_width,
            height: global.screen_height,
        });
        // FIXME if scale-monitor-framebuffer is enabled, with mixed dpi the lower dpi screen
        // will be glitchy. Should this be done for every screen separately instead?
        const [, , , scale] = global.stage.get_capture_final_size(rect);
        const content = global.stage.paint_to_content(rect, scale, Clutter.PaintFlag.NO_CURSORS);
        const clone = new St.Widget({ content });
        this.add_actor(clone);

        this.add_constraint(new Clutter.BindConstraint({
            source: Main.uiGroup,
            coordinate: Clutter.BindCoordinate.ALL,
        }));

        this.ease({
            opacity: 0,
            duration: SCREEN_TRANSITION_DURATION,
            mode: Clutter.AnimationMode.LINEAR,
            onComplete: () => {
                if (doneCallback)
                    doneCallback();
                this.destroy();
            },
        });
    }
});
