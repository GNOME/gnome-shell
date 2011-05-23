/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const BoxPointer = imports.ui.boxpointer;
const GnomeSession = imports.misc.gnomeSession;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;
const Util = imports.misc.util;

const ANIMATION_TIME = 0.2;
const NOTIFICATION_TIMEOUT = 4;
const SUMMARY_TIMEOUT = 1;
const LONGER_SUMMARY_TIMEOUT = 4;

const HIDE_TIMEOUT = 0.2;
const LONGER_HIDE_TIMEOUT = 0.6;

const MAX_SOURCE_TITLE_WIDTH = 180;

// We delay hiding of the tray if the mouse is within MOUSE_LEFT_ACTOR_THRESHOLD
// range from the point where it left the tray.
const MOUSE_LEFT_ACTOR_THRESHOLD = 20;

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
}

function _fixMarkup(text, allowMarkup) {
    if (allowMarkup) {
        // Support &amp;, &quot;, &apos;, &lt; and &gt;, escape all other
        // occurrences of '&'.
        let _text = text.replace(/&(?!amp;|quot;|apos;|lt;|gt;)/g, '&amp;');
        // Support <b>, <i>, and <u>, escape anything else
        // so it displays as raw markup.
        return _text.replace(/<(\/?[^biu]>|[^>\/][^>])/g, '&lt;$1');
    } else {
        // Escape everything
        let _text = text.replace(/&/g, '&amp;');
        return _text.replace(/</g, '&lt;');
    }
}

function URLHighlighter(text, lineWrap, allowMarkup) {
    this._init(text, lineWrap, allowMarkup);
}

URLHighlighter.prototype = {
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
            // Keep Notification.actor from seeing this and taking
            // a pointer grab, which would block our button-release-event
            // handler, if an URL is clicked
            return this._findUrlAtPos(event) != -1;
        }));
        this.actor.connect('button-release-event', Lang.bind(this, function (actor, event) {
            let urlId = this._findUrlAtPos(event);
            if (urlId != -1) {
                let url = this._urls[urlId].url;
                if (url.indexOf(':') == -1)
                    url = 'http://' + url;
                try {
                    Gio.app_info_launch_default_for_uri(url, global.create_app_launch_context());
                    return true;
                } catch (e) {
                    // TODO: remove this after gnome 3 release
                    Util.spawn(['gvfs-open', url]);
                    return true;
                }
            }
            return false;
        }));
        this.actor.connect('motion-event', Lang.bind(this, function(actor, event) {
            let urlId = this._findUrlAtPos(event);
            if (urlId != -1 && !this._cursorChanged) {
                global.set_cursor(Shell.Cursor.POINTING_HAND);
                this._cursorChanged = true;
            } else if (urlId == -1) {
                global.unset_cursor();
                this._cursorChanged = false;
            }
            return false;
        }));
        this.actor.connect('leave-event', Lang.bind(this, function() {
            if (this._cursorChanged) {
                this._cursorChanged = false;
                global.unset_cursor();
            }
        }));
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
};

function FocusGrabber() {
    this._init();
}

FocusGrabber.prototype = {
    _init: function() {
        this.actor = null;

        this._hasFocus = false;
        // We use this._prevFocusedWindow and this._prevKeyFocusActor to return the
        // focus where it previously belonged after a focus grab, unless the user
        // has explicitly changed that.
        this._prevFocusedWindow = null;
        this._prevKeyFocusActor = null;

        this._focusActorChangedId = 0;
        this._stageInputModeChangedId = 0;
        this._capturedEventId = 0;
        this._togglingFocusGrabMode = false;

        Main.overview.connect('showing', Lang.bind(this,
            function() {
                this._toggleFocusGrabMode();
            }));
        Main.overview.connect('hidden', Lang.bind(this,
            function() {
                this._toggleFocusGrabMode();
            }));
    },

    grabFocus: function(actor) {
        if (this._hasFocus)
            return;

        this.actor = actor;

        let metaDisplay = global.screen.get_display();

        this._prevFocusedWindow = metaDisplay.focus_window;
        this._prevKeyFocusActor = global.stage.get_key_focus();

        if (!Main.overview.visible)
            global.set_stage_input_mode(Shell.StageInputMode.FOCUSED);

        // Use captured-event to notice clicks outside the focused actor
        // without consuming them.
        this._capturedEventId = global.stage.connect('captured-event', Lang.bind(this, this._onCapturedEvent));

        this._stageInputModeChangedId = global.connect('notify::stage-input-mode', Lang.bind(this, this._stageInputModeChanged));
        this._focusActorChangedId = global.stage.connect('notify::key-focus', Lang.bind(this, this._focusActorChanged));

        this._hasFocus = true;

        this.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
        this.emit('focus-grabbed');
    },

    _focusActorChanged: function() {
        let focusedActor = global.stage.get_key_focus();
        if (!focusedActor || !this.actor.contains(focusedActor)) {
            this._prevKeyFocusActor = null;
            this.ungrabFocus();
        }
    },

    _stageInputModeChanged: function() {
        this.ungrabFocus();
    },

    _onCapturedEvent: function(actor, event) {
        let source = event.get_source();
        switch (event.type()) {
            case Clutter.EventType.BUTTON_PRESS:
                if (!this.actor.contains(source))
                    this.emit('button-pressed', source);
                break;
            case Clutter.EventType.KEY_PRESS:
                let symbol = event.get_key_symbol();
                if (symbol == Clutter.Escape) {
                    this.emit('escape-pressed');
                    return true;
                }
                break;
        }

        return false;
    },

    ungrabFocus: function() {
        if (!this._hasFocus)
            return;

        let metaDisplay = global.screen.get_display();

        if (this._focusActorChangedId > 0) {
            global.stage.disconnect(this._focusActorChangedId);
            this._focusActorChangedId = 0;
        }

        if (this._stageInputModeChangedId) {
            global.disconnect(this._stageInputModeChangedId);
            this._stageInputModeChangedId = 0;
        }

        if (this._capturedEventId > 0) {
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }

        this._hasFocus = false;
        this.emit('focus-ungrabbed');

        if (this._prevFocusedWindow && !metaDisplay.focus_window) {
            metaDisplay.set_input_focus_window(this._prevFocusedWindow, false, global.get_current_time());
            this._prevFocusedWindow = null;
        }
        if (this._prevKeyFocusActor) {
            global.stage.set_key_focus(this._prevKeyFocusActor);
            this._prevKeyFocusActor = null;
        } else {
            // We don't want to keep any actor inside the previously focused actor focused.
            let focusedActor = global.stage.get_key_focus();
            if (focusedActor && this.actor.contains(focusedActor))
                global.stage.set_key_focus(null);
        }
        if (!this._togglingFocusGrabMode)
            this.actor = null;
    },

    // Because we grab focus differently in the overview
    // and in the main view, we need to change how it is
    // done when we move between the two.
    _toggleFocusGrabMode: function() {
        if (this._hasFocus) {
            this._togglingFocusGrabMode = true;
            this.ungrabFocus();
            this.grabFocus(this.actor);
            this._togglingFocusGrabMode = false;
        }
    }
}
Signals.addSignalMethods(FocusGrabber.prototype);

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
// @banner text is added as the first element in the content section,
// unless 'customContent' parameter with the value 'true' is specified
// in @params.
//
// Additional notification content can be added with addActor() and
// addBody() methods. The notification content is put inside a
// scrollview, so if it gets too tall, the notification will scroll
// rather than continue to grow. In addition to this main content
// area, there is also a single-row action area, which is not
// scrolled and can contain a single actor. The action area can
// be set by calling setActionArea() method. There is also a
// convenience method addButton() for adding a button to the action
// area.
//
// @params can contain values for 'customContent', 'body', 'icon',
// 'titleMarkup', 'bannerMarkup', 'bodyMarkup', and 'clear'
// parameters.
//
// If @params contains a 'customContent' parameter with the value %true,
// then @banner will not be shown in the body of the notification when the
// notification is expanded and calls to update() will not clear the content
// unless 'clear' parameter with value %true is explicitly specified.
//
// If @params contains a 'body' parameter, then that text will be added to
// the content area (as with addBody()).
//
// By default, the icon shown is created by calling
// source.createNotificationIcon(). However, if @params contains an 'icon'
// parameter, the passed in icon will be used.
//
// If @params contains a 'titleMarkup', 'bannerMarkup', or
// 'bodyMarkup' parameter with the value %true, then the corresponding
// element is assumed to use pango markup. If the parameter is not
// present for an element, then anything that looks like markup in
// that element will appear literally in the output.
//
// If @params contains a 'clear' parameter with the value %true, then
// the content and the action area of the notification will be cleared.
// The content area is also always cleared if 'customContent' is false
// because it might contain the @banner that didn't fit in the banner mode.
function Notification(source, title, banner, params) {
    this._init(source, title, banner, params);
}

