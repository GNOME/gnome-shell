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
const SUMMARY_TIMEOUT = 1;
const LONGER_SUMMARY_TIMEOUT = 4;

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

const FocusGrabber = new Lang.Class({
    Name: 'FocusGrabber',

    _init: function(actor) {
        this._actor = actor;
        this._prevKeyFocusActor = null;
        this._focusActorChangedId = 0;
        this._focused = false;
    },

    grabFocus: function() {
        if (this._focused)
            return;

        this._prevFocusedWindow = global.display.focus_window;
        this._prevKeyFocusActor = global.stage.get_key_focus();

        this._focusActorChangedId = global.stage.connect('notify::key-focus', Lang.bind(this, this._focusActorChanged));

        if (!this._actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false))
            this._actor.grab_key_focus();

        this._focused = true;
    },

    _focusUngrabbed: function() {
        if (!this._focused)
            return false;

        if (this._focusActorChangedId > 0) {
            global.stage.disconnect(this._focusActorChangedId);
            this._focusActorChangedId = 0;
        }

        this._focused = false;
        return true;
    },

    _focusActorChanged: function() {
        let focusedActor = global.stage.get_key_focus();
        if (!focusedActor || !this._actor.contains(focusedActor))
            this._focusUngrabbed();
    },

    ungrabFocus: function() {
        if (!this._focusUngrabbed())
            return;

        if (this._prevKeyFocusActor) {
            global.stage.set_key_focus(this._prevKeyFocusActor);
            this._prevKeyFocusActor = null;
        } else {
            let focusedActor = global.stage.get_key_focus();
            if (focusedActor && this._actor.contains(focusedActor))
                global.stage.set_key_focus(null);
        }
    }
});

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
        // 'transient' is a reserved keyword in JS, so we have to use an alternate variable name
        this.isTransient = false;
        this.isMusic = false;
        this.forFeedback = false;
        this.expanded = false;
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
        // | .----. | .----.-----------------------. |
        // | |    | | |    |                       |--- this._titleBox
        // | '....' | '....'.......................' |
        // |        |                                |- this._hbox
        // |        |        this._bodyBin           |-.
        // |________|________________________________| |- this.actor
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

        // Separates the icon and title/body
        this._hbox = new St.BoxLayout({ style_class: 'notification-main-content' });
        this._mainButton.child = this._hbox;

        this._iconBin = new St.Bin({ y_align: St.Align.START });
        this._hbox.add_child(this._iconBin);

        this._titleBodyBox = new St.BoxLayout({ style_class: 'notification-title-body-box',
                                                vertical: true });
        this._hbox.add_child(this._titleBodyBox);

        this._titleBox = new St.BoxLayout({ style_class: 'notification-title-box',
                                            x_expand: true, x_align: Clutter.ActorAlign.START });
        this._secondaryIconBin = new St.Bin();
        this._titleBox.add_child(this._secondaryIconBin);
        this._titleLabel = new St.Label({ x_expand: true });
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
        this._actionAreaBin.visible = this.expanded && (this._actionArea != null);
        this._buttonBox.visible = this.expanded && (this._buttonBox.get_n_children() > 0);

        this._iconBin.visible = (this._icon != null && this._icon.visible);
        this._secondaryIconBin.visible = (this._secondaryIcon != null);

        if (this.expanded) {
            this._titleLabel.clutter_text.line_wrap = true;
            this._titleLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
            this._bodyUrlHighlighter.actor.clutter_text.line_wrap = true;
            this._bodyUrlHighlighter.actor.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        } else {
            this._titleLabel.clutter_text.line_wrap = false;
            this._titleLabel.clutter_text.ellipsize = Pango.EllipsizeMode.END;
            this._bodyUrlHighlighter.actor.clutter_text.line_wrap = false;
            this._bodyUrlHighlighter.actor.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        }
        this.enableScrolling(this.expanded);

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

    setTransient: function(isTransient) {
        this.isTransient = isTransient;
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

    expand: function(animate) {
        this.expanded = true;
        this._sync();
        this.emit('expanded');
    },

    collapseCompleted: function() {
        if (this._destroyed)
            return;

        this.expanded = false;
        this._sync();
    },

    _onClicked: function() {
        this.emit('clicked');
        this.emit('done-displaying');
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

const SourceActor = new Lang.Class({
    Name: 'SourceActor',

    _init: function(source, size) {
        this._source = source;
        this._size = size;

        this.actor = new Shell.GenericContainer();
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));
        this.actor.connect('destroy', Lang.bind(this, function() {
            this._actorDestroyed = true;
        }));
        this._actorDestroyed = false;

        let scale_factor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._iconBin = new St.Bin({ x_fill: true,
                                     height: size * scale_factor,
                                     width: size * scale_factor });

        this.actor.add_actor(this._iconBin);

        this._source.connect('icon-updated', Lang.bind(this, this._updateIcon));
        this._updateIcon();
    },

    setIcon: function(icon) {
        this._iconBin.child = icon;
        this._iconSet = true;
    },

    _getPreferredWidth: function (actor, forHeight, alloc) {
        let [min, nat] = this._iconBin.get_preferred_width(forHeight);
        alloc.min_size = min; alloc.nat_size = nat;
    },

    _getPreferredHeight: function (actor, forWidth, alloc) {
        let [min, nat] = this._iconBin.get_preferred_height(forWidth);
        alloc.min_size = min; alloc.nat_size = nat;
    },

    _allocate: function(actor, box, flags) {
        // the iconBin should fill our entire box
        this._iconBin.allocate(box, flags);
    },

    _updateIcon: function() {
        if (this._actorDestroyed)
            return;

        if (!this._iconSet)
            this._iconBin.child = this._source.createIcon(this._size);
    }
});

