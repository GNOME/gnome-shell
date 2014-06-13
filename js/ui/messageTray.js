// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Atk = imports.gi.Atk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Tp = imports.gi.TelepathyGLib;

const BoxPointer = imports.ui.boxpointer;
const CtrlAltTab = imports.ui.ctrlAltTab;
const GnomeSession = imports.misc.gnomeSession;
const GrabHelper = imports.ui.grabHelper;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const PointerWatcher = imports.ui.pointerWatcher;
const PopupMenu = imports.ui.popupMenu;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;
const Util = imports.misc.util;

const SHELL_KEYBINDINGS_SCHEMA = 'org.gnome.shell.keybindings';

const ANIMATION_TIME = 0.2;
const NOTIFICATION_TIMEOUT = 4;

const HIDE_TIMEOUT = 0.2;
const LONGER_HIDE_TIMEOUT = 0.6;

// We delay hiding of the tray if the mouse is within MOUSE_LEFT_ACTOR_THRESHOLD
// range from the point where it left the tray.
const MOUSE_LEFT_ACTOR_THRESHOLD = 20;

// Time the user needs to leave the mouse on the bottom pixel row to open the tray
const TRAY_DWELL_TIME = 1000; // ms
// Time resolution when tracking the mouse to catch the open tray dwell
const TRAY_DWELL_CHECK_INTERVAL = 100; // ms

const IDLE_TIME = 1000;

const State = {
    HIDDEN:  0,
    SHOWING: 1,
    SHOWN:   2,
    HIDING:  3
};

// These reasons are useful when we destroy the notifications received through
// the notification daemon. We use EXPIRED for transient notifications that the
// user did not interact with, DISMISSED for all other notifications that were
// destroyed as a result of a user action, and SOURCE_CLOSED for the notifications
// that were requested to be destroyed by the associated source.
const NotificationDestroyedReason = {
    EXPIRED: 1,
    DISMISSED: 2,
    SOURCE_CLOSED: 3
};

// Message tray has its custom Urgency enumeration. LOW, NORMAL and CRITICAL
// urgency values map to the corresponding values for the notifications received
// through the notification daemon. HIGH urgency value is used for chats received
// through the Telepathy client.
const Urgency = {
    LOW: 0,
    NORMAL: 1,
    HIGH: 2,
    CRITICAL: 3
};

function _fixMarkup(text, allowMarkup) {
    if (allowMarkup) {
        // Support &amp;, &quot;, &apos;, &lt; and &gt;, escape all other
        // occurrences of '&'.
        let _text = text.replace(/&(?!amp;|quot;|apos;|lt;|gt;)/g, '&amp;');

        // Support <b>, <i>, and <u>, escape anything else
        // so it displays as raw markup.
        _text = _text.replace(/<(?!\/?[biu]>)/g, '&lt;');

        try {
            Pango.parse_markup(_text, -1, '');
            return _text;
        } catch (e) {}
    }

    // !allowMarkup, or invalid markup
    return GLib.markup_escape_text(text, -1);
}

