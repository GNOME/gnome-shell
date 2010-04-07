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

const ANIMATION_TIME = 0.2;
const NOTIFICATION_TIMEOUT = 4;
const SUMMARY_TIMEOUT = 1;

const HIDE_TIMEOUT = 0.2;

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
// Additional notification details can be added, in which case the
// notification can be expanded by moving the pointer into it. In
// expanded mode, the banner text disappears, and there can be one or
// more rows of additional content. This content is put inside a
// scrollview, so if it gets too tall, the notification will scroll
// rather than continuing to grow. In addition to this main content
// area, there is also a single-row "action area", which is not
// scrolled and can contain a single actor. There are also convenience
// methods for creating a button box in the action area.
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

        this.actor = new St.Table({ name: 'notification',
                                    reactive: true });
        this.update(title, banner, true);
    },

    // update:
    // @title: the new title
    // @banner: the new banner
    // @clear: whether or not to clear out body and action actors
    //
    // Updates the notification by regenerating its icon and updating
    // the title/banner. If @clear is %true, it will also remove any
    // additional actors/action buttons previously added.
    update: function(title, banner, clear) {
        if (this._icon)
            this._icon.destroy();
        if (this._bannerBox)
            this._bannerBox.destroy();
        if (this._scrollArea && (this._bannerBody || clear)) {
            this._scrollArea.destroy();
            this._scrollArea = null;
            this._contentArea = null;
        }
        if (this._actionArea && clear) {
            this._actionArea.destroy();
            this._actionArea = null;
            this._buttonBox = null;
        }

        this._icon = this.source.createIcon(ICON_SIZE);
        this._icon.reactive = true;
        this.actor.add(this._icon, { row: 0,
                                     col: 0,
                                     x_expand: false,
                                     y_expand: false,
                                     y_fill: false });

        this._icon.connect('button-release-event', Lang.bind(this,
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
        this.addBody(this._bannerBodyText);
        this._bannerBodyText = null;
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
            if (this._bannerBodyText)
                this._addBannerBody();

            let box = new St.BoxLayout({ name: 'notification-actions' });
            this.setActionArea(box, { x_expand: false,
                                      x_fill: false,
                                      x_align: St.Align.END });
            this._buttonBox = box;
        }

        let button = new St.Button({ style_class: 'notification-button',
                                     label: label });
        this._buttonBox.add(button);
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
    },

    // This can be overridden by a subclass, or by the createIcon
    // parameter to _init()
    createIcon: function(size) {
        throw new Error('no implementation of createIcon in ' + this);
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
    }
};
Signals.addSignalMethods(Source.prototype);

function MessageTray() {
    this._init();
}

