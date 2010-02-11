/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Tweener = imports.ui.tweener;

const Main = imports.ui.main;

const ANIMATION_TIME = 0.2;
const NOTIFICATION_TIMEOUT = 4;
const SUMMARY_TIMEOUT = 1;

const MESSAGE_TRAY_TIMEOUT = 0.2;

const ICON_SIZE = 24;

const MessageTrayState = {
    HIDDEN: 0, // entire message tray is hidden
    NOTIFICATION: 1, // notifications are visible
    SUMMARY: 2, // summary is visible
    TRAY_ONLY: 3 // neither notifiations nor summary are visible, only tray
};

function _cleanMarkup(text) {
    // Support <b>, <i>, and <u>, escape anything else
    // so it displays as raw markup.
    return text.replace(/<(\/?[^biu]>|[^>\/][^>])/g, "&lt;$1");
}

// Notification:
// @source: the notification's Source
// @title: the title
// @banner: the banner text
// @bannerBody: whether or not to promote the banner to the body on overflow
//
// Creates a notification. In banner mode, it will show
// @source's icon, @title (in bold) and @banner, all on a single line
// (with @banner ellipsized if necessary).
//
// Additional notification details can be added via addBody(),
// addAction(), and addActor(). If any of these are called, then the
// notification will expand to show the additional actors (while
// hiding the @banner) if the pointer is moved into it while it is
// visible.
//
// If @bannerBody is %true, then @banner will also be used as the body
// of the notification (as with addBody()) when the banner is expanded.
// In this case, if @banner is too long to fit in the single-line mode,
// the notification will be made expandable automatically.
function Notification(source, title, banner, bannerBody) {
    this._init(source, title, banner, bannerBody);
}

