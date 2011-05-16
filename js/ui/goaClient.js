/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Goa = imports.gi.Goa;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;
const C_ = Gettext.pgettext;
const Gtk = imports.gi.Gtk;
const Pango = imports.gi.Pango;

const History = imports.misc.history;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

// ----------------------------------------------------------------------------------------------------

function Client() {
    this._init();
}

Client.prototype = {
    _init : function() {
        this._client = null;

        this._accountIdToMailMonitor = {}
        this._mailSource = null;

        // TODO: need to call refreshAllMonitors() when network-connectivity changes

        Goa.Client.new(null, /* cancellable */
                       Lang.bind(this, this._onClientConstructed));
    },

    _onClientConstructed : function(object, asyncRes) {
        this._client = object.new_finish(asyncRes);
        this._updateAccounts();
        this._client.connect('account-added', Lang.bind(this, this._updateAccounts));
        this._client.connect('account-removed', Lang.bind(this, this._updateAccounts));
        this._client.connect('account-changed', Lang.bind(this, this._updateAccounts));
    },

    _updateAccounts : function () {

        let objects = this._client.get_accounts();
        let mailIds = {};

        // Add monitors for accounts that now exist
        for (let n = 0; n < objects.length; n++) {
            let object = objects[n];
            let id = object.account.id;

            if (object.mail) {
                mailIds[id] = true;
                if (!(id in this._accountIdToMailMonitor)) {
                    let monitor = new MailMonitor(this, object);
                    this._accountIdToMailMonitor[id] = monitor;
                }
            }
        }

        // Nuke monitors for accounts that are now non-existant
        let monitorsToRemove = []
        for (let existingMonitorId in this._accountIdToMailMonitor) {
            if (!(existingMonitorId in mailIds)) {
                monitorsToRemove.push(existingMonitorId);
            }
        }
        for (let n = 0; n < monitorsToRemove.length; n++) {
            let id = monitorsToRemove[n];
            let monitor = this._accountIdToMailMonitor[id];
            delete this._accountIdToMailMonitor[id]
            monitor.destroy();
        }
    },

    _ensureMailSource: function() {
        if (!this._mailSource) {
            this._mailSource = new MailSource(this);
            this._mailSource.connect('destroy', Lang.bind(this,
                                                          function () {
                                                              this._mailSource = null;
                                                          }));
            Main.messageTray.add(this._mailSource);
        }
    },

    addPendingMessage: function(message) {
        this._ensureMailSource();
        this._mailSource.addMessage(message);
    },

    refreshAllMonitors: function() {
        log('Refreshing all mail monitors');
        for (let id in this._accountIdToMailMonitor) {
            let monitor = this._accountIdToMailMonitor[id];
            monitor.refresh();
        }
    }
}

// ----------------------------------------------------------------------------------------------------

function Message(uid, from, subject, excerpt, uri) {
    this._init(uid, from, subject, excerpt, uri);
}

Message.prototype = {
    _init: function(uid, from, subject, excerpt, uri) {
        this.uid = uid;
        this.from = from;
        this.subject = subject;
        this.excerpt = excerpt;
        this.uri = uri;
        this.receivedAt = new Date();
    }
}

// ----------------------------------------------------------------------------------------------------

function MailMonitor(client, accountObject) {
    this._init(client, accountObject);
}

MailMonitor.prototype = {
    _init : function(client, accountObject) {
        this._client = client;
        this._accountObject = accountObject;
        this._account = this._accountObject.get_account();
        this._mail = this._accountObject.get_mail();

        // Create the remote monitor object
        this._proxy = null;
        this._cancellable = new Gio.Cancellable();
        this._mail.call_create_monitor(this._cancellable, Lang.bind(this, this._onMonitorCreated));
    },

    destroy : function() {
        this._cancellable.cancel();
        if (this._proxy) {
            // We don't really care if this fails or not
            this._proxy.call_close(null, Lang.bind(this, function() { }));
            this._proxy.disconnect(this._messageReceivedId);
            this._proxy = null;
        }
    },

    refresh : function() {
        if (this._proxy) {
            // We don't really care if this fails or not
            log('Refreshing mail monitor for account ' + this._account.name);
            this._proxy.call_refresh(null, Lang.bind(this, function() { }));
        }
    },

    _onMonitorCreated : function(mail, asyncRes) {
        // TODO: a (gboolean, object_path) tuple is returned here
        // See https://bugzilla.gnome.org/show_bug.cgi?id=649657
        let ret = mail.call_create_monitor_finish(asyncRes);
        let object_path = ret[1];
        Goa.MailMonitorProxy.new_for_bus(Gio.BusType.SESSION,
                                       Gio.DBusProxyFlags.NONE,
                                       'org.gnome.OnlineAccounts',
                                       object_path,
                                       null, /* cancellable */
                                       Lang.bind(this, this._onMonitorProxyConstructed));
    },

    _onMonitorProxyConstructed : function(monitor, asyncRes) {
        this._proxy = monitor.new_for_bus_finish(asyncRes);

        // Now listen for changes on the mail monitor proxy
        this._messageReceivedId = this._proxy.connect('message-received',
                                                      Lang.bind(this, this._onMessageReceived));
    },

    _onMessageReceived : function(monitor, uid, from, subject, excerpt, uri) {
        let message = new Message(uid, from, subject, excerpt, uri);
        if (!Main.messageTray.getBusy()) {
            let source = new Source(this._client, message);
            let notification = new Notification(source, this._client, message);
            // If the user is not marked as busy, present the notification to the user
            Main.messageTray.add(source);
            source.notify(notification);
        } else {
            // ... otherwise, if the user is busy, just add it to the MailSource's list
            // of pending messages
            this._client.addPendingMessage(message);
        }
    }
}