Notification.prototype = {
    _init: function(source, title, banner, params) {
        this.source = source;
        this.urgency = Urgency.NORMAL;
        this.resident = false;
        // 'transient' is a reserved keyword in JS, so we have to use an alternate variable name
        this.isTransient = false;
        this.expanded = false;
        this._destroyed = false;
        this._useActionIcons = false;
        this._customContent = false;
        this._bannerBodyText = null;
        this._bannerBodyMarkup = false;
        this._titleFitsInBannerMode = true;
        this._titleDirection = St.TextDirection.NONE;
        this._spacing = 0;
        this._scrollPolicy = Gtk.PolicyType.AUTOMATIC;

        source.connect('destroy', Lang.bind(this,
            function (source, reason) {
                this.destroy(reason);
            }));

        this.actor = new St.Button();
        this.actor._delegate = this;
        this.actor.connect('clicked', Lang.bind(this, this._onClicked));
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._table = new St.Table({ name: 'notification',
                                     reactive: true });
        this._table.connect('style-changed', Lang.bind(this, this._styleChanged));
        this.actor.set_child(this._table);

        this._buttonFocusManager = St.FocusManager.get_for_stage(global.stage);

        // The first line should have the title, followed by the
        // banner text, but ellipsized if they won't both fit. We can't
        // make St.Table or St.BoxLayout do this the way we want (don't
        // show banner at all if title needs to be ellipsized), so we
        // use Shell.GenericContainer.
        this._bannerBox = new Shell.GenericContainer();
        this._bannerBox.connect('get-preferred-width', Lang.bind(this, this._bannerBoxGetPreferredWidth));
        this._bannerBox.connect('get-preferred-height', Lang.bind(this, this._bannerBoxGetPreferredHeight));
        this._bannerBox.connect('allocate', Lang.bind(this, this._bannerBoxAllocate));
        this._table.add(this._bannerBox, { row: 0,
                                           col: 1,
                                           y_expand: false,
                                           y_fill: false });

        this._titleLabel = new St.Label();
        this._bannerBox.add_actor(this._titleLabel);
        this._bannerUrlHighlighter = new URLHighlighter();
        this._bannerLabel = this._bannerUrlHighlighter.actor;
        this._bannerBox.add_actor(this._bannerLabel);

        this.update(title, banner, params);
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
        params = Params.parse(params, { customContent: false,
                                        body: null,
                                        icon: null,
                                        titleMarkup: false,
                                        bannerMarkup: false,
                                        bodyMarkup: false,
                                        clear: false });

        this._customContent = params.customContent;

        let oldFocus = global.stage.key_focus;

        if (this._icon)
            this._icon.destroy();
        // We always clear the content area if we don't have custom
        // content because it might contain the @banner that didn't
        // fit in the banner mode.
        if (this._scrollArea && (!this._customContent || params.clear)) {
            if (oldFocus && this._scrollArea.contains(oldFocus))
                this.actor.grab_key_focus();

            this._scrollArea.destroy();
            this._scrollArea = null;
            this._contentArea = null;
        }
        if (this._actionArea && params.clear) {
            if (oldFocus && this._actionArea.contains(oldFocus))
                this.actor.grab_key_focus();

            this._actionArea.destroy();
            this._actionArea = null;
            this._buttonBox = null;
        }
        if (!this._scrollArea && !this._actionArea)
            this._table.remove_style_class_name('multi-line-notification');

        this._icon = params.icon || this.source.createNotificationIcon();
        this._table.add(this._icon, { row: 0,
                                      col: 0,
                                      x_expand: false,
                                      y_expand: false,
                                      y_fill: false,
                                      y_align: St.Align.START });

        title = title ? _fixMarkup(title.replace(/\n/g, ' '), params.titleMarkup) : '';
        this._titleLabel.clutter_text.set_markup('<b>' + title + '</b>');

        if (Pango.find_base_dir(title, -1) == Pango.Direction.RTL)
            this._titleDirection = St.TextDirection.RTL;
        else
            this._titleDirection = St.TextDirection.LTR;

        // Let the title's text direction control the overall direction
        // of the notification - in case where different scripts are used
        // in the notification, this is the right thing for the icon, and
        // arguably for action buttons as well. Labels other than the title
        // will be allocated at the available width, so that their alignment
        // is done correctly automatically.
        this._table.set_direction(this._titleDirection);

        // Unless the notification has custom content, we save this._bannerBodyText
        // to add it to the content of the notification if the notification is
        // expandable due to other elements in its content area or due to the banner
        // not fitting fully in the single-line mode.
        this._bannerBodyText = this._customContent ? null : banner;
        this._bannerBodyMarkup = params.bannerMarkup;

        banner = banner ? banner.replace(/\n/g, '  ') : '';

        this._bannerUrlHighlighter.setMarkup(banner, params.bannerMarkup);
        this._bannerLabel.queue_relayout();

        // Add the bannerBody now if we know for sure we'll need it
        if (this._bannerBodyText && this._bannerBodyText.indexOf('\n') > -1)
            this._addBannerBody();

        if (params.body)
            this.addBody(params.body, params.bodyMarkup);
        this._updated();
    },

    setIconVisible: function(visible) {
        this._icon.visible = visible;
    },

    enableScrolling: function(enableScrolling) {
        this._scrollPolicy = enableScrolling ? Gtk.PolicyType.AUTOMATIC : Gtk.PolicyType.NEVER;
        if (this._scrollArea)
            this._scrollArea.vscrollbar_policy = this._scrollPolicy;
    },

    _createScrollArea: function() {
        this._table.add_style_class_name('multi-line-notification');
        this._scrollArea = new St.ScrollView({ name: 'notification-scrollview',
                                               vscrollbar_policy: this._scrollPolicy,
                                               hscrollbar_policy: Gtk.PolicyType.NEVER,
                                               vfade: true });
        this._table.add(this._scrollArea, { row: 1, col: 1 });
        this._contentArea = new St.BoxLayout({ name: 'notification-body',
                                               vertical: true });
        this._scrollArea.add_actor(this._contentArea);
        // If we know the notification will be expandable, we need to add
        // the banner text to the body as the first element.
        this._addBannerBody();
    },

    // addActor:
    // @actor: actor to add to the body of the notification
    //
    // Appends @actor to the notification's body
    addActor: function(actor, style) {
        if (!this._scrollArea) {
            this._createScrollArea();
        }

        this._contentArea.add(actor, style ? style : {});
        this._updated();
    },

    // addBody:
    // @text: the text
    // @markup: %true if @text contains pango markup
    // @style: style to use when adding the actor containing the text
    //
    // Adds a multi-line label containing @text to the notification.
    //
    // Return value: the newly-added label
    addBody: function(text, markup, style) {
        let label = new URLHighlighter(text, true, markup);

        this.addActor(label.actor, style);
        return label.actor;
    },

    _addBannerBody: function() {
        if (this._bannerBodyText) {
            let text = this._bannerBodyText;
            this._bannerBodyText = null;
            this.addBody(text, this._bannerBodyMarkup);
        }
    },

    // scrollTo:
    // @side: St.Side.TOP or St.Side.BOTTOM
    //
    // Scrolls the content area (if scrollable) to the indicated edge
    scrollTo: function(side) {
        let adjustment = this._scrollArea.vscroll.adjustment;
        if (side == St.Side.TOP)
            adjustment.value = adjustment.lower;
        else if (side == St.Side.BOTTOM)
            adjustment.value = adjustment.upper;
    },

    // setActionArea:
    // @actor: the actor
    // @props: (option) St.Table child properties
    //
    // Puts @actor into the action area of the notification, replacing
    // the previous contents
    setActionArea: function(actor, props) {
        if (this._actionArea) {
            this._actionArea.destroy();
            this._actionArea = null;
            if (this._buttonBox)
                this._buttonBox = null;
        } else {
            this._addBannerBody();
        }
        this._actionArea = actor;

        if (!props)
            props = {};
        props.row = 2;
        props.col = 1;

        this._table.add_style_class_name('multi-line-notification');
        this._table.add(this._actionArea, props);
        this._updated();
    },

    // addButton:
    // @id: the action ID
    // @label: the label for the action's button
    //
    // Adds a button with the given @label to the notification. All
    // action buttons will appear in a single row at the bottom of
    // the notification.
    //
    // If the button is clicked, the notification will emit the
    // %action-invoked signal with @id as a parameter
    addButton: function(id, label) {
        if (!this._buttonBox) {

            let box = new St.BoxLayout({ name: 'notification-actions' });
            this.setActionArea(box, { x_expand: false,
                                      x_fill: false,
                                      x_align: St.Align.END });
            this._buttonBox = box;
        }

        let button = new St.Button({ can_focus: true });

        if (this._useActionIcons && Gtk.IconTheme.get_default().has_icon(id)) {
            button.add_style_class_name('notification-icon-button');
            button.child = new St.Icon({ icon_name: id });
        } else {
            button.add_style_class_name('notification-button');
            button.label = label;
        }

        if (this._buttonBox.get_children().length > 0)
            this._buttonFocusManager.remove_group(this._buttonBox);

        this._buttonBox.add(button);
        this._buttonFocusManager.add_group(this._buttonBox);
        button.connect('clicked', Lang.bind(this, this._onActionInvoked, id));

        this._updated();
    },

    setUrgency: function(urgency) {
        this.urgency = urgency;
    },

    setResident: function(resident) {
        this.resident = resident;
    },

    setTransient: function(isTransient) {
        this.isTransient = isTransient;
    },

    setUseActionIcons: function(useIcons) {
        this._useActionIcons = useIcons;
    },

    _styleChanged: function() {
        this._spacing = this._table.get_theme_node().get_length('spacing-columns');
    },

    _bannerBoxGetPreferredWidth: function(actor, forHeight, alloc) {
        let [titleMin, titleNat] = this._titleLabel.get_preferred_width(forHeight);
        let [bannerMin, bannerNat] = this._bannerLabel.get_preferred_width(forHeight);

        alloc.min_size = titleMin;
        alloc.natural_size = titleNat + this._spacing + bannerNat;
    },

    _bannerBoxGetPreferredHeight: function(actor, forWidth, alloc) {
        [alloc.min_size, alloc.natural_size] =
            this._titleLabel.get_preferred_height(forWidth);
    },

    _bannerBoxAllocate: function(actor, box, flags) {
        let [titleMinW, titleNatW] = this._titleLabel.get_preferred_width(-1);
        let [titleMinH, titleNatH] = this._titleLabel.get_preferred_height(-1);
        let [bannerMinW, bannerNatW] = this._bannerLabel.get_preferred_width(-1);
        let availWidth = box.x2 - box.x1;

        let titleBox = new Clutter.ActorBox();
        let titleBoxW = Math.min(titleNatW, availWidth);
        if (this._titleDirection == St.TextDirection.RTL) {
            titleBox.x1 = availWidth - titleBoxW;
            titleBox.x2 = availWidth;
        } else {
            titleBox.x1 = 0;
            titleBox.x2 = titleBoxW;
        }
        titleBox.y1 = 0;
        titleBox.y2 = titleNatH;
        this._titleLabel.allocate(titleBox, flags);
        this._titleFitsInBannerMode = (titleNatW <= availWidth);

        let bannerFits = true;
        if (titleBoxW + this._spacing > availWidth) {
            this._bannerLabel.opacity = 0;
            bannerFits = false;
        } else {
            let bannerBox = new Clutter.ActorBox();

            if (this._titleDirection == St.TextDirection.RTL) {
                bannerBox.x1 = 0;
                bannerBox.x2 = titleBox.x1 - this._spacing;

                bannerFits = (bannerBox.x2 - bannerNatW >= 0);
            } else {
                bannerBox.x1 = titleBox.x2 + this._spacing;
                bannerBox.x2 = availWidth;

                bannerFits = (bannerBox.x1 + bannerNatW <= availWidth);
            }
            bannerBox.y1 = 0;
            bannerBox.y2 = titleNatH;
            this._bannerLabel.allocate(bannerBox, flags);

            // Make _bannerLabel visible if the entire notification
            // fits on one line, or if the notification is currently
            // unexpanded and only showing one line anyway.
            if (!this.expanded || (bannerFits && this._table.row_count == 1))
                this._bannerLabel.opacity = 255;
        }

        // If the banner doesn't fully fit in the banner box, we possibly need to add the
        // banner to the body. We can't do that from here though since that will force a
        // relayout, so we add it to the main loop.
        if (!bannerFits && this._canExpandContent())
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW,
                           Lang.bind(this,
                                     function() {
                                        if (this._canExpandContent()) {
                                            this._addBannerBody();
                                            this._table.add_style_class_name('multi-line-notification');
                                            this._updated();
                                        }
                                        return false;
                                     }));
    },

    _canExpandContent: function() {
        return this._bannerBodyText ||
               (!this._titleFitsInBannerMode && !this._table.has_style_class_name('multi-line-notification'));
    },

    _updated: function() {
        if (this.expanded)
            this.expand(false);
    },

    expand: function(animate) {
        this.expanded = true;
        // The banner is never shown when the title did not fit, so this
        // can be an if-else statement.
        if (!this._titleFitsInBannerMode) {
            // Remove ellipsization from the title label and make it wrap so that
            // we show the full title when the notification is expanded.
            this._titleLabel.clutter_text.line_wrap = true;
            this._titleLabel.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;
            this._titleLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        } else if (this._table.row_count > 1 && this._bannerLabel.opacity != 0) {
            // We always hide the banner if the notification has additional content.
            //
            // We don't need to wrap the banner that doesn't fit the way we wrap the
            // title that doesn't fit because we won't have a notification with
            // row_count=1 that has a banner that doesn't fully fit. We'll either add
            // that banner to the content of the notification in _bannerBoxAllocate()
            // or the notification will have custom content.
            if (animate)
                Tweener.addTween(this._bannerLabel,
                                 { opacity: 0,
                                   time: ANIMATION_TIME,
                                   transition: 'easeOutQuad' });
            else
                this._bannerLabel.opacity = 0;
        }
        this.emit('expanded');
    },

    collapseCompleted: function() {
        if (this._destroyed)
            return;
        this.expanded = false;
        // Make sure we don't line wrap the title, and ellipsize it instead.
        this._titleLabel.clutter_text.line_wrap = false;
        this._titleLabel.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        // Restore banner opacity in case the notification is shown in the
        // banner mode again on update.
        this._bannerLabel.opacity = 255;
    },

    _onActionInvoked: function(actor, mouseButtonClicked, id) {
        this.emit('action-invoked', id);
        if (!this.resident) {
            // We don't hide a resident notification when the user invokes one of its actions,
            // because it is common for such notifications to update themselves with new
            // information based on the action. We'd like to display the updated information
            // in place, rather than pop-up a new notification.
            this.emit('done-displaying');
            this.destroy();
        }
    },

    _onClicked: function() {
        this.emit('clicked');
        // We hide all types of notifications once the user clicks on them because the common
        // outcome of clicking should be the relevant window being brought forward and the user's
        // attention switching to the window.
        this.emit('done-displaying');
        if (!this.resident)
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
};
Signals.addSignalMethods(Notification.prototype);

