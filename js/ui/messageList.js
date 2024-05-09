import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Graphene from 'gi://Graphene';
import Meta from 'gi://Meta';
import Pango from 'gi://Pango';
import St from 'gi://St';

import * as Main from './main.js';
import * as MessageTray from './messageTray.js';
import * as Mpris from './mpris.js';

import * as Util from '../misc/util.js';
import {formatTimeSpan} from '../misc/dateUtils.js';

const MAX_NOTIFICATION_BUTTONS = 3;
const MESSAGE_ANIMATION_TIME = 100;

const DEFAULT_EXPAND_LINES = 6;

export const URLHighlighter = GObject.registerClass(
class URLHighlighter extends St.Label {
    _init(text = '', lineWrap, allowMarkup) {
        super._init({
            reactive: true,
            style_class: 'url-highlighter',
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
        });
        this._linkColor = '#ccccff';
        this.connect('style-changed', () => {
            let [hasColor, color] = this.get_theme_node().lookup_color('link-color', false);
            if (hasColor) {
                let linkColor = color.to_string().substring(0, 7);
                if (linkColor !== this._linkColor) {
                    this._linkColor = linkColor;
                    this._highlightUrls();
                }
            }
        });
        this.clutter_text.line_wrap = lineWrap;
        this.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;

        this.setMarkup(text, allowMarkup);
    }

    vfunc_button_press_event(event) {
        // Don't try to URL highlight when invisible.
        // The MessageTray doesn't actually hide us, so
        // we need to check for paint opacities as well.
        if (!this.visible || this.get_paint_opacity() === 0)
            return Clutter.EVENT_PROPAGATE;

        // Keep Notification from seeing this and taking
        // a pointer grab, which would block our button-release-event
        // handler, if an URL is clicked
        return this._findUrlAtPos(event) !== -1;
    }

    vfunc_button_release_event(event) {
        if (!this.visible || this.get_paint_opacity() === 0)
            return Clutter.EVENT_PROPAGATE;

        const urlId = this._findUrlAtPos(event);
        if (urlId !== -1) {
            let url = this._urls[urlId].url;
            if (!url.includes(':'))
                url = `http://${url}`;

            Gio.app_info_launch_default_for_uri(
                url, global.create_app_launch_context(0, -1));
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_motion_event(event) {
        if (!this.visible || this.get_paint_opacity() === 0)
            return Clutter.EVENT_PROPAGATE;

        const urlId = this._findUrlAtPos(event);
        if (urlId !== -1 && !this._cursorChanged) {
            global.display.set_cursor(Meta.Cursor.POINTING_HAND);
            this._cursorChanged = true;
        } else if (urlId === -1) {
            global.display.set_cursor(Meta.Cursor.DEFAULT);
            this._cursorChanged = false;
        }
        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_leave_event(event) {
        if (!this.visible || this.get_paint_opacity() === 0)
            return Clutter.EVENT_PROPAGATE;

        if (this._cursorChanged) {
            this._cursorChanged = false;
            global.display.set_cursor(Meta.Cursor.DEFAULT);
        }
        return super.vfunc_leave_event(event);
    }

    setMarkup(text, allowMarkup) {
        text = text ? Util.fixMarkup(text, allowMarkup) : '';
        this._text = text;

        this.clutter_text.set_markup(text);
        /* clutter_text.text contain text without markup */
        this._urls = Util.findUrls(this.clutter_text.text);
        this._highlightUrls();
    }

    _highlightUrls() {
        // text here contain markup
        let urls = Util.findUrls(this._text);
        let markup = '';
        let pos = 0;
        for (let i = 0; i < urls.length; i++) {
            let url = urls[i];
            let str = this._text.substring(pos, url.pos);
            markup += `${str}<span foreground="${this._linkColor}"><u>${url.url}</u></span>`;
            pos = url.pos + url.url.length;
        }
        markup += this._text.substring(pos);
        this.clutter_text.set_markup(markup);
    }

    _findUrlAtPos(event) {
        let [x, y] = event.get_coords();
        [, x, y] = this.transform_stage_point(x, y);
        let findPos = -1;
        for (let i = 0; i < this.clutter_text.text.length; i++) {
            let [, px, py, lineHeight] = this.clutter_text.position_to_coords(i);
            if (py > y || py + lineHeight < y || x < px)
                continue;
            findPos = i;
        }
        if (findPos !== -1) {
            for (let i = 0; i < this._urls.length; i++) {
                if (findPos >= this._urls[i].pos &&
                    this._urls[i].pos + this._urls[i].url.length > findPos)
                    return i;
            }
        }
        return -1;
    }
});

const ScaleLayout = GObject.registerClass(
class ScaleLayout extends Clutter.BinLayout {
    _init(params) {
        this._container = null;
        super._init(params);
    }

    _connectContainer(container) {
        if (this._container === container)
            return;

        this._container?.disconnectObject(this);

        this._container = container;

        if (this._container) {
            this._container.connectObject(
                'notify::scale-x', () => this.layout_changed(),
                'notify::scale-y', () => this.layout_changed(), this);
        }
    }

    vfunc_get_preferred_width(container, forHeight) {
        this._connectContainer(container);

        let [min, nat] = super.vfunc_get_preferred_width(container, forHeight);
        return [
            Math.floor(min * container.scale_x),
            Math.floor(nat * container.scale_x),
        ];
    }

    vfunc_get_preferred_height(container, forWidth) {
        this._connectContainer(container);

        let [min, nat] = super.vfunc_get_preferred_height(container, forWidth);
        return [
            Math.floor(min * container.scale_y),
            Math.floor(nat * container.scale_y),
        ];
    }
});

const LabelExpanderLayout = GObject.registerClass({
    Properties: {
        'expansion': GObject.ParamSpec.double(
            'expansion', null, null,
            GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
            0, 1, 0),
    },
}, class LabelExpanderLayout extends Clutter.BinLayout {
    constructor(params) {
        super(params);

        this._expansion = 0;
        this._expandLines = DEFAULT_EXPAND_LINES;
    }

    get expansion() {
        return this._expansion;
    }

    set expansion(v) {
        if (v === this._expansion)
            return;
        this._expansion = v;
        this.notify('expansion');

        this.layout_changed();
    }

    set expandLines(v) {
        if (v === this._expandLines)
            return;
        this._expandLines = v;
        if (this._expansion > 0)
            this.layout_changed();
    }

    vfunc_get_preferred_height(container, forWidth) {
        let [min, nat] = [0, 0];

        const [child] = container;

        if (child) {
            [min, nat] = child.get_preferred_height(-1);

            const [, nat2] = child.get_preferred_height(forWidth);
            const expHeight =
                Math.min(nat2, nat * this._expandLines);
            [min, nat] = [
                min + this._expansion * (expHeight - min),
                nat + this._expansion * (expHeight - nat),
            ];
        }

        return [min, nat];
    }
});

export const Source = GObject.registerClass({
    Properties: {
        'title': GObject.ParamSpec.string(
            'title', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'icon': GObject.ParamSpec.object(
            'icon', null, null,
            GObject.ParamFlags.READWRITE,
            Gio.Icon),
        'icon-name': GObject.ParamSpec.string(
            'icon-name', null, null,
            GObject.ParamFlags.READWRITE,
            null),
    },
}, class Source extends GObject.Object {
    get iconName() {
        if (this.gicon instanceof Gio.ThemedIcon)
            return this.gicon.iconName;
        else
            return null;
    }

    set iconName(iconName) {
        this.icon = new Gio.ThemedIcon({name: iconName});
    }
});

const TimeLabel = GObject.registerClass(
class TimeLabel extends St.Label {
    _init() {
        super._init({
            style_class: 'event-time',
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.END,
            visible: false,
        });
    }

    get datetime() {
        return this._datetime;
    }

    set datetime(datetime) {
        if (this._datetime?.equal(datetime))
            return;

        this._datetime = datetime;

        this.visible = !!this._datetime;

        if (this.mapped)
            this._updateText();
    }

    _updateText() {
        if (this._datetime)
            this.text = formatTimeSpan(this._datetime);
    }

    vfunc_map() {
        this._updateText();

        super.vfunc_map();
    }
});

const MessageHeader = GObject.registerClass(
class MessageHeader extends St.BoxLayout {
    constructor(source) {
        super({
            style_class: 'message-header',
            x_expand: true,
        });

        const sourceIconEffect = new Clutter.DesaturateEffect();
        const sourceIcon = new St.Icon({
            style_class: 'message-source-icon',
            y_align: Clutter.ActorAlign.CENTER,
            fallback_icon_name: 'application-x-executable-symbolic',
        });
        sourceIcon.add_effect(sourceIconEffect);
        this.add_child(sourceIcon);

        sourceIcon.connect('style-changed', () => {
            const themeNode = sourceIcon.get_theme_node();
            sourceIconEffect.enabled = themeNode.get_icon_style() === St.IconStyle.SYMBOLIC;
        });

        const headerContent = new St.BoxLayout({
            style_class: 'message-header-content',
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
        });
        this.add_child(headerContent);

        this.expandButton = new St.Button({
            style_class: 'message-expand-button',
            icon_name: 'notification-expand-symbolic',
            y_align: Clutter.ActorAlign.CENTER,
            pivot_point: new Graphene.Point({x: 0.5, y: 0.5}),
        });
        this.add_child(this.expandButton);

        this.closeButton = new St.Button({
            style_class: 'message-close-button',
            icon_name: 'window-close-symbolic',
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this.closeButton);

        const sourceTitle = new St.Label({
            style_class: 'message-source-title',
            y_align: Clutter.ActorAlign.END,
        });
        headerContent.add_child(sourceTitle);

        source.bind_property_full('title',
            sourceTitle,
            'text',
            GObject.BindingFlags.SYNC_CREATE,
            // Translators: this is the string displayed in the header when a message
            // source doesn't have a name
            (bind, value) => [true, value === null || value === '' ? _('Unknown App') : value],
            null);
        source.bind_property('icon',
            sourceIcon,
            'gicon',
            GObject.BindingFlags.SYNC_CREATE);

        this.timeLabel = new TimeLabel();
        headerContent.add_child(this.timeLabel);
    }
});

export const Message = GObject.registerClass({
    Properties: {
        'title': GObject.ParamSpec.string(
            'title', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'body': GObject.ParamSpec.string(
            'body', null, null,
            GObject.ParamFlags.READWRITE,
            null),
        'use-body-markup': GObject.ParamSpec.boolean(
            'use-body-markup', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'icon': GObject.ParamSpec.object(
            'icon', null, null,
            GObject.ParamFlags.READWRITE,
            Gio.Icon),
        'datetime': GObject.ParamSpec.boxed(
            'datetime', null, null,
            GObject.ParamFlags.READWRITE,
            GLib.DateTime),
    },
    Signals: {
        'close': {
            flags: GObject.SignalFlags.RUN_LAST,
        },
        'expanded': {},
        'unexpanded': {},
    },
}, class Message extends St.Button {
    constructor(source) {
        super({
            style_class: 'message',
            accessible_role: Atk.Role.NOTIFICATION,
            can_focus: true,
            x_expand: true,
            y_expand: false,
        });

        this.expanded = false;
        this._useBodyMarkup = false;

        let vbox = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
        });
        this.set_child(vbox);

        this._header = new MessageHeader(source);
        vbox.add_child(this._header);

        const hbox = new St.BoxLayout({
            style_class: 'message-box',
        });
        vbox.add_child(hbox);

        this._actionBin = new St.Bin({
            style_class: 'message-action-bin',
            layout_manager: new ScaleLayout(),
            visible: false,
        });
        vbox.add_child(this._actionBin);

        this._icon = new St.Icon({
            style_class: 'message-icon',
            y_expand: true,
            y_align: Clutter.ActorAlign.START,
            visible: false,
        });
        hbox.add_child(this._icon);

        this._icon.connect('notify::is-symbolic', () => {
            if (this._icon.is_symbolic)
                this._icon.add_style_class_name('message-themed-icon');
            else
                this._icon.remove_style_class_name('message-themed-icon');
        });

        const contentBox = new St.BoxLayout({
            style_class: 'message-content',
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
        });
        hbox.add_child(contentBox);

        this._mediaControls = new St.BoxLayout();
        hbox.add_child(this._mediaControls);

        this.titleLabel = new St.Label({
            style_class: 'message-title',
            y_align: Clutter.ActorAlign.END,
        });
        contentBox.add_child(this.titleLabel);

        this._bodyLabel = new URLHighlighter('', true, this._useBodyMarkup);
        this._bodyLabel.add_style_class_name('message-body');
        this._bodyBin = new St.Bin({
            x_expand: true,
            layout_manager: new LabelExpanderLayout(),
            child: this._bodyLabel,
        });
        contentBox.add_child(this._bodyBin);

        this.connect('destroy', this._onDestroy.bind(this));

        this._header.closeButton.connect('clicked', this.close.bind(this));
        this._header.closeButton.visible = this.canClose();

        this._header.expandButton.connect('clicked', () => {
            if (this.expanded)
                this.unexpand(true);
            else
                this.expand(true);
        });
        this._bodyLabel.connect('notify::allocation', this._updateExpandButton.bind(this));
        this._updateExpandButton();
    }

    _updateExpandButton() {
        if (!this._bodyLabel.has_allocation())
            return;
        const layout = this._bodyLabel.clutter_text.get_layout();
        const canExpand = layout.is_ellipsized() || this.expanded || !!this._actionBin.child;
        // Use opacity to not trigger a relayout
        this._header.expandButton.opacity = canExpand ? 255 : 0;
    }

    close() {
        this.emit('close');
    }

    set icon(icon) {
        this._icon.gicon = icon;

        this._icon.visible = !!icon;
        this.notify('icon');
    }

    get icon() {
        return this._icon.gicon;
    }

    set datetime(datetime) {
        this._header.timeLabel.datetime = datetime;
        this.notify('datetime');
    }

    get datetime() {
        return this._header.timeLabel.datetime;
    }

    set title(text) {
        this._titleText = text;
        const title = text ? Util.fixMarkup(text.replace(/\n/g, ' '), false) : '';
        this.titleLabel.clutter_text.set_markup(title);
        this.notify('title');
    }

    get title() {
        return this._titleText;
    }

    set body(text) {
        this._bodyText = text;
        this._bodyLabel.setMarkup(text ? text.replace(/\n/g, ' ') : '',
            this._useBodyMarkup);
        this.notify('body');
    }

    get body() {
        return this._bodyText;
    }

    set useBodyMarkup(enable) {
        if (this._useBodyMarkup === enable)
            return;
        this._useBodyMarkup = enable;
        this.body = this._bodyText;
        this.notify('use-body-markup');
    }

    get useBodyMarkup() {
        return this._useBodyMarkup;
    }

    setActionArea(actor) {
        this._actionBin.child = actor;
        this._actionBin.visible = actor && this.expanded;
        this._updateExpandButton();
    }

    addMediaControl(iconName, callback) {
        const button = new St.Button({
            style_class: 'message-media-control',
            iconName,
        });
        button.connect('clicked', callback);
        this._mediaControls.add_child(button);
        return button;
    }

    expand(animate) {
        this.expanded = true;

        this._actionBin.visible = !!this._actionBin.child;

        const duration = animate ? MessageTray.ANIMATION_TIME : 0;
        this._bodyBin.ease_property('@layout.expansion', 1, {
            progress_mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration,
        });

        this._actionBin.scale_y = 0;
        this._actionBin.ease({
            scale_y: 1,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });

        this._header.expandButton.ease({
            rotation_angle_z: 180,
            duration,
        });

        this.emit('expanded');
    }

    unexpand(animate) {
        const duration = animate ? MessageTray.ANIMATION_TIME : 0;
        this._bodyBin.ease_property('@layout.expansion', 0, {
            progress_mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration,
        });

        this._actionBin.ease({
            scale_y: 0,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this._actionBin.hide();
                this.expanded = false;
            },
        });

        this._header.expandButton.ease({
            rotation_angle_z: 0,
            duration,
        });

        this.emit('unexpanded');
    }

    canClose() {
        return false;
    }

    _onDestroy() {
    }

    vfunc_key_press_event(event) {
        let keysym = event.get_key_symbol();

        if (keysym === Clutter.KEY_Delete ||
            keysym === Clutter.KEY_KP_Delete ||
            keysym === Clutter.KEY_BackSpace) {
            if (this.canClose()) {
                this.close();
                return Clutter.EVENT_STOP;
            }
        }
        return super.vfunc_key_press_event(event);
    }
});

export const NotificationMessage = GObject.registerClass(
class NotificationMessage extends Message {
    constructor(notification) {
        super(notification.source);

        this.notification = notification;

        notification.connectObject(
            'action-added', (_, action) => this._addAction(action),
            'action-removed', (_, action) => this._removeAction(action),
            'destroy', () => {
                this.notification = null;
                if (!this._closed)
                    this.close();
            }, this);

        notification.bind_property('title',
            this, 'title',
            GObject.BindingFlags.SYNC_CREATE);
        notification.bind_property('body',
            this, 'body',
            GObject.BindingFlags.SYNC_CREATE);
        notification.bind_property('use-body-markup',
            this, 'use-body-markup',
            GObject.BindingFlags.SYNC_CREATE);
        notification.bind_property('datetime',
            this, 'datetime',
            GObject.BindingFlags.SYNC_CREATE);
        notification.bind_property('gicon',
            this, 'icon',
            GObject.BindingFlags.SYNC_CREATE);

        this._actions = new Map();
        this.notification.actions.forEach(action => {
            this._addAction(action);
        });
    }

    on_close() {
        this._closed = true;
        this.notification?.destroy(MessageTray.NotificationDestroyedReason.DISMISSED);
    }

    vfunc_clicked() {
        this.notification?.activate();
    }

    canClose() {
        return true;
    }

    _addAction(action) {
        if (!this._buttonBox) {
            this._buttonBox = new St.BoxLayout({
                x_expand: true,
                style_class: 'notification-buttons-bin',
            });
            this.setActionArea(this._buttonBox);
            global.focus_manager.add_group(this._buttonBox);
        }

        if (this._buttonBox.get_n_children() >= MAX_NOTIFICATION_BUTTONS)
            return;

        const button = new St.Button({
            style_class: 'notification-button',
            x_expand: true,
            label: action.label,
        });

        button.connect('clicked', () => action.activate());

        this._actions.set(action, button);
        this._buttonBox.add_child(button);
    }

    _removeAction(action) {
        this._actions.get(action)?.destroy();
        this._actions.delete(action);
    }
});

const MediaMessage = GObject.registerClass(
class MediaMessage extends Message {
    constructor(player) {
        super(player.source);

        this._player = player;
        this.add_style_class_name('media-message');

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

        this._player.connectObject('changed', this._update.bind(this), this);
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
        let icon;
        if (this._player.trackCoverUrl) {
            const file = Gio.File.new_for_uri(this._player.trackCoverUrl);
            icon = new Gio.FileIcon({file});
        } else {
            icon = new Gio.ThemedIcon({name: 'audio-x-generic-symbolic'});
        }

        this.set({
            title: this._player.trackTitle,
            body: this._player.trackArtists.join(', '),
            icon,
        });

        let isPlaying = this._player.status === 'Playing';
        let iconName = isPlaying
            ? 'media-playback-pause-symbolic'
            : 'media-playback-start-symbolic';
        this._playPauseButton.child.icon_name = iconName;

        this._updateNavButton(this._prevButton, this._player.canGoPrevious);
        this._updateNavButton(this._nextButton, this._player.canGoNext);
    }
});

export const MessageListSection = GObject.registerClass({
    Properties: {
        'can-clear': GObject.ParamSpec.boolean(
            'can-clear', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'empty': GObject.ParamSpec.boolean(
            'empty', null, null,
            GObject.ParamFlags.READABLE,
            true),
    },
    Signals: {
        'can-clear-changed': {},
        'empty-changed': {},
        'message-focused': {param_types: [Message.$gtype]},
    },
}, class MessageListSection extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'message-list-section',
            clip_to_allocation: true,
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
        });

        this._list = new St.BoxLayout({
            style_class: 'message-list-section-list',
            orientation: Clutter.Orientation.VERTICAL,
        });
        this.add_child(this._list);

        this._list.connect('child-added', this._sync.bind(this));
        this._list.connect('child-removed', this._sync.bind(this));

        Main.sessionMode.connectObject(
            'updated', () => this._sync(), this);

        this._empty = true;
        this._canClear = false;
        this._sync();
    }

    get empty() {
        return this._empty;
    }

    get canClear() {
        return this._canClear;
    }

    get _messages() {
        return this._list.get_children().map(i => i.child);
    }

    _onKeyFocusIn(messageActor) {
        this.emit('message-focused', messageActor);
    }

    get allowed() {
        return true;
    }

    addMessage(message, animate) {
        this.addMessageAtIndex(message, -1, animate);
    }

    addMessageAtIndex(message, index, animate) {
        if (this._messages.includes(message))
            throw new Error('Message was already added previously');

        let listItem = new St.Bin({
            child: message,
            layout_manager: new ScaleLayout(),
            pivot_point: new Graphene.Point({x: .5, y: .5}),
        });
        listItem._connectionsIds = [];

        listItem._connectionsIds.push(message.connect('key-focus-in',
            this._onKeyFocusIn.bind(this)));
        listItem._connectionsIds.push(message.connect('close', () => {
            this.removeMessage(message, true);
        }));
        listItem._connectionsIds.push(message.connect('destroy', () => {
            listItem._connectionsIds.forEach(id => message.disconnect(id));
            listItem.destroy();
        }));

        this._list.insert_child_at_index(listItem, index);

        const duration = animate ? MESSAGE_ANIMATION_TIME : 0;
        listItem.set({scale_x: 0, scale_y: 0});
        listItem.ease({
            scale_x: 1,
            scale_y: 1,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    moveMessage(message, index, animate) {
        if (!this._messages.includes(message))
            throw new Error('Impossible to move untracked message');

        let listItem = message.get_parent();

        if (!animate) {
            this._list.set_child_at_index(listItem, index);
            return;
        }

        let onComplete = () => {
            this._list.set_child_at_index(listItem, index);
            listItem.ease({
                scale_x: 1,
                scale_y: 1,
                duration: MESSAGE_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        };
        listItem.ease({
            scale_x: 0,
            scale_y: 0,
            duration: MESSAGE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete,
        });
    }

    removeMessage(message, animate) {
        const messages = this._messages;

        if (!messages.includes(message))
            throw new Error('Impossible to remove untracked message');

        let listItem = message.get_parent();
        listItem._connectionsIds.forEach(id => message.disconnect(id));

        let nextMessage = null;

        if (message.has_key_focus()) {
            const index = messages.indexOf(message);
            nextMessage =
                messages[index + 1] ||
                messages[index - 1] ||
                this._list;
        }

        const duration = animate ? MESSAGE_ANIMATION_TIME : 0;
        listItem.ease({
            scale_x: 0,
            scale_y: 0,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                listItem.destroy();
                nextMessage?.grab_key_focus();
            },
        });
    }

    clear() {
        let messages = this._messages.filter(msg => msg.canClose());

        // If there are few messages, letting them all zoom out looks OK
        if (messages.length < 2) {
            messages.forEach(message => {
                message.close();
            });
        } else {
            // Otherwise we slide them out one by one, and then zoom them
            // out "off-screen" in the end to smoothly shrink the parent
            let delay = MESSAGE_ANIMATION_TIME / Math.max(messages.length, 5);
            for (let i = 0; i < messages.length; i++) {
                let message = messages[i];
                message.get_parent().ease({
                    translation_x: this._list.width,
                    opacity: 0,
                    duration: MESSAGE_ANIMATION_TIME,
                    delay: i * delay,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    onComplete: () => message.close(),
                });
            }
        }
    }

    _shouldShow() {
        return !this.empty;
    }

    _sync() {
        let messages = this._messages;
        let empty = messages.length === 0;

        if (this._empty !== empty) {
            this._empty = empty;
            this.notify('empty');
        }

        let canClear = messages.some(m => m.canClose());
        if (this._canClear !== canClear) {
            this._canClear = canClear;
            this.notify('can-clear');
        }

        this.visible = this.allowed && this._shouldShow();
    }
});

export const NotificationSection = GObject.registerClass(
class NotificationSection extends MessageListSection {
    _init() {
        super._init();

        this._nUrgent = 0;

        Main.messageTray.connect('source-added', this._sourceAdded.bind(this));
        Main.messageTray.getSources().forEach(source => {
            this._sourceAdded(Main.messageTray, source);
        });
    }

    get allowed() {
        return Main.sessionMode.hasNotifications &&
               !Main.sessionMode.isGreeter;
    }

    _sourceAdded(tray, source) {
        source.connectObject(
            'notification-added', this._onNotificationAdded.bind(this),
            'notification-removed', this._onNotificationRemoved.bind(this),
            this);
    }

    _onNotificationAdded(source, notification) {
        let message = new NotificationMessage(notification);

        const isUrgent = notification.urgency === MessageTray.Urgency.CRITICAL;

        notification.connectObject(
            'notify::datetime', () => {
                // The datetime property changes whenever the notification is updated
                this.moveMessage(message, isUrgent ? 0 : this._nUrgent, this.mapped);
            }, this);

        if (isUrgent) {
            // Keep track of urgent notifications to keep them on top
            this._nUrgent++;
        } else if (this.mapped) {
            // Only acknowledge non-urgent notifications in case it
            // has important actions that are inaccessible when not
            // shown as banner
            notification.acknowledged = true;
        }

        const index = isUrgent ? 0 : this._nUrgent;
        this.addMessageAtIndex(message, index, this.mapped);
    }

    _onNotificationRemoved(source_, notification) {
        if (notification.urgency === MessageTray.Urgency.CRITICAL)
            this._nUrgent--;
    }

    vfunc_map() {
        this._messages.forEach(message => {
            if (message.notification.urgency !== MessageTray.Urgency.CRITICAL)
                message.notification.acknowledged = true;
        });
        super.vfunc_map();
    }
});

export const MediaSection = GObject.registerClass(
class MediaSection extends MessageListSection {
    constructor() {
        super();
        this._players = new Map();
        this._mediaSource = new Mpris.MprisSource();

        this._mediaSource.connectObject(
            'player-added', (_, player) => this._addPlayer(player),
            'player-removed', (_, player) => this._removePlayer(player),
            this);

        this._mediaSource.players.forEach(player => {
            this._addPlayer(player);
        });
    }

    _addPlayer(player) {
        if (this._players.has(player))
            throw new Error('Player was already added previously');

        const message = new MediaMessage(player);
        this._players.set(player, message);
        this.addMessage(message, true);
    }

    _removePlayer(player) {
        const message = this._players.get(player);

        if (message)
            this.removeMessage(message, true);

        this._players.delete(player);
    }

    get allowed() {
        return !Main.sessionMode.isGreeter;
    }
});