// ----------------------------------------------------------------------------------------------------

function Source(client, message) {
    this._init(client, message);
}

Source.prototype = {
    __proto__:  MessageTray.Source.prototype,

    _init : function(client, message) {
        this._client = client;
        this._message = message;

        // Init super class and add ourselves to the message tray
        MessageTray.Source.prototype._init.call(this, 'Message from ' + _stripEmailAddress(this._message.from));
        this.setTransient(true);
        this.isChat = true;
        this._setSummaryIcon(this.createNotificationIcon());
    },

    createNotificationIcon : function() {
        // TODO: use account icon
        let icon = new St.Icon({ icon_type: St.IconType.FULLCOLOR,
                                 icon_size: this.ICON_SIZE,
                                 icon_name: 'mail-send'});
        return icon;
    }
}

// ----------------------------------------------------------------------------------------------------

function _stripEmailAddress(name_and_addr) {
    let bracketStartPos = name_and_addr.indexOf(' <');
    if (bracketStartPos == -1) {
        return name_and_addr;
    } else {
        return name_and_addr.slice(0, bracketStartPos);
    }
}

function Notification(source, client, message) {
    this._init(source, client, message);
}

Notification.prototype = {
    __proto__: MessageTray.Notification.prototype,

    _init : function(source, client, message) {
        this._message = message;
        this._client = client;
        this._ignore = false;
        this._alreadyExpanded = false;

        this._strippedFrom = _stripEmailAddress(this._message.from);

        let title = this._strippedFrom;
        let banner = this._message.subject + ' \u2014 ' + this._message.excerpt; // — U+2014 EM DASH

        // Init super class
        MessageTray.Notification.prototype._init.call(this, source, title, banner);

        // Change the contents once expanded
        this.connect('expanded', Lang.bind (this, this._onExpanded));

        this.update(title, banner);
        this.setUrgency(MessageTray.Urgency.NORMAL);
        this.setTransient(true);

        this.addButton('ignore', 'Ignore');
        this.addButton('junk', 'Junk');
        if (this._message.uri.length > 0) {
            this.addButton('open', 'Open');
        }
        this.connect('action-invoked', Lang.bind(this,
                                                 function(notification, id) {
                                                     if (id == 'ignore') {
                                                         this._actionIgnore();
                                                     } else if (id == 'junk') {
                                                         this._actionJunk();
                                                     } else if (id == 'open') {
                                                         this._actionOpen();
                                                     }
                                                 }));
        this.connect('clicked', Lang.bind(this,
                                          function() {
                                              if (this._message.uri.length > 0) {
                                                  this._actionOpen();
                                              }
                                          }));
        // Hmm, should be ::done-displaying instead?
        this.connect('destroy', Lang.bind(this, this._onDestroyed));
    },

    _onExpanded : function() {
        if (this._alreadyExpanded)
            return;
        this._alreadyExpanded = true;
        let escapedExcerpt = GLib.markup_escape_text(this._message.excerpt, -1);
        let bannerMarkup = '<b>Subject:</b> ' + this._message.subject + '\n';
        // TODO: if available, insert other headers such as Cc
        bannerMarkup += '\n' + escapedExcerpt;
        this.update(this._strippedFrom, bannerMarkup, {bannerMarkup: true});
    },

    _onDestroyed : function(reason) {
        // If not ignoring the message, push it onto the Mail source
        if (!this._ignore) {
            this._client.addPendingMessage(this._message);
        }
    },

    _actionIgnore : function() {
        this._ignore = true;
    },

    _actionJunk : function() {
        this._ignore = true;
        log('TODO: actually junk the message');
    },

    _actionOpen : function() {
        this._ignore = true;
        Gio.app_info_launch_default_for_uri(this._message.uri,
                                            global.create_app_launch_context());
    }

}

