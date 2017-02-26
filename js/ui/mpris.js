const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Calendar = imports.ui.calendar;
const Main = imports.ui.main;
const MessageList = imports.ui.messageList;

const DBusIface = '<node> \
<interface name="org.freedesktop.DBus"> \
  <method name="ListNames"> \
    <arg type="as" direction="out" name="names" /> \
  </method> \
  <signal name="NameOwnerChanged"> \
    <arg type="s" direction="out" name="name" /> \
    <arg type="s" direction="out" name="oldOwner" /> \
    <arg type="s" direction="out" name="newOwner" /> \
  </signal> \
</interface> \
</node>';
const DBusProxy = Gio.DBusProxy.makeProxyWrapper(DBusIface);

const MprisIface = '<node> \
<interface name="org.mpris.MediaPlayer2"> \
  <method name="Raise" /> \
  <property name="CanRaise" type="b" access="read" /> \
  <property name="DesktopEntry" type="s" access="read" /> \
</interface> \
</node>';
const MprisProxy = Gio.DBusProxy.makeProxyWrapper(MprisIface);

const MprisPlayerIface = '<node> \
<interface name="org.mpris.MediaPlayer2.Player"> \
  <method name="PlayPause" /> \
  <method name="Next" /> \
  <method name="Previous" /> \
  <property name="CanPlay" type="b" access="read" /> \
  <property name="Metadata" type="a{sv}" access="read" /> \
  <property name="PlaybackStatus" type="s" access="read" /> \
</interface> \
</node>';
const MprisPlayerProxy = Gio.DBusProxy.makeProxyWrapper(MprisPlayerIface);

const MPRIS_PLAYER_PREFIX = 'org.mpris.MediaPlayer2.';

const MediaMessage = new Lang.Class({
    Name: 'MediaMessage',
    Extends: MessageList.Message,

    _init: function(player) {
        this._player = player;

        this.parent('', '');

        this._icon = new St.Icon({ style_class: 'media-message-cover-icon' });
        this.setIcon(this._icon);

        this.addMediaControl('media-skip-backward-symbolic',
            Lang.bind(this, function() {
                this._player.previous();
            }));

        this._playPauseButton = this.addMediaControl(null,
            Lang.bind(this, function() {
                this._player.playPause();
            }));

        this.addMediaControl('media-skip-forward-symbolic',
            Lang.bind(this, function() {
                this._player.next();
            }));

        this._player.connect('changed', Lang.bind(this, this._update));
        this._player.connect('closed', Lang.bind(this, this.close));
        this._update();
    },

    _onClicked: function() {
        this._player.raise();
        Main.panel.closeCalendar();
    },

    _update: function() {
        this.setTitle(this._player.trackArtists.join(', '));
        this.setBody(this._player.trackTitle);

        if (this._player.trackCoverUrl) {
            let file = Gio.File.new_for_uri(this._player.trackCoverUrl);
            this._icon.gicon = new Gio.FileIcon({ file: file });
            this._icon.remove_style_class_name('fallback');
        } else {
            this._icon.icon_name = 'audio-x-generic-symbolic';
            this._icon.add_style_class_name('fallback');
        }

        let isPlaying = this._player.status == 'Playing';
        let iconName = isPlaying ? 'media-playback-pause-symbolic'
                                 : 'media-playback-start-symbolic';
        this._playPauseButton.child.icon_name = iconName;
    }
});

