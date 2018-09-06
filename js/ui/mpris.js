const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Calendar = imports.ui.calendar;
const Main = imports.ui.main;
const MessageList = imports.ui.messageList;

const { loadInterfaceXML } = imports.misc.fileUtils;

const DBusIface = loadInterfaceXML('org.freedesktop.DBus');
const DBusProxy = Gio.DBusProxy.makeProxyWrapper(DBusIface);

const MprisIface = loadInterfaceXML('org.mpris.MediaPlayer2');
const MprisProxy = Gio.DBusProxy.makeProxyWrapper(MprisIface);

const MprisPlayerIface = loadInterfaceXML('org.mpris.MediaPlayer2.Player');
const MprisPlayerProxy = Gio.DBusProxy.makeProxyWrapper(MprisPlayerIface);

const MPRIS_PLAYER_PREFIX = 'org.mpris.MediaPlayer2.';

var MediaMessage = new Lang.Class({
    Name: 'MediaMessage',
    Extends: MessageList.Message,

    _init(player) {
        this._player = player;

        this.parent('', '');

        this._icon = new St.Icon({ style_class: 'media-message-cover-icon' });
        this.setIcon(this._icon);

        this._prevButton = this.addMediaControl('media-skip-backward-symbolic',
            () => {
                this._player.previous();
            });

        this._playPauseButton = this.addMediaControl(null,
            () => {
                this._player.playPause();
            });

        this._nextButton = this.addMediaControl('media-skip-forward-symbolic',
            () => {
                this._player.next();
            });

        this._player.connect('changed', this._update.bind(this));
        this._player.connect('closed', this.close.bind(this));
        this._update();
    },

    _onClicked() {
        this._player.raise();
        Main.panel.closeCalendar();
    },

    _updateNavButton(button, sensitive) {
        button.reactive = sensitive;
    },

    _update() {
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

        this._updateNavButton(this._prevButton, this._player.canGoPrevious);
        this._updateNavButton(this._nextButton, this._player.canGoNext);
    }
});

var MprisPlayer = new Lang.Class({
    Name: 'MprisPlayer',

    _init(busName) {
        this._mprisProxy = new MprisProxy(Gio.DBus.session, busName,
                                          '/org/mpris/MediaPlayer2',
                                          this._onMprisProxyReady.bind(this));
        this._playerProxy = new MprisPlayerProxy(Gio.DBus.session, busName,
                                                 '/org/mpris/MediaPlayer2',
                                                 this._onPlayerProxyReady.bind(this));

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

    playPause() {
        this._playerProxy.PlayPauseRemote();
    },

    get canGoNext() {
        return this._playerProxy.CanGoNext;
    },

    next() {
        this._playerProxy.NextRemote();
    },

    get canGoPrevious() {
        return this._playerProxy.CanGoPrevious;
    },

    previous() {
        this._playerProxy.PreviousRemote();
    },

    raise() {
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

    _close() {
        this._mprisProxy.disconnect(this._ownerNotifyId);
        this._mprisProxy = null;

        this._playerProxy.disconnect(this._propsChangedId);
        this._playerProxy = null;

        this.emit('closed');
    },

    _onMprisProxyReady() {
        this._ownerNotifyId = this._mprisProxy.connect('notify::g-name-owner',
            () => {
                if (!this._mprisProxy.g_name_owner)
                    this._close();
            });
    },

    _onPlayerProxyReady() {
        this._propsChangedId = this._playerProxy.connect('g-properties-changed',
                                                         this._updateState.bind(this));
        this._updateState();
    },

    _updateState() {
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

var MediaSection = new Lang.Class({
    Name: 'MediaSection',
    Extends: MessageList.MessageListSection,

    _init() {
        this.parent();

        this._players = new Map();

        this._proxy = new DBusProxy(Gio.DBus.session,
                                    'org.freedesktop.DBus',
                                    '/org/freedesktop/DBus',
                                    this._onProxyReady.bind(this));
    },

    _shouldShow() {
        return !this.empty && Calendar.isToday(this._date);
    },

    _addPlayer(busName) {
        if (this._players.get(busName))
            return;

        let player = new MprisPlayer(busName);
        player.connect('closed',
            () => {
                this._players.delete(busName);
            });
        player.connect('show',
            () => {
                let message = new MediaMessage(player);
                this.addMessage(message, true);
            });
        this._players.set(busName, player);
    },

    _onProxyReady() {
        this._proxy.ListNamesRemote(([names]) => {
            names.forEach(name => {
                if (!name.startsWith(MPRIS_PLAYER_PREFIX))
                    return;

                this._addPlayer(name);
            });
        });
        this._proxy.connectSignal('NameOwnerChanged',
                                  this._onNameOwnerChanged.bind(this));
    },

    _onNameOwnerChanged(proxy, sender, [name, oldOwner, newOwner]) {
        if (!name.startsWith(MPRIS_PLAYER_PREFIX))
            return;

        if (newOwner && !oldOwner)
            this._addPlayer(name);
    }
});
