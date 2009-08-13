/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Lang = imports.lang;

const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;

const GLOW_COLOR = new Clutter.Color();
GLOW_COLOR.from_pixel(0x4f6ba4ff);
const GLOW_PADDING = 5;

const APP_ICON_SIZE = 48;

function AppIcon(appInfo) {
    this._init(appInfo);
}

AppIcon.prototype = {
    _init : function(appInfo) {
        this.appInfo = appInfo;

        this.actor = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                   corner_radius: 2,
                                   border: 0,
                                   padding: 1,
                                   border_color: GenericDisplay.ITEM_DISPLAY_SELECTED_BACKGROUND_COLOR,
                                   reactive: true });
        this.actor._delegate = this;

        let iconBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                    x_align: Big.BoxAlignment.CENTER,
                                    y_align: Big.BoxAlignment.CENTER });
        this._icon = appInfo.create_icon_texture(APP_ICON_SIZE);
        iconBox.append(this._icon, Big.BoxPackFlags.NONE);

        this.actor.append(iconBox, Big.BoxPackFlags.EXPAND);

        this._windows = Shell.AppMonitor.get_default().get_windows_for_app(appInfo.get_id());

        let nameBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                    x_align: Big.BoxAlignment.CENTER });
        this._nameBox = nameBox;

        this._name = new Clutter.Text({ color: GenericDisplay.ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 12px",
                                        line_alignment: Pango.Alignment.CENTER,
                                        ellipsize: Pango.EllipsizeMode.END,
                                        text: appInfo.get_name() });
        nameBox.append(this._name, Big.BoxPackFlags.NONE);
        if (this._windows.length > 0) {
            let glow = new Shell.DrawingArea({});
            glow.connect('redraw', Lang.bind(this, function (e, tex) {
                Shell.draw_app_highlight(tex,
                                         this._windows.length,
                                         GLOW_COLOR.red / 255,
                                         GLOW_COLOR.green / 255,
                                         GLOW_COLOR.blue / 255,
                                         GLOW_COLOR.alpha / 255);
            }));
            this._name.connect('notify::allocation', Lang.bind(this, function () {
                let x = this._name.x;
                let y = this._name.y;
                let width = this._name.width;
                let height = this._name.height;
                // If we're smaller than the allocated box width, pad out the glow a bit
                // to make it more visible
                if ((width + GLOW_PADDING * 2) < this._nameBox.width) {
                    width += GLOW_PADDING * 2;
                    x -= GLOW_PADDING;
                }
                glow.set_size(width, height);
                glow.set_position(x, y);
            }));
            nameBox.add_actor(glow);
            glow.lower(this._name);
        }
        this.actor.append(nameBox, Big.BoxPackFlags.NONE);
    }
};