const URLHighlighter = new Lang.Class({
    Name: 'URLHighlighter',

    _init: function(text, lineWrap, allowMarkup) {
        if (!text)
            text = '';
        this.actor = new St.Label({ reactive: true, style_class: 'url-highlighter' });
        this._linkColor = '#ccccff';
        this.actor.connect('style-changed', Lang.bind(this, function() {
            let [hasColor, color] = this.actor.get_theme_node().lookup_color('link-color', false);
            if (hasColor) {
                let linkColor = color.to_string().substr(0, 7);
                if (linkColor != this._linkColor) {
                    this._linkColor = linkColor;
                    this._highlightUrls();
                }
            }
        }));
        if (lineWrap) {
            this.actor.clutter_text.line_wrap = true;
            this.actor.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;
            this.actor.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        }

        this.setMarkup(text, allowMarkup);
        this.actor.connect('button-press-event', Lang.bind(this, function(actor, event) {
            // Don't try to URL highlight when invisible.
            // The MessageTray doesn't actually hide us, so
            // we need to check for paint opacities as well.
            if (!actor.visible || actor.get_paint_opacity() == 0)
                return Clutter.EVENT_PROPAGATE;

            // Keep Notification.actor from seeing this and taking
            // a pointer grab, which would block our button-release-event
            // handler, if an URL is clicked
            return this._findUrlAtPos(event) != -1;
        }));
        this.actor.connect('button-release-event', Lang.bind(this, function (actor, event) {
            if (!actor.visible || actor.get_paint_opacity() == 0)
                return Clutter.EVENT_PROPAGATE;

            let urlId = this._findUrlAtPos(event);
            if (urlId != -1) {
                let url = this._urls[urlId].url;
                if (url.indexOf(':') == -1)
                    url = 'http://' + url;

                Gio.app_info_launch_default_for_uri(url, global.create_app_launch_context(0, -1));
                return Clutter.EVENT_STOP;
            }
            return Clutter.EVENT_PROPAGATE;
        }));
        this.actor.connect('motion-event', Lang.bind(this, function(actor, event) {
            if (!actor.visible || actor.get_paint_opacity() == 0)
                return Clutter.EVENT_PROPAGATE;

            let urlId = this._findUrlAtPos(event);
            if (urlId != -1 && !this._cursorChanged) {
                global.screen.set_cursor(Meta.Cursor.POINTING_HAND);
                this._cursorChanged = true;
            } else if (urlId == -1) {
                global.screen.set_cursor(Meta.Cursor.DEFAULT);
                this._cursorChanged = false;
            }
            return Clutter.EVENT_PROPAGATE;
        }));
        this.actor.connect('leave-event', Lang.bind(this, function() {
            if (!this.actor.visible || this.actor.get_paint_opacity() == 0)
                return Clutter.EVENT_PROPAGATE;

            if (this._cursorChanged) {
                this._cursorChanged = false;
                global.screen.set_cursor(Meta.Cursor.DEFAULT);
            }
            return Clutter.EVENT_PROPAGATE;
        }));
    },

    hasText: function() {
        return !!this._text;
    },

    setMarkup: function(text, allowMarkup) {
        text = text ? _fixMarkup(text, allowMarkup) : '';
        this._text = text;

        this.actor.clutter_text.set_markup(text);
        /* clutter_text.text contain text without markup */
        this._urls = Util.findUrls(this.actor.clutter_text.text);
        this._highlightUrls();
    },

    _highlightUrls: function() {
        // text here contain markup
        let urls = Util.findUrls(this._text);
        let markup = '';
        let pos = 0;
        for (let i = 0; i < urls.length; i++) {
            let url = urls[i];
            let str = this._text.substr(pos, url.pos - pos);
            markup += str + '<span foreground="' + this._linkColor + '"><u>' + url.url + '</u></span>';
            pos = url.pos + url.url.length;
        }
        markup += this._text.substr(pos);
        this.actor.clutter_text.set_markup(markup);
    },

    _findUrlAtPos: function(event) {
        let success;
        let [x, y] = event.get_coords();
        [success, x, y] = this.actor.transform_stage_point(x, y);
        let find_pos = -1;
        for (let i = 0; i < this.actor.clutter_text.text.length; i++) {
            let [success, px, py, line_height] = this.actor.clutter_text.position_to_coords(i);
            if (py > y || py + line_height < y || x < px)
                continue;
            find_pos = i;
        }
        if (find_pos != -1) {
            for (let i = 0; i < this._urls.length; i++)
            if (find_pos >= this._urls[i].pos &&
                this._urls[i].pos + this._urls[i].url.length > find_pos)
                return i;
        }
        return -1;
    }
});

// NotificationPolicy:
// An object that holds all bits of configurable policy related to a notification
// source, such as whether to play sound or honour the critical bit.
//
// A notification without a policy object will inherit the default one.
const NotificationPolicy = new Lang.Class({
    Name: 'NotificationPolicy',

    _init: function(params) {
        params = Params.parse(params, { enable: true,
                                        enableSound: true,
                                        showBanners: true,
                                        forceExpanded: false,
                                        showInLockScreen: true,
                                        detailsInLockScreen: false
                                      });
        Lang.copyProperties(params, this);
    },

    // Do nothing for the default policy. These methods are only useful for the
    // GSettings policy.
    store: function() { },
    destroy: function() { }
});
Signals.addSignalMethods(NotificationPolicy.prototype);

