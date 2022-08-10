/* exported MediaSection */
const { Gio, GObject, Shell, St } = imports.gi;
const Signals = imports.misc.signals;

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

var MediaMessage = GObject.registerClass(
class MediaMessage extends MessageList.Message {
    _init(player) {
        super._init('', '');

        this._player = player;

        this._icon = new St.Icon({ style_class: 'media-message-cover-icon' });
        this.setIcon(this._icon);

        // reclaim space used by unused elements
        this._secondaryBin.hide();
        this._closeButton.hide();

        this._prevButton = this.addMediaControl('media-skip-backward-symbolic',
            () => {
                this._player.previous();
            });

        this._playPauseButton = this.addMediaControl('',
            () => {
                this._player.playPause();
            });

        this._nextButton = this.addMediaControl('media-skip-forward-symbolic',
            () => {
                this._player.next();
            });

        this._player.connectObject(
            'changed', this._update.bind(this),
            'closed', this.close.bind(this), this);
        this._update();
    }

    vfunc_clicked() {
        this._player.raise();
        Main.panel.closeCalendar();
    }

    _updateNavButton(button, sensitive) {
        button.reactive = sensitive;
    }

    _update() {
        this.setTitle(this._player.trackTitle);
        this.setBody(this._player.trackArtists.join(', '));

        if (this._player.trackCoverUrl) {
            let file = Gio.File.new_for_uri(this._player.trackCoverUrl);
            this._icon.gicon = new Gio.FileIcon({ file });
            this._icon.remove_style_class_name('fallback');
        } else {
            this._icon.icon_name = 'audio-x-generic-symbolic';
            this._icon.add_style_class_name('fallback');
        }

        let isPlaying = this._player.status == 'Playing';
        let iconName = isPlaying
            ? 'media-playback-pause-symbolic'
            : 'media-playback-start-symbolic';
        this._playPauseButton.child.icon_name = iconName;

        this._updateNavButton(this._prevButton, this._player.canGoPrevious);
        this._updateNavButton(this._nextButton, this._player.canGoNext);
    }
});

var MprisPlayer = class MprisPlayer extends Signals.EventEmitter {
    constructor(busName) {
        super();

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
        this._busName = busName;
    }

    get status() {
        return this._playerProxy.PlaybackStatus;
    }

    get trackArtists() {
        return this._trackArtists;
    }

    get trackTitle() {
        return this._trackTitle;
    }

    get trackCoverUrl() {
        return this._trackCoverUrl;
    }

    playPause() {
        this._playerProxy.PlayPauseAsync().catch(logError);
    }

    get canGoNext() {
        return this._playerProxy.CanGoNext;
    }

    next() {
        this._playerProxy.NextAsync().catch(logError);
    }

    get canGoPrevious() {
        return this._playerProxy.CanGoPrevious;
    }

    previous() {
        this._playerProxy.PreviousAsync().catch(logError);
    }

    raise() {
        // The remote Raise() method may run into focus stealing prevention,
        // so prefer activating the app via .desktop file if possible
        let app = null;
        if (this._mprisProxy.DesktopEntry) {
            let desktopId = `${this._mprisProxy.DesktopEntry}.desktop`;
            app = Shell.AppSystem.get_default().lookup_app(desktopId);
        }

        if (app)
            app.activate();
        else if (this._mprisProxy.CanRaise)
            this._mprisProxy.RaiseAsync().catch(logError);
    }

    _close() {
        this._mprisProxy.disconnectObject(this);
        this._mprisProxy = null;

        this._playerProxy.disconnectObject(this);
        this._playerProxy = null;

        this.emit('closed');
    }

    _onMprisProxyReady() {
        this._mprisProxy.connectObject('notify::g-name-owner',
            () => {
                if (!this._mprisProxy.g_name_owner)
                    this._close();
            }, this);
        // It is possible for the bus to disappear before the previous signal
        // is connected, so we must ensure that the bus still exists at this
        // point.
        if (!this._mprisProxy.g_name_owner)
            this._close();
    }

    _onPlayerProxyReady() {
        this._playerProxy.connectObject(
            'g-properties-changed', () => this._updateState(), this);
        this._updateState();
    }

    _updateState() {
        let metadata = {};
        for (let prop in this._playerProxy.Metadata)
            metadata[prop] = this._playerProxy.Metadata[prop].deepUnpack();

        // Validate according to the spec; some clients send buggy metadata:
        // https://www.freedesktop.org/wiki/Specifications/mpris-spec/metadata
        this._trackArtists = metadata['xesam:artist'];
        if (!Array.isArray(this._trackArtists) ||
            !this._trackArtists.every(artist => typeof artist === 'string')) {
            if (typeof this._trackArtists !== 'undefined') {
                log(`Received faulty track artist metadata from ${
                    this._busName}; expected an array of strings, got ${
                    this._trackArtists} (${typeof this._trackArtists})`);
            }
            this._trackArtists =  [_("Unknown artist")];
        }

        this._trackTitle = metadata['xesam:title'];
        if (typeof this._trackTitle !== 'string') {
            if (typeof this._trackTitle !== 'undefined') {
                log(`Received faulty track title metadata from ${
                    this._busName}; expected a string, got ${
                    this._trackTitle} (${typeof this._trackTitle})`);
            }
            this._trackTitle = _("Unknown title");
        }

        this._trackCoverUrl = metadata['mpris:artUrl'];
        if (typeof this._trackCoverUrl !== 'string') {
            if (typeof this._trackCoverUrl !== 'undefined') {
                log(`Received faulty track cover art metadata from ${
                    this._busName}; expected a string, got ${
                    this._trackCoverUrl} (${typeof this._trackCoverUrl})`);
            }
            this._trackCoverUrl = '';
        }

        this.emit('changed');

        let visible = this._playerProxy.CanPlay;

        if (this._visible != visible) {
            this._visible = visible;
            if (visible)
                this.emit('show');
            else
                this.emit('hide');
        }
    }
};

var MediaSection = GObject.registerClass(
class MediaSection extends MessageList.MessageListSection {
    _init() {
        super._init();

        this._players = new Map();

        this._proxy = new DBusProxy(Gio.DBus.session,
                                    'org.freedesktop.DBus',
                                    '/org/freedesktop/DBus',
                                    this._onProxyReady.bind(this));
    }

    get allowed() {
        return !Main.sessionMode.isGreeter;
    }

    _addPlayer(busName) {
        if (this._players.get(busName))
            return;

        let player = new MprisPlayer(busName);
        let message = null;
        player.connect('closed',
            () => {
                this._players.delete(busName);
            });
        player.connect('show', () => {
            message = new MediaMessage(player);
            this.addMessage(message, true);
        });
        player.connect('hide', () => {
            this.removeMessage(message, true);
            message = null;
        });

        this._players.set(busName, player);
    }

    async _onProxyReady() {
        const [names] = await this._proxy.ListNamesAsync();
        names.forEach(name => {
            if (!name.startsWith(MPRIS_PLAYER_PREFIX))
                return;

            this._addPlayer(name);
        });
        this._proxy.connectSignal('NameOwnerChanged',
                                  this._onNameOwnerChanged.bind(this));
    }

    _onNameOwnerChanged(proxy, sender, [name, oldOwner, newOwner]) {
        if (!name.startsWith(MPRIS_PLAYER_PREFIX))
            return;

        if (newOwner && !oldOwner)
            this._addPlayer(name);
    }
});
