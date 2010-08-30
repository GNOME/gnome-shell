/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Tweener = imports.ui.tweener;

const Main = imports.ui.main;
const Params = imports.misc.params;

const ANIMATION_TIME = 0.2;
const NOTIFICATION_TIMEOUT = 4;
const SUMMARY_TIMEOUT = 1;

const HIDE_TIMEOUT = 0.2;

const BUTTON_ICON_SIZE = 36;

const MAX_SOURCE_TITLE_WIDTH = 180;

const State = {
    HIDDEN:  0,
    SHOWING: 1,
    SHOWN:   2,
    HIDING:  3
};

function _cleanMarkup(text) {
    // Support &amp;, &quot;, &apos;, &lt; and &gt;, escape all other
    // occurrences of '&'.
    let _text = text.replace(/&(?!amp;|quot;|apos;|lt;|gt;)/g, '&amp;');
    // Support <b>, <i>, and <u>, escape anything else
    // so it displays as raw markup.
    return _text.replace(/<(\/?[^biu]>|[^>\/][^>])/g, '&lt;$1');
}

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
// and 'clear' parameters.
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
        this.urgent = false;
        this._customContent = false;
        this._bannerBodyText = null;
        this._spacing = 0;

        this._hasFocus = false;
        this._lockTrayOnFocusGrab = false;
        // We use this._prevFocusedWindow and this._prevKeyFocusActor to return the
        // focus where it previously belonged after a focus grab, unless the user
        // has explicitly changed that.
        this._prevFocusedWindow = null;
        this._prevKeyFocusActor = null;

        this._focusWindowChangedId = 0;
        this._focusActorChangedId = 0;
        this._stageInputModeChangedId = 0;
        this._capturedEventId = 0;
        this._keyPressId = 0;

        source.connect('clicked', Lang.bind(this,
            function() {
                this.emit('dismissed');
            }));

        source.connect('destroy', Lang.bind(this, this.destroy));

        this.actor = new St.Table({ name: 'notification',
                                    reactive: true });
        this.actor.connect('style-changed', Lang.bind(this, this._styleChanged));
        this.actor.connect('button-release-event', Lang.bind(this,
            function (actor, event) {
                if (!this._actionArea ||
                    !this._actionArea.contains(event.get_source()))
                    this.source.clicked();
            }));

        // The first line should have the title, followed by the
        // banner text, but ellipsized if they won't both fit. We can't
        // make St.Table or St.BoxLayout do this the way we want (don't
        // show banner at all if title needs to be ellipsized), so we
        // use Shell.GenericContainer.
        this._bannerBox = new Shell.GenericContainer();
        this._bannerBox.connect('get-preferred-width', Lang.bind(this, this._bannerBoxGetPreferredWidth));
        this._bannerBox.connect('get-preferred-height', Lang.bind(this, this._bannerBoxGetPreferredHeight));
        this._bannerBox.connect('allocate', Lang.bind(this, this._bannerBoxAllocate));
        this.actor.add(this._bannerBox, { row: 0,
                                          col: 1,
                                          y_expand: false,
                                          y_fill: false });

        this._titleLabel = new St.Label();
        this._bannerBox.add_actor(this._titleLabel);
        this._bannerLabel = new St.Label();
        this._bannerBox.add_actor(this._bannerLabel);

        this.update(title, banner, params);

        Main.overview.connect('showing', Lang.bind(this,
            function() {
                this._toggleFocusGrabMode();
            }));
        Main.overview.connect('hidden', Lang.bind(this,
            function() {
                this._toggleFocusGrabMode();
            }));
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
                                        clear: false });

        this._customContent = params.customContent;

        if (this._icon)
            this._icon.destroy();
        // We always clear the content area if we don't have custom
        // content because it might contain the @banner that didn't
        // fit in the banner mode.
        if (this._scrollArea && (!this._customContent || params.clear)) {
            this._scrollArea.destroy();
            this._scrollArea = null;
            this._contentArea = null;
        }
        if (this._actionArea && params.clear) {
            this._actionArea.destroy();
            this._actionArea = null;
            this._buttonBox = null;
        }

        this._icon = params.icon || this.source.createNotificationIcon();
        this.actor.add(this._icon, { row: 0,
                                     col: 0,
                                     x_expand: false,
                                     y_expand: false,
                                     y_fill: false });

        title = title ? _cleanMarkup(title.replace(/\n/g, ' ')) : '';
        this._titleLabel.clutter_text.set_markup('<b>' + title + '</b>');

        // Unless the notification has custom content, we save this._bannerBodyText
        // to add it to the content of the notification if the notification is
        // expandable due to other elements in its content area or due to the banner
        // not fitting fully in the single-line mode.
        this._bannerBodyText = this._customContent ? null : banner;

        banner = banner ? _cleanMarkup(banner.replace(/\n/g, '  ')) : '';
        this._bannerLabel.clutter_text.set_markup(banner);
        this._bannerLabel.queue_relayout();

        // Add the bannerBody now if we know for sure we'll need it
        if (this._bannerBodyText && this._bannerBodyText.indexOf('\n') > -1)
            this._addBannerBody();

        if (params.body)
            this.addBody(params.body);
    },

    // addActor:
    // @actor: actor to add to the body of the notification
    //
    // Appends @actor to the notification's body
    addActor: function(actor) {
        if (!this._scrollArea) {
            this._scrollArea = new St.ScrollView({ name: 'notification-scrollview',
                                                   vscrollbar_policy: Gtk.PolicyType.AUTOMATIC,
                                                   hscrollbar_policy: Gtk.PolicyType.NEVER,
                                                   vshadows: true });
            this.actor.add(this._scrollArea, { row: 1,
                                               col: 1 });
            this._contentArea = new St.BoxLayout({ name: 'notification-body',
                                                   vertical: true });
            this._scrollArea.add_actor(this._contentArea);
            // If we know the notification will be expandable, we need to add
            // the banner text to the body as the first element.
            this._addBannerBody();
        }

        this._contentArea.add(actor);
    },

    // addBody:
    // @text: the text
    //
    // Adds a multi-line label containing @text to the notification.
    //
    // Return value: the newly-added label
    addBody: function(text) {
        let body = new St.Label();
        body.clutter_text.line_wrap = true;
        body.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;
        body.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        text = text ? _cleanMarkup(text) : '';
        body.clutter_text.set_markup(text);

        this.addActor(body);
        return body;
    },

    _addBannerBody: function() {
        if (this._bannerBodyText) {
            let text = this._bannerBodyText;
            this._bannerBodyText = null;
            this.addBody(text);
        }
    },

    // scrollTo:
    // @side: St.Side.TOP or St.Side.BOTTOM
    //
    // Scrolls the content area (if scrollable) to the indicated edge
    scrollTo: function(side) {
        // Hack to force a relayout, since the caller probably
        // just added or removed something to scrollArea, and
        // the adjustment needs to reflect that.
        global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, 0, 0);

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
        }
        this._actionArea = actor;

        if (!props)
            props = {};
        props.row = 2;
        props.col = 1;

        this.actor.add(this._actionArea, props);
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

        let button = new St.Button();

        if (Gtk.IconTheme.get_default().has_icon(id)) {
            button.add_style_class_name('notification-icon-button');
            button.child = St.TextureCache.get_default().load_icon_name(id, BUTTON_ICON_SIZE);
        } else {
            button.add_style_class_name('notification-button');
            button.label = label;
        }

        this._buttonBox.add(button);
        button.connect('clicked', Lang.bind(this, function() { this.emit('action-invoked', id); }));
    },

    setUrgent: function(urgent) {
        this.urgent = urgent;
    },

    _styleChanged: function() {
        let [hasSpacing, spacing] = this.actor.get_theme_node().get_length('spacing-columns', false);
        this._spacing = hasSpacing ? spacing : 0;
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
        titleBox.x1 = titleBox.y1 = 0;
        titleBox.x2 = Math.min(titleNatW, availWidth);
        titleBox.y2 = titleNatH;
        this._titleLabel.allocate(titleBox, flags);

        let bannerFits = true;
        if (titleBox.x2 + this._spacing > availWidth) {
            this._bannerLabel.hide();
            bannerFits = false;
        } else {
            let bannerBox = new Clutter.ActorBox();
            bannerBox.x1 = titleBox.x2 + this._spacing;
            bannerBox.y1 = 0;
            bannerBox.x2 = Math.min(bannerBox.x1 + bannerNatW, availWidth);
            bannerBox.y2 = titleNatH;
            bannerFits = (bannerBox.x1 + bannerNatW <= availWidth);
            this._bannerLabel.show();
            this._bannerLabel.allocate(bannerBox, flags);
        }

        // If the banner doesn't fully fit in the banner box, we possibly need to add the
        // banner to the body. We can't do that from here though since that will force a
        // relayout, so we add it to the main loop.
        if (!bannerFits)
            Mainloop.idle_add(Lang.bind(this,
                                        function() {
                                            this._addBannerBody();
                                            return false;
                                        }));
    },

    popOut: function(animate) {
        if (this.actor.row_count <= 1)
            return false;

        if (animate)
            Tweener.addTween(this._bannerLabel,
                             { opacity: 0,
                               time: ANIMATION_TIME,
                               transition: 'easeOutQuad' });
        else
            this._bannerLabel.opacity = 0;

        return true;
    },

    popInCompleted: function() {
        this._bannerLabel.opacity = 255;
    },

    grabFocus: function(lockTray) {
        if (this._hasFocus)
            return;

        this._lockTrayOnFocusGrab = lockTray;

        let metaDisplay = global.screen.get_display();

        this._prevFocusedWindow = metaDisplay.focus_window;
        this._prevKeyFocus = global.stage.get_key_focus();

        // We need to use the captured event in the overview, because we don't want to change the stage input mode to
        // FOCUSED there. On the other hand, using the captured event doesn't work correctly in the main view because
        // it doesn't allow focusing the windows again correctly. So we are using the FOCUSED stage input mode in the
        // main view.
        if (Main.overview.visible) {
            if (!Main.pushModal(this.actor))
                return;
            this._capturedEventId = global.stage.connect('captured-event', Lang.bind(this, this._onCapturedEvent));
        } else {
            global.set_stage_input_mode(Shell.StageInputMode.FOCUSED);

            this._focusWindowChangedId = metaDisplay.connect('notify::focus-window', Lang.bind(this, this._focusWindowChanged));
            this._stageInputModeChangedId = global.connect('notify::stage-input-mode', Lang.bind(this, this._stageInputModeChanged));

            this._keyPressId = global.stage.connect('key-press-event', Lang.bind(this, this._onKeyPress));
        }

        // We need to listen to this signal in the overview, as well as in the main view, to make the key bindings such as
        // Alt+F2 work. When a notification has key focus, which is the case with chat notifications, all captured KEY_PRESS
        // events have the actor with the key focus as their source. This makes it impossible to distinguish between the chat
        // window input and the key bindings based solely on the KEY_PRESS event.
        this._focusActorChangedId = global.stage.connect('notify::key-focus', Lang.bind(this, this._focusActorChanged));

        this._hasFocus = true;
        if (lockTray)
            Main.messageTray.lock();
    },

    _focusWindowChanged: function() {
        let metaDisplay = global.screen.get_display();
        // this._focusWindowChanged() will be called when we call
        // global.set_stage_input_mode(Shell.StageInputMode.FOCUSED) ,
        // however metaDisplay.focus_window will be null in that case. We only
        // want to ungrab focus if the focus has been moved to an application
        // window.
        if (metaDisplay.focus_window) {
            this._prevFocusedWindow = null;
            this.ungrabFocus();
        }
    },

    _focusActorChanged: function() {
        let focusedActor = global.stage.get_key_focus();
        if (!focusedActor || !this.actor.contains(focusedActor)) {
            this._prevKeyFocusActor = null;
            this.ungrabFocus();
        }
    },

    _stageInputModeChanged: function() {
        let focusedActor = global.stage.get_key_focus();
        // TODO: We need to set this._prevFocusedWindow to null in order to
        // get the cursor in the run dialog. However, that also means it's
        // set to null when the application menu is activated, which defeats
        // the point of keeping the name of the previously focused application
        // in the panel. It'd be good to be able to distinguish between these
        // two cases.
        this._prevFocusedWindow = null;
        this._prevKeyFocusActor = null;
        this.ungrabFocus();
    },

    _onCapturedEvent: function(actor, event) {
        let source = event.get_source();
        switch (event.type()) {
            case Clutter.EventType.BUTTON_PRESS:
                if (!this.actor.contains(source))
                    this.ungrabFocus();
                break;
            case Clutter.EventType.KEY_PRESS:
                let symbol = event.get_key_symbol();
                if (symbol == Clutter.Escape) {
                    Main.messageTray.escapeTray();
                    return true;
                }
                break;
        }

        return false;
    },

    _onKeyPress: function(actor, event) {
        let symbol = event.get_key_symbol();
        if (symbol == Clutter.Escape) {
            Main.messageTray.escapeTray();
            return true;
        }
        return false;
    },

    ungrabFocus: function() {
        if (!this._hasFocus)
            return;

        let metaDisplay = global.screen.get_display();
        if (this._focusWindowChangedId > 0) {
            metaDisplay.disconnect(this._focusWindowChangedId);
            this._focusWindowChangedId = 0;
        }

        if (this._focusActorChangedId > 0) {
            global.stage.disconnect(this._focusActorChangedId);
            this._focusActorChangedId = 0;
        }

        if (this._stageInputModeChangedId) {
            global.disconnect(this._stageInputModeChangedId);
            this._stageInputModeChangedId = 0;
        }

        if (this._capturedEventId > 0) {
            Main.popModal(this.actor);
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }

        if (this._keyPressId > 0) {
            global.stage.disconnect(this._keyPressId);
            this._keyPressId = 0;
        }

        this._hasFocus = false;
        Main.messageTray.unlock();

        if (this._prevFocusedWindow) {
            metaDisplay.set_input_focus_window(this._prevFocusedWindow, false, global.get_current_time());
            this._prevFocusedWindow = null;
        }
        if (this._prevKeyFocusActor) {
            global.stage.set_key_focus(this._prevKeyFocusActor);
            this._prevKeyFocusActor = null;
        } else {
            // We don't want to keep the actor inside the notification focused.
            let focusedActor = global.stage.get_key_focus();
            if (focusedActor && this.actor.contains(focusedActor))
                global.stage.set_key_focus(null);
        }
    },

    // Because we grab focus differently in the overview
    // and in the main view, we need to change how it is
    // done when we move between the two.
    _toggleFocusGrabMode: function() {
        if (this._hasFocus) {
            this.ungrabFocus();
            this.grabFocus(this._lockTrayOnFocusGrab);
        }
    },

    destroy: function() {
        this.emit('destroy');
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
                                     height: this.ICON_SIZE });
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

    notify: function(notification) {
        if (this.notification)
            this.notification.disconnect(this._notificationDestroyedId);

        this.notification = notification;

        this._notificationDestroyedId = notification.connect('destroy', Lang.bind(this,
            function () {
                if (this.notification == notification) {
                    this.notification = null;
                    this._notificationDestroyedId = 0;
                }
            }));

        this.emit('notify', notification);
    },

    clicked: function() {
        this.emit('clicked');
    },

    destroy: function() {
        this.emit('destroy');
    },

    //// Protected methods ////

    // The subclass must call this at least once to set the summary icon.
    _setSummaryIcon: function(icon) {
        if (this._iconBin.child)
            this._iconBin.child.destroy();
        this._iconBin.child = icon;
    }
};
Signals.addSignalMethods(Source.prototype);