const NotificationGenericPolicy = new Lang.Class({
    Name: 'NotificationGenericPolicy',
    Extends: NotificationPolicy,

    _init: function() {
        // Don't chain to parent, it would try setting
        // our properties to the defaults

        this.id = 'generic';

        this._masterSettings = new Gio.Settings({ schema: 'org.gnome.desktop.notifications' });
        this._masterSettings.connect('changed', Lang.bind(this, this._changed));
    },

    store: function() { },

    destroy: function() {
        this._masterSettings.run_dispose();
    },

    _changed: function(settings, key) {
        this.emit('policy-changed', key);
    },

    get enable() {
        return true;
    },

    get enableSound() {
        return true;
    },

    get showBanners() {
        return this._masterSettings.get_boolean('show-banners');
    },

    get forceExpanded() {
        return false;
    },

    get showInLockScreen() {
        return this._masterSettings.get_boolean('show-in-lock-screen');
    },

    get detailsInLockScreen() {
        return false;
    }
});

const NotificationApplicationPolicy = new Lang.Class({
    Name: 'NotificationApplicationPolicy',
    Extends: NotificationPolicy,

    _init: function(id) {
        // Don't chain to parent, it would try setting
        // our properties to the defaults

        this.id = id;
        this._canonicalId = this._canonicalizeId(id);

        this._masterSettings = new Gio.Settings({ schema: 'org.gnome.desktop.notifications' });
        this._settings = new Gio.Settings({ schema: 'org.gnome.desktop.notifications.application',
                                            path: '/org/gnome/desktop/notifications/application/' + this._canonicalId + '/' });

        this._masterSettings.connect('changed', Lang.bind(this, this._changed));
        this._settings.connect('changed', Lang.bind(this, this._changed));
    },

    store: function() {
        this._settings.set_string('application-id', this.id + '.desktop');

        let apps = this._masterSettings.get_strv('application-children');
        if (apps.indexOf(this._canonicalId) < 0) {
            apps.push(this._canonicalId);
            this._masterSettings.set_strv('application-children', apps);
        }
    },

    destroy: function() {
        this._masterSettings.run_dispose();
        this._settings.run_dispose();
    },

    _changed: function(settings, key) {
        this.emit('policy-changed', key);
    },

    _canonicalizeId: function(id) {
        // Keys are restricted to lowercase alphanumeric characters and dash,
        // and two dashes cannot be in succession
        return id.toLowerCase().replace(/[^a-z0-9\-]/g, '-').replace(/--+/g, '-');
    },

    get enable() {
        return this._settings.get_boolean('enable');
    },

    get enableSound() {
        return this._settings.get_boolean('enable-sound-alerts');
    },

    get showBanners() {
        return this._masterSettings.get_boolean('show-banners') &&
            this._settings.get_boolean('show-banners');
    },

    get forceExpanded() {
        return this._settings.get_boolean('force-expanded');
    },

    get showInLockScreen() {
        return this._masterSettings.get_boolean('show-in-lock-screen') &&
            this._settings.get_boolean('show-in-lock-screen');
    },

    get detailsInLockScreen() {
        return this._settings.get_boolean('details-in-lock-screen');
    }
});

