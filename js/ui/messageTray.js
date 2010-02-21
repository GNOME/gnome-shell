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

const State = {
    HIDDEN:  0,
    SHOWING: 1,
    SHOWN:   2,
    HIDING:  3
};

function _cleanMarkup(text) {
    // Support &amp;, &quot;, &apos;, &lt; and &gt;, escape all other
    // occurrences of '&'.
    let _text = text.replace(/&(?!amp;|quot;|apos;|lt;|gt;)/g, "&amp;");
    // Support <b>, <i>, and <u>, escape anything else
    // so it displays as raw markup.
    return _text.replace(/<(\/?[^biu]>|[^>\/][^>])/g, "&lt;$1");
}

// Notification:
// @id: the notification's id
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
function Notification(id, source, title, banner, bannerBody) {
    this._init(id, source, title, banner, bannerBody);
}

Notification.prototype = {
    _init: function(id, source, title, banner, bannerBody) {
        this.id = id;
        this.source = source;
        this._bannerBody = bannerBody;

        source.connect('clicked', Lang.bind(this,
            function() {
                this.emit('dismissed');
            }));

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
            if (clear || meta.row == 0 || (this._bannerBody && meta.row == 1))
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
        title = title ? _cleanMarkup(title.replace(/\n/g, ' ')) : '';
        this._titleLabel.clutter_text.set_markup('<b>' + title + '</b>');
        this._bannerBox.add_actor(this._titleLabel);

        if (this._bannerBody)
            this._bannerBodyText = banner;
        else
            this._bannerBodyText = null;

        this._bannerLabel = new St.Label();
        banner = banner ? _cleanMarkup(banner.replace(/\n/g, '  ')) : '';
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

        if (this._bannerBodyText &&
            (overflow || this._bannerBodyText.indexOf('\n') > -1))
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
    },

    destroy: function() {
        this.emit('destroy');
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
        this.handleReplacing = true;
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
                              Lang.bind(this, this._onSummaryEntered));
        this._summary.connect('leave-event',
                              Lang.bind(this, this._onSummaryLeft));
        this._summaryBin.opacity = 0;

        this.actor.connect('enter-event', Lang.bind(this, this._onTrayEntered));
        this.actor.connect('leave-event', Lang.bind(this, this._onTrayLeft));

        this._trayState = State.HIDDEN;
        this._trayLeftTimeoutId = 0;
        this._pointerInTray = false;
        this._summaryState = State.HIDDEN;
        this._summaryTimeoutId = 0;
        this._pointerInSummary = false;
        this._notificationState = State.HIDDEN;
        this._notificationTimeoutId = 0;
        this._overviewVisible = false;
        this._notificationRemoved = false;

        this.actor.show();
        Main.chrome.addActor(this.actor, { affectsStruts: false,
                                           visibleInOverview: true });
        Main.chrome.trackActor(this._notificationBin, { affectsStruts: false });

        global.connect('screen-size-changed',
                       Lang.bind(this, this._setSizePosition));
        this._setSizePosition();

        Main.overview.connect('showing', Lang.bind(this,
            function() {
                this._overviewVisible = true;
                this._updateState();
            }));
        Main.overview.connect('hiding', Lang.bind(this,
            function() {
                this._overviewVisible = false;
                this._updateState();
            }));

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
        this._summaryNeedsToBeShown = true;
        this._icons[source.id] = iconBox;
        this._sources[source.id] = source;

        source.connect('notify', Lang.bind(this, this._onNotify));

        iconBox.connect('button-release-event', Lang.bind(this,
            function () {
                source.clicked();
            }));

        source.connect('destroy', Lang.bind(this,
            function () {
                this.removeSource(source);
            }));
    },

    removeSource: function(source) {
        if (!this.contains(source))
            return;

        // remove all notifications with this source from the queue
        let newNotificationQueue = [];
        for (let i = 0; i < this._notificationQueue.length; i++) {
            if (this._notificationQueue[i].source != source)
                newNotificationQueue.push(this._notificationQueue[i]);
        }
        this._notificationQueue = newNotificationQueue;

        this._summary.remove_actor(this._icons[source.id]);
        if (this._summary.get_children().length > 0)
            this._summaryNeedsToBeShown = true;
        else
            this._summaryNeedsToBeShown = false;
        delete this._icons[source.id];
        delete this._sources[source.id];

        if (this._notification && this._notification.source == source) {
            if (this._notificationTimeoutId) {
                Mainloop.source_remove(this._notificationTimeoutId);
                this._notificationTimeoutId = 0;
            }
            this._notificationRemoved = true;
            this._updateState();
        }
    },

    removeSourceByApp: function(app) {
        for (let source in this._sources)
            if (this._sources[source].app == app)
                this.removeSource(this._sources[source]);
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

    getSource: function(id) {
        return this._sources[id];
    },

    _getNotification: function(id, source) {
        if (this._notification && this._notification.id == id)
            return this._notification;

        for (let i = 0; i < this._notificationQueue.length; i++) {
            if (this._notificationQueue[i].id == id && this._notificationQueue[i].source == source)
                return this._notificationQueue[i];
        }

        return null;
    },

    _onNotify: function(source, notification) {
        if (!notification.source.handleReplacing || this._getNotification(notification.id, source) == null) {
            notification.connect('destroy',
                                 Lang.bind(this, this.removeNotification));
            this._notificationQueue.push(notification);
        }

        this._updateState();
    },

    _onSummaryEntered: function() {
        this._pointerInSummary = true;
        this._updateState();
    },

    _onSummaryLeft: function() {
        this._pointerInSummary = false;
        this._updateState();
    },

    _onTrayEntered: function() {
        if (this._trayLeftTimeoutId) {
            Mainloop.source_remove(this._trayLeftTimeoutId);
            this._trayLeftTimeoutId = 0;
            return;
        }

        this._pointerInTray = true;
        this._updateState();
    },

    _onTrayLeft: function() {
        // We wait just a little before hiding the message tray in case the
        // user quickly moves the mouse back into it.
        let timeout = MESSAGE_TRAY_TIMEOUT * 1000;
        this._trayLeftTimeoutId = Mainloop.timeout_add(timeout, Lang.bind(this, this._onTrayLeftTimeout));
    },

    _onTrayLeftTimeout: function() {
        this._trayLeftTimeoutId = 0;
        this._pointerInTray = false;
        this._pointerInSummary = false;
        this._updateState();
        return false;
    },

    // All of the logic for what happens when occurs here; the various
    // event handlers merely update variables such as
    // "this._pointerInTray", "this._summaryState", etc, and
    // _updateState() figures out what (if anything) needs to be done
    // at the present time.
    _updateState: function() {
        // Notifications
        let notificationsPending = this._notificationQueue.length > 0;
        let notificationPinned = this._pointerInTray && !this._pointerInSummary && !this._notificationRemoved;
        let notificationExpanded = this._notificationBin.y < 0;
        let notificationExpired = (this._notificationTimeoutId == 0 && !this._pointerInTray) || this._notificationRemoved;

        if (this._notificationState == State.HIDDEN) {
            if (notificationsPending)
                this._showNotification();
        } else if (this._notificationState == State.SHOWN) {
            if (notificationExpired)
                this._hideNotification();
            else if (notificationPinned && !notificationExpanded)
                this._expandNotification();
        }

        // Summary
        let summarySummoned = this._pointerInSummary || this._overviewVisible;
        let summaryPinned = this._summaryTimeoutId != 0 || this._pointerInTray || summarySummoned;

        let notificationsVisible = (this._notificationState == State.SHOWING ||
                                    this._notificationState == State.SHOWN);
        let notificationsDone = !notificationsVisible && !notificationsPending;

        if (this._summaryState == State.HIDDEN) {
            if (notificationsDone && this._summaryNeedsToBeShown)
                this._showSummary(true);
            else if (!notificationsVisible && summarySummoned)
                this._showSummary(false);
        } else if (this._summaryState == State.SHOWN) {
            if (!summaryPinned)
                this._hideSummary();
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
        this._tween(this.actor, "_trayState", State.SHOWN,
                    { y: primary.y + primary.height - this.actor.height,
                      time: ANIMATION_TIME,
                      transition: "easeOutQuad"
                    });
    },

    _hideTray: function() {
        let primary = global.get_primary_monitor();
        this._tween(this.actor, "_trayState", State.HIDDEN,
                    { y: primary.y + primary.height - 1,
                      time: ANIMATION_TIME,
                      transition: "easeOutQuad"
                    });
    },

    _showNotification: function() {
        this._notification = this._notificationQueue.shift();
        this._notificationBin.child = this._notification.actor;

        this._notificationBin.opacity = 0;
        this._notificationBin.y = this.actor.height;
        this._notificationBin.show();

        this._tween(this._notificationBin, "_notificationState", State.SHOWN,
                    { y: 0,
                      opacity: 255,
                      time: ANIMATION_TIME,
                      transition: "easeOutQuad",
                      onComplete: this._showNotificationCompleted,
                      onCompleteScope: this
                    });
    },

    _showNotificationCompleted: function() {
        this._notificationTimeoutId =
            Mainloop.timeout_add(NOTIFICATION_TIMEOUT * 1000,
                                 Lang.bind(this, this._notificationTimeout));
    },

    _notificationTimeout: function() {
        this._notificationTimeoutId = 0;
        this._updateState();
        return false;
    },

    _hideNotification: function() {
        this._notificationRemoved = false;
        this._notification.popIn();

        this._tween(this._notificationBin, "_notificationState", State.HIDDEN,
                    { y: this.actor.height,
                      opacity: 0,
                      time: ANIMATION_TIME,
                      transition: "easeOutQuad",
                      onComplete: this._hideNotificationCompleted,
                      onCompleteScope: this
                    });
    },

    _hideNotificationCompleted: function() {
        this._notificationBin.hide();
        this._notificationBin.child = null;
        this._notification = null;
    },

    _expandNotification: function() {
        if (this._notification && this._notification.popOut()) {
            this._tween(this._notificationBin, "_notificationState", State.SHOWN,
                        { y: this.actor.height - this._notificationBin.height,
                          time: ANIMATION_TIME,
                          transition: "easeOutQuad"
                        });
        }
    },

    _showSummary: function(withTimeout) {
        let primary = global.get_primary_monitor();
        this._summaryBin.opacity = 0;
        this._summaryBin.y = this.actor.height;
        this._tween(this._summaryBin, "_summaryState", State.SHOWN,
                    { y: 0,
                      opacity: 255,
                      time: ANIMATION_TIME,
                      transition: "easeOutQuad",
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
        this._tween(this._summaryBin, "_summaryState", State.HIDDEN,
                    { opacity: 0,
                      time: ANIMATION_TIME,
                      transition: "easeOutQuad"
                    });
        this._summaryNeedsToBeShown = false;
    }
};