function SummaryItem(source, minTitleWidth) {
    this._init(source, minTitleWidth);
}

SummaryItem.prototype = {
    _init: function(source, minTitleWidth) {
        this.source = source;
        // The message tray items should all be the same width when expanded. Because the only variation is introduced by the width of the title,
        // we pass in the desired minimum title width, which is the maximum title width of the items which are currently in the tray. If the width
        // of the title of this item is greater (up to MAX_SOURCE_TITLE_WIDTH), then that width will be used, and the width of all the other items
        // in the message tray will be readjusted.
        this._minTitleWidth = minTitleWidth;
        this.actor = new St.Button({ style_class: 'summary-source-button',
                                     reactive: true,
                                     track_hover: true });

        this._sourceBox = new  Shell.GenericContainer({ style_class: 'summary-source',
                                                        reactive: true });
        this._sourceBox.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this._sourceBox.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this._sourceBox.connect('allocate', Lang.bind(this, this._allocate));

        this._sourceIcon = source.getSummaryIcon();
        this._sourceTitleBin = new St.Bin({ y_align: St.Align.MIDDLE, x_fill: true });
        this._sourceTitle = new St.Label({ style_class: 'source-title',
                                           text: source.title });
        this._sourceTitle.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._sourceTitleBin.child = this._sourceTitle;

        this._sourceBox.add_actor(this._sourceIcon);
        this._sourceBox.add_actor(this._sourceTitleBin);
        this._widthFraction = 0;
        this.actor.child = this._sourceBox;
    },

    getTitleNaturalWidth: function() {
        let [sourceTitleBinMinWidth, sourceTitleBinNaturalWidth] =
            this._sourceTitleBin.get_preferred_width(-1);
        return Math.min(sourceTitleBinNaturalWidth, MAX_SOURCE_TITLE_WIDTH);
    },

    setMinTitleWidth: function(minTitleWidth) {
        this._minTitleWidth = minTitleWidth;
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let [found, spacing] = this._sourceBox.get_theme_node().get_length('spacing', false);
        if (!found)
            spacing = 0;
        let [sourceIconMinWidth, sourceIconNaturalWidth] = this._sourceIcon.get_preferred_width(forHeight);
        let [sourceTitleBinMinWidth, sourceTitleBinNaturalWidth] =
            this._sourceTitleBin.get_preferred_width(forHeight);
        let minWidth = sourceIconNaturalWidth +
                       (this._widthFraction > 0 ? spacing : 0) +
                       this._widthFraction * Math.min(Math.max(sourceTitleBinNaturalWidth, this._minTitleWidth),
                                                      MAX_SOURCE_TITLE_WIDTH);
        alloc.min_size = minWidth;
        alloc.natural_size = minWidth;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [sourceIconMinHeight, sourceIconNaturalHeight] = this._sourceIcon.get_preferred_height(forWidth);
        alloc.min_size = sourceIconNaturalHeight;
        alloc.natural_size = sourceIconNaturalHeight;
    },

    _allocate: function (actor, box, flags) {
        let width = box.x2 - box.x1;
        let height = box.y2 - box.y1;

        let [sourceIconMinWidth, sourceIconNaturalWidth] = this._sourceIcon.get_preferred_width(-1);
        let [sourceIconMinHeight, sourceIconNaturalHeight] = this._sourceIcon.get_preferred_height(-1);

        let iconBox = new Clutter.ActorBox();
        iconBox.x1 = 0;
        iconBox.y1 = 0;
        iconBox.x2 = sourceIconNaturalWidth;
        iconBox.y2 = sourceIconNaturalHeight;

        this._sourceIcon.allocate(iconBox, flags);

        let [found, spacing] = this._sourceBox.get_theme_node().get_length('spacing', false);
        if (!found)
            spacing = 0;

        let titleBox = new Clutter.ActorBox();
        if (width > sourceIconNaturalWidth + spacing) {
            titleBox.x1 = iconBox.x2 + spacing;
            titleBox.x2 = width;
        } else {
            titleBox.x1 = iconBox.x2;
            titleBox.x2 = iconBox.x2;
        }
        titleBox.y1 = 0;
        titleBox.y2 = height;

        this._sourceTitleBin.allocate(titleBox, flags);

        this._sourceTitleBin.set_clip(0, 0, titleBox.x2 - titleBox.x1, height);
    },

    expand: function() {
        // this._adjustEllipsization replaces some text with the dots at the end of the animation,
        // and then we replace the dots with the text before we begin the animation to collapse
        // the title. These changes are not noticeable at the speed with which we do the animation,
        // while animating in the ellipsized mode does not look good.
        Tweener.addTween(this,
                         { widthFraction: 1,
                           time: ANIMATION_TIME,
                           transition: 'linear',
                           onComplete: this._adjustEllipsization,
                           onCompleteScope: this });
    },

    collapse: function() {
        this._sourceTitle.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        Tweener.addTween(this,
                         { widthFraction: 0,
                           time: ANIMATION_TIME,
                           transition: 'linear' });
    },

    _adjustEllipsization: function() {
        let [sourceTitleBinMinWidth, sourceTitleBinNaturalWidth] = this._sourceTitleBin.get_preferred_width(-1);
        if (sourceTitleBinNaturalWidth > MAX_SOURCE_TITLE_WIDTH)
            this._sourceTitle.clutter_text.ellipsize = Pango.EllipsizeMode.END;
    },

    set widthFraction(widthFraction) {
        this._widthFraction = widthFraction;
        this._sourceBox.queue_relayout();
    },

    get widthFraction() {
        return this._widthFraction;
    }
};