// Notification:
// @source: the notification's Source
// @title: the title
// @banner: the banner text
// @params: optional additional params
//
// Creates a notification. In the banner mode, the notification
// will show an icon, @title (in bold) and @banner, all on a single
// line (with @banner ellipsized if necessary).
//
// The notification will be expandable if either it has additional
// elements that were added to it or if the @banner text did not
// fit fully in the banner mode. When the notification is expanded,
// the @banner text from the top line is always removed. The complete
// @banner text is added to the notification by default. You can change
// what is displayed by setting the child of this._bodyBin.
//
// You can also add buttons to the notification with addButton(),
// and you can construct simple default buttons with addAction().
//
// By default, the icon shown is the same as the source's.
// However, if @params contains a 'gicon' parameter, the passed in gicon
// will be used.
//
// You can add a secondary icon to the banner with 'secondaryGIcon'. There
// is no fallback for this icon.
//
// If @params contains 'bannerMarkup', with the value %true, then
// the corresponding element is assumed to use pango markup. If the
// parameter is not present for an element, then anything that looks
// like markup in that element will appear literally in the output.
//
// If @params contains a 'clear' parameter with the value %true, then
// the content and the action area of the notification will be cleared.
//
// If @params contains 'soundName' or 'soundFile', the corresponding
// event sound is played when the notification is shown (if the policy for
// @source allows playing sounds).
const Notification = new Lang.Class({
    Name: 'Notification',

    ICON_SIZE: 32,

    _init: function(source, title, banner, params) {
        this.source = source;
        this.title = title;
        this.urgency = Urgency.NORMAL;
        this.isMusic = false;
        this.forFeedback = false;
        this.focused = false;
        this.acknowledged = false;
        this._destroyed = false;
        this._soundName = null;
        this._soundFile = null;
        this._soundPlayed = false;

        // Let me draw you a picture. I am a bad artist:
        //
        //      ,. this._iconBin         ,. this._titleLabel
        //      |        ,-- this._second|ryIconBin
        // .----|--------|---------------|-----------.
        // | .----. | .----.-------------------. | X |
        // | |    | | |    |                   |-|----- this._titleBox
        // | '....' | '....'...................' |   |
        // |        |                            |   |- this._hbox
        // |        |        this._bodyBin       |   |-.
        // |________|____________________________|___| |- this.actor
        // | this._actionArea                        |-'
        // |_________________________________________|
        // | this._buttonBox                         |
        // |_________________________________________|

        this.actor = new St.BoxLayout({ vertical: true,
                                        style_class: 'notification',
                                        accessible_role: Atk.Role.NOTIFICATION });
        this.actor._delegate = this;
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._mainButton = new St.Button({ style_class: 'notification-main-button',
                                           can_focus: true,
                                           x_fill: true, y_fill: true });
        this._mainButton.connect('clicked', Lang.bind(this, this._onClicked));
        this.actor.add_child(this._mainButton);

        // Separates the icon, title/body and close button
        this._hbox = new St.BoxLayout({ style_class: 'notification-main-content' });
        this._mainButton.child = this._hbox;

        this._iconBin = new St.Bin({ y_align: St.Align.START });
        this._hbox.add_child(this._iconBin);

        this._titleBodyBox = new St.BoxLayout({ style_class: 'notification-title-body-box',
                                                vertical: true });
        this._titleBodyBox.set_x_expand(true);
        this._hbox.add_child(this._titleBodyBox);

        this._closeButton = new St.Button({ style_class: 'notification-close-button',
                                            can_focus: true });
        this._closeButton.set_y_align(Clutter.ActorAlign.START);
        this._closeButton.set_y_expand(true);
        this._closeButton.child = new St.Icon({ icon_name: 'window-close-symbolic', icon_size: 16 });
        this._closeButton.connect('clicked', Lang.bind(this, this._onCloseClicked));
        this._hbox.add_child(this._closeButton);

        this._titleBox = new St.BoxLayout({ style_class: 'notification-title-box',
                                            x_expand: true, x_align: Clutter.ActorAlign.START });
        this._secondaryIconBin = new St.Bin();
        this._titleBox.add_child(this._secondaryIconBin);
        this._titleLabel = new St.Label({ x_expand: true });
        this._titleLabel.clutter_text.line_wrap = true;
        this._titleLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._titleBox.add_child(this._titleLabel);
        this._titleBodyBox.add(this._titleBox);

        this._bodyScrollArea = new St.ScrollView({ style_class: 'notification-scrollview',
                                                   hscrollbar_policy: Gtk.PolicyType.NEVER });
        this._titleBodyBox.add(this._bodyScrollArea);

        this._bodyScrollable = new St.BoxLayout();
        this._bodyScrollArea.add_actor(this._bodyScrollable);

        this._bodyBin = new St.Bin();
        this._bodyScrollable.add_actor(this._bodyBin);

        // By default, this._bodyBin contains a URL highlighter. Subclasses
        // can override this to provide custom content if they want to.
        this._bodyUrlHighlighter = new URLHighlighter();
        this._bodyUrlHighlighter.actor.clutter_text.line_wrap = true;
        this._bodyUrlHighlighter.actor.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._bodyBin.child = this._bodyUrlHighlighter.actor;

        this._actionAreaBin = new St.Bin({ style_class: 'notification-action-area',
                                           x_expand: true, y_expand: true });
        this.actor.add_child(this._actionAreaBin);

        this._buttonBox = new St.BoxLayout({ style_class: 'notification-button-box',
                                             x_expand: true, y_expand: true });
        global.focus_manager.add_group(this._buttonBox);
        this.actor.add_child(this._buttonBox);

        // If called with only one argument we assume the caller
        // will call .update() later on. This is the case of
        // NotificationDaemon, which wants to use the same code
        // for new and updated notifications
        if (arguments.length != 1)
            this.update(title, banner, params);

        this._sync();
    },

    _sync: function() {
        this._iconBin.visible = (this._icon != null && this._icon.visible);
        this._secondaryIconBin.visible = (this._secondaryIcon != null);

        this._actionAreaBin.visible = (this._actionAreaBin.child != null);
        this._buttonBox.visible = (this._buttonBox.get_n_children() > 0);

        this._bodyUrlHighlighter.actor.visible = this._bodyUrlHighlighter.hasText();
    },

    // update:
    // @title: the new title
    // @banner: the new banner
    // @params: as in the Notification constructor
    //
    // Updates the notification by regenerating its icon and updating
    // the title/banner. If @params.clear is %true, it will also
    // remove any additional actors/action buttons previously added.
    update: function(title, banner, params) {
        params = Params.parse(params, { gicon: null,
                                        secondaryGIcon: null,
                                        bannerMarkup: false,
                                        clear: false,
                                        soundName: null,
                                        soundFile: null });

        let oldFocus = global.stage.key_focus;

        if (this._actionArea && params.clear) {
            if (oldFocus && this._actionArea.contains(oldFocus))
                this.actor.grab_key_focus();

            this._actionArea.destroy();
            this._actionArea = null;
        }

        if (params.clear) {
            this._buttonBox.destroy_all_children();
        }

        this.gicon = params.gicon;

        if (this._icon && (params.gicon || params.clear)) {
            this._icon.destroy();
            this._icon = null;
        }

        if (params.gicon) {
            this._icon = new St.Icon({ gicon: params.gicon,
                                       icon_size: this.ICON_SIZE });
        } else {
            this._icon = this.source.createIcon(this.ICON_SIZE);
        }

        if (this._icon)
            this._iconBin.child = this._icon;

        if (this._secondaryIcon && (params.secondaryGIcon || params.clear)) {
            this._secondaryIcon.destroy();
            this._secondaryIcon = null;
        }

        if (params.secondaryGIcon) {
            this._secondaryIcon = new St.Icon({ gicon: params.secondaryGIcon,
                                                style_class: 'secondary-icon' });
            this._secondaryIconBin.child = this._secondaryIcon;
        }

        this.title = title;
        title = title ? _fixMarkup(title.replace(/\n/g, ' '), false) : '';
        this._titleLabel.clutter_text.set_markup('<b>' + title + '</b>');

        let titleDirection;
        if (Pango.find_base_dir(title, -1) == Pango.Direction.RTL)
            titleDirection = Clutter.TextDirection.RTL;
        else
            titleDirection = Clutter.TextDirection.LTR;

        // Let the title's text direction control the overall direction
        // of the notification - in case where different scripts are used
        // in the notification, this is the right thing for the icon, and
        // arguably for action buttons as well. Labels other than the title
        // will be allocated at the available width, so that their alignment
        // is done correctly automatically.
        this.actor.set_text_direction(titleDirection);

        this.bannerBodyText = banner;

        this._bodyUrlHighlighter.setMarkup(banner, params.bannerMarkup);

        if (this._soundName != params.soundName ||
            this._soundFile != params.soundFile) {
            this._soundName = params.soundName;
            this._soundFile = params.soundFile;
            this._soundPlayed = false;
        }

        this._sync();
    },

    setIconVisible: function(visible) {
        this._icon.visible = visible;
        this._sync();
    },

    enableScrolling: function(enableScrolling) {
        let scrollPolicy = enableScrolling ? Gtk.PolicyType.AUTOMATIC : Gtk.PolicyType.NEVER;
        this._bodyScrollArea.vscrollbar_policy = scrollPolicy;
        this._bodyScrollArea.enable_mouse_scrolling = enableScrolling;
    },

    // scrollTo:
    // @side: St.Side.TOP or St.Side.BOTTOM
    //
    // Scrolls the content area (if scrollable) to the indicated edge
    scrollTo: function(side) {
        let adjustment = this._bodyScrollArea.vscroll.adjustment;
        if (side == St.Side.TOP)
            adjustment.value = adjustment.lower;
        else if (side == St.Side.BOTTOM)
            adjustment.value = adjustment.upper;
    },

    setActionArea: function(actor) {
        if (this._actionArea)
            this._actionArea.destroy();

        this._actionArea = actor;
        this._actionAreaBin.child = actor;
        this._sync();
    },

    addButton: function(button, callback) {
        this._buttonBox.add(button);
        button.connect('clicked', Lang.bind(this, function() {
            callback();

            this.emit('done-displaying');
            this.destroy();
        }));

        this._sync();
        return button;
    },

    // addAction:
    // @label: the label for the action's button
    // @callback: the callback for the action
    //
    // Adds a button with the given @label to the notification. All
    // action buttons will appear in a single row at the bottom of
    // the notification.
    addAction: function(label, callback) {
        let button = new St.Button({ style_class: 'notification-button',
                                     x_expand: true, label: label, can_focus: true });

        return this.addButton(button, callback);
    },

    setUrgency: function(urgency) {
        this.urgency = urgency;
    },

    setForFeedback: function(forFeedback) {
        this.forFeedback = forFeedback;
    },

    _styleChanged: function() {
        this._spacing = this._table.get_theme_node().get_length('spacing-columns');
    },

    _bannerBoxGetPreferredWidth: function(actor, forHeight, alloc) {
        let [titleMin, titleNat] = this._titleLabel.get_preferred_width(forHeight);
        let [bannerMin, bannerNat] = this._bannerLabel.get_preferred_width(forHeight);

        if (this._secondaryIcon) {
            let [secondaryIconMin, secondaryIconNat] = this._secondaryIcon.get_preferred_width(forHeight);

            alloc.min_size = secondaryIconMin + this._spacing + titleMin;
            alloc.natural_size = secondaryIconNat + this._spacing + titleNat + this._spacing + bannerNat;
        } else {
            alloc.min_size = titleMin;
            alloc.natural_size = titleNat + this._spacing + bannerNat;
        }
    },

    playSound: function() {
        if (this._soundPlayed)
            return;

        if (!this.source.policy.enableSound) {
            this._soundPlayed = true;
            return;
        }

        if (this._soundName) {
            if (this.source.app) {
                let app = this.source.app;

                global.play_theme_sound_full(0, this._soundName,
                                             this.title, null,
                                             app.get_id(), app.get_name());
            } else {
                global.play_theme_sound(0, this._soundName, this.title, null);
            }
        } else if (this._soundFile) {
            if (this.source.app) {
                let app = this.source.app;

                global.play_sound_file_full(0, this._soundFile,
                                            this.title, null,
                                            app.get_id(), app.get_name());
            } else {
                global.play_sound_file(0, this._soundFile, this.title, null);
            }
        }
    },

    _onClicked: function() {
        this.emit('clicked');
        this.emit('done-displaying');
        this.destroy();
    },

    _onCloseClicked: function() {
        this.destroy();
    },

    _onDestroy: function() {
        if (this._destroyed)
            return;
        this._destroyed = true;
        if (!this._destroyedReason)
            this._destroyedReason = NotificationDestroyedReason.DISMISSED;
        this.emit('destroy', this._destroyedReason);
    },

    destroy: function(reason) {
        this._destroyedReason = reason;
        this.actor.destroy();
        this.actor._delegate = null;
    }
});
Signals.addSignalMethods(Notification.prototype);