function Source(title) {
    this._init(title);
}

Source.prototype = {
    ICON_SIZE: 24,

    _init: function(title) {
        this.title = title;
        this._iconBin = new St.Bin({ width: this.ICON_SIZE,
                                     height: this.ICON_SIZE,
                                     x_fill: true,
                                     y_fill: true });
        this.isTransient = false;
        this.isChat = false;

        this.notifications = [];
    },

    setTransient: function(isTransient) {
        this.isTransient = isTransient;
    },

    // Called to create a new icon actor (of size this.ICON_SIZE).
    // Must be overridden by the subclass if you do not pass icons
    // explicitly to the Notification() constructor.
    createNotificationIcon: function() {
        throw new Error('no implementation of createNotificationIcon in ' + this);
    },

    // Unlike createNotificationIcon, this always returns the same actor;
    // there is only one summary icon actor for a Source.
    getSummaryIcon: function() {
        return this._iconBin;
    },

    pushNotification: function(notification) {
        if (this.notifications.indexOf(notification) < 0) {
            this.notifications.push(notification);
            this.emit('notification-added', notification);
        }

        notification.connect('clicked', Lang.bind(this, this.open));
        notification.connect('destroy', Lang.bind(this,
            function () {
                let index = this.notifications.indexOf(notification);
                if (index < 0)
                    return;

                this.notifications.splice(index, 1);
                if (this.notifications.length == 0)
                    this._lastNotificationRemoved();
            }));
    },

    notify: function(notification) {
        this.pushNotification(notification);
        this.emit('notify', notification);
    },

    destroy: function(reason) {
        this.emit('destroy', reason);
    },

    // A subclass can redefine this to "steal" clicks from the
    // summaryitem; Use Clutter.get_current_event() to get the
    // details, return true to prevent the default handling from
    // ocurring.
    handleSummaryClick: function() {
        return false;
    },

    //// Protected methods ////

    // The subclass must call this at least once to set the summary icon.
    _setSummaryIcon: function(icon) {
        if (this._iconBin.child)
            this._iconBin.child.destroy();
        this._iconBin.child = icon;
    },

    // Default implementation is to do nothing, but subclasses can override
    open: function(notification) {
    },

    destroyNonResidentNotifications: function() {
        for (let i = this.notifications.length - 1; i >= 0; i--)
            if (!this.notifications[i].resident)
                this.notifications[i].destroy();
    },

    // Default implementation is to destroy this source, but subclasses can override
    _lastNotificationRemoved: function() {
        this.destroy();
    }
};
Signals.addSignalMethods(Source.prototype);

function SummaryItem(source) {
    this._init(source);
}