MessageTray.prototype = {
    _init: function() {
        this.actor = new St.BoxLayout({ name: 'message-tray',
                                        reactive: true,
                                        track_hover: true });
        this.actor.connect('notify::hover', Lang.bind(this, this._onTrayHoverChanged));

        this._notificationBin = new St.Bin();
        this.actor.add(this._notificationBin);
        this._notificationBin.hide();
        this._notificationQueue = [];
        this._notification = null;

        this._summaryBin = new St.Bin({ anchor_gravity: Clutter.Gravity.NORTH_EAST });
        this.actor.add(this._summaryBin);
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
        this.actor.add(this._summaryNotificationBin);
        this._summaryNotificationBin.lower_bottom();
        this._summaryNotificationBin.hide();
        this._summaryNotificationBin.connect('notify::hover', Lang.bind(this, this._onSummaryNotificationHoverChanged));
        this._summaryNotification = null;
        this._hoverSource = null;

        this._trayState = State.HIDDEN;
        this._trayLeftTimeoutId = 0;
        this._pointerInTray = false;
        this._summaryState = State.HIDDEN;
        this._summaryTimeoutId = 0;
        this._pointerInSummary = false;
        this._notificationState = State.HIDDEN;
        this._notificationTimeoutId = 0;
        this._summaryNotificationState = State.HIDDEN;
        this._summaryNotificationTimeoutId = 0;
        this._overviewVisible = false;
        this._notificationRemoved = false;

        Main.chrome.addActor(this.actor, { affectsStruts: false,
                                           visibleInOverview: true });
        Main.chrome.trackActor(this._notificationBin, { affectsStruts: false });
        Main.chrome.trackActor(this._summaryNotificationBin, { affectsStruts: false });

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
        this._notificationBin.x = 0;
        this._notificationBin.width = primary.width;

        // These work because of their anchor_gravity
        this._summaryBin.x = primary.width;
        this._summaryNotificationBin.x = primary.width;
    },

    contains: function(source) {
        return this._sources.hasOwnProperty(source.id);
    },

    add: function(source) {
        if (this.contains(source)) {
            log('Trying to re-add source ' + source.id);
            return;
        }

        let iconBox = new St.Clickable({ style_class: 'summary-icon',
                                         reactive: true });
        iconBox.child = source.createIcon(ICON_SIZE);
        this._summary.insert_actor(iconBox, 0);
        this._summaryNeedsToBeShown = true;
        this._icons[source.id] = iconBox;
        this._sources[source.id] = source;

        source.connect('notify', Lang.bind(this, this._onNotify));

        iconBox.connect('notify::hover', Lang.bind(this,
            function () {
                this._onSourceHoverChanged(source, iconBox.hover);
            }));
        iconBox.connect('clicked', Lang.bind(this,
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

        let needUpdate = false;

        if (this._notification && this._notification.source == source) {
            if (this._notificationTimeoutId) {
                Mainloop.source_remove(this._notificationTimeoutId);
                this._notificationTimeoutId = 0;
            }
            this._notificationRemoved = true;
            needUpdate = true;
        }
        if (this._hoverSource == source) {
            this._hoverSource = null;
            needUpdate = true;
        }

        if (needUpdate);
            this._updateState();
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

    lock: function() {
        this._locked = true;
    },

    unlock: function() {
        this._locked = false;

        this.actor.sync_hover();
        this._summary.sync_hover();

        this._updateState();
    },

    _onNotify: function(source, notification) {
        if (notification == this._summaryNotification)
            return;

        if (this._getNotification(notification.id, source) == null) {
            notification.connect('destroy',
                                 Lang.bind(this, this.removeNotification));
            this._notificationQueue.push(notification);
        }

        this._updateState();
    },

    _onSourceHoverChanged: function(source, hover) {
        if (!source.notification)
            return;

        if (this._summaryNotificationTimeoutId != 0) {
            Mainloop.source_remove(this._summaryNotificationTimeoutId);
            this._summaryNotificationTimeoutId = 0;
        }

        if (hover) {
            this._hoverSource = source;
            this._updateState();
        } else if (this._hoverSource == source) {
            let timeout = HIDE_TIMEOUT * 1000;
            this._summaryNotificationTimeoutId = Mainloop.timeout_add(timeout, Lang.bind(this, this._onSourceHoverChangedTimeout, source));
        }
    },

    _onSourceHoverChangedTimeout: function(source) {
        this._summaryNotificationTimeoutId = 0;
        if (this._hoverSource == source) {
            this._hoverSource = null;
            this._updateState();
        }
    },

    _onSummaryNotificationHoverChanged: function() {
        if (!this._summaryNotification)
            return;
        this._onSourceHoverChanged(this._summaryNotification.source,
                                   this._summaryNotificationBin.hover);
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
        let notificationExpired = (this._notificationTimeoutId == 0 && !this._pointerInTray && !this._locked) || this._notificationRemoved;

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

        // Summary notification
        let haveSummaryNotification = this._hoverSource != null;
        let summaryNotificationIsMainNotification = (haveSummaryNotification &&
                                                     this._hoverSource.notification == this._notification);
        let canShowSummaryNotification = this._summaryState == State.SHOWN;
        let wrongSummaryNotification = (haveSummaryNotification &&
                                        this._summaryNotification != this._hoverSource.notification);

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
        this._notification.popIn();

        if (this._reExpandNotificationId) {
            this._notificationBin.disconnect(this._reExpandNotificationId);
            this._reExpandNotificationId = 0;
        }

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
        this._notificationRemoved = false;
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

            if (!this._reExpandNotificationId)
                this._reExpandNotificationId = this._notificationBin.connect('notify::height', Lang.bind(this, this._expandNotification));
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
    },

    _showSummaryNotification: function() {
        this._summaryNotification = this._hoverSource.notification;

        let index = this._notificationQueue.indexOf(this._summaryNotification);
        if (index != -1)
            this._notificationQueue.splice(index, 1);

        this._summaryNotificationBin.child = this._summaryNotification.actor;
        this._summaryNotification.popOut();

        this._summaryNotificationBin.opacity = 0;
        this._summaryNotificationBin.y = this.actor.height;
        this._summaryNotificationBin.show();

        this._tween(this._summaryNotificationBin, "_summaryNotificationState", State.SHOWN,
                    { y: this.actor.height - this._summaryNotificationBin.height,
                      opacity: 255,
                      time: ANIMATION_TIME,
                      transition: "easeOutQuad"
                    });

        if (!this._reExpandSummaryNotificationId)
            this._reExpandSummaryNotificationId = this._summaryNotificationBin.connect('notify::height', Lang.bind(this, this._reExpandSummaryNotification));
    },

    _reExpandSummaryNotification: function() {
        this._tween(this._summaryNotificationBin, "_summaryNotificationState", State.SHOWN,
                    { y: this.actor.height - this._summaryNotificationBin.height,
                      time: ANIMATION_TIME,
                      transition: "easeOutQuad"
                    });
    },

    _hideSummaryNotification: function() {
        this._summaryNotification.popIn();

        this._tween(this._summaryNotificationBin, "_summaryNotificationState", State.HIDDEN,
                    { y: this.actor.height,
                      opacity: 0,
                      time: ANIMATION_TIME,
                      transition: "easeOutQuad",
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
        this._summaryNotification = null;
    }
};