const Source = new Lang.Class({
    Name: 'MessageTraySource',

    SOURCE_ICON_SIZE: 48,

    _init: function(title, iconName) {
        this.title = title;
        this.iconName = iconName;

        this.isChat = false;
        this.isMuted = false;
        this.keepTrayOnSummaryClick = false;

        this.notifications = [];

        this.policy = this._createPolicy();
    },

    get count() {
        return this.notifications.length;
    },

    get indicatorCount() {
        return this.notifications.length;
    },

    get unseenCount() {
        return this.notifications.filter(function(n) { return !n.acknowledged; }).length;
    },

    get countVisible() {
        return this.count > 1;
    },

    get isClearable() {
        return !this.trayIcon && !this.isChat;
    },

    countUpdated: function() {
        this.emit('count-updated');
    },

    _createPolicy: function() {
        return new NotificationPolicy();
    },

    setTitle: function(newTitle) {
        this.title = newTitle;
        this.emit('title-changed');
    },

    setMuted: function(muted) {
        if (!this.isChat || this.isMuted == muted)
            return;
        this.isMuted = muted;
        this.emit('muted-changed');
    },

    // Called to create a new icon actor.
    // Provides a sane default implementation, override if you need
    // something more fancy.
    createIcon: function(size) {
        return new St.Icon({ gicon: this.getIcon(),
                             icon_size: size });
    },

    getIcon: function() {
        return new Gio.ThemedIcon({ name: this.iconName });
    },

    _onNotificationDestroy: function(notification) {
        let index = this.notifications.indexOf(notification);
        if (index < 0)
            return;

        this.notifications.splice(index, 1);
        if (this.notifications.length == 0)
            this._lastNotificationRemoved();

        this.countUpdated();
    },

    pushNotification: function(notification) {
        if (this.notifications.indexOf(notification) >= 0)
            return;

        notification.connect('destroy', Lang.bind(this, this._onNotificationDestroy));
        this.notifications.push(notification);
        this.emit('notification-added', notification);

        this.countUpdated();
    },

    notify: function(notification) {
        notification.acknowledged = false;
        this.pushNotification(notification);

        if (!this.isMuted) {
            // Play the sound now, if banners are disabled.
            // Otherwise, it will be played when the notification
            // is next shown.
            if (this.policy.showBanners) {
                this.emit('notify', notification);
            } else {
                notification.playSound();
            }
        }
    },

    destroy: function(reason) {
        this.policy.destroy();

        let notifications = this.notifications;
        this.notifications = [];

        for (let i = 0; i < notifications.length; i++)
            notifications[i].destroy(reason);

        this.emit('destroy', reason);
    },

    iconUpdated: function() {
        this.emit('icon-updated');
    },

    //// Protected methods ////
    _setSummaryIcon: function(icon) {
        this._mainIcon.setIcon(icon);
        this.iconUpdated();
    },

    // To be overridden by subclasses
    open: function() {
    },

    destroyNotifications: function() {
        for (let i = this.notifications.length - 1; i >= 0; i--)
            this.notifications[i].destroy();

        this.countUpdated();
    },

    // Default implementation is to destroy this source, but subclasses can override
    _lastNotificationRemoved: function() {
        this.destroy();
    },

    getMusicNotification: function() {
        for (let i = 0; i < this.notifications.length; i++) {
            if (this.notifications[i].isMusic)
                return this.notifications[i];
        }

        return null;
    },
});
Signals.addSignalMethods(Source.prototype);