SummaryItem.prototype = {
    _init: function(source) {
        this.source = source;
        this.source.connect('notification-added', Lang.bind(this, this._notificationAddedToSource));

        this.actor = new St.Button({ style_class: 'summary-source-button',
                                     y_fill: true,
                                     reactive: true,
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.TWO | St.ButtonMask.THREE,
                                     track_hover: true });

        this._sourceBox = new St.BoxLayout({ style_class: 'summary-source' });

        this._sourceIcon = source.getSummaryIcon();
        this._sourceTitleBin = new St.Bin({ y_align: St.Align.MIDDLE,
                                            x_fill: true,
                                            clip_to_allocation: true });
        this._sourceTitle = new St.Label({ style_class: 'source-title',
                                           text: source.title });
        this._sourceTitle.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._sourceTitleBin.child = this._sourceTitle;
        this._sourceTitleBin.width = 0;

        this._sourceBox.add(this._sourceIcon, { y_fill: false });
        this._sourceBox.add(this._sourceTitleBin, { expand: true, y_fill: false });
        this.actor.child = this._sourceBox;

        this.notificationStackView = new St.ScrollView({ name: source.isChat ? '' : 'summary-notification-stack-scrollview',
                                                         vscrollbar_policy: source.isChat ? Gtk.PolicyType.NEVER : Gtk.PolicyType.AUTOMATIC,
                                                         hscrollbar_policy: Gtk.PolicyType.NEVER,
                                                         vfade: true });
        this.notificationStack = new St.BoxLayout({ name: 'summary-notification-stack',
                                                     vertical: true });
        this.notificationStackView.add_actor(this.notificationStack);
        this._notificationExpandedIds = [];
        this._notificationDoneDisplayingIds = [];
        this._notificationDestroyedIds = [];

        this._oldMaxScrollAdjustment = 0;

        this.notificationStackView.vscroll.adjustment.connect('changed', Lang.bind(this, function(adjustment) {
            let currentValue = adjustment.value + adjustment.page_size;
            if (currentValue == this._oldMaxScrollAdjustment)
                this.scrollTo(St.Side.BOTTOM);
            this._oldMaxScrollAdjustment = adjustment.upper;
        }));

        this.rightClickMenu = new St.BoxLayout({ name: 'summary-right-click-menu',
                                                 vertical: true });

        let item;

        item = new PopupMenu.PopupMenuItem(_("Open"));
        item.connect('activate', Lang.bind(this, function() {
            source.open();
            this.emit('done-displaying-content');
        }));
        this.rightClickMenu.add(item.actor);

        item = new PopupMenu.PopupMenuItem(_("Remove"));
        item.connect('activate', Lang.bind(this, function() {
            source.destroy();
            this.emit('done-displaying-content');
        }));
        this.rightClickMenu.add(item.actor);

        let focusManager = St.FocusManager.get_for_stage(global.stage);
        focusManager.add_group(this.rightClickMenu);
    },

    // getTitleNaturalWidth, getTitleWidth, and setTitleWidth include
    // the spacing between the icon and title (which is actually
    // _sourceTitle's padding-left) as part of the width.

    getTitleNaturalWidth: function() {
        let [minWidth, naturalWidth] = this._sourceTitle.get_preferred_width(-1);

        return Math.min(naturalWidth, MAX_SOURCE_TITLE_WIDTH);
    },

    getTitleWidth: function() {
        return this._sourceTitleBin.width;
    },

    setTitleWidth: function(width) {
        width = Math.round(width);
        if (width != this._sourceTitleBin.width)
            this._sourceTitleBin.width = width;
    },

    setEllipsization: function(mode) {
        this._sourceTitle.clutter_text.ellipsize = mode;
    },

    prepareNotificationStackForShowing: function() {
        if (this.notificationStack.get_children().length > 0)
            return;

        for (let i = 0; i < this.source.notifications.length; i++) {
            this._appendNotificationToStack(this.source.notifications[i]);
        }
    },

    doneShowingNotificationStack: function() {
        let notificationActors = this.notificationStack.get_children();
        for (let i = 0; i < notificationActors.length; i++) {
            notificationActors[i]._delegate.collapseCompleted();
            notificationActors[i]._delegate.disconnect(this._notificationExpandedIds[i]);
            notificationActors[i]._delegate.disconnect(this._notificationDoneDisplayingIds[i]);
            notificationActors[i]._delegate.disconnect(this._notificationDestroyedIds[i]);
            this.notificationStack.remove_actor(notificationActors[i]);
            notificationActors[i]._delegate.setIconVisible(true);
            notificationActors[i]._delegate.enableScrolling(true);
        }
        this._notificationExpandedIds = [];
        this._notificationDoneDisplayingIds = [];
        this._notificationDestroyedIds = [];
    },

    _notificationAddedToSource: function(source, notification) {
        if (this.notificationStack.mapped)
            this._appendNotificationToStack(notification);
    },

    _appendNotificationToStack: function(notification) {
        let notificationExpandedId = notification.connect('expanded', Lang.bind(this, this._contentUpdated));
        this._notificationExpandedIds.push(notificationExpandedId);
        let notificationDoneDisplayingId = notification.connect('done-displaying', Lang.bind(this, this._notificationDoneDisplaying));
        this._notificationDoneDisplayingIds.push(notificationDoneDisplayingId);
        let notificationDestroyedId = notification.connect('destroy', Lang.bind(this, this._notificationDestroyed));
        this._notificationDestroyedIds.push(notificationDestroyedId);
        if (!this.source.isChat)
            notification.enableScrolling(false);
        if (this.notificationStack.get_children().length > 0)
            notification.setIconVisible(false);
        this.notificationStack.add(notification.actor);
        notification.expand(false);
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

    _contentUpdated: function() {
        this.emit('content-updated');
    },

    _notificationDoneDisplaying: function() {
        this.emit('done-displaying-content');
    },

    _notificationDestroyed: function(notification) {
        let index = this.notificationStack.get_children().indexOf(notification.actor);
        if (index >= 0) {
            notification.disconnect(this._notificationExpandedIds[index]);
            this._notificationExpandedIds.splice(index, 1);
            notification.disconnect(this._notificationDoneDisplayingIds[index]);
            this._notificationDoneDisplayingIds.splice(index, 1);
            notification.disconnect(this._notificationDestroyedIds[index]);
            this._notificationDestroyedIds.splice(index, 1);
            this.notificationStack.remove_actor(notification.actor);
            this._contentUpdated();
        }
        if (this.notificationStack.get_children().length > 0)
            this.notificationStack.get_children()[0]._delegate.setIconVisible(true);
    }
};
Signals.addSignalMethods(SummaryItem.prototype);

function MessageTray() {
    this._init();
}

MessageTray.prototype = {
    _init: function() {
        this._presence = new GnomeSession.Presence();
        this._userStatus = GnomeSession.PresenceStatus.AVAILABLE;
        this._busy = false;
        this._backFromAway = false;
        this._presence.connect('StatusChanged', Lang.bind(this, this._onStatusChanged));
        this._presence.getStatus(Lang.bind(this, this._onStatusChanged));

        this.actor = new St.Group({ name: 'message-tray',
                                    reactive: true,
                                    track_hover: true });
        this.actor.connect('notify::hover', Lang.bind(this, this._onTrayHoverChanged));

        this._notificationBin = new St.Bin();
        this.actor.add_actor(this._notificationBin);
        this._notificationBin.hide();
        this._notificationQueue = [];
        this._notification = null;
        this._notificationClickedId = 0;

        this._summaryBin = new St.Bin({ x_align: St.Align.END });
        this.actor.add_actor(this._summaryBin);
        this._summary = new St.BoxLayout({ name: 'summary-mode',
                                           reactive: true,
                                           track_hover: true });
        this._summary.connect('notify::hover', Lang.bind(this, this._onSummaryHoverChanged));
        this._summaryBin.child = this._summary;
        this._summaryBin.opacity = 0;

        this._summaryMotionId = 0;

        this._summaryBoxPointer = new BoxPointer.BoxPointer(St.Side.BOTTOM,
                                                           { reactive: true,
                                                             track_hover: true });
        this._summaryBoxPointer.actor.style_class = 'summary-boxpointer';
        this.actor.add_actor(this._summaryBoxPointer.actor);
        this._summaryBoxPointer.actor.lower_bottom();
        this._summaryBoxPointer.actor.hide();

        this._summaryBoxPointerItem = null;
        this._summaryBoxPointerContentUpdatedId = 0;
        this._summaryBoxPointerDoneDisplayingId = 0;
        this._clickedSummaryItem = null;
        this._clickedSummaryItemMouseButton = -1;
        this._clickedSummaryItemAllocationChangedId = 0;
        this._expandedSummaryItem = null;
        this._summaryItemTitleWidth = 0;
        this._pointerBarrier = 0;

        // To simplify the summary item animation code, we pretend
        // that there's an invisible SummaryItem to the left of the
        // leftmost real summary item, and that it's expanded when all
        // of the other items are collapsed.
        this._imaginarySummaryItemTitleWidth = 0;

        this._focusGrabber = new FocusGrabber();
        this._focusGrabber.connect('focus-grabbed', Lang.bind(this,
            function() {
                if (this._summaryBoxPointer.bin.child)
                    this._lock();
            }));
        this._focusGrabber.connect('focus-ungrabbed', Lang.bind(this, this._unlock));
        this._focusGrabber.connect('button-pressed', Lang.bind(this,
           function(focusGrabber, source) {
               if (this._clickedSummaryItem && !this._clickedSummaryItem.actor.contains(source))
                   this._unsetClickedSummaryItem();
               this._focusGrabber.ungrabFocus();
           }));
        this._focusGrabber.connect('escape-pressed', Lang.bind(this, this._escapeTray));

        this._trayState = State.HIDDEN;
        this._locked = false;
        this._useLongerTrayLeftTimeout = false;
        this._trayLeftTimeoutId = 0;
        this._pointerInTray = false;
        this._summaryState = State.HIDDEN;
        this._summaryTimeoutId = 0;
        this._pointerInSummary = false;
        this._notificationState = State.HIDDEN;
        this._notificationTimeoutId = 0;
        this._notificationExpandedId = 0;
        this._summaryBoxPointerState = State.HIDDEN;
        this._summaryBoxPointerTimeoutId = 0;
        this._overviewVisible = Main.overview.visible;
        this._notificationRemoved = false;
        this._reNotifyAfterHideNotification = null;

        Main.chrome.addActor(this.actor, { affectsStruts: false,
                                           visibleInOverview: true });
        Main.chrome.trackActor(this._notificationBin);
        Main.chrome.trackActor(this._summaryBoxPointer.actor);

        global.screen.connect('monitors-changed', Lang.bind(this, this._setSizePosition));

        this._setSizePosition();

        Main.overview.connect('showing', Lang.bind(this,
            function() {
                this._overviewVisible = true;
                if (this._locked) {
                    this._unsetClickedSummaryItem();
                    this._unlock();
                } else {
                    this._updateState();
                }
            }));
        Main.overview.connect('hiding', Lang.bind(this,
            function() {
                this._overviewVisible = false;
                if (this._locked) {
                    this._unsetClickedSummaryItem();
                    this._unlock();
                } else {
                    this._updateState();
                }
            }));

        this._summaryItems = [];
        // We keep a list of new summary items that were added to the summary since the last
        // time it was shown to the user. We automatically show the summary to the user if there
        // are items in this list once the notifications are done showing or once an item gets
        // added to the summary without a notification being shown.
        this._newSummaryItems = [];
        this._longestSummaryItem = null;
        this._chatSummaryItemsCount = 0;
    },

    _setSizePosition: function() {
        let primary = global.get_primary_monitor();
        this.actor.x = primary.x;
        this.actor.y = primary.y + primary.height - 1;
        this.actor.width = primary.width;
        this._notificationBin.x = 0;
        this._notificationBin.width = primary.width;
        this._summaryBin.x = 0;
        this._summaryBin.width = primary.width;

        if (this._pointerBarrier)
            global.destroy_pointer_barrier(this._pointerBarrier);
        this._pointerBarrier =
            global.create_pointer_barrier(primary.x + primary.width, primary.y + primary.height - this.actor.height,
                                          primary.x + primary.width, primary.y + primary.height,
                                          4 /* BarrierNegativeX */);


    },

    contains: function(source) {
        return this._getIndexOfSummaryItemForSource(source) >= 0;
    },

    _getIndexOfSummaryItemForSource: function(source) {
        for (let i = 0; i < this._summaryItems.length; i++) {
            if (this._summaryItems[i].source == source)
                return i;
        }
        return -1;
    },

    add: function(source) {
        if (this.contains(source)) {
            log('Trying to re-add source ' + source.title);
            return;
        }

        let summaryItem = new SummaryItem(source);

        if (source.isChat) {
            this._summary.insert_actor(summaryItem.actor, 0);
            this._chatSummaryItemsCount++;
        } else {
            this._summary.insert_actor(summaryItem.actor, this._chatSummaryItemsCount);
        }

        let titleWidth = summaryItem.getTitleNaturalWidth();
        if (titleWidth > this._summaryItemTitleWidth) {
            this._summaryItemTitleWidth = titleWidth;
            if (!this._expandedSummaryItem)
                this._imaginarySummaryItemTitleWidth = titleWidth;
            this._longestSummaryItem = summaryItem;
        }

        this._summaryItems.push(summaryItem);

        // We keep this._newSummaryItems to track any new sources that were added to the
        // summary and show the summary with them to the user for a short period of time
        // after notifications are done showing. However, we don't want that to happen for
        // transient sources, which are removed after the notification is shown, but are
        // not removed fast enough because of the callbacks to avoid the summary popping up.
        // So we just don't add transient sources to this._newSummaryItems .
        if (!source.isTransient)
            this._newSummaryItems.push(summaryItem);

        source.connect('notify', Lang.bind(this, this._onNotify));

        summaryItem.actor.connect('notify::hover', Lang.bind(this,
            function () {
                this._onSummaryItemHoverChanged(summaryItem);
            }));

        summaryItem.actor.connect('clicked', Lang.bind(this,
            function (actor, button) {
                this._onSummaryItemClicked(summaryItem, button);
            }));

        source.connect('destroy', Lang.bind(this, this._onSourceDestroy));

        // We need to display the newly-added summary item, but if the
        // caller is about to post a notification, we want to show that
        // *first* and not show the summary item until after it hides.
        // So postpone calling _updateState() a tiny bit.
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function() { this._updateState(); return false; }));
    },

    _onSourceDestroy: function(source) {
        let index = this._getIndexOfSummaryItemForSource(source);
        if (index == -1)
            return;

        let summaryItemToRemove = this._summaryItems[index];

        let newSummaryItemsIndex = this._newSummaryItems.indexOf(this._summaryItems[index]);
        if (newSummaryItemsIndex != -1)
            this._newSummaryItems.splice(newSummaryItemsIndex, 1);

        this._summaryItems.splice(index, 1);

        if (source.isChat)
            this._chatSummaryItemsCount--;

        if (this._expandedSummaryItem == summaryItemToRemove)
            this._expandedSummaryItem = null;

        if (this._longestSummaryItem.source == source) {
            let newTitleWidth = 0;
            this._longestSummaryItem = null;
            for (let i = 0; i < this._summaryItems.length; i++) {
                let summaryItem = this._summaryItems[i];
                let titleWidth = summaryItem.getTitleNaturalWidth();
                if (titleWidth > newTitleWidth) {
                    newTitleWidth = titleWidth;
                    this._longestSummaryItem = summaryItem;
                }
            }

            this._summaryItemTitleWidth = newTitleWidth;
            if (!this._expandedSummaryItem)
                this._imaginarySummaryItemTitleWidth = newTitleWidth;
        }

        let needUpdate = false;

        if (this._notification && this._notification.source == source) {
            this._updateNotificationTimeout(0);
            this._notificationRemoved = true;
            needUpdate = true;
        }
        if (this._clickedSummaryItem == summaryItemToRemove) {
            this._unsetClickedSummaryItem();
            needUpdate = true;
        }

        summaryItemToRemove.actor.destroy();

        if (needUpdate);
            this._updateState();
    },

    _onNotificationDestroy: function(notification) {
        if (this._notification == notification && (this._notificationState == State.SHOWN || this._notificationState == State.SHOWING)) {
            this._updateNotificationTimeout(0);
            this._notificationRemoved = true;
            this._updateState();
            return;
        }

        let index = this._notificationQueue.indexOf(notification);
        notification.destroy();
        if (index != -1)
            this._notificationQueue.splice(index, 1);
    },

    _lock: function() {
        this._locked = true;
    },

    _unlock: function() {
        if (!this._locked)
            return;
        this._locked = false;
        this._pointerInTray = this.actor.hover && !this._summaryBoxPointer.bin.hover;
        this._updateState();
    },

    _onNotify: function(source, notification) {
        if (this._summaryBoxPointerItem && this._summaryBoxPointerItem.source == source) {
            if (this._summaryBoxPointerState == State.HIDING)
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

    _onSummaryItemHoverChanged: function(summaryItem) {
        if (summaryItem.actor.hover)
            this._setExpandedSummaryItem(summaryItem);
    },

    _setExpandedSummaryItem: function(summaryItem) {
        if (summaryItem == this._expandedSummaryItem)
            return;

        // We can't just animate individual summary items as the
        // pointer moves in and out of them, because if they don't
        // move in sync you get weird-looking wobbling. So whenever
        // there's a change, we have to re-tween the entire summary
        // area.

        // Turn off ellipsization for the previously expanded item that is
        // collapsing and for the item that is expanding because it looks
        // better that way.
        if (this._expandedSummaryItem) {
            // Ideally, we would remove 'expanded' pseudo class when the item
            // is done collapsing, but we don't track when that happens.
            this._expandedSummaryItem.actor.remove_style_pseudo_class('expanded');
            this._expandedSummaryItem.setEllipsization(Pango.EllipsizeMode.NONE);
        }

        this._expandedSummaryItem = summaryItem;
        if (this._expandedSummaryItem) {
            this._expandedSummaryItem.actor.add_style_pseudo_class('expanded');
            this._expandedSummaryItem.setEllipsization(Pango.EllipsizeMode.NONE);
        }

        // We tween on a "_expandedSummaryItemTitleWidth" pseudo-property
        // that represents the current title width of the
        // expanded/expanding item, or the width of the imaginary
        // invisible item if we're collapsing everything.
        Tweener.addTween(this,
                         { _expandedSummaryItemTitleWidth: this._summaryItemTitleWidth,
                           time: ANIMATION_TIME,
                           transition: 'easeOutQuad',
                           onComplete: this._expandSummaryItemCompleted,
                           onCompleteScope: this });
    },

    get _expandedSummaryItemTitleWidth() {
        if (this._expandedSummaryItem)
            return this._expandedSummaryItem.getTitleWidth();
        else
            return this._imaginarySummaryItemTitleWidth;
    },

    set _expandedSummaryItemTitleWidth(expansion) {
        expansion = Math.round(expansion);

        // Expand the expanding item to its new width
        if (this._expandedSummaryItem)
            this._expandedSummaryItem.setTitleWidth(expansion);
        else
            this._imaginarySummaryItemTitleWidth = expansion;

        // Figure out how much space the other items are currently
        // using, and how much they need to be shrunk to keep the
        // total width (including the width of the imaginary item)
        // constant.
        let excess = this._summaryItemTitleWidth - expansion;
        let oldExcess = 0, shrinkage;
        if (excess) {
            for (let i = 0; i < this._summaryItems.length; i++) {
                if (this._summaryItems[i] != this._expandedSummaryItem)
                    oldExcess += this._summaryItems[i].getTitleWidth();
            }
            if (this._expandedSummaryItem)
                oldExcess += this._imaginarySummaryItemTitleWidth;
        }
        if (excess && oldExcess)
            shrinkage = excess / oldExcess;
        else
            shrinkage = 0;

        // Now shrink each one proportionately
        for (let i = 0; i < this._summaryItems.length; i++) {
            if (this._summaryItems[i] == this._expandedSummaryItem)
                continue;

            let oldWidth = this._summaryItems[i].getTitleWidth();
            let newWidth = Math.floor(oldWidth * shrinkage);
            excess -= newWidth;
            this._summaryItems[i].setTitleWidth(newWidth);
        }
        if (this._expandedSummaryItem) {
            let oldWidth = this._imaginarySummaryItemTitleWidth;
            let newWidth = Math.floor(oldWidth * shrinkage);
            excess -= newWidth;
            this._imaginarySummaryItemTitleWidth = newWidth;
        }

        // If the tray as a whole is fully-expanded, make sure the
        // left edge doesn't wobble during animation due to rounding.
        if (this._imaginarySummaryItemTitleWidth == 0 && excess != 0) {
            for (let i = 0; i < this._summaryItems.length; i++) {
                if (this._summaryItems[i] == this._expandedSummaryItem)
                    continue;

                let oldWidth = this._summaryItems[i].getTitleWidth();
                if (oldWidth != 0) {
                    this._summaryItems[i].setTitleWidth (oldWidth + excess);
                    break;
                }
            }
        }
    },

    _expandSummaryItemCompleted: function() {
        if (this._expandedSummaryItem)
            this._expandedSummaryItem.setEllipsization(Pango.EllipsizeMode.END);
    },

    _onSummaryItemClicked: function(summaryItem, button) {
        if (summaryItem.source.handleSummaryClick())
            this._unsetClickedSummaryItem();
        else if (!this._clickedSummaryItem ||
                 this._clickedSummaryItem != summaryItem ||
                 this._clickedSummaryItemMouseButton != button) {
            this._clickedSummaryItem = summaryItem;
            this._clickedSummaryItemMouseButton = button;
        } else {
            this._unsetClickedSummaryItem();
        }

        this._updateState();
    },

    _onSummaryHoverChanged: function() {
        this._pointerInSummary = this._summary.hover;
        this._updateState();
    },

    _onTrayHoverChanged: function() {
        if (this.actor.hover) {
            // Don't do anything if the one pixel area at the bottom is hovered over while the tray is hidden.
            if (this._trayState == State.HIDDEN)
                return;

            // Don't do anything if this._useLongerTrayLeftTimeout is true, meaning the notification originally
            // popped up under the pointer, but this._trayLeftTimeoutId is 0, meaning the pointer didn't leave
            // the tray yet. We need to check for this case because sometimes _onTrayHoverChanged() gets called
            // multiple times while this.actor.hover is true.
            if (this._useLongerTrayLeftTimeout && !this._trayLeftTimeoutId)
                return;

            // Don't do anything if the mouse is over the summary notification as this should be considered as
            // leaving the tray. The tray is locked when the summary notification is visible anyway, but we
            // should treat the mouse being over the summary notification as the tray being left for collapsing
            // any expanded summary item other than the one related to the notification.
            if (this._summaryBoxPointer.bin.hover)
                return;

            this._useLongerTrayLeftTimeout = false;
            if (this._trayLeftTimeoutId) {
                Mainloop.source_remove(this._trayLeftTimeoutId);
                this._trayLeftTimeoutId = 0;
                this._trayLeftMouseX = -1;
                this._trayLeftMouseY = -1;
                return;
            }

            if (this._showNotificationMouseX >= 0) {
                let actorAtShowNotificationPosition =
                    global.stage.get_actor_at_pos(Clutter.PickMode.ALL, this._showNotificationMouseX, this._showNotificationMouseY);
                this._showNotificationMouseX = -1;
                this._showNotificationMouseY = -1;
                // Don't set this._pointerInTray to true if the pointer was initially in the area where the notification
                // popped up. That way we will not be expanding notifications that happen to pop up over the pointer
                // automatically. Instead, the user is able to expand the notification by mousing away from it and then
                // mousing back in. Because this is an expected action, we set the boolean flag that indicates that a longer
                // timeout should be used before popping down the notification.
                if (this._notificationBin.contains(actorAtShowNotificationPosition)) {
                    this._useLongerTrayLeftTimeout = true;
                    return;
                }
            }
            this._pointerInTray = true;
            this._updateState();
        } else {
            // We record the position of the mouse the moment it leaves the tray. These coordinates are used in
            // this._onTrayLeftTimeout() to determine if the mouse has moved far enough during the initial timeout for us
            // to consider that the user intended to leave the tray and therefore hide the tray. If the mouse is still
            // close to its previous position, we extend the timeout once.
            let [x, y, mods] = global.get_pointer();
            this._trayLeftMouseX = x;
            this._trayLeftMouseY = y;

            // We wait just a little before hiding the message tray in case the user quickly moves the mouse back into it.
            // We wait for a longer period if the notification popped up where the mouse pointer was already positioned.
            // That gives the user more time to mouse away from the notification and mouse back in in order to expand it.
            let timeout = this._useLongerHideTimeout ? LONGER_HIDE_TIMEOUT * 1000 : HIDE_TIMEOUT * 1000;
            this._trayLeftTimeoutId = Mainloop.timeout_add(timeout, Lang.bind(this, this._onTrayLeftTimeout));
        }
    },

    _onStatusChanged: function(presence, status) {
        this._backFromAway = (this._userStatus == GnomeSession.PresenceStatus.IDLE && this._userStatus != status);
        this._userStatus = status;

        if (status == GnomeSession.PresenceStatus.BUSY) {
            // remove notification and allow the summary to be closed now
            this._updateNotificationTimeout(0);
            if (this._summaryTimeoutId) {
                Mainloop.source_remove(this._summaryTimeoutId);
                this._summaryTimeoutId = 0;
            }
            this._busy = true;
        } else if (status != GnomeSession.PresenceStatus.IDLE) {
            // We preserve the previous value of this._busy if the status turns to IDLE
            // so that we don't start showing notifications queued during the BUSY state
            // as the screensaver gets activated.
            this._busy = false;
        }

        this._updateState();
    },

    _onTrayLeftTimeout: function() {
        let [x, y, mods] = global.get_pointer();
        // We extend the timeout once if the mouse moved no further than MOUSE_LEFT_ACTOR_THRESHOLD to either side or up.
        // We don't check how far down the mouse moved because any point above the tray, but below the exit coordinate,
        // is close to the tray.
        if (this._trayLeftMouseX > -1 &&
            y > this._trayLeftMouseY - MOUSE_LEFT_ACTOR_THRESHOLD &&
            x < this._trayLeftMouseX + MOUSE_LEFT_ACTOR_THRESHOLD &&
            x > this._trayLeftMouseX - MOUSE_LEFT_ACTOR_THRESHOLD) {
            this._trayLeftMouseX = -1;
            this._trayLeftTimeoutId = Mainloop.timeout_add(LONGER_HIDE_TIMEOUT * 1000,
                                                             Lang.bind(this, this._onTrayLeftTimeout));
        } else {
            this._trayLeftTimeoutId = 0;
            this._useLongerTrayLeftTimeout = false;
            this._pointerInTray = false;
            this._pointerInSummary = false;
            this._updateNotificationTimeout(0);
            this._updateState();
        }
        return false;
    },

    _escapeTray: function() {
        this._unlock();
        this._pointerInTray = false;
        this._pointerInSummary = false;
        this._updateNotificationTimeout(0);
        this._updateState();
    },

    // All of the logic for what happens when occurs here; the various
    // event handlers merely update variables such as
    // 'this._pointerInTray', 'this._summaryState', etc, and
    // _updateState() figures out what (if anything) needs to be done
    // at the present time.
    _updateState: function() {
        // Notifications
        let notificationUrgent = this._notificationQueue.length > 0 && this._notificationQueue[0].urgency == Urgency.CRITICAL;
        let notificationsPending = this._notificationQueue.length > 0 && (!this._busy || notificationUrgent);
        let notificationPinned = this._pointerInTray && !this._pointerInSummary && !this._notificationRemoved;
        let notificationExpanded = this._notificationBin.y < 0;
        let notificationExpired = (this._notificationTimeoutId == 0 && !(this._notification && this._notification.urgency == Urgency.CRITICAL) && !this._pointerInTray && !this._locked) || this._notificationRemoved;
        let canShowNotification = notificationsPending && this._summaryState == State.HIDDEN;

        if (this._notificationState == State.HIDDEN) {
            if (canShowNotification)
                this._showNotification();
        } else if (this._notificationState == State.SHOWN) {
            if (notificationExpired)
                this._hideNotification();
            else if (notificationPinned && !notificationExpanded)
                this._expandNotification(false);
            else if (notificationPinned)
                this._ensureNotificationFocused();
        }

        // Summary
        let summarySummoned = this._pointerInSummary || this._overviewVisible;
        let summaryPinned = this._summaryTimeoutId != 0 || this._pointerInTray || summarySummoned || this._locked;
        let summaryHovered = this._pointerInTray || this._pointerInSummary;
        let summaryVisibleWithNoHover = (this._overviewVisible || this._locked) && !summaryHovered;
        let summaryNotificationIsForExpandedSummaryItem = (this._clickedSummaryItem == this._expandedSummaryItem);

        let notificationsVisible = (this._notificationState == State.SHOWING ||
                                    this._notificationState == State.SHOWN);
        let notificationsDone = !notificationsVisible && !notificationsPending;

        let summaryOptionalInOverview = this._overviewVisible && !this._locked && !summaryHovered;
        let mustHideSummary = (notificationsPending && (notificationUrgent || summaryOptionalInOverview))
                              || notificationsVisible;

        if (this._summaryState == State.HIDDEN && !mustHideSummary) {
            if (this._backFromAway) {
                // Immediately set this to false, so that we don't schedule a timeout later
                this._backFromAway = false;
                if (!this._busy)
                    this._showSummary(LONGER_SUMMARY_TIMEOUT);
            } else if (notificationsDone && this._newSummaryItems.length > 0 && !this._busy) {
                this._showSummary(SUMMARY_TIMEOUT);
            } else if (summarySummoned) {
                this._showSummary(0);
            }
        } else if (this._summaryState == State.SHOWN) {
            if (!summaryPinned || mustHideSummary)
                this._hideSummary();
            else if (summaryVisibleWithNoHover && !summaryNotificationIsForExpandedSummaryItem)
                // If we are hiding the summary, we'll collapse the expanded summary item when we are done
                // so that there is no animation. However, we should collapse the expanded summary item
                // if the summary is visible, but not hovered over, and the summary notification for the
                // expanded summary item is not being shown.
                this._setExpandedSummaryItem(null);
        }

        // Summary notification
        let haveClickedSummaryItem = this._clickedSummaryItem != null;
        let summarySourceIsMainNotificationSource = (haveClickedSummaryItem && this._notification &&
                                                     this._clickedSummaryItem.source == this._notification.source);
        let canShowSummaryBoxPointer = this._summaryState == State.SHOWN;
        // We only have sources with empty notification stacks for legacy tray icons. Currently, we never attempt
        // to show notifications for legacy tray icons, but this would be necessary if we did.
        let requestedNotificationStackIsEmpty = (this._clickedSummaryItemMouseButton == 1 && this._clickedSummaryItem.source.notifications.length == 0);
        let wrongSummaryNotificationStack = (this._clickedSummaryItemMouseButton == 1 &&
                                             this._summaryBoxPointer.bin.child != this._clickedSummaryItem.notificationStackView);
        let wrongSummaryRightClickMenu = (this._clickedSummaryItemMouseButton == 3 &&
                                          this._summaryBoxPointer.bin.child != this._clickedSummaryItem.rightClickMenu);
        let wrongSummaryBoxPointer = (haveClickedSummaryItem &&
                                      (wrongSummaryNotificationStack || wrongSummaryRightClickMenu));

        if (this._summaryBoxPointerState == State.HIDDEN) {
            if (haveClickedSummaryItem && !summarySourceIsMainNotificationSource && canShowSummaryBoxPointer && !requestedNotificationStackIsEmpty)
                this._showSummaryBoxPointer();
        } else if (this._summaryBoxPointerState == State.SHOWN) {
            if (!haveClickedSummaryItem || !canShowSummaryBoxPointer || wrongSummaryBoxPointer || mustHideSummary)
                this._hideSummaryBoxPointer();
        }

        // Tray itself
        let trayIsVisible = (this._trayState == State.SHOWING ||
                             this._trayState == State.SHOWN);
        let trayShouldBeVisible = (!notificationsDone ||
                                   this._summaryState == State.SHOWING ||
                                   this._summaryState == State.SHOWN);
        if (!trayIsVisible && trayShouldBeVisible)
            this._showTray();
        else if (trayIsVisible && !trayShouldBeVisible)
            this._hideTray();
    },

    _tween: function(actor, statevar, value, params) {
        let onComplete = params.onComplete;
        let onCompleteScope = params.onCompleteScope;
        let onCompleteParams = params.onCompleteParams;

        params.onComplete = this._tweenComplete;
        params.onCompleteScope = this;
        params.onCompleteParams = [statevar, value, onComplete, onCompleteScope, onCompleteParams];

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
        let primary = global.get_primary_monitor();
        this._tween(this.actor, '_trayState', State.SHOWN,
                    { y: primary.y + primary.height - this.actor.height,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad'
                    });
    },

    _hideTray: function() {
        let primary = global.get_primary_monitor();
        this._tween(this.actor, '_trayState', State.HIDDEN,
                    { y: primary.y + primary.height - 1,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad'
                    });
    },

    _showNotification: function() {
        this._notification = this._notificationQueue.shift();
        this._notificationClickedId = this._notification.connect('done-displaying',
                                                                 Lang.bind(this, this._escapeTray));
        this._notificationBin.child = this._notification.actor;

        this._notificationBin.opacity = 0;
        this._notificationBin.y = this.actor.height;
        this._notificationBin.show();

        this._updateShowingNotification();

        let [x, y, mods] = global.get_pointer();
        // We save the position of the mouse at the time when we started showing the notification
        // in order to determine if the notification popped up under it. We make that check if
        // the user starts moving the mouse and _onTrayHoverChanged() gets called. We don't
        // expand the notification if it just happened to pop up under the mouse unless the user
        // explicitly mouses away from it and then mouses back in.
        this._showNotificationMouseX = x;
        this._showNotificationMouseY = y;
        // We save the y coordinate of the mouse at the time when we started showing the notification
        // and then we update it in _notifiationTimeout() if the mouse is moving towards the
        // notification. We don't pop down the notification if the mouse is moving towards it.
        this._lastSeenMouseY = y;
    },

    _updateShowingNotification: function() {
        Tweener.removeTweens(this._notificationBin);

        // We auto-expand notifications with CRITICAL urgency.
        // We use Tweener.removeTweens() to remove a tween that was hiding the notification we are
        // updating, in case that notification was in the process of being hidden. However,
        // Tweener.removeTweens() would also remove a tween that was updating the position of the
        // notification we are updating, in case that notification was already expanded and its height
        // changed. Therefore we need to call this._expandNotification() for expanded notifications
        // to make sure their position is updated.
        if (this._notification.urgency == Urgency.CRITICAL || this._notification.expanded)
            this._expandNotification(true);

        // We tween all notifications to full opacity. This ensures that both new notifications and
        // notifications that might have been in the process of hiding get full opacity.
        //
        // We tween any notification showing in the banner mode to banner height (this._notificationBin.y = 0).
        // This ensures that both new notifications and notifications in the banner mode that might
        // have been in the process of hiding are shown with the banner height.
        //
        // We use this._showNotificationCompleted() onComplete callback to extend the time the updated
        // notification is being shown.
        //
        // We don't set the y parameter for the tween for expanded notifications because
        // this._expandNotification() will result in getting this._notificationBin.y set to the appropriate
        // fully expanded value.
        let tweenParams = { opacity: 255,
                            time: ANIMATION_TIME,
                            transition: 'easeOutQuad',
                            onComplete: this._showNotificationCompleted,
                            onCompleteScope: this
                          };
        if (!this._notification.expanded)
            tweenParams.y = 0;

        this._tween(this._notificationBin, '_notificationState', State.SHOWN, tweenParams);
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
        if (timeout > 0)
            this._notificationTimeoutId =
                Mainloop.timeout_add(timeout,
                                     Lang.bind(this, this._notificationTimeout));
    },

    _notificationTimeout: function() {
        let [x, y, mods] = global.get_pointer();
        if (y > this._lastSeenMouseY + 10 && y < this.actor.y) {
            // The mouse is moving towards the notification, so don't
            // hide it yet. (We just create a new timeout (and destroy
            // the old one) each time because the bookkeeping is
            // simpler.)
            this._lastSeenMouseY = y;
            this._updateNotificationTimeout(1000);
        } else {
            this._notificationTimeoutId = 0;
            this._updateState();
        }

        return false;
    },

    _hideNotification: function() {
        this._focusGrabber.ungrabFocus();
        if (this._notificationExpandedId) {
            this._notification.disconnect(this._notificationExpandedId);
            this._notificationExpandedId = 0;
        }

        this._tween(this._notificationBin, '_notificationState', State.HIDDEN,
                    { y: this.actor.height,
                      opacity: 0,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad',
                      onComplete: this._hideNotificationCompleted,
                      onCompleteScope: this
                    });
    },

    _hideNotificationCompleted: function() {
        this._notificationRemoved = false;
        this._notificationBin.hide();
        this._notificationBin.child = null;
        this._notification.collapseCompleted();
        this._notification.disconnect(this._notificationClickedId);
        this._notificationClickedId = 0;
        let notification = this._notification;
        this._notification = null;
        if (notification.isTransient)
            notification.destroy(NotificationDestroyedReason.EXPIRED);
    },

    _expandNotification: function(autoExpanding) {
        // Don't grab focus in notifications that are auto-expanded.
        if (!autoExpanding)
            this._focusGrabber.grabFocus(this._notification.actor);

        if (!this._notificationExpandedId)
            this._notificationExpandedId =
                this._notification.connect('expanded',
                                           Lang.bind(this, this._onNotificationExpanded));
        // Don't animate changes in notifications that are auto-expanding.
        this._notification.expand(!autoExpanding);
    },

    _onNotificationExpanded: function() {
        let expandedY = this.actor.height - this._notificationBin.height;

        // Don't animate the notification to its new position if it has shrunk:
        // there will be a very visible "gap" that breaks the illusion.

        if (this._notificationBin.y < expandedY)
            this._notificationBin.y = expandedY;
        else if (this._notification.y != expandedY)
            this._tween(this._notificationBin, '_notificationState', State.SHOWN,
                        { y: expandedY,
                          time: ANIMATION_TIME,
                          transition: 'easeOutQuad'
                        });
   },

    // We use this function to grab focus when the user moves the pointer
    // to a notification with CRITICAL urgency that was already auto-expanded.
    _ensureNotificationFocused: function() {
        this._focusGrabber.grabFocus(this._notification.actor);
    },

    _showSummary: function(timeout) {
        let primary = global.get_primary_monitor();
        this._summaryBin.opacity = 0;
        this._summaryBin.y = this.actor.height;
        this._tween(this._summaryBin, '_summaryState', State.SHOWN,
                    { y: 0,
                      opacity: 255,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad',
                      onComplete: this._showSummaryCompleted,
                      onCompleteScope: this,
                      onCompleteParams: [timeout]
                    });
    },

    _showSummaryCompleted: function(timeout) {
        this._newSummaryItems = [];

        if (timeout != 0) {
            this._summaryTimeoutId =
                Mainloop.timeout_add(timeout * 1000,
                                     Lang.bind(this, this._summaryTimeout));
        }
    },

    _summaryTimeout: function() {
        this._summaryTimeoutId = 0;
        this._updateState();
        return false;
    },

    _hideSummary: function() {
        this._tween(this._summaryBin, '_summaryState', State.HIDDEN,
                    { opacity: 0,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad',
                      onComplete: this._hideSummaryCompleted,
                      onCompleteScope: this,
                    });
        this._newSummaryItems = [];
    },

    _hideSummaryCompleted: function() {
        this._setExpandedSummaryItem(null);
    },

    _showSummaryBoxPointer: function() {
        this._summaryBoxPointerItem = this._clickedSummaryItem;
        this._summaryBoxPointerContentUpdatedId = this._summaryBoxPointerItem.connect('content-updated',
                                                                                      Lang.bind(this, this._adjustSummaryBoxPointerPosition));
        this._summaryBoxPointerDoneDisplayingId = this._summaryBoxPointerItem.connect('done-displaying-content',
                                                                                      Lang.bind(this, this._escapeTray));
        if (this._clickedSummaryItemMouseButton == 1) {
            this._notificationQueue = this._notificationQueue.filter( Lang.bind(this,
                function(notification) {
                    return this._summaryBoxPointerItem.source != notification.source;
                }));
            this._summaryBoxPointerItem.prepareNotificationStackForShowing();
            this._summaryBoxPointer.bin.child = this._summaryBoxPointerItem.notificationStackView;
            this._summaryBoxPointerItem.scrollTo(St.Side.BOTTOM);
        } else if (this._clickedSummaryItemMouseButton == 3) {
            this._summaryBoxPointer.bin.child = this._clickedSummaryItem.rightClickMenu;
        }

        this._focusGrabber.grabFocus(this._summaryBoxPointer.bin.child);

        this._clickedSummaryItemAllocationChangedId =
            this._clickedSummaryItem.actor.connect('allocation-changed',
                                                   Lang.bind(this, this._adjustSummaryBoxPointerPosition));
        // _clickedSummaryItem.actor can change absolute position without changing allocation
        this._summaryMotionId = this._summary.connect('allocation-changed',
                                                      Lang.bind(this, this._adjustSummaryBoxPointerPosition));

        this._summaryBoxPointer.actor.opacity = 0;
        this._summaryBoxPointer.actor.show();
        this._adjustSummaryBoxPointerPosition();

        this._summaryBoxPointerState = State.SHOWING;
        this._clickedSummaryItem.actor.add_style_pseudo_class('selected');
        this._summaryBoxPointer.show(true, Lang.bind(this, function() {
            this._summaryBoxPointerState = State.SHOWN;
        }));
    },

    _adjustSummaryBoxPointerPosition: function() {
        // The position of the arrow origin should be the same as center of this._clickedSummaryItem.actor
        if (!this._clickedSummaryItem)
            return;

        this._summaryBoxPointer.setPosition(this._clickedSummaryItem.actor, 0, 0.5);
    },

    _unsetClickedSummaryItem: function() {
        if (this._clickedSummaryItemAllocationChangedId) {
            this._clickedSummaryItem.actor.disconnect(this._clickedSummaryItemAllocationChangedId);
            this._summary.disconnect(this._summaryMotionId);
            this._clickedSummaryItemAllocationChangedId = 0;
            this._summaryMotionId = 0;
        }

        if (this._clickedSummaryItem)
            this._clickedSummaryItem.actor.remove_style_pseudo_class('selected');
        this._clickedSummaryItem = null;
        this._clickedSummaryItemMouseButton = -1;
    },

    _hideSummaryBoxPointer: function() {
        this._summaryBoxPointerState = State.HIDING;
        // Unset this._clickedSummaryItem if we are no longer showing the summary
        if (this._summaryState != State.SHOWN)
            this._unsetClickedSummaryItem();

        this._focusGrabber.ungrabFocus();
        if (this._summaryBoxPointerItem.source.notifications.length == 0) {
            this._summaryBoxPointer.actor.hide();
            this._hideSummaryBoxPointerCompleted();
        } else {
            this._summaryBoxPointer.hide(true, Lang.bind(this, this._hideSummaryBoxPointerCompleted));
        }
    },

    _hideSummaryBoxPointerCompleted: function() {
        let doneShowingNotificationStack = (this._summaryBoxPointer.bin.child == this._summaryBoxPointerItem.notificationStackView);

        this._summaryBoxPointerState = State.HIDDEN;
        this._summaryBoxPointer.bin.child = null;
        this._summaryBoxPointerItem.disconnect(this._summaryBoxPointerContentUpdatedId);
        this._summaryBoxPointerContentUpdatedId = 0;
        this._summaryBoxPointerItem.disconnect(this._summaryBoxPointerDoneDisplayingId);
        this._summaryBoxPointerDoneDisplayingId = 0;

        let sourceNotificationStackDoneShowing = null;
        if (doneShowingNotificationStack) {
            this._summaryBoxPointerItem.doneShowingNotificationStack();
            sourceNotificationStackDoneShowing = this._summaryBoxPointerItem.source;
        }

        this._summaryBoxPointerItem = null;

        if (sourceNotificationStackDoneShowing) {
            if (sourceNotificationStackDoneShowing.isTransient && !this._reNotifyAfterHideNotification)
                sourceNotificationStackDoneShowing.destroy(NotificationDestroyedReason.EXPIRED);
            if (this._reNotifyAfterHideNotification) {
                this._onNotify(this._reNotifyAfterHideNotification.source, this._reNotifyAfterHideNotification);
                this._reNotifyAfterHideNotification = null;
            }
        }

        if (this._clickedSummaryItem)
            this._updateState();
    }
};

function SystemNotificationSource() {
    this._init();
}

SystemNotificationSource.prototype = {
    __proto__:  Source.prototype,

    _init: function() {
        Source.prototype._init.call(this, _("System Information"));

        this._setSummaryIcon(this.createNotificationIcon());
    },

    createNotificationIcon: function() {
        return new St.Icon({ icon_name: 'dialog-information',
                             icon_type: St.IconType.SYMBOLIC,
                             icon_size: this.ICON_SIZE });
    },

    open: function() {
        this.destroy();
    }
};