const MprisPlayer = new Lang.Class({
    Name: 'MprisPlayer',

    _init: function(busName) {
        this._mprisProxy = new MprisProxy(Gio.DBus.session, busName,
                                          '/org/mpris/MediaPlayer2',
                                          Lang.bind(this, this._onMprisProxyReady));
        this._playerProxy = new MprisPlayerProxy(Gio.DBus.session, busName,
                                                 '/org/mpris/MediaPlayer2',
                                                 Lang.bind(this, this._onPlayerProxyReady));

        this._visible = false;
        this._trackArtists = [];
        this._trackTitle = '';
        this._trackCoverUrl = '';
    },

    get status() {
        return this._playerProxy.PlaybackStatus;
    },

    get trackArtists() {
        return this._trackArtists;
    },

    get trackTitle() {
        return this._trackTitle;
    },

    get trackCoverUrl() {
        return this._trackCoverUrl;
    },

    playPause: function() {
        this._playerProxy.PlayPauseRemote();
    },

    next: function() {
        this._playerProxy.NextRemote();
    },

    previous: function() {
        this._playerProxy.PreviousRemote();
    },

    raise: function() {
        // The remote Raise() method may run into focus stealing prevention,
        // so prefer activating the app via .desktop file if possible
        let app = null;
        if (this._mprisProxy.DesktopEntry) {
            let desktopId = this._mprisProxy.DesktopEntry + '.desktop';
            app = Shell.AppSystem.get_default().lookup_app(desktopId);
        }

        if (app)
            app.activate();
        else if (this._mprisProxy.CanRaise)
            this._mprisProxy.RaiseRemote();
    },

    _close: function() {
        this._mprisProxy.disconnect(this._ownerNotifyId);
        this._mprisProxy = null;

        this._playerProxy.disconnect(this._propsChangedId);
        this._playerProxy = null;

        this.emit('closed');
    },

    _onMprisProxyReady: function() {
        this._ownerNotifyId = this._mprisProxy.connect('notify::g-name-owner',
            Lang.bind(this, function() {
                if (!this._mprisProxy.g_name_owner)
                    this._close();
            }));
    },

    _onPlayerProxyReady: function() {
        this._propsChangedId = this._playerProxy.connect('g-properties-changed',
                                                         Lang.bind(this, this._updateState));
        this._updateState();
    },

    _updateState: function() {
        let metadata = {};
        for (let prop in this._playerProxy.Metadata)
            metadata[prop] = this._playerProxy.Metadata[prop].deep_unpack();

        this._trackArtists = metadata['xesam:artist'] || [_("Unknown artist")];
        this._trackTitle = metadata['xesam:title'] || _("Unknown title");
        this._trackCoverUrl = metadata['mpris:artUrl'] || '';
        this.emit('changed');

        let visible = this._playerProxy.CanPlay;

        if (this._visible != visible) {
            this._visible = visible;
            if (visible)
                this.emit('show');
            else
                this._close();
        }
    }
});
Signals.addSignalMethods(MprisPlayer.prototype);

const MediaSection = new Lang.Class({
    Name: 'MediaSection',
    Extends: MessageList.MessageListSection,

    _init: function() {
        this.parent();

        this._players = new Map();

        this._proxy = new DBusProxy(Gio.DBus.session,
                                    'org.freedesktop.DBus',
                                    '/org/freedesktop/DBus',
                                    Lang.bind(this, this._onProxyReady));
    },

    _shouldShow: function() {
        return !this.empty && Calendar.isToday(this._date);
    },

    _addPlayer: function(busName) {
        if (this._players.get(busName))
            return;

        let player = new MprisPlayer(busName);
        player.connect('closed', Lang.bind(this,
            function() {
                this._players.delete(busName);
            }));
        player.connect('show', Lang.bind(this,
            function() {
                let message = new MediaMessage(player);
                this.addMessage(message, true);
            }));
        this._players.set(busName, player);
    },

    _onProxyReady: function() {
        this._proxy.ListNamesRemote(Lang.bind(this,
            function([names]) {
                names.forEach(Lang.bind(this,
                    function(name) {
                        if (!name.startsWith(MPRIS_PLAYER_PREFIX))
                            return;

                        this._addPlayer(name);
                    }));
            }));
        this._proxy.connectSignal('NameOwnerChanged',
                                  Lang.bind(this, this._onNameOwnerChanged));
    },

    _onNameOwnerChanged: function(proxy, sender, [name, oldOwner, newOwner]) {
        if (!name.startsWith(MPRIS_PLAYER_PREFIX))
            return;

        if (newOwner && !oldOwner)
            this._addPlayer(name);
    }
});