function MessageTray() {
    this._init();
}

MessageTray.prototype = {
    _init: function() {
        this.actor = new St.Group({ name: 'message-tray',
                                    reactive: true,
                                    track_hover: true });
        this.actor.connect('notify::hover', Lang.bind(this, this._onTrayHoverChanged));

        this._notificationBin = new St.Bin();
        this.actor.add_actor(this._notificationBin);
        this._notificationBin.hide();
        this._notificationQueue = [];
        this._notification = null;

        this._summaryBin = new St.Bin({ anchor_gravity: Clutter.Gravity.NORTH_EAST });
        this.actor.add_actor(this._summaryBin);
        this._summary = new St.BoxLayout({ name: 'summary-mode',
                                           reactive: true,
                                           track_hover: true });
        this._summary.connect('notify::hover', Lang.bind(this, this._onSummaryHoverChanged));
        this._summaryBin.child = this._summary;
        this._summaryBin.opacity = 0;

        this._summaryNotificationBin = new St.Bin({ name: 'summary-notification-bin',
                                                    anchor_gravity: Clutter.Gravity.NORTH_EAST,
                                                    reactive: true,
                                                    track_hover: true });
        this.actor.add_actor(this._summaryNotificationBin);
        this._summaryNotificationBin.lower_bottom();
        this._summaryNotificationBin.hide();
        this._summaryNotification = null;
        this._clickedSummaryItem = null;

        this._trayState = State.HIDDEN;
        this._locked = false;
        this._trayLeftTimeoutId = 0;
        this._pointerInTray = false;
        this._summaryState = State.HIDDEN;
        this._summaryTimeoutId = 0;
        this._pointerInSummary = false;
        this._notificationState = State.HIDDEN;
        this._notificationTimeoutId = 0;
        this._summaryNotificationState = State.HIDDEN;
        this._summaryNotificationTimeoutId = 0;
        this._overviewVisible = Main.overview.visible;
        this._notificationRemoved = false;

        Main.chrome.addActor(this.actor, { affectsStruts: false,
                                           visibleInOverview: true });
        Main.chrome.trackActor(this._notificationBin);
        Main.chrome.trackActor(this._summaryNotificationBin);

        global.gdk_screen.connect('monitors-changed', Lang.bind(this, this._setSizePosition));

        this._setSizePosition();

        Main.overview.connect('showing', Lang.bind(this,
            function() {
                this._overviewVisible = true;
                if (this._locked)
                    this.unlock();
                else
                    this._updateState();
            }));
        Main.overview.connect('hiding', Lang.bind(this,
            function() {
                this._overviewVisible = false;
                if (this._locked)
                    this.unlock();
                else
                    this._updateState();
            }));

        this._summaryItems = [];
        this._longestSummaryItem = null;
    },

    _setSizePosition: function() {
        let primary = global.get_primary_monitor();
        this.actor.x = primary.x;
        this.actor.y = primary.y + primary.height - 1;
        this.actor.width = primary.width;
        this._notificationBin.x = 0;
        this._notificationBin.width = primary.width;

        // These work because of their anchor_gravity
        this._summaryBin.x = primary.width;
        this._summaryNotificationBin.x = primary.width;
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

        let minTitleWidth = (this._longestSummaryItem ? this._longestSummaryItem.getTitleNaturalWidth() : 0);
        let summaryItem = new SummaryItem(source, minTitleWidth);

        this._summary.insert_actor(summaryItem.actor, 0);
        this._summaryNeedsToBeShown = true;

        let newItemTitleWidth = summaryItem.getTitleNaturalWidth();
        if (newItemTitleWidth > minTitleWidth) {
            for (let i = 0; i < this._summaryItems.length; i++) {
                this._summaryItems[i].setMinTitleWidth(newItemTitleWidth);
            }
            summaryItem.setMinTitleWidth(newItemTitleWidth);
            this._longestSummaryItem = summaryItem;
        }

        this._summaryItems.push(summaryItem);

        source.connect('notify', Lang.bind(this, this._onNotify));

        summaryItem.actor.connect('notify::hover', Lang.bind(this,
            function () {
                this._onSummaryItemHoverChanged(summaryItem);
            }));

        summaryItem.actor.connect('clicked', Lang.bind(this,
            function () {
                this._onSummaryItemClicked(summaryItem);
            }));

        source.connect('destroy', Lang.bind(this,
            function () {
                this.removeSource(source);
            }));
    },

    removeSource: function(source) {
        let index = this._getIndexOfSummaryItemForSource(source);
        if (index == -1)
            return;

        // remove all notifications with this source from the queue
        let newNotificationQueue = [];
        for (let i = 0; i < this._notificationQueue.length; i++) {
            if (this._notificationQueue[i].source != source)
                newNotificationQueue.push(this._notificationQueue[i]);
        }
        this._notificationQueue = newNotificationQueue;

        this._summary.remove_actor(this._summaryItems[index].actor);
        if (this._summary.get_children().length > 0)
            this._summaryNeedsToBeShown = true;
        else
            this._summaryNeedsToBeShown = false;

        this._summaryItems.splice(index, 1);
        if (this._longestSummaryItem.source == source) {

            let maxTitleWidth = 0;
            this._longestSummaryItem = null;
            for (let i = 0; i < this._summaryItems.length; i++) {
                let summaryItem = this._summaryItems[i];
                if (summaryItem.getTitleNaturalWidth() > maxTitleWidth) {
                    maxTitleWidth = summaryItem.getTitleNaturalWidth();
                    this._longestSummaryItem = summaryItem;
                }
            }
            for (let i = 0; i < this._summaryItems.length; i++) {
                this._summaryItems[i].setMinTitleWidth(maxTitleWidth);
            }
        }

        let needUpdate = false;

        if (this._notification && this._notification.source == source) {
            if (this._notificationTimeoutId) {
                Mainloop.source_remove(this._notificationTimeoutId);
                this._notificationTimeoutId = 0;
            }
            this._notificationRemoved = true;
            needUpdate = true;
        }
        if (this._clickedSummaryItem && this._clickedSummaryItem.source == source) {
            this._clickedSummaryItem = null;
            needUpdate = true;
        }

        if (needUpdate);
            this._updateState();
    },

    removeNotification: function(notification) {
        if (this._notification == notification && (this._notificationState == State.SHOWN || this._notificationState == State.SHOWING)) {
            if (this._notificationTimeoutId) {
                Mainloop.source_remove(this._notificationTimeoutId);
                this._notificationTimeoutId = 0;
            }
            this._notificationRemoved = true;
            this._updateState();
            return;
        }

        let index = this._notificationQueue.indexOf(notification);
        if (index != -1)
            this._notificationQueue.splice(index, 1);
    },

    _hasNotification: function(notification) {
        if (this._notification == notification)
            return true;
        return this._notificationQueue.indexOf(notification) != -1;
    },

    lock: function() {
        this._locked = true;
    },

    unlock: function() {
        if (!this._locked)
            return;
        this._locked = false;
        this._clickedSummaryItem = null;
        this._updateState();
    },

    _onNotify: function(source, notification) {
        if (notification == this._summaryNotification)
            return;

        if (!this._hasNotification(notification)) {
            notification.connect('destroy',
                                 Lang.bind(this, this.removeNotification));

            if (notification.urgent)
                this._notificationQueue.unshift(notification);
            else
                this._notificationQueue.push(notification);
        }

        this._updateState();
    },

    _onSummaryItemHoverChanged: function(summaryItem) {
        if (summaryItem.actor.hover)
            summaryItem.expand();
        else
            summaryItem.collapse();
    },

    _onSummaryItemClicked: function(summaryItem) {
        if (!this._clickedSummaryItem || this._clickedSummaryItem != summaryItem)
            this._clickedSummaryItem = summaryItem;
        else
            this._clickedSummaryItem = null;

        this._updateState();
    },

    _onSummaryHoverChanged: function() {
        this._pointerInSummary = this._summary.hover;
        this._updateState();
    },

    _onTrayHoverChanged: function() {
        if (this.actor.hover) {
            if (this._trayLeftTimeoutId) {
                Mainloop.source_remove(this._trayLeftTimeoutId);
                this._trayLeftTimeoutId = 0;
                return;
            }

            this._pointerInTray = true;
            this._updateState();
        } else {
            // We wait just a little before hiding the message tray in case the
            // user quickly moves the mouse back into it.
            let timeout = HIDE_TIMEOUT * 1000;
            this._trayLeftTimeoutId = Mainloop.timeout_add(timeout, Lang.bind(this, this._onTrayLeftTimeout));
        }
    },

    _onTrayLeftTimeout: function() {
        this._trayLeftTimeoutId = 0;
        this._pointerInTray = false;
        this._pointerInSummary = false;
        this._updateState();
        return false;
    },

    escapeTray: function() {
        this.unlock();
        this._pointerInTray = false;
        this._pointerInSummary = false;
        if (this._notificationTimeoutId) {
            Mainloop.source_remove(this._notificationTimeoutId);
            this._notificationTimeoutId = 0;
        }
        this._updateState();
    },

    // All of the logic for what happens when occurs here; the various
    // event handlers merely update variables such as
    // 'this._pointerInTray', 'this._summaryState', etc, and
    // _updateState() figures out what (if anything) needs to be done
    // at the present time.
    _updateState: function() {
        // Notifications
        let notificationsPending = this._notificationQueue.length > 0;
        let notificationPinned = this._pointerInTray && !this._pointerInSummary && !this._notificationRemoved;
        let notificationExpanded = this._notificationBin.y < 0;
        let notificationExpired = (this._notificationTimeoutId == 0 && !this._pointerInTray && !this._locked) || this._notificationRemoved;

        if (this._notificationState == State.HIDDEN) {
            if (notificationsPending)
                this._showNotification();
        } else if (this._notificationState == State.SHOWN) {
            if (notificationExpired)
                this._hideNotification();
            else if (notificationPinned && !notificationExpanded)
                this._expandNotification();
            else if (notificationPinned)
                this._ensureNotificationFocused();
        }

        // Summary
        let summarySummoned = this._pointerInSummary || this._overviewVisible;
        let summaryPinned = this._summaryTimeoutId != 0 || this._pointerInTray || summarySummoned || this._locked;

        let notificationsVisible = (this._notificationState == State.SHOWING ||
                                    this._notificationState == State.SHOWN);
        let notificationsDone = !notificationsVisible && !notificationsPending;

        if (this._summaryState == State.HIDDEN) {
            if (notificationsDone && this._summaryNeedsToBeShown)
                this._showSummary(true);
            else if (summarySummoned)
                this._showSummary(false);
        } else if (this._summaryState == State.SHOWN) {
            if (!summaryPinned)
                this._hideSummary();
        }

        // Summary notification
        let haveSummaryNotification = this._clickedSummaryItem != null;
        let summaryNotificationIsMainNotification = (haveSummaryNotification &&
                                                     this._clickedSummaryItem.source.notification == this._notification);
        let canShowSummaryNotification = this._summaryState == State.SHOWN;
        let wrongSummaryNotification = (haveSummaryNotification &&
                                        this._summaryNotification != this._clickedSummaryItem.source.notification);

        if (this._summaryNotificationState == State.HIDDEN) {
            if (haveSummaryNotification && !summaryNotificationIsMainNotification && canShowSummaryNotification)
                this._showSummaryNotification();
        } else if (this._summaryNotificationState == State.SHOWN) {
            if (!haveSummaryNotification || !canShowSummaryNotification || wrongSummaryNotification)
                this._hideSummaryNotification();
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
        this._notificationBin.child = this._notification.actor;

        this._notificationBin.opacity = 0;
        this._notificationBin.y = this.actor.height;
        this._notificationBin.show();

        this._tween(this._notificationBin, '_notificationState', State.SHOWN,
                    { y: 0,
                      opacity: 255,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad',
                      onComplete: this._showNotificationCompleted,
                      onCompleteScope: this
                    });

        if (this._notification.urgent) {
            // This will overwrite the y tween, but leave the opacity
            // tween, and so the onComplete will remain as well.
            this._expandNotification();
        }

        let [x, y, mods] = global.get_pointer();
        this._lastSeenMouseY = y;
    },

    _showNotificationCompleted: function() {
        this._notificationTimeoutId =
            Mainloop.timeout_add(NOTIFICATION_TIMEOUT * 1000,
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
            this._notificationTimeoutId =
                Mainloop.timeout_add(1000,
                                     Lang.bind(this, this._notificationTimeout));
        } else {
            this._notificationTimeoutId = 0;
            this._updateState();
        }

        return false;
    },

    _hideNotification: function() {
        this._notification.ungrabFocus();

        if (this._reExpandNotificationId) {
            this._notificationBin.disconnect(this._reExpandNotificationId);
            this._reExpandNotificationId = 0;
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
        this._notification.popInCompleted();
        this._notification = null;
    },

    _expandNotification: function() {
        if (this._notification && this._notification.popOut(true)) {
            // Don't grab focus in urgent notifications that are auto-expanded.
            if (!this._notification.urgent)
                this._notification.grabFocus(false);
            this._tween(this._notificationBin, '_notificationState', State.SHOWN,
                        { y: this.actor.height - this._notificationBin.height,
                          time: ANIMATION_TIME,
                          transition: 'easeOutQuad'
                        });

            if (!this._reExpandNotificationId)
                this._reExpandNotificationId = this._notificationBin.connect('notify::height', Lang.bind(this, this._expandNotification));
        }
    },

    // We use this function to grab focus when the user moves the pointer
    // to an urgent notification that was already auto-expanded.
    _ensureNotificationFocused: function() {
        this._notification.grabFocus(false);
    },

    _showSummary: function(withTimeout) {
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
                      onCompleteParams: [withTimeout]
                    });
    },

    _showSummaryCompleted: function(withTimeout) {
        this._summaryNeedsToBeShown = false;

        if (withTimeout) {
            this._summaryTimeoutId =
                Mainloop.timeout_add(SUMMARY_TIMEOUT * 1000,
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
                      transition: 'easeOutQuad'
                    });
        this._summaryNeedsToBeShown = false;
    },

    _showSummaryNotification: function() {
        this._summaryNotification = this._clickedSummaryItem.source.notification;

        let index = this._notificationQueue.indexOf(this._summaryNotification);
        if (index != -1)
            this._notificationQueue.splice(index, 1);

        this._summaryNotificationBin.child = this._summaryNotification.actor;
        this._summaryNotification.popOut(false);
        this._summaryNotification.grabFocus(true);

        this._summaryNotificationBin.opacity = 0;
        this._summaryNotificationBin.y = this.actor.height;
        this._summaryNotificationBin.show();

        this._tween(this._summaryNotificationBin, '_summaryNotificationState', State.SHOWN,
                    { y: this.actor.height - this._summaryNotificationBin.height,
                      opacity: 255,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad'
                    });

        if (!this._reExpandSummaryNotificationId)
            this._reExpandSummaryNotificationId = this._summaryNotificationBin.connect('notify::height', Lang.bind(this, this._reExpandSummaryNotification));
    },

    _reExpandSummaryNotification: function() {
        this._tween(this._summaryNotificationBin, '_summaryNotificationState', State.SHOWN,
                    { y: this.actor.height - this._summaryNotificationBin.height,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad'
                    });
    },

    _hideSummaryNotification: function() {
        // Unset this._clickedSummaryItem if we are no longer showing the summary
        if (this._summaryState != State.SHOWN)
            this._clickedSummaryItem = null;
        this._summaryNotification.ungrabFocus();

        this._tween(this._summaryNotificationBin, '_summaryNotificationState', State.HIDDEN,
                    { y: this.actor.height,
                      opacity: 0,
                      time: ANIMATION_TIME,
                      transition: 'easeOutQuad',
                      onComplete: this._hideSummaryNotificationCompleted,
                      onCompleteScope: this
                    });

        if (this._reExpandSummaryNotificationId) {
            this._summaryNotificationBin.disconnect(this._reExpandSummaryNotificationId);
            this._reExpandSummaryNotificationId = 0;
        }
    },

    _hideSummaryNotificationCompleted: function() {
        this._summaryNotificationBin.hide();
        this._summaryNotificationBin.child = null;
        this._summaryNotification.popInCompleted();
        this._summaryNotification = null;
    }
};
