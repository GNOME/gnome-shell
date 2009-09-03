/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Lang = imports.lang;

const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;

const GLOW_COLOR = new Clutter.Color();
GLOW_COLOR.from_pixel(0x4f6ba4ff);
const GLOW_PADDING_HORIZONTAL = 3;
const GLOW_PADDING_VERTICAL = 3;

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

        let nameBox = new Shell.GenericContainer();
        nameBox.connect('get-preferred-width', Lang.bind(this, this._nameBoxGetPreferredWidth));
        nameBox.connect('get-preferred-height', Lang.bind(this, this._nameBoxGetPreferredHeight));
        nameBox.connect('allocate', Lang.bind(this, this._nameBoxAllocate));
        this._nameBox = nameBox;

        this._name = new Clutter.Text({ color: GenericDisplay.ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 12px",
                                        line_alignment: Pango.Alignment.CENTER,
                                        ellipsize: Pango.EllipsizeMode.END,
                                        text: appInfo.get_name() });
        nameBox.add_actor(this._name);
        this._glowBox = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
        let glowPath = GLib.filename_to_uri(Shell.Global.get().imagedir + 'app-well-glow.png', '');
        for (let i = 0; i < this._windows.length && i < 3; i++) {
            let glow = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                          glowPath, -1, -1);
            glow.keep_aspect_ratio = false;
            this._glowBox.append(glow, Big.BoxPackFlags.EXPAND);
        }
        this._nameBox.add_actor(this._glowBox);
        this._glowBox.lower(this._name);
        this.actor.append(nameBox, Big.BoxPackFlags.NONE);
    },

    _nameBoxGetPreferredWidth: function (nameBox, forHeight, alloc) {
        let [min, natural] = this._name.get_preferred_width(forHeight);
        alloc.min_size = min + GLOW_PADDING_HORIZONTAL * 2;
        alloc.natural_size = natural + GLOW_PADDING_HORIZONTAL * 2;
    },

    _nameBoxGetPreferredHeight: function (nameBox, forWidth, alloc) {
        let [min, natural] = this._name.get_preferred_height(forWidth);
        alloc.min_size = min + GLOW_PADDING_VERTICAL * 2;
        alloc.natural_size = natural + GLOW_PADDING_VERTICAL * 2;
    },

    _nameBoxAllocate: function (nameBox, box, flags) {
        let childBox = new Clutter.ActorBox();
        let [minWidth, naturalWidth] = this._name.get_preferred_width(-1);
        let [minHeight, naturalHeight] = this._name.get_preferred_height(-1);
        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;
        let targetWidth = availWidth;
        let xPadding = 0;
        if (naturalWidth < availWidth) {
            xPadding = (availWidth - naturalWidth) / 2;
        }
        childBox.x1 = Math.floor(xPadding);
        childBox.x2 = availWidth;
        childBox.y1 = GLOW_PADDING_VERTICAL;
        childBox.y2 = availHeight - GLOW_PADDING_VERTICAL;
        this._name.allocate(childBox, flags);

        // Now the glow

        if (this._glowBox != null) {
            let glowPaddingHoriz = Math.max(0, xPadding - GLOW_PADDING_HORIZONTAL);
            childBox.x1 = Math.floor(glowPaddingHoriz);
            childBox.x2 = availWidth - glowPaddingHoriz;
            childBox.y1 = 0;
            childBox.y2 = availHeight;
            this._glowBox.allocate(childBox, flags);
        }
    }
};