Notification.prototype = {
    _init: function(source, title, banner, bannerBody) {
        this.source = source;
        this._bannerBody = bannerBody;

        this.actor = new St.Table({ name: 'notification' });
        this.update(title, banner, true);
    },

    // update:
    // @title: the new title
    // @banner: the new banner
    // @clear: whether or not to clear out extra actors
    //
    // Updates the notification by regenerating its icon and updating
    // the title/banner. If @clear is %true, it will also remove any
    // additional actors/action buttons previously added.
    update: function(title, banner, clear) {
        let children = this.actor.get_children();
        for (let i = 0; i < children.length; i++) {
            let meta = this.actor.get_child_meta(children[i]);
            if (clear || meta.row == 0)
                children[i].destroy();
        }
        if (clear) {
            this.actions = {};
            this._actionBox = null;
        }

        let icon = this.source.createIcon(ICON_SIZE);
        icon.reactive = true;
        this.actor.add(icon, { row: 0,
                               col: 0,
                               x_expand: false,
                               y_expand: false,
                               y_fill: false });

        icon.connect('button-release-event', Lang.bind(this,
            function () {
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
        title = title ? _cleanMarkup(title.replace('\n', ' ')) : '';
        this._titleLabel.clutter_text.set_markup('<b>' + title + '</b>');
        this._bannerBox.add_actor(this._titleLabel);

        if (this._bannerBody)
            this._bannerBodyText = banner;
        else
            this._bannerBodyText = null;

        this._bannerLabel = new St.Label();
        banner = banner ? _cleanMarkup(banner.replace('\n', '  ')) : '';
        this._bannerLabel.clutter_text.set_markup(banner);
        this._bannerBox.add_actor(this._bannerLabel);
    },

    // addActor:
    // @actor: actor to add to the notification
    // @props: (optional) child properties
    //
    // Adds @actor to the notification's St.Table, using @props.
    //
    // If @props does not specify a %row, then @actor will be added
    // to the bottom of the notification (unless there are action
    // buttons present, in which case it will be added above them).
    //
    // If @props does not specify a %col, it will default to column 1.
    // (Normally only the icon is in column 0.)
    //
    // If @props specifies an already-occupied cell, then the existing
    // contents of the table will be shifted down to make room for it.
    addActor: function(actor, props) {
        if (!props)
            props = {};

        if (!('col' in props))
            props.col = 1;

        if ('row' in props) {
            let children = this.actor.get_children();
            let i, meta, collision = false;

            for (i = 0; i < children.length; i++) {
                meta = this.actor.get_child_meta(children[i]);
                if (meta.row == props.row && meta.col == props.col) {
                    collision = true;
                    break;
                }
            }

            if (collision) {
                for (i = 0; i < children.length; i++) {
                    meta = this.actor.get_child_meta(children[i]);
                    if (meta.row >= props.row)
                        meta.row++;
                }
            }
        } else {
            if (this._actionBox) {
                props.row = this.actor.row_count - 1;
                this.actor.get_child_meta(this._actionBox).row++;
            } else {
                props.row = this.actor.row_count;
            }
        }

        this.actor.add(actor, props);
    },

    // addBody:
    // @text: the text
    // @props: (optional) properties for addActor()
    //
    // Adds a multi-line label containing @text to the notification.
    addBody: function(text, props) {
        let body = new St.Label();
        body.clutter_text.line_wrap = true;
        body.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;
        body.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        text = text ? _cleanMarkup(text) : '';
        body.clutter_text.set_markup(text);

        this.addActor(body, props);
    },

    _addBannerBody: function() {
        this.addBody(this._bannerBodyText, { row: 1 });
        this._bannerBodyText = null;
    },

    // addAction:
    // @id: the action ID
    // @label: the label for the action's button
    //
    // Adds a button with the given @label to the notification. All
    // action buttons will appear in a single row at the bottom of
    // the notification.
    //
    // If the button is clicked, the notification will emit the
    // %action-invoked signal with @id as a parameter
    addAction: function(id, label) {
        if (!this._actionBox) {
            if (this._bannerBodyText)
                this._addBannerBody();

            let box = new St.BoxLayout({ name: 'notification-actions' });
            this.addActor(box, { x_expand: false,
                                 x_fill: false,
                                 x_align: St.Align.END });
            this._actionBox = box;
        }

        let button = new St.Button({ style_class: 'notification-button',
                                     label: label });
        this._actionBox.add(button);
        button.connect('clicked', Lang.bind(this, function() { this.emit('action-invoked', id); }));
    },

    _bannerBoxGetPreferredWidth: function(actor, forHeight, alloc) {
        let [titleMin, titleNat] = this._titleLabel.get_preferred_width(forHeight);
        let [bannerMin, bannerNat] = this._bannerLabel.get_preferred_width(forHeight);
        let [has_spacing, spacing] = this.actor.get_theme_node().get_length('spacing-columns', false);

        alloc.min_size = titleMin;
        alloc.natural_size = titleNat + (has_spacing ? spacing : 0) + bannerNat;
    },

    _bannerBoxGetPreferredHeight: function(actor, forWidth, alloc) {
        [alloc.min_size, alloc.natural_size] =
            this._titleLabel.get_preferred_height(forWidth);
    },

    _bannerBoxAllocate: function(actor, box, flags) {
        let [titleMinW, titleNatW] = this._titleLabel.get_preferred_width(-1);
        let [titleMinH, titleNatH] = this._titleLabel.get_preferred_height(-1);
        let [bannerMinW, bannerNatW] = this._bannerLabel.get_preferred_width(-1);
        let [has_spacing, spacing] = this.actor.get_theme_node().get_length('spacing-columns', false);
        if (!has_spacing)
            spacing = 0;
        let availWidth = box.x2 - box.x1;

        let titleBox = new Clutter.ActorBox();
        titleBox.x1 = titleBox.y1 = 0;
        titleBox.x2 = Math.min(titleNatW, availWidth);
        titleBox.y2 = titleNatH;
        this._titleLabel.allocate(titleBox, flags);

        let overflow = false;
        if (titleBox.x2 + spacing > availWidth) {
            this._bannerLabel.hide();
            overflow = true;
        } else {
            let bannerBox = new Clutter.ActorBox();
            bannerBox.x1 = titleBox.x2 + spacing;
            bannerBox.y1 = 0;
            bannerBox.x2 = Math.min(bannerBox.x1 + bannerNatW, availWidth);
            bannerBox.y2 = titleNatH;
            this._bannerLabel.show();
            this._bannerLabel.allocate(bannerBox, flags);

            if (bannerBox.x2 < bannerBox.x1 + bannerNatW)
                overflow = true;
        }

        if (overflow && this._bannerBodyText)
            this._addBannerBody();
    },

    popOut: function() {
        if (this.actor.row_count <= 1)
            return false;

        Tweener.addTween(this._bannerLabel,
                         { opacity: 0,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad" });
        return true;
    },

    popIn: function() {
        if (this.actor.row_count <= 1)
            return false;

        Tweener.addTween(this._bannerLabel,
                         { opacity: 255,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad" });
        return true;
    }
};
Signals.addSignalMethods(Notification.prototype);

function Source(id, createIcon) {
    this._init(id, createIcon);
}

Source.prototype = {
    _init: function(id, createIcon) {
        this.id = id;
        this.text = null;
        if (createIcon)
            this.createIcon = createIcon;
    },

    // This can be overridden by a subclass, or by the createIcon
    // parameter to _init()
    createIcon: function(size) {
        throw new Error('no implementation of createIcon in ' + this);
    },

    notify: function(notification) {
        this.emit('notify', notification);
    },

    clicked: function() {
        this.emit('clicked');
    },

    destroy: function() {
        this.emit('destroy');
    }
};
Signals.addSignalMethods(Source.prototype);

function MessageTray() {
    this._init();
}

MessageTray.prototype = {
    _init: function() {
        this.actor = new St.BoxLayout({ name: 'message-tray',
                                        reactive: true });

        this._notificationBin = new St.Bin({ reactive: true,
                                             x_align: St.Align.MIDDLE });
        this.actor.add(this._notificationBin);
        this._notificationBin.hide();
        this._notificationQueue = [];
        this._notification = null;

        this._summaryBin = new St.BoxLayout();
        this.actor.add(this._summaryBin);
        this._summary = new St.BoxLayout({ name: 'summary-mode',
                                           reactive: true });
        this._summaryBin.add(this._summary, { x_align: St.Align.END,
                                              x_fill: false,
                                              expand: true });

        this._summary.connect('enter-event',
                              Lang.bind(this, this._showMessageTray));
        this._summaryBin.opacity = 0;

        this.actor.connect('enter-event',
                           Lang.bind(this, function() {
                                                if (this._state == MessageTrayState.NOTIFICATION || this._state == MessageTrayState.SUMMARY)
                                                    this._showMessageTray();
                                            }));

        this.actor.connect('leave-event',
                           Lang.bind(this, this._hideMessageTray));
        this._state = MessageTrayState.HIDDEN;
        this.actor.show();
        Main.chrome.addActor(this.actor, { affectsStruts: false });
        Main.chrome.trackActor(this._notificationBin, { affectsStruts: false });

        global.connect('screen-size-changed',
                       Lang.bind(this, this._setSizePosition));
        this._setSizePosition();

        this._sources = {};
        this._icons = {};
    },

    _setSizePosition: function() {
        let primary = global.get_primary_monitor();
        this.actor.x = primary.x;
        this.actor.y = primary.y + primary.height - 1;
        this.actor.width = primary.width;

        this._notificationBin.x = this._summaryBin.x = 0;
        this._notificationBin.width = this._summaryBin.width = primary.width;
    },

    contains: function(source) {
        return this._sources.hasOwnProperty(source.id);
    },

    add: function(source) {
        if (this.contains(source)) {
            log('Trying to re-add source ' + source.id);
            return;
        }

        let iconBox = new St.Bin({ reactive: true });
        iconBox.child = source.createIcon(ICON_SIZE);
        this._summary.insert_actor(iconBox, 0);
        this._icons[source.id] = iconBox;
        this._sources[source.id] = source;

        source.connect('notify', Lang.bind(this, this._onNotify));

        iconBox.connect('button-release-event', Lang.bind(this,
            function () {
                source.clicked();
            }));

        source.connect('destroy', Lang.bind(this,
            function () {
                this.remove(source);
            }));
    },

    remove: function(source) {
        if (!this.contains(source))
            return;

        // remove all notifications with this source from the queue
        let newNotificationQueue = [];
        for (let i = 0; i < this._notificationQueue.length; i++) {
            if (this._notificationQueue[i].source != source)
                newNotificationQueue.push(this._notificationQueue[i]);
        }
        this._notificationQueue = newNotificationQueue;

        // Update state if we are showing a notification from the removed source
        if (this._state == MessageTrayState.NOTIFICATION &&
            this._notification.source == source)
            this._updateState();

        this._summary.remove_actor(this._icons[source.id]);
        delete this._icons[source.id];
        delete this._sources[source.id];
    },

    getSource: function(id) {
        return this._sources[id];
    },

    _onNotify: function(source, notification) {
        this._notificationQueue.push(notification);

        if (this._state == MessageTrayState.HIDDEN)
            this._updateState();
    },

    _showMessageTray: function() {
        // Don't hide the message tray after a timeout if the user has moved
        // the mouse over it.
        // We might have a timeout in place if the user moved the mouse away
        // from the message tray for a very short period of time or if we are
        // showing a notification.
        if (this._updateTimeoutId > 0)
            Mainloop.source_remove(this._updateTimeoutId);

        if (this._state == MessageTrayState.HIDDEN)
            this._updateState();
        else if (this._state == MessageTrayState.NOTIFICATION) {
            if (this._notification.popOut()) {
                Tweener.addTween(this._notificationBin,
                                 { y: this.actor.height - this._notificationBin.height,
                                   time: ANIMATION_TIME,
                                   transition: "easeOutQuad"
                                 });
            }
        }
    },

    _hideMessageTray: function() {
        if (this._state == MessageTrayState.HIDDEN)
            return;

        // We wait just a little before hiding the message tray in case the
        // user will quickly move the mouse back over it.
        let timeout = MESSAGE_TRAY_TIMEOUT * 1000;
        this._updateTimeoutId = Mainloop.timeout_add(timeout, Lang.bind(this, this._updateState));
    },

    // As tray, notification box and summary view are all animated separately,
    // but dependant on each other's states, it appears less confusing
    // handling all transitions in a state machine rather than spread out
    // over different event handlers.
    //
    // State changes are triggered when
    // - a notification arrives (see _onNotify())
    // - the mouse enters the tray (see _showMessageTray())
    // - the mouse leaves the tray (see _hideMessageTray())
    // - a timeout expires (usually set up in a previous invocation of this function)
    _updateState: function() {
        if (this._updateTimeoutId > 0)
            Mainloop.source_remove(this._updateTimeoutId);

        this._updateTimeoutId = 0;
        let timeout = -1;

        switch (this._state) {
        case MessageTrayState.HIDDEN:
            if (this._notificationQueue.length > 0) {
                this._showNotification();
                this._showTray();
                this._state = MessageTrayState.NOTIFICATION;
                // Because we set up the timeout before we do the animation,
                // we add ANIMATION_TIME to NOTIFICATION_TIMEOUT, so that
                // NOTIFICATION_TIMEOUT represents the time the notifiation
                // is fully shown.
                timeout = (ANIMATION_TIME + NOTIFICATION_TIMEOUT) * 1000;
            } else {
                this._showSummary();
                this._showTray();
                this._state = MessageTrayState.SUMMARY;
            }
            break;
        case MessageTrayState.NOTIFICATION:
            if (this._notificationQueue.length > 0) {
                this._hideNotification();
                this._state = MessageTrayState.TRAY_ONLY;
                timeout = ANIMATION_TIME * 1000;
            } else {
                this._hideNotification();
                this._showSummary();
                this._state = MessageTrayState.SUMMARY;
                timeout = (ANIMATION_TIME + SUMMARY_TIMEOUT) * 1000;
            }
            break;
        case MessageTrayState.SUMMARY:
            if (this._notificationQueue.length > 0) {
                this._hideSummary();
                this._showNotification();
                this._state = MessageTrayState.NOTIFICATION;
                timeout = (ANIMATION_TIME + NOTIFICATION_TIMEOUT) * 1000;
            } else {
                this._hideSummary();
                this._hideTray();
                this._state = MessageTrayState.HIDDEN;
            }
            break;
        case MessageTrayState.TRAY_ONLY:
            this._showNotification();
            this._state = MessageTrayState.NOTIFICATION;
            timeout = (ANIMATION_TIME + NOTIFICATION_TIMEOUT) * 1000;
            break;
        }

        if (timeout > -1)
            this._updateTimeoutId = Mainloop.timeout_add(timeout, Lang.bind(this, this._updateState));
    },

    _showTray: function() {
        let primary = global.get_primary_monitor();
        Tweener.addTween(this.actor,
                         { y: primary.y + primary.height - this.actor.height,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
    },

    _hideTray: function() {
        let primary = global.get_primary_monitor();

        Tweener.addTween(this.actor,
                         { y: primary.y + primary.height - 1,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
        return false;
    },

    _showNotification: function() {
        this._notification = this._notificationQueue.shift();
        this._notificationBin.child = this._notification.actor;

        this._notificationBin.opacity = 0;
        this._notificationBin.y = this.actor.height;
        this._notificationBin.show();

        Tweener.addTween(this._notificationBin,
                         { y: 0,
                           opacity: 255,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad" });
    },

    _hideNotification: function() {
        this._notification.popIn();

        Tweener.addTween(this._notificationBin,
                         { y: this.actor.height,
                           opacity: 0,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: Lang.bind(this, function() {
                               this._notificationBin.hide();
                               this._notificationBin.child = null;
                               this._notification = null;
                           })});
    },

    _showSummary: function() {
        let primary = global.get_primary_monitor();
        this._summaryBin.opacity = 0;
        this._summaryBin.y = this.actor.height;
        Tweener.addTween(this._summaryBin,
                         { y: 0,
                           opacity: 255,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad" });
    },

    _hideSummary: function() {
        Tweener.addTween(this._summaryBin,
                         { opacity: 0,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                          });
    }
};