const TopBarNotificationPreview = new Lang.Class({
    Name: 'TopBarNotificationPreview',

    _init: function() {
        this.actor = new St.BoxLayout({ style_class: 'top-bar-notification-preview' });

        this._icon = new St.Icon({ icon_size: 22 });
        this.actor.add_child(this._icon);

        this._title = new St.Label({ style_class: 'top-bar-notification-preview-title' });
        this.actor.add_child(this._title);

        this._body = new St.Label({ style_class: 'top-bar-notification-preview-body' });
        this.actor.add_child(this._body);
    },

    setNotification: function(notification) {
        this._icon.gicon = notification.gicon;

        let title = notification.title;
        title = title ? _fixMarkup(title.replace(/\n/g, ' '), false) : '';
        this._title.clutter_text.set_markup('<b>' + title + '</b>');

        this._body.clutter_text.set_text(_fixMarkup(notification.bannerBodyText, false));
    },
});

const TopBarNotificationController = new Lang.Class({
    Name: 'TopBarNotificationController',

    _init: function() {
        this.actor = new St.Widget();

        this._notificationPreview = new TopBarNotificationPreview();
        this.actor.add_child(this._notificationPreview.actor);

        this._notificationQueue = [];
        this._timeoutId = 0;
    },

    _timeout: function() {
        this._notificationQueue.shift();
        this._update();
        this._timeoutId = 0;
        return GLib.SOURCE_REMOVE;
    },

    _ensureTimeout: function() {
        if (this._timeoutId == 0)
            this._timeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 2, Lang.bind(this, this._timeout));
    },

    get hasNotification() {
        return (this._notificationQueue.length > 0);
    },

    _update: function() {
        if (this._notificationQueue.length > 0) {
            let notification = this._notificationQueue[0];
            this._notificationPreview.setNotification(notification);

            this._ensureTimeout();
        }

        this.emit('updated');
    },

    pushNotification: function(notification) {
        this._notificationQueue.push(notification);

        notification.connect('destroy', Lang.bind(this, function() {
            let idx = this._notificationQueue.indexOf(notification);
            this._notificationQueue.splice(idx, 1);
            this._update();
        }));

        this._update();
    },
});
Signals.addSignalMethods(TopBarNotificationController.prototype);