const SourceActorWithLabel = new Lang.Class({
    Name: 'SourceActorWithLabel',
    Extends: SourceActor,

    _init: function(source, size) {
        this.parent(source, size);

        this._counterLabel = new St.Label({ x_align: Clutter.ActorAlign.CENTER,
                                            x_expand: true,
                                            y_align: Clutter.ActorAlign.CENTER,
                                            y_expand: true });

        this._counterBin = new St.Bin({ style_class: 'summary-source-counter',
                                        child: this._counterLabel,
                                        layout_manager: new Clutter.BinLayout() });
        this._counterBin.hide();

        this._counterBin.connect('style-changed', Lang.bind(this, function() {
            let themeNode = this._counterBin.get_theme_node();
            this._counterBin.translation_x = themeNode.get_length('-shell-counter-overlap-x');
            this._counterBin.translation_y = themeNode.get_length('-shell-counter-overlap-y');
        }));

        this.actor.add_actor(this._counterBin);

        this._source.connect('count-updated', Lang.bind(this, this._updateCount));
        this._updateCount();
    },

    _allocate: function(actor, box, flags) {
        this.parent(actor, box, flags);

        let childBox = new Clutter.ActorBox();

        let [minWidth, minHeight, naturalWidth, naturalHeight] = this._counterBin.get_preferred_size();
        let direction = this.actor.get_text_direction();

        if (direction == Clutter.TextDirection.LTR) {
            // allocate on the right in LTR
            childBox.x1 = box.x2 - naturalWidth;
            childBox.x2 = box.x2;
        } else {
            // allocate on the left in RTL
            childBox.x1 = 0;
            childBox.x2 = naturalWidth;
        }

        childBox.y1 = box.y2 - naturalHeight;
        childBox.y2 = box.y2;

        this._counterBin.allocate(childBox, flags);
    },

    _updateCount: function() {
        if (this._actorDestroyed)
            return;

        this._counterBin.visible = this._source.countVisible;

        let text;
        if (this._source.count < 100)
            text = this._source.count.toString();
        else
            text = String.fromCharCode(0x22EF); // midline horizontal ellipsis

        this._counterLabel.set_text(text);
    }
});

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
        let notifications = this.notifications.filter(function(n) { return !n.isTransient; });
        return notifications.length;
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

    buildRightClickMenu: function() {
        let item;
        let rightClickMenu = new St.BoxLayout({ name: 'summary-right-click-menu',
                                                vertical: true });

        item = new PopupMenu.PopupMenuItem(_("Open"));
        item.connect('activate', Lang.bind(this, function() {
            this.open();
            this.emit('done-displaying-content', true);
        }));
        rightClickMenu.add(item.actor);

        item = new PopupMenu.PopupMenuItem(_("Remove"));
        item.connect('activate', Lang.bind(this, function() {
            this.destroy();
            this.emit('done-displaying-content', false);
        }));
        rightClickMenu.add(item.actor);
        return rightClickMenu;
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

    _ensureMainIcon: function() {
        if (this._mainIcon)
            return;

        this._mainIcon = new SourceActorWithLabel(this, this.SOURCE_ICON_SIZE);
    },

    // Unlike createIcon, this always returns the same actor;
    // there is only one summary icon actor for a Source.
    getSummaryIcon: function() {
        this._ensureMainIcon();
        return this._mainIcon.actor;
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

    // A subclass can redefine this to "steal" clicks from the
    // summaryitem; Use Clutter.get_current_event() to get the
    // details, return true to prevent the default handling from
    // ocurring.
    handleSummaryClick: function() {
        return false;
    },

    iconUpdated: function() {
        this.emit('icon-updated');
    },

    //// Protected methods ////
    _setSummaryIcon: function(icon) {
        this._ensureMainIcon();
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

const SummaryItem = new Lang.Class({
    Name: 'SummaryItem',

    _init: function(source) {
        this.source = source;
        this.source.connect('notification-added', Lang.bind(this, this._notificationAddedToSource));

        this.actor = new St.Button({ style_class: 'summary-source-button',
                                     y_fill: true,
                                     reactive: true,
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.TWO | St.ButtonMask.THREE,
                                     can_focus: true,
                                     track_hover: true });
        this.actor.label_actor = new St.Label({ text: source.title });
        this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPress));
        this._sourceBox = new St.BoxLayout({ style_class: 'summary-source' });

        this._sourceIcon = source.getSummaryIcon();
        this._sourceBox.add(this._sourceIcon, { y_fill: false });
        this.actor.child = this._sourceBox;

        this.notificationStackWidget = new St.Widget({ layout_manager: new Clutter.BinLayout() });

        this.notificationStackView = new St.ScrollView({ style_class: source.isChat ? '' : 'summary-notification-stack-scrollview',
                                                         vscrollbar_policy: source.isChat ? Gtk.PolicyType.NEVER : Gtk.PolicyType.AUTOMATIC,
                                                         hscrollbar_policy: Gtk.PolicyType.NEVER });
        this.notificationStackView.add_style_class_name('vfade');
        this.notificationStack = new St.BoxLayout({ style_class: 'summary-notification-stack',
                                                    vertical: true });
        this.notificationStackView.add_actor(this.notificationStack);
        this.notificationStackWidget.add_actor(this.notificationStackView);

        this._closeButton = Util.makeCloseButton();
        this._closeButton.connect('clicked', Lang.bind(this, function() {
            source.destroy();
            source.emit('done-displaying-content', false);
        }));
        this.notificationStackWidget.add_actor(this._closeButton);
        this._stackedNotifications = [];

        this._oldMaxScrollAdjustment = 0;

        this.notificationStackView.vscroll.adjustment.connect('changed', Lang.bind(this, function(adjustment) {
            let currentValue = adjustment.value + adjustment.page_size;
            if (currentValue == this._oldMaxScrollAdjustment)
                this.scrollTo(St.Side.BOTTOM);
            this._oldMaxScrollAdjustment = adjustment.upper;
        }));

        this.rightClickMenu = source.buildRightClickMenu();
        if (this.rightClickMenu)
            global.focus_manager.add_group(this.rightClickMenu);
    },

    destroy: function() {
        // remove the actor from the summary item so it doesn't get destroyed
        // with us
        this._sourceBox.remove_actor(this._sourceIcon);

        this.actor.destroy();
    },

    _onKeyPress: function(actor, event) {
        if (event.get_key_symbol() == Clutter.KEY_Up) {
            actor.emit('clicked', 1);
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    },

    prepareNotificationStackForShowing: function() {
        if (this.notificationStack.get_n_children() > 0)
            return;

        this.source.notifications.forEach(Lang.bind(this, this._appendNotificationToStack));
        this.scrollTo(St.Side.BOTTOM);
    },

    doneShowingNotificationStack: function() {
        this.source.notifications.forEach(Lang.bind(this, function(notification) {
            notification.collapseCompleted();
            notification.setIconVisible(true);
            notification.enableScrolling(true);
            this.notificationStack.remove_actor(notification.actor);
        }));
    },

    _notificationAddedToSource: function(source, notification) {
        if (this.notificationStack.mapped)
            this._appendNotificationToStack(notification);
    },

    _contentUpdated: function() {
        this.source.notifications.forEach(function(notification, i) {
            notification.setIconVisible(i == 0);
        });

        this.emit('content-updated');
    },

    _appendNotificationToStack: function(notification) {
        notification.connect('destroy', Lang.bind(this, this._contentUpdated));
        if (!this.source.isChat)
            notification.enableScrolling(false);
        notification.expand(false);
        this.notificationStack.add(notification.actor);
        this._contentUpdated();
    },

    // scrollTo:
    // @side: St.Side.TOP or St.Side.BOTTOM
    //
    // Scrolls the notifiction stack to the indicated edge
    scrollTo: function(side) {
        let adjustment = this.notificationStackView.vscroll.adjustment;
        if (side == St.Side.TOP)
            adjustment.value = adjustment.lower;
        else if (side == St.Side.BOTTOM)
            adjustment.value = adjustment.upper;
    },
});
Signals.addSignalMethods(SummaryItem.prototype);

const MessageTrayMenu = new Lang.Class({
    Name: 'MessageTrayMenu',
    Extends: PopupMenu.PopupMenu,

    _init: function(button, tray) {
        this.parent(button, 0, St.Side.BOTTOM);

        this._tray = tray;

        this._presence = new GnomeSession.Presence(Lang.bind(this, function(proxy, error) {
            if (error) {
                logError(error, 'Error while reading gnome-session presence');
                return;
            }

            this._onStatusChanged(proxy.status);
        }));
        this._presence.connectSignal('StatusChanged', Lang.bind(this, function(proxy, senderName, [status]) {
            this._onStatusChanged(status);
        }));

        this._accountManager = Tp.AccountManager.dup();
        this._accountManager.connect('most-available-presence-changed',
                                     Lang.bind(this, this._onIMPresenceChanged));
        this._accountManager.prepare_async(null, Lang.bind(this, this._onIMPresenceChanged));

        this.actor.hide();
        Main.layoutManager.addChrome(this.actor);

        this._busyItem = new PopupMenu.PopupSwitchMenuItem(_("Notifications"));
        this._busyItem.connect('toggled', Lang.bind(this, this._updatePresence));
        this.addMenuItem(this._busyItem);

        let separator = new PopupMenu.PopupSeparatorMenuItem();
        this.addMenuItem(separator);

        this._clearItem = this.addAction(_("Clear Messages"), function() {
            let toDestroy = tray.getSources().filter(function(source) {
                return source.isClearable;
            });

            toDestroy.forEach(function(source) {
                source.destroy();
            });

            tray.close();
        });

        tray.connect('source-added', Lang.bind(this, this._updateClearSensitivity));
        tray.connect('source-removed', Lang.bind(this, this._updateClearSensitivity));
        this._updateClearSensitivity();

        let separator = new PopupMenu.PopupSeparatorMenuItem();
        this.addMenuItem(separator);

        let settingsItem = this.addSettingsAction(_("Notification Settings"), 'gnome-notifications-panel.desktop');
        settingsItem.connect('activate', function() { tray.close(); });
    },

    _onStatusChanged: function(status) {
        this._sessionStatus = status;
        this._busyItem.setToggleState(status != GnomeSession.PresenceStatus.BUSY);
    },

    _onIMPresenceChanged: function(am, type) {
        if (type == Tp.ConnectionPresenceType.AVAILABLE &&
            this._sessionStatus == GnomeSession.PresenceStatus.BUSY)
            this._presence.SetStatusRemote(GnomeSession.PresenceStatus.AVAILABLE);
    },

    _updateClearSensitivity: function() {
        this._clearItem.setSensitive(this._tray.clearableCount > 0);
    },

    _updatePresence: function(item, state) {
        let status = state ? GnomeSession.PresenceStatus.AVAILABLE
                           : GnomeSession.PresenceStatus.BUSY;
        this._presence.SetStatusRemote(status);

        let [type, s ,msg] = this._accountManager.get_most_available_presence();
        let newType = 0;
        let newStatus;
        if (status == GnomeSession.PresenceStatus.BUSY &&
            type == Tp.ConnectionPresenceType.AVAILABLE) {
            newType = Tp.ConnectionPresenceType.BUSY;
            newStatus = 'busy';
        } else if (status == GnomeSession.PresenceStatus.AVAILABLE &&
                 type == Tp.ConnectionPresenceType.BUSY) {
            newType = Tp.ConnectionPresenceType.AVAILABLE;
            newStatus = 'available';
        }

        if (newType > 0)
            this._accountManager.set_all_requested_presences(newType,
                                                             newStatus, msg);
    }
});

const MessageTrayMenuButton = new Lang.Class({
    Name: 'MessageTrayMenuButton',

    _init: function(tray) {
        this._icon = new St.Icon();
        this.actor = new St.Button({ style_class: 'message-tray-menu-button',
                                     reactive: true,
                                     track_hover: true,
                                     can_focus: true,
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.TWO | St.ButtonMask.THREE,
                                     accessible_name: _("Tray Menu"),
                                     accessible_role: Atk.Role.MENU,
                                     child: this._icon });

        // Standard hack for ClutterBinLayout.
        this.actor.set_x_expand(true);
        this.actor.set_y_expand(true);
        this.actor.set_x_align(Clutter.ActorAlign.START);

        this._menu = new MessageTrayMenu(this.actor, tray);
        this._manager = new PopupMenu.PopupMenuManager({ actor: this.actor });
        this._manager.addMenu(this._menu);
        this._menu.connect('open-state-changed', Lang.bind(this, function(menu, open) {
            if (open)
                this.actor.add_style_pseudo_class('active');
            else
                this.actor.remove_style_pseudo_class('active');
        }));

        this.actor.connect('clicked', Lang.bind(this, function() {
            this._menu.toggle();
        }));

        this._accountManager = Tp.AccountManager.dup();
        this._accountManager.connect('most-available-presence-changed',
                                     Lang.bind(this, this._sync));
        this._accountManager.prepare_async(null, Lang.bind(this, this._sync));
    },

    _iconForPresence: function(presence) {
        if (presence == Tp.ConnectionPresenceType.AVAILABLE)
            return 'user-available-symbolic';
        else if (presence == Tp.ConnectionPresenceType.BUSY)
            return 'user-busy-symbolic';
        else if (presence == Tp.ConnectionPresenceType.HIDDEN)
            return 'user-hidden-symbolic';
        else if (presence == Tp.ConnectionPresenceType.AWAY)
            return 'user-away-symbolic';
        else if (presence == Tp.ConnectionPresenceType.EXTENDED_AWAY)
            return 'user-idle-symbolic';
        else
            return 'emblem-system-symbolic';
    },

    _sync: function() {
        let [presence, status, message] = this._accountManager.get_most_available_presence();
        this._icon.icon_name = this._iconForPresence(presence);
    },
});

const MessageTray = new Lang.Class({
    Name: 'MessageTray',

    _init: function() {
        this._presence = new GnomeSession.Presence(Lang.bind(this, function(proxy, error) {
            this._onStatusChanged(proxy.status);
        }));
        this._busy = false;
        this._presence.connectSignal('StatusChanged', Lang.bind(this, function(proxy, senderName, [status]) {
            this._onStatusChanged(status);
        }));

        this.actor = new St.Widget({ name: 'message-tray',
                                     reactive: true,
                                     layout_manager: new Clutter.BinLayout(),
                                     x_expand: true,
                                     y_expand: true,
                                     y_align: Clutter.ActorAlign.START,
                                   });

        this._notificationWidget = new St.Widget({ name: 'notification-container',
                                                   reactive: true,
                                                   track_hover: true,
                                                   y_align: Clutter.ActorAlign.START,
                                                   x_align: Clutter.ActorAlign.CENTER,
                                                   y_expand: true,
                                                   x_expand: true,
                                                   layout_manager: new Clutter.BinLayout() });
        this._notificationWidget.connect('key-release-event', Lang.bind(this, this._onNotificationKeyRelease));
        this._notificationWidget.connect('notify::hover', Lang.bind(this, this._onNotificationHoverChanged));
        this._notificationWidget.connect('notify::height', Lang.bind(this, function() {
            this._notificationWidget.translation_y = -this._notificationWidget.height;
        }));

        this._notificationBin = new St.Bin({ y_expand: true });
        this._notificationBin.set_y_align(Clutter.ActorAlign.START);
        this._notificationWidget.add_actor(this._notificationBin);
        this._notificationWidget.hide();
        this._notificationFocusGrabber = new FocusGrabber(this._notificationWidget);
        this._notificationQueue = [];
        this._notification = null;
        this._notificationClickedId = 0;

        this.actor.connect('button-release-event', Lang.bind(this, function(actor, event) {
            this._setClickedSummaryItem(null);
            this._updateState();
            actor.grab_key_focus();
            return Clutter.EVENT_PROPAGATE;
        }));
        global.focus_manager.add_group(this.actor);
        this._summary = new St.BoxLayout({ style_class: 'message-tray-summary',
                                           reactive: true,
                                           x_align: Clutter.ActorAlign.END,
                                           x_expand: true,
                                           y_align: Clutter.ActorAlign.CENTER,
                                           y_expand: true });
        this.actor.add_actor(this._summary);

        this._summaryMotionId = 0;

        this._summaryBoxPointer = new BoxPointer.BoxPointer(St.Side.BOTTOM,
                                                            { reactive: true,
                                                              track_hover: true });
        this._summaryBoxPointer.actor.connect('key-press-event',
                                              Lang.bind(this, this._onSummaryBoxPointerKeyPress));
        this._summaryBoxPointer.actor.style_class = 'summary-boxpointer';
        this._summaryBoxPointer.actor.hide();
        Main.layoutManager.addChrome(this._summaryBoxPointer.actor);

        this._summaryBoxPointerItem = null;
        this._summaryBoxPointerContentUpdatedId = 0;
        this._summaryBoxPointerDoneDisplayingId = 0;
        this._clickedSummaryItem = null;
        this._clickedSummaryItemMouseButton = -1;
        this._clickedSummaryItemAllocationChangedId = 0;

        this._closeButton = Util.makeCloseButton();
        this._closeButton.hide();
        this._closeButton.connect('clicked', Lang.bind(this, this._closeNotification));
        this._notificationWidget.add_actor(this._closeButton);

        this._userActiveWhileNotificationShown = false;

        this.idleMonitor = Meta.IdleMonitor.get_core();

        this._grabHelper = new GrabHelper.GrabHelper(this.actor,
                                                     { keybindingMode: Shell.KeyBindingMode.MESSAGE_TRAY });
        this._grabHelper.addActor(this._summaryBoxPointer.actor);
        this._grabHelper.addActor(this.actor);

        Main.layoutManager.connect('keyboard-visible-changed', Lang.bind(this, this._onKeyboardVisibleChanged));

        this._trayState = State.HIDDEN;
        this._traySummoned = false;
        this._useLongerNotificationLeftTimeout = false;
        this._trayLeftTimeoutId = 0;

        // pointerInNotification is sort of a misnomer -- it tracks whether
        // a message tray notification should expand. The value is
        // partially driven by the hover state of the notification, but has
        // a lot of complex state related to timeouts and the current
        // state of the pointer when a notification pops up.
        this._pointerInNotification = false;

        // This tracks this._notificationWidget.hover and is used to fizzle
        // out non-changing hover notifications in onNotificationHoverChanged.
        this._notificationHovered = false;

        this._keyboardVisible = false;
        this._notificationState = State.HIDDEN;
        this._notificationTimeoutId = 0;
        this._notificationExpandedId = 0;
        this._summaryBoxPointerState = State.HIDDEN;
        this._summaryBoxPointerTimeoutId = 0;
        this._desktopCloneState = State.HIDDEN;
        this._notificationRemoved = false;
        this._reNotifyAfterHideNotification = null;
        this._desktopClone = null;
        this._inCtrlAltTab = false;

        this.clearableCount = 0;

        this._lightboxes = [];
        let lightboxContainers = [global.window_group,
                                  Main.layoutManager.overviewGroup];
        for (let i = 0; i < lightboxContainers.length; i++)
            this._lightboxes.push(new Lightbox.Lightbox(lightboxContainers[i],
                                                        { inhibitEvents: true,
                                                          fadeFactor: 0.2
                                                        }));

        Main.layoutManager.trayBox.add_actor(this.actor);
        Main.layoutManager.trayBox.add_actor(this._notificationWidget);
        Main.layoutManager.trackChrome(this.actor);
        Main.layoutManager.trackChrome(this._notificationWidget);
        Main.layoutManager.trackChrome(this._closeButton);

        global.screen.connect('in-fullscreen-changed', Lang.bind(this, this._updateState));
        Main.layoutManager.connect('hot-corners-changed', Lang.bind(this, this._hotCornersChanged));

        // If the overview shows or hides while we're in
        // the message tray, revert back to normal mode.
        Main.overview.connect('showing', Lang.bind(this, this._escapeTray));
        Main.overview.connect('hiding', Lang.bind(this, this._escapeTray));

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));

        Main.wm.addKeybinding('toggle-message-tray',
                              new Gio.Settings({ schema: SHELL_KEYBINDINGS_SCHEMA }),
                              Meta.KeyBindingFlags.NONE,
                              Shell.KeyBindingMode.NORMAL |
                              Shell.KeyBindingMode.MESSAGE_TRAY |
                              Shell.KeyBindingMode.OVERVIEW,
                              Lang.bind(this, this.toggleAndNavigate));
        Main.wm.addKeybinding('focus-active-notification',
                              new Gio.Settings({ schema: SHELL_KEYBINDINGS_SCHEMA }),
                              Meta.KeyBindingFlags.NONE,
                              Shell.KeyBindingMode.NORMAL |
                              Shell.KeyBindingMode.MESSAGE_TRAY |
                              Shell.KeyBindingMode.OVERVIEW,
                              Lang.bind(this, this._expandActiveNotification));

        this._sources = new Map();
        this._chatSummaryItemsCount = 0;

        this._trayDwellTimeoutId = 0;
        this._setupTrayDwellIfNeeded();
        this._sessionUpdated();
        this._hotCornersChanged();

        this._noMessages = new St.Label({ text: _("No Messages"),
                                          style_class: 'no-messages-label',
                                          x_align: Clutter.ActorAlign.CENTER,
                                          x_expand: true,
                                          y_align: Clutter.ActorAlign.CENTER,
                                          y_expand: true });
        this.actor.add_actor(this._noMessages);
        this._updateNoMessagesLabel();

        this._messageTrayMenuButton = new MessageTrayMenuButton(this);
        this.actor.add_actor(this._messageTrayMenuButton.actor);
    },

    close: function() {
        this._escapeTray();
    },

    _setupTrayDwellIfNeeded: function() {
        // If we don't have extended barrier features, then we need
        // to support the old tray dwelling mechanism.
        if (!global.display.supports_extended_barriers()) {
            let pointerWatcher = PointerWatcher.getPointerWatcher();
            pointerWatcher.addWatch(TRAY_DWELL_CHECK_INTERVAL, Lang.bind(this, this._checkTrayDwell));
            this._trayDwelling = false;
            this._trayDwellUserTime = 0;
        }
    },

    _updateNoMessagesLabel: function() {
        this._noMessages.visible = this._sources.size == 0;
    },

    _sessionUpdated: function() {
        if (Main.sessionMode.isLocked || Main.sessionMode.isGreeter) {
            if (this._inCtrlAltTab)
                Main.ctrlAltTabManager.removeGroup(this._summary);
            this._inCtrlAltTab = false;
        } else if (!this._inCtrlAltTab) {
            Main.ctrlAltTabManager.addGroup(this._summary, _("Message Tray"), 'user-available-symbolic',
                                            { focusCallback: Lang.bind(this, this.toggleAndNavigate),
                                              sortGroup: CtrlAltTab.SortGroup.BOTTOM });
            this._inCtrlAltTab = true;
        }
        this._updateState();
    },

    _checkTrayDwell: function(x, y) {
        let monitor = Main.layoutManager.bottomMonitor;
        let shouldDwell = (x >= monitor.x && x <= monitor.x + monitor.width &&
                           y == monitor.y + monitor.height - 1);
        if (shouldDwell) {
            // We only set up dwell timeout when the user is not hovering over the tray
            // (!this._notificationHovered). This avoids bringing up the message tray after the
            // user clicks on a notification with the pointer on the bottom pixel
            // of the monitor. The _trayDwelling variable is used so that we only try to
            // fire off one tray dwell - if it fails (because, say, the user has the mouse down),
            // we don't try again until the user moves the mouse up and down again.
            if (!this._trayDwelling && !this._notificationHovered && this._trayDwellTimeoutId == 0) {
                // Save the interaction timestamp so we can detect user input
                let focusWindow = global.display.focus_window;
                this._trayDwellUserTime = focusWindow ? focusWindow.user_time : 0;

                this._trayDwellTimeoutId = Mainloop.timeout_add(TRAY_DWELL_TIME,
                                                                Lang.bind(this, this._trayDwellTimeout));
                GLib.Source.set_name_by_id(this._trayDwellTimeoutId, '[gnome-shell] this._trayDwellTimeout');
            }
            this._trayDwelling = true;
        } else {
            this._cancelTrayDwell();
            this._trayDwelling = false;
        }
    },

    _cancelTrayDwell: function() {
        if (this._trayDwellTimeoutId != 0) {
            Mainloop.source_remove(this._trayDwellTimeoutId);
            this._trayDwellTimeoutId = 0;
        }
    },

    _trayDwellTimeout: function() {
        this._trayDwellTimeoutId = 0;

        if (Main.layoutManager.bottomMonitor.inFullscreen)
            return GLib.SOURCE_REMOVE;

        // We don't want to open the tray when a modal dialog
        // is up, so we check the modal count for that. When we are in the
        // overview we have to take the overview's modal push into account
        if (Main.modalCount > (Main.overview.visible ? 1 : 0))
            return GLib.SOURCE_REMOVE;

        // If the user interacted with the focus window since we started the tray
        // dwell (by clicking or typing), don't activate the message tray
        let focusWindow = global.display.focus_window;
        let currentUserTime = focusWindow ? focusWindow.user_time : 0;
        if (currentUserTime != this._trayDwellUserTime)
            return GLib.SOURCE_REMOVE;

        this.openTray();
        return GLib.SOURCE_REMOVE;
    },

    _onNotificationKeyRelease: function(actor, event) {
        if (event.get_key_symbol() == Clutter.KEY_Escape && event.get_state() == 0) {
            this._expireNotification();
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    },

    _expireNotification: function() {
        this._notificationExpired = true;
        this._updateState();
    },

    _closeNotification: function() {
        if (this._notificationState == State.SHOWN) {
            this._closeButton.hide();
            this._notification.emit('done-displaying');
            this._notification.destroy();
        }
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
        source.policy.connect('policy-changed', Lang.bind(this, this._updateState));
        this._onSourceEnableChanged(source.policy, source);
    },

    _addSource: function(source) {
        let obj = {
            source: source,
            summaryItem: new SummaryItem(source),
            notifyId: 0,
            destroyId: 0,
            mutedChangedId: 0
        };
        let summaryItem = obj.summaryItem;

        if (source.isChat) {
            this._summary.insert_child_at_index(summaryItem.actor, 0);
            this._chatSummaryItemsCount++;
        } else {
            this._summary.insert_child_at_index(summaryItem.actor, this._chatSummaryItemsCount);
        }

        if (source.isClearable)
            this.clearableCount++;

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

        summaryItem.actor.connect('clicked', Lang.bind(this,
            function(actor, button) {
                actor.grab_key_focus();
                this._onSummaryItemClicked(summaryItem, button);
            }));
        summaryItem.actor.connect('popup-menu', Lang.bind(this,
            function(actor, button) {
                actor.grab_key_focus();
                this._onSummaryItemClicked(summaryItem, 3);
            }));

        // We need to display the newly-added summary item, but if the
        // caller is about to post a notification, we want to show that
        // *first* and not show the summary item until after it hides.
        // So postpone calling _updateState() a tiny bit.
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function() { this._updateState(); return false; }));

        this.emit('source-added', source);

        this._updateNoMessagesLabel();
    },

    _removeSource: function(source) {
        let obj = this._sources.get(source);
        this._sources.delete(source);
        let summaryItem = obj.summaryItem;

        if (source.isChat)
            this._chatSummaryItemsCount--;

        if (source.isClearable)
            this.clearableCount--;

        source.disconnect(obj.notifyId);
        source.disconnect(obj.destroyId);
        source.disconnect(obj.mutedChangedId);

        summaryItem.destroy();

        this.emit('source-removed', source);

        this._updateNoMessagesLabel();
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

    _onNotificationDestroy: function(notification) {
        if (this._notification == notification && (this._notificationState == State.SHOWN || this._notificationState == State.SHOWING)) {
            this._updateNotificationTimeout(0);
            this._notificationRemoved = true;
            this._updateState();
            return;
        }

        let index = this._notificationQueue.indexOf(notification);
        if (index != -1)
            this._notificationQueue.splice(index, 1);
    },

    openTray: function() {
        if (Main.overview.animationInProgress)
            return;

        this._traySummoned = true;
        this._updateState();
    },

    toggle: function() {
        if (Main.overview.animationInProgress)
            return false;

        this._traySummoned = !this._traySummoned;
        this._updateState();
        return true;
    },

    toggleAndNavigate: function() {
        if (!this.toggle())
            return;

        if (this._traySummoned)
            this._summary.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
    },

    hide: function() {
        this._traySummoned = false;
        this._updateState();
    },

    _onNotify: function(source, notification) {
        if (this._summaryBoxPointerItem && this._summaryBoxPointerItem.source == source) {
            if (this._summaryBoxPointerState == State.HIDING) {
                // We are in the process of hiding the summary box pointer.
                // If there is an update for one of the notifications or
                // a new notification to be added to the notification stack
                // while it is in the process of being hidden, we show it as
                // a new notification. However, we first wait till the hide
                // is complete. This is especially important if one of the
                // notifications in the stack was updated because we will
                // need to be able to re-parent its actor to a different
                // part of the stage.
                this._reNotifyAfterHideNotification = notification;
            } else {
                // The summary box pointer is showing or shown (otherwise,
                // this._summaryBoxPointerItem would be null)
                // Immediately mark the notification as acknowledged and play its
                // sound, as it's not going into the queue
                notification.acknowledged = true;
                notification.playSound();
            }

            return;
        }

        if (this._notification == notification) {
            // If a notification that is being shown is updated, we update
            // how it is shown and extend the time until it auto-hides.
            // If a new notification is updated while it is being hidden,
            // we stop hiding it and show it again.
            this._updateShowingNotification();
        } else if (this._notificationQueue.indexOf(notification) < 0) {
            notification.connect('destroy',
                                 Lang.bind(this, this._onNotificationDestroy));
            this._notificationQueue.push(notification);
            this._notificationQueue.sort(function(notification1, notification2) {
                return (notification2.urgency - notification1.urgency);
            });
        }
        this._updateState();
    },

    _onSummaryItemClicked: function(summaryItem, button) {
        if (summaryItem.source.handleSummaryClick(button)) {
            if (summaryItem.source.keepTrayOnSummaryClick)
                this._setClickedSummaryItem(null);
            else
                this._escapeTray();
        } else {
            if (!this._setClickedSummaryItem(summaryItem, button))
                this._setClickedSummaryItem(null);
        }

        this._updateState();
    },

    _hotCornersChanged: function() {
        let primary = Main.layoutManager.primaryIndex;
        let corner = Main.layoutManager.hotCorners[primary];
        if (corner && corner.actor)
            this._grabHelper.addActor(corner.actor);
    },

    _resetNotificationLeftTimeout: function() {
        this._useLongerNotificationLeftTimeout = false;
        if (this._notificationLeftTimeoutId) {
            Mainloop.source_remove(this._notificationLeftTimeoutId);
            this._notificationLeftTimeoutId = 0;
            this._notificationLeftMouseX = -1;
            this._notificationLeftMouseY = -1;
        }
    },

    _onNotificationHoverChanged: function() {
        if (this._notificationWidget.hover == this._notificationHovered)
            return;

        this._notificationHovered = this._notificationWidget.hover;
        if (this._notificationHovered) {
            // No dwell inside notifications at the bottom of the screen
            this._cancelTrayDwell();

            this._resetNotificationLeftTimeout();

            if (this._showNotificationMouseX >= 0) {
                let actorAtShowNotificationPosition =
                    global.stage.get_actor_at_pos(Clutter.PickMode.ALL, this._showNotificationMouseX, this._showNotificationMouseY);
                this._showNotificationMouseX = -1;
                this._showNotificationMouseY = -1;
                // Don't set this._pointerInNotification to true if the pointer was initially in the area where the notification
                // popped up. That way we will not be expanding notifications that happen to pop up over the pointer
                // automatically. Instead, the user is able to expand the notification by mousing away from it and then
                // mousing back in. Because this is an expected action, we set the boolean flag that indicates that a longer
                // timeout should be used before popping down the notification.
                if (this.actor.contains(actorAtShowNotificationPosition)) {
                    this._useLongerNotificationLeftTimeout = true;
                    return;
                }
            }

            this._pointerInNotification = true;
            this._updateState();
        } else {
            // We record the position of the mouse the moment it leaves the tray. These coordinates are used in
            // this._onNotificationLeftTimeout() to determine if the mouse has moved far enough during the initial timeout for us
            // to consider that the user intended to leave the tray and therefore hide the tray. If the mouse is still
            // close to its previous position, we extend the timeout once.
            let [x, y, mods] = global.get_pointer();
            this._notificationLeftMouseX = x;
            this._notificationLeftMouseY = y;

            // We wait just a little before hiding the message tray in case the user quickly moves the mouse back into it.
            // We wait for a longer period if the notification popped up where the mouse pointer was already positioned.
            // That gives the user more time to mouse away from the notification and mouse back in in order to expand it.
            let timeout = this._useLongerNotificationLeftTimeout ? LONGER_HIDE_TIMEOUT * 1000 : HIDE_TIMEOUT * 1000;
            this._notificationLeftTimeoutId = Mainloop.timeout_add(timeout, Lang.bind(this, this._onNotificationLeftTimeout));
            GLib.Source.set_name_by_id(this._notificationLeftTimeoutId, '[gnome-shell] this._onNotificationLeftTimeout');
        }
    },

    _onKeyboardVisibleChanged: function(layoutManager, keyboardVisible) {
        this._keyboardVisible = keyboardVisible;
        this._updateState();
    },

    _onStatusChanged: function(status) {
        if (status == GnomeSession.PresenceStatus.BUSY) {
            // remove notification and allow the summary to be closed now
            this._updateNotificationTimeout(0);
            this._busy = true;
        } else if (status != GnomeSession.PresenceStatus.IDLE) {
            // We preserve the previous value of this._busy if the status turns to IDLE
            // so that we don't start showing notifications queued during the BUSY state
            // as the screensaver gets activated.
            this._busy = false;
        }

        this._updateState();
    },

    _onNotificationLeftTimeout: function() {
        let [x, y, mods] = global.get_pointer();
        // We extend the timeout once if the mouse moved no further than MOUSE_LEFT_ACTOR_THRESHOLD to either side or up.
        // We don't check how far down the mouse moved because any point above the tray, but below the exit coordinate,
        // is close to the tray.
        if (this._notificationLeftMouseX > -1 &&
            y > this._notificationLeftMouseY - MOUSE_LEFT_ACTOR_THRESHOLD &&
            x < this._notificationLeftMouseX + MOUSE_LEFT_ACTOR_THRESHOLD &&
            x > this._notificationLeftMouseX - MOUSE_LEFT_ACTOR_THRESHOLD) {
            this._notificationLeftMouseX = -1;
            this._notificationLeftTimeoutId = Mainloop.timeout_add(LONGER_HIDE_TIMEOUT * 1000,
                                                             Lang.bind(this, this._onNotificationLeftTimeout));
            GLib.Source.set_name_by_id(this._notificationLeftTimeoutId, '[gnome-shell] this._onNotificationLeftTimeout');
        } else {
            this._notificationLeftTimeoutId = 0;
            this._useLongerNotificationLeftTimeout = false;
            this._pointerInNotification = false;
            this._updateNotificationTimeout(0);
            this._updateState();
        }
        return GLib.SOURCE_REMOVE;
    },

    _escapeTray: function() {
        this._pointerInNotification = false;
        this._traySummoned = false;
        this._setClickedSummaryItem(null);
        this._updateNotificationTimeout(0);
        this._updateState();
    },

    // All of the logic for what happens when occurs here; the various
    // event handlers merely update variables such as
    // 'this._pointerInNotification', 'this._traySummoned', etc, and
    // _updateState() figures out what (if anything) needs to be done
    // at the present time.
    _updateState: function() {
        // If our state changes caused _updateState to be called,
        // just exit now to prevent reentrancy issues.
        if (this._updatingState)
            return;

        this._updatingState = true;

        // Filter out acknowledged notifications.
        this._notificationQueue = this._notificationQueue.filter(function(n) {
            return !n.acknowledged;
        });

        let hasNotifications = Main.sessionMode.hasNotifications;

        if (this._notificationState == State.HIDDEN) {
            let shouldShowNotification = (hasNotifications && this._trayState == State.HIDDEN && !this._traySummoned);
            let nextNotification = this._notificationQueue[0] || null;
            if (shouldShowNotification && nextNotification) {
                let limited = this._busy || Main.layoutManager.bottomMonitor.inFullscreen;
                let showNextNotification = (!limited || nextNotification.forFeedback || nextNotification.urgency == Urgency.CRITICAL);
                if (showNextNotification)
                    this._showNotification();
            }
        } else if (this._notificationState == State.SHOWN) {
            let expired = (this._userActiveWhileNotificationShown &&
                           this._notificationTimeoutId == 0 &&
                           this._notification.urgency != Urgency.CRITICAL &&
                           !this._notification.focused &&
                           !this._pointerInNotification) || this._notificationExpired;
            let mustClose = (this._notificationRemoved || !hasNotifications || expired || this._traySummoned);

            if (mustClose) {
                let animate = hasNotifications && !this._notificationRemoved;
                this._hideNotification(animate);
            } else if (this._pointerInNotification && !this._notification.expanded) {
                this._expandNotification(false);
            } else if (this._pointerInNotification) {
                this._ensureNotificationFocused();
            }
        }

        // Summary notification
        let haveClickedSummaryItem = this._clickedSummaryItem != null;
        let requestedNotificationStackIsEmpty = (haveClickedSummaryItem &&
                                                 this._clickedSummaryItemMouseButton == 1 &&
                                                 this._clickedSummaryItem.source.notifications.length == 0);

        if (this._summaryBoxPointerState == State.HIDDEN) {
            if (haveClickedSummaryItem && !requestedNotificationStackIsEmpty)
                this._showSummaryBoxPointer();
        } else if (this._summaryBoxPointerState == State.SHOWN) {
            if (haveClickedSummaryItem && hasNotifications) {
                let wrongSummaryNotificationStack = (this._clickedSummaryItemMouseButton == 1 &&
                                                     this._summaryBoxPointer.bin.child != this._clickedSummaryItem.notificationStackWidget &&
                                                     requestedNotificationStackIsEmpty);
                let wrongSummaryRightClickMenu = (this._clickedSummaryItemMouseButton == 3 &&
                                                  this._clickedSummaryItem.rightClickMenu != null &&
                                                  this._summaryBoxPointer.bin.child != this._clickedSummaryItem.rightClickMenu);
                let wrongSummaryBoxPointer = (wrongSummaryNotificationStack || wrongSummaryRightClickMenu);

                if (wrongSummaryBoxPointer) {
                    this._hideSummaryBoxPointer();
                    this._showSummaryBoxPointer();
                }
            } else {
                this._hideSummaryBoxPointer();
            }
        }

        // Tray itself
        let trayIsVisible = (this._trayState == State.SHOWING ||
                             this._trayState == State.SHOWN);
        let trayShouldBeVisible = this._traySummoned && !this._keyboardVisible && hasNotifications;
        if (!trayIsVisible && trayShouldBeVisible)
            trayShouldBeVisible = this._showTray();
        else if (trayIsVisible && !trayShouldBeVisible)
            this._hideTray();

        // Desktop clone
        let desktopCloneIsVisible = (this._desktopCloneState == State.SHOWING ||
                                     this._desktopCloneState == State.SHOWN);
        let desktopCloneShouldBeVisible = (trayShouldBeVisible);

        if (!desktopCloneIsVisible && desktopCloneShouldBeVisible)
            this._showDesktopClone();
        else if (desktopCloneIsVisible && !desktopCloneShouldBeVisible)
            this._hideDesktopClone();

        this._updatingState = false;

        // Clean transient variables that are used to communicate actions
        // to updateState()
        this._notificationExpired = false;
    },

    _tween: function(actor, statevar, value, params) {
        let onComplete = params.onComplete;
        let onCompleteScope = params.onCompleteScope;
        let onCompleteParams = params.onCompleteParams;

        params.onComplete = this._tweenComplete;
        params.onCompleteScope = this;
        params.onCompleteParams = [statevar, value, onComplete, onCompleteScope, onCompleteParams];

        // Remove other tweens that could mess with the state machine
        Tweener.removeTweens(actor);
        Tweener.addTween(actor, params);

        let valuing = (value == State.SHOWN) ? State.SHOWING : State.HIDING;
        this[statevar] = valuing;
    },

    _tweenComplete: function(statevar, value, onComplete, onCompleteScope, onCompleteParams) {
        this[statevar] = value;
        if (onComplete)
            onComplete.apply(onCompleteScope, onCompleteParams);
        this._updateState();
    },

    _showTray: function() {
        if (!this._grabHelper.grab({ actor: this.actor,
                                     onUngrab: Lang.bind(this, this._escapeTray) })) {
            this._traySummoned = false;
            return false;
        }

        this.emit('showing');
        this._tween(this.actor, '_trayState', State.SHOWN,
                    { y: -this.actor.height,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad'
                    });

        for (let i = 0; i < this._lightboxes.length; i++)
            this._lightboxes[i].show(ANIMATION_TIME);

        return true;
    },

    _updateDesktopCloneClip: function() {
        let geometry = this._bottomMonitorGeometry;
        let progress = -Math.round(this._desktopClone.y);
        this._desktopClone.set_clip(geometry.x,
                                    geometry.y + progress,
                                    geometry.width,
                                    geometry.height - progress);
    },

    _showDesktopClone: function() {
        let bottomMonitor = Main.layoutManager.bottomMonitor;
        this._bottomMonitorGeometry = { x: bottomMonitor.x,
                                        y: bottomMonitor.y,
                                        width: bottomMonitor.width,
                                        height: bottomMonitor.height };

        if (this._desktopClone)
            this._desktopClone.destroy();
        let cloneSource = Main.overview.visible ? Main.layoutManager.overviewGroup : global.window_group;
        this._desktopClone = new Clutter.Clone({ source: cloneSource,
                                                 clip: new Clutter.Geometry(this._bottomMonitorGeometry) });
        Main.uiGroup.insert_child_above(this._desktopClone, cloneSource);
        this._desktopClone.x = 0;
        this._desktopClone.y = 0;
        this._desktopClone.show();

        this._tween(this._desktopClone, '_desktopCloneState', State.SHOWN,
                    { y: -this.actor.height,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad',
                      onUpdate: Lang.bind(this, this._updateDesktopCloneClip)
                    });
    },

    _hideTray: function() {
        // Having the summary item animate out while sliding down the tray
        // is distracting, so hide it immediately in case it was visible.
        this._summaryBoxPointer.actor.hide();

        this.emit('hiding');
        this._tween(this.actor, '_trayState', State.HIDDEN,
                    { y: 0,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad'
                    });

        // Note that we might have entered here without a grab,
        // which would happen if GrabHelper ungrabbed for us.
        // This is a no-op in that case.
        this._grabHelper.ungrab({ actor: this.actor });
        for (let i = 0; i < this._lightboxes.length; i++)
            this._lightboxes[i].hide(ANIMATION_TIME);
    },

    _hideDesktopClone: function() {
        this._tween(this._desktopClone, '_desktopCloneState', State.HIDDEN,
                    { y: 0,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad',
                      onComplete: Lang.bind(this, function() {
                          this._desktopClone.destroy();
                          this._desktopClone = null;
                          this._bottomMonitorGeometry = null;
                      }),
                      onUpdate: Lang.bind(this, this._updateDesktopCloneClip)
                    });
    },

    _onIdleMonitorBecameActive: function() {
        this._userActiveWhileNotificationShown = true;
        this._updateNotificationTimeout(2000);
        this._updateState();
    },

    _showNotification: function() {
        this._notification = this._notificationQueue.shift();

        this._userActiveWhileNotificationShown = this.idleMonitor.get_idletime() <= IDLE_TIME;
        if (!this._userActiveWhileNotificationShown) {
            // If the user isn't active, set up a watch to let us know
            // when the user becomes active.
            this.idleMonitor.add_user_active_watch(Lang.bind(this, this._onIdleMonitorBecameActive));
        }

        this._notificationClickedId = this._notification.connect('done-displaying',
                                                                 Lang.bind(this, this._escapeTray));
        this._notificationUnfocusedId = this._notification.connect('unfocused', Lang.bind(this, function() {
            this._updateState();
        }));
        this._notificationBin.child = this._notification.actor;

        this._notificationWidget.opacity = 0;
        this._notificationWidget.show();

        this._updateShowingNotification();

        let [x, y, mods] = global.get_pointer();
        // We save the position of the mouse at the time when we started showing the notification
        // in order to determine if the notification popped up under it. We make that check if
        // the user starts moving the mouse and _onNotificationHoverChanged() gets called. We don't
        // expand the notification if it just happened to pop up under the mouse unless the user
        // explicitly mouses away from it and then mouses back in.
        this._showNotificationMouseX = x;
        this._showNotificationMouseY = y;
        // We save the coordinates of the mouse at the time when we started showing the notification
        // and then we update it in _notificationTimeout(). We don't pop down the notification if
        // the mouse is moving towards it or within it.
        this._lastSeenMouseX = x;
        this._lastSeenMouseY = y;

        this._resetNotificationLeftTimeout();
    },

    _updateShowingNotification: function() {
        this._notification.acknowledged = true;
        this._notification.playSound();

        // We auto-expand notifications with CRITICAL urgency, or for which the relevant setting
        // is on in the control center.
        if (this._notification.urgency == Urgency.CRITICAL ||
            this._notification.source.policy.forceExpanded)
            this._expandNotification(true);

        // We tween all notifications to full opacity. This ensures that both new notifications and
        // notifications that might have been in the process of hiding get full opacity.
        //
        // We use this._showNotificationCompleted() onComplete callback to extend the time the updated
        // notification is being shown.

        this._tween(this._notificationWidget, '_notificationState', State.SHOWN,
                    { opacity: 255,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad',
                      onComplete: this._showNotificationCompleted,
                      onCompleteScope: this
                    });
   },

    _showNotificationCompleted: function() {
        if (this._notification.urgency != Urgency.CRITICAL)
            this._updateNotificationTimeout(NOTIFICATION_TIMEOUT * 1000);
    },

    _updateNotificationTimeout: function(timeout) {
        if (this._notificationTimeoutId) {
            Mainloop.source_remove(this._notificationTimeoutId);
            this._notificationTimeoutId = 0;
        }
        if (timeout > 0) {
            this._notificationTimeoutId =
                Mainloop.timeout_add(timeout,
                                     Lang.bind(this, this._notificationTimeout));
            GLib.Source.set_name_by_id(this._notificationTimeoutId, '[gnome-shell] this._notificationTimeout');
        }
    },

    _notificationTimeout: function() {
        let [x, y, mods] = global.get_pointer();
        if (y > this._lastSeenMouseY + 10 && !this._notificationHovered) {
            // The mouse is moving towards the notification, so don't
            // hide it yet. (We just create a new timeout (and destroy
            // the old one) each time because the bookkeeping is
            // simpler.)
            this._updateNotificationTimeout(1000);
        } else if (this._useLongerNotificationLeftTimeout && !this._notificationLeftTimeoutId &&
                  (x != this._lastSeenMouseX || y != this._lastSeenMouseY)) {
            // Refresh the timeout if the notification originally
            // popped up under the pointer, and the pointer is hovering
            // inside it.
            this._updateNotificationTimeout(1000);
        } else {
            this._notificationTimeoutId = 0;
            this._updateState();
        }

        this._lastSeenMouseX = x;
        this._lastSeenMouseY = y;
        return GLib.SOURCE_REMOVE;
    },

    _hideNotification: function(animate) {
        this._notificationFocusGrabber.ungrabFocus();

        if (this._notificationExpandedId) {
            this._notification.disconnect(this._notificationExpandedId);
            this._notificationExpandedId = 0;
        }
        if (this._notificationClickedId) {
            this._notification.disconnect(this._notificationClickedId);
            this._notificationClickedId = 0;
        }
        if (this._notificationUnfocusedId) {
            this._notification.disconnect(this._notificationUnfocusedId);
            this._notificationUnfocusedId = 0;
        }

        this._resetNotificationLeftTimeout();

        if (animate) {
            this._tween(this._notificationWidget, '_notificationState', State.HIDDEN,
                        { opacity: 0,
                          time: ANIMATION_TIME,
                          transition: 'easeOutQuad',
                          onComplete: this._hideNotificationCompleted,
                          onCompleteScope: this
                        });
        } else {
            Tweener.removeTweens(this._notificationWidget);
            this._notificationWidget.opacity = 0;
            this._notificationState = State.HIDDEN;
            this._hideNotificationCompleted();
        }
    },

    _hideNotificationCompleted: function() {
        this._notification.collapseCompleted();

        let notification = this._notification;
        this._notification = null;
        if (notification.isTransient)
            notification.destroy(NotificationDestroyedReason.EXPIRED);

        this._closeButton.hide();
        this._pointerInNotification = false;
        this._notificationRemoved = false;
        this._notificationBin.child = null;
        this._notificationWidget.hide();
    },

    _expandActiveNotification: function() {
        if (!this._notification)
            return;

        this._expandNotification(false);
    },

    _expandNotification: function(autoExpanding) {
        if (!this._notificationExpandedId)
            this._notificationExpandedId =
                this._notification.connect('expanded',
                                           Lang.bind(this, this._onNotificationExpanded));
        // Don't animate changes in notifications that are auto-expanding.
        this._notification.expand(!autoExpanding);

        // Don't focus notifications that are auto-expanding.
        if (!autoExpanding)
            this._ensureNotificationFocused();
    },

    _onNotificationExpanded: function() {
        this._closeButton.show();
    },

    _ensureNotificationFocused: function() {
        this._notificationFocusGrabber.grabFocus();
    },

    _onSourceDoneDisplayingContent: function(source, closeTray) {
        if (closeTray) {
            this._escapeTray();
        } else {
            this._setClickedSummaryItem(null);
            this._updateState();
        }
    },

    _showSummaryBoxPointer: function() {
        let child;
        let summaryItem = this._clickedSummaryItem;
        if (this._clickedSummaryItemMouseButton == 1) {
            // Acknowledge all our notifications
            summaryItem.source.notifications.forEach(function(n) { n.acknowledged = true; });
            summaryItem.prepareNotificationStackForShowing();
            child = summaryItem.notificationStackWidget;
        } else if (this._clickedSummaryItemMouseButton == 3) {
            child = summaryItem.rightClickMenu;
        }

        // If the user clicked the middle mouse button, or the item
        // doesn't have a right-click menu, do nothing.
        if (!child)
            return;

        this._summaryBoxPointerItem = summaryItem;
        this._summaryBoxPointerContentUpdatedId = this._summaryBoxPointerItem.connect('content-updated',
                                                                                      Lang.bind(this, this._onSummaryBoxPointerContentUpdated));
        this._sourceDoneDisplayingId = this._summaryBoxPointerItem.source.connect('done-displaying-content',
                                                                                  Lang.bind(this, this._onSourceDoneDisplayingContent));

        this._summaryBoxPointer.bin.child = child;
        this._summaryBoxPointer.actor.opacity = 0;
        this._summaryBoxPointer.actor.show();
        this._adjustSummaryBoxPointerPosition();

        this._grabHelper.grab({ actor: this._summaryBoxPointer.bin.child,
                                onUngrab: Lang.bind(this, this._onSummaryBoxPointerUngrabbed) });

        this._summaryBoxPointerState = State.SHOWING;
        this._summaryBoxPointer.show(BoxPointer.PopupAnimation.FULL, Lang.bind(this, function() {
            this._summaryBoxPointerState = State.SHOWN;
        }));
    },

    _onSummaryBoxPointerContentUpdated: function() {
        if (this._summaryBoxPointerItem.notificationStack.get_n_children() == 0)
            this._hideSummaryBoxPointer();
    },

    _adjustSummaryBoxPointerPosition: function() {
        this._summaryBoxPointer.setPosition(this._summaryBoxPointerItem.actor, 0);
    },

    _setClickedSummaryItem: function(item, button) {
        if (item == this._clickedSummaryItem &&
            button == this._clickedSummaryItemMouseButton)
            return false;

        if (this._clickedSummaryItem) {
            this._clickedSummaryItem.actor.remove_style_pseudo_class('selected');
            this._clickedSummaryItem.actor.disconnect(this._clickedSummaryItemAllocationChangedId);
            this._summary.disconnect(this._summaryMotionId);
            this._clickedSummaryItemAllocationChangedId = 0;
            this._summaryMotionId = 0;
        }

        this._clickedSummaryItem = item;
        this._clickedSummaryItemMouseButton = button;

        if (this._clickedSummaryItem) {
            this._clickedSummaryItem.actor.add_style_pseudo_class('selected');
            this._clickedSummaryItem.actor.connect('destroy', Lang.bind(this, function() {
                this._setClickedSummaryItem(null);
                this._updateState();
            }));
            this._clickedSummaryItemAllocationChangedId =
                this._clickedSummaryItem.actor.connect('allocation-changed',
                                                       Lang.bind(this, this._adjustSummaryBoxPointerPosition));
            // _clickedSummaryItem.actor can change absolute position without changing allocation
            this._summaryMotionId = this._summary.connect('allocation-changed',
                                                          Lang.bind(this, this._adjustSummaryBoxPointerPosition));
        }

        return true;
    },

    _onSummaryBoxPointerKeyPress: function(actor, event) {
        switch (event.get_key_symbol()) {
        case Clutter.KEY_Down:
        case Clutter.KEY_Escape:
            this._setClickedSummaryItem(null);
            this._updateState();
            return Clutter.EVENT_STOP;
        case Clutter.KEY_Delete:
            this._clickedSummaryItem.source.destroy();
            this._escapeTray();
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    },

    _onSummaryBoxPointerUngrabbed: function() {
        this._summaryBoxPointerState = State.HIDING;
        this._setClickedSummaryItem(null);

        if (this._summaryBoxPointerContentUpdatedId) {
            this._summaryBoxPointerItem.disconnect(this._summaryBoxPointerContentUpdatedId);
            this._summaryBoxPointerContentUpdatedId = 0;
        }

        if (this._sourceDoneDisplayingId) {
            this._summaryBoxPointerItem.source.disconnect(this._sourceDoneDisplayingId);
            this._sourceDoneDisplayingId = 0;
        }

        let animate = (this._summaryBoxPointerItem.source.notifications.length > 0);
        this._summaryBoxPointer.hide(animate ? BoxPointer.PopupAnimation.FULL : BoxPointer.PopupAnimation.NONE,
                                     Lang.bind(this, this._hideSummaryBoxPointerCompleted));
    },

    _hideSummaryBoxPointer: function() {
        this._grabHelper.ungrab({ actor: this._summaryBoxPointer.bin.child });
    },

    _hideSummaryBoxPointerCompleted: function() {
        let doneShowingNotificationStack = (this._summaryBoxPointer.bin.child == this._summaryBoxPointerItem.notificationStackWidget);

        this._summaryBoxPointerState = State.HIDDEN;
        this._summaryBoxPointer.bin.child = null;

        if (doneShowingNotificationStack) {
            let source = this._summaryBoxPointerItem.source;

            this._summaryBoxPointerItem.doneShowingNotificationStack();
            this._summaryBoxPointerItem = null;

            if (this._reNotifyAfterHideNotification) {
                this._onNotify(this._reNotifyAfterHideNotification.source, this._reNotifyAfterHideNotification);
                this._reNotifyAfterHideNotification = null;
            }
        }

        if (this._clickedSummaryItem)
            this._updateState();
    }
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