// ----------------------------------------------------------------------------------------------------

function _sameDay(dateA, dateB) {
    return (dateA.getDate() == dateB.getDate() &&
            dateA.getMonth() == dateB.getMonth() &&
            dateA.getYear() == dateB.getYear());
}

function _sameYear(dateA, dateB) {
    return (dateA.getYear() == dateB.getYear());
}

function _formatRelativeDate(date) {
    let ret = ''
    let now = new Date();
    if (_sameDay(date, now)) {
        ret = date.toLocaleFormat("%l:%M %p");
    } else {
        if (_sameYear(date, now)) {
            ret = date.toLocaleFormat("%B %e");
        } else {
            ret = date.toLocaleFormat("%B %e, %Y");
        }
    }
    return ret;
}

function _addMessageToTable(table, message) {
    let formattedExcerpt = message.excerpt.replace(/\r/g, '').replace(/\n/g, ' ');
    let formattedDate = _formatRelativeDate(message.receivedAt);

    let fromLabel = new St.Label({ style_class: 'goa-message-base goa-message-from',
                                   text: _stripEmailAddress(message.from)});
    let hbox = new St.BoxLayout({ style_class: 'goa-message-hbox', vertical: false });
    let subjectLabel = new St.Label({ style_class: 'goa-message-base goa-message-subject',
                                      text: message.subject });
    let excerptLabel = new St.Label({ style_class: 'goa-message-base goa-message-excerpt',
                                      text: formattedExcerpt });
    let dateLabel = new St.Label({ style_class: 'goa-message-base goa-message-date',
                                   text: formattedDate });

    excerptLabel.clutter_text.line_wrap = false;
    excerptLabel.clutter_text.ellipsize = Pango.EllipsizeMode.END;

    hbox.add(subjectLabel, { x_fill:  true,
                             y_fill:  false,
                             x_align: St.Align.END,
                             y_align: St.Align.START });
    hbox.add(excerptLabel, { x_fill:  true,
                             y_fill:  false,
                             x_align: St.Align.END,
                             y_align: St.Align.START });

    let n = table.get_row_count();
    table.add(fromLabel, { x_fill: true, x_expand: true, row: n, col: 0 });
    table.add(hbox, { row: n, col: 1 });
    table.add(dateLabel, { row: n, col: 2 });
}

function MailSource(client) {
    this._init(client);
}

MailSource.prototype = {
    __proto__:  MessageTray.Source.prototype,

    _init : function(client) {
        this._client = client;
        this._pendingMessages = [];

        // Init super class and add ourselves to the message tray
        MessageTray.Source.prototype._init.call(this, 'Mail');

        // Create the notification
        this._notification = new MessageTray.Notification(this)
        this._notification.setUrgency(MessageTray.Urgency.NORMAL);
        this._notification.setResident(true);
        this._updateNotification();
        this.pushNotification(this._notification);
        // Refresh all monitors everytime the "Mail" notification is displayed
        this._notification.connect('expanded', Lang.bind(this,
                                                         function() {
                                                             this._client.refreshAllMonitors();
                                                         }));
    },

    createNotificationIcon : function() {
        let numPending = this._pendingMessages.length;
        let baseIcon = new Gio.ThemedIcon({ name: 'mail-mark-unread'});
        let numerableIcon = new Gtk.NumerableIcon({ gicon: baseIcon });
        numerableIcon.set_count(numPending);
        let icon = new St.Icon({ icon_type: St.IconType.FULLCOLOR,
                                 icon_size: this.ICON_SIZE });
        icon.set_gicon(numerableIcon);
        return icon;
    },

    _updateNotification: function() {
        if (!this._notification)
            return

        let title = 'Mail';
        let banner = ''
        let table = new St.Table({ homogeneous: false,
                                   style_class: 'goa-message-table',
                                   reactive: true });

        for (let n = 0; n < this._pendingMessages.length; n++)
            _addMessageToTable (table, this._pendingMessages[n]);

        this._notification.update(title, banner, { clear: true,
                                                   icon: this.createNotificationIcon() });
        this._notification.addActor(table);
        this._notification.addButton('clear', 'Clear');
        this._notification.connect('action-invoked', Lang.bind(this,
                                                               function(notification, id) {
                                                                   if (id == 'clear') {
                                                                       this.clearMessages();
                                                                   }
                                                               }));
    },

    addMessage: function(message) {
        this._pendingMessages.push(message);
        // Update notification
        this._updateNotification();
        // Update icon with latest pending count
        this._setSummaryIcon(this.createNotificationIcon());
    },

    clearMessages: function() {
        let notification = this._notification;
        this._notification = null;
        if (notification)
            notification.destroy();
        this.destroy();
    },
}