const MessageTray = new Lang.Class({
    Name: 'MessageTray',

    _init: function() {
        this._sources = new Map();

        this.notificationPreview = new TopBarNotificationController();
    },

    _expireNotification: function() {
        this._notificationExpired = true;
        this._updateState();
    },

    contains: function(source) {
        return this._sources.has(source);
    },

    add: function(source) {
        if (this.contains(source)) {
            log('Trying to re-add source ' + source.title);
            return;
        }

        // Register that we got a notification for this source
        source.policy.store();

        source.policy.connect('enable-changed', Lang.bind(this, this._onSourceEnableChanged, source));
        // source.policy.connect('policy-changed', Lang.bind(this, this._updateState));
        this._onSourceEnableChanged(source.policy, source);
    },

    _addSource: function(source) {
        let obj = {
            source: source,
            notifyId: 0,
            destroyId: 0,
            mutedChangedId: 0
        };

        this._sources.set(source, obj);

        obj.notifyId = source.connect('notify', Lang.bind(this, this._onNotify));
        obj.destroyId = source.connect('destroy', Lang.bind(this, this._onSourceDestroy));
        obj.mutedChangedId = source.connect('muted-changed', Lang.bind(this,
            function () {
                if (source.isMuted)
                    this._notificationQueue = this._notificationQueue.filter(function(notification) {
                        return source != notification.source;
                    });
            }));

        this.emit('source-added', source);
    },

    _removeSource: function(source) {
        let obj = this._sources.get(source);
        this._sources.delete(source);

        source.disconnect(obj.notifyId);
        source.disconnect(obj.destroyId);
        source.disconnect(obj.mutedChangedId);

        this.emit('source-removed', source);
    },

    getSources: function() {
        return [k for (k of this._sources.keys())];
    },

    _onSourceEnableChanged: function(policy, source) {
        let wasEnabled = this.contains(source);
        let shouldBeEnabled = policy.enable;

        if (wasEnabled != shouldBeEnabled) {
            if (shouldBeEnabled)
                this._addSource(source);
            else
                this._removeSource(source);
        }
    },

    _onSourceDestroy: function(source) {
        this._removeSource(source);
    },

    _onNotify: function(source, notification) {
        this.notificationPreview.pushNotification(notification);
    },
});
Signals.addSignalMethods(MessageTray.prototype);

const SystemNotificationSource = new Lang.Class({
    Name: 'SystemNotificationSource',
    Extends: Source,

    _init: function() {
        this.parent(_("System Information"), 'dialog-information-symbolic');
    },

    open: function() {
        this.destroy();
    }
});
