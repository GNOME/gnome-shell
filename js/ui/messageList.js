import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import Cogl from 'gi://Cogl';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Graphene from 'gi://Graphene';
import Meta from 'gi://Meta';
import Pango from 'gi://Pango';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Main from './main.js';
import * as MessageTray from './messageTray.js';
import * as Mpris from './mpris.js';

import * as Util from '../misc/util.js';
import {formatTimeSpan} from '../misc/dateUtils.js';

const MAX_NOTIFICATION_BUTTONS = 3;
const MESSAGE_ANIMATION_TIME = 100;

const EXPANDED_GROUP_OVERSHOT_HEIGHT = 50;
const DEFAULT_EXPAND_LINES = 6;

const GROUP_EXPENSION_TIME = 200;
const MAX_VISIBLE_STACKED_MESSAGES = 3;
const ADDITIONAL_BOTTOM_MARGIN_EXPANDED_GROUP = 15;
const WIDTH_OFFSET_STACKED = 6;
const HEIGHT_OFFSET_STACKED = 10;
const HEIGHT_OFFSET_REDUCTION_STACKED = 1.4;

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
            global.display.set_cursor(Meta.Cursor.POINTER);
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

const ScaleLayout = GObject.registerClass({
    Properties: {
        'scaling-enabled': GObject.ParamSpec.boolean(
            'scaling-enabled', null, null,
            GObject.ParamFlags.READWRITE,
            true),
    },
}, class ScaleLayout extends Clutter.BinLayout {
    _container = null;
    _scalingEnabled = true;

    get scalingEnabled() {
        return this._scalingEnabled;
    }

    set scalingEnabled(value) {
        if (this._scalingEnabled === value)
            return;

        this._scalingEnabled = value;
        this.notify('scaling-enabled');
        this.layout_changed();
    }

    vfunc_set_container(container) {
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
        const [min, nat] = super.vfunc_get_preferred_width(container, forHeight);

        if (this._scalingEnabled) {
            return [
                Math.floor(min * container.scale_x),
                Math.floor(nat * container.scale_x),
            ];
        } else {
            return [min, nat];
        }
    }

    vfunc_get_preferred_height(container, forWidth) {
        const [min, nat] = super.vfunc_get_preferred_height(container, forWidth);

        if (this._scalingEnabled) {
            return [
                Math.floor(min * container.scale_y),
                Math.floor(nat * container.scale_y),
            ];
        } else {
            return [min, nat];
        }
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

        return [Math.floor(min), Math.floor(nat)];
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

export const MediaMessage = GObject.registerClass(
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

        Main.sessionMode.connectObject('updated',
            () => this._applyPolicy(), this);
        this._player.connectObject('changed', this._update.bind(this), this);
        this._update();
    }

    vfunc_clicked() {
        if (Main.sessionMode.isLocked)
            return;

        this._player.raise();
        Main.panel.closeCalendar();
    }

    _applyPolicy() {
        this.visible =
            this._policy.enable &&
            (!Main.sessionMode.isLocked || this._policy.showInLockScreen);
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

        const appId = this._player.app?.id.replace(/\.desktop$/, '') ?? 'generic';
        if (this._policy?.id !== appId) {
            this._policy?.disconnectObject(this);

            this._policy = MessageTray.NotificationPolicy.newForApp(this._player.app);

            // Register notification source
            this._policy.store();

            this._policy.connectObject(
                'notify::enable', () => this._applyPolicy(),
                'notify::show-in-lock-screen', () => this._applyPolicy(),
                this);
            this._applyPolicy();
        }
    }
});

export const NotificationMessageGroup = GObject.registerClass({
    Properties: {
        'expanded': GObject.ParamSpec.boolean(
            'expanded', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'has-urgent': GObject.ParamSpec.boolean(
            'has-urgent', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'focus-child': GObject.ParamSpec.object(
            'focus-child', null, null,
            GObject.ParamFlags.READABLE,
            Message),
    },
    Signals: {
        'notification-added': {},
        'expand-toggle-requested': {},
    },
}, class NotificationMessageGroup extends St.Widget {
    constructor(source) {
        const action =  new Clutter.ClickGesture();

        // A widget that covers stacked messages so that they don't receive events
        const cover = new St.Widget({
            name: 'cover',
            reactive: true,
        });

        const header = new St.BoxLayout({
            style_class: 'message-group-header',
            x_expand: true,
            visible: false,
        });

        super({
            style_class: 'message-notification-group',
            x_expand: true,
            layout_manager: new MessageGroupExpanderLayout(cover, header),
            actions: action,
            reactive: true,
        });

        // The cover is always the second child to prevent interaction
        // with stacked messages when collapsed.
        this._cover = cover;
        // The headerBox will always be the last child
        this._headerBox = header;

        this.source = source;
        this._expanded = false;
        this._notificationToMessage = new Map();
        this._nUrgent = 0;
        this._focusChild = null;

        const titleLabel = new St.Label({
            style_class: 'message-group-title',
            y_align: Clutter.ActorAlign.CENTER,
        });

        source.bind_property('title',
            titleLabel,
            'text',
            GObject.BindingFlags.SYNC_CREATE);

        this._headerBox.add_child(titleLabel);

        this._unexpandButton = new St.Button({
            style_class: 'message-collapse-button',
            icon_name: 'group-collapse-symbolic',
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            y_expand: true,
        });

        this._unexpandButton.connect('clicked', () => this.emit('expand-toggle-requested'));
        action.connect('recognize', () => this.emit('expand-toggle-requested'));

        this._headerBox.add_child(this._unexpandButton);
        this.add_child(this._headerBox);
        this.add_child(this._cover);

        source.connectObject(
            'notification-added', (_, notification) => this._addNotification(notification),
            'notification-removed', (_, notification) => this._removeNotification(notification),
            this);

        source.notifications.forEach(notification => {
            this._addNotification(notification);
        });
    }

    get expanded() {
        // Consider this group to be expanded when it has only one message
        return this._expanded || this._notificationToMessage.size === 1;
    }

    get hasUrgent() {
        return this._nUrgent > 0;
    }

    _onKeyFocusIn(actor) {
        if (this._focusChild === actor)
            return;
        this._focusChild = actor;
        this.notify('focus-child');
    }

    get focusChild() {
        return this._focusChild;
    }

    async expand() {
        if (this._expanded)
            return;

        this._headerBox.show();
        this._expanded = true;
        this._updateStackedMessagesFade();
        this.notify('expanded');
        this._cover.hide();

        await new Promise((resolve, _) => {
            this.ease_property('@layout.expansion', 1, {
                progress_mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                duration: GROUP_EXPENSION_TIME,
                onComplete: () => resolve(),
            });
        });
    }

    async collapse() {
        if (!this._expanded)
            return;

        this._notificationToMessage.forEach(message => message.unexpand(true));

        // Give focus to the fully visible message
        if (this.focusChild?.has_key_focus())
            this.get_first_child().child.grab_key_focus();

        this._expanded = false;
        this.notify('expanded');
        this._cover.show();
        this._updateStackedMessagesFade();

        await new Promise((resolve, _) => {
            this.ease_property('@layout.expansion', 0, {
                progress_mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                duration: GROUP_EXPENSION_TIME,
                onComplete: () => resolve(),
            });
        });

        this._headerBox.hide();
    }

    get expandedHeight() {
        const [min] = this.layoutManager.getExpandedHeight(this, -1);
        return min;
    }

    // Ensure that the cover is still below the top most message
    _ensureCoverPosition() {
        // If the group doesn't have any messages,
        // don't move the cover before the headerBox
        if (this.get_n_children() > 2)
            this.set_child_at_index(this._cover, 1);
    }

    _updateStackedMessagesFade() {
        const pseudoClasses = ['second-in-stack', 'lower-in-stack'];

        // The group doesn't have any messages
        if (this.get_n_children() < 3)
            return;

        const [top, cover, ...stack] = this.get_children();
        const header = stack.pop();

        console.assert(cover === this._cover,
            'Cover has expected stack position');
        console.assert(header === this._headerBox,
            'Header has expected stack position');

        // A message may have moved so we need to remove the classes from all messages
        const messages = [top, ...stack];
        messages.forEach(item => {
            pseudoClasses.forEach(name => {
                item.child.remove_style_pseudo_class(name);
            });
        });

        if (!this.expanded && stack.length > 0) {
            const stackTop = stack.shift();
            stackTop.child.add_style_pseudo_class(pseudoClasses[0]);

            // Use the same class for the third message and all messages
            // after that, since they won't be visible anyways
            stack.forEach(item => {
                const message = item.child;
                message.add_style_pseudo_class(pseudoClasses[1]);
            });
        }
    }

    canClose() {
        return true;
    }

    _addNotification(notification) {
        const message = new NotificationMessage(notification);

        this._notificationToMessage.set(notification, message);

        notification.connectObject(
            'notify::urgency', () => {
                const isUrgent = notification.urgency === MessageTray.Urgency.CRITICAL;
                const oldHasUrgent = this.hasUrgent;

                if (isUrgent)
                    this._nUrgent++;
                else
                    this._nUrgent--;

                const index = isUrgent ? 0 : this._nUrgent;
                this._moveMessage(message, index);
                if (oldHasUrgent !== this.hasUrgent)
                    this.notify('has-urgent');
            }, message);

        const isUrgent = notification.urgency === MessageTray.Urgency.CRITICAL;
        const oldHasUrgent = this.hasUrgent;

        if (isUrgent)
            this._nUrgent++;

        const wasExpanded = this.expanded;
        const item = new St.Bin({
            child: message,
            canFocus: false,
            layout_manager: new ScaleLayout(),
            pivot_point: new Graphene.Point({x: .5, y: .5}),
            scale_x: 0,
            scale_y: 0,
        });

        message.connectObject(
            'key-focus-in', this._onKeyFocusIn.bind(this),
            'expanded', () => {
                if (!this.expanded)
                    this.emit('expand-toggle-requested');
            },
            'close', () => {
                // If the group is collapsed and one notification is closed, close the entire group
                if (!this.expanded) {
                    GObject.signal_stop_emission_by_name(message, 'close');
                    this.close();
                }
            },
            'clicked', () => {
                if (!this.expanded) {
                    GObject.signal_stop_emission_by_name(message, 'clicked');
                    this.emit('expand-toggle-requested');
                }
            }, this);

        let index = isUrgent ? 0 : this._nUrgent;
        // If we add a child below the top child we need to adjust index to skip the cover child
        if (index > 0)
            index += 1;

        this.insert_child_at_index(item, index);
        this._ensureCoverPosition();
        this._updateStackedMessagesFade();

        item.layout_manager.scalingEnabled = this._expanded;

        // The first message doesn't need to be animated since the entire group is animated
        if (this._notificationToMessage.size > 1) {
            item.ease({
                scale_x: 1,
                scale_y: 1,
                duration: MESSAGE_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        } else {
            item.set_scale(1.0, 1.0);
        }

        if (wasExpanded !== this.expanded)
            this.notify('expanded');

        if (oldHasUrgent !== this.hasUrgent)
            this.notify('has-urgent');
        this.emit('notification-added');
    }

    _removeNotification(notification) {
        const message = this._notificationToMessage.get(notification);
        const item = message.get_parent();

        if (notification.urgency === MessageTray.Urgency.CRITICAL)
            this._nUrgent--;

        message.disconnectObject(this);

        item.layout_manager.scalingEnabled = this._expanded;

        item.ease({
            scale_x: 0,
            scale_y: 0,
            duration: MESSAGE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                item.destroy();
                this._notificationToMessage.delete(notification);
                this._ensureCoverPosition();
                this._updateStackedMessagesFade();

                if (this._notificationToMessage.size === 1)
                    this.emit('expand-toggle-requested');
            },
        });
    }

    vfunc_paint(paintContext) {
        // Invert the paint order, so that messages are collapsed with the
        // newest message (the first child) on top of the stack
        for (const child of this.get_children().reverse())
            child.paint(paintContext);
    }

    vfunc_pick(pickContext) {
        // Invert the pick order, so that messages are collapsed with the
        // newest message (the first child) on top of the stack
        for (const child of this.get_children().reverse())
            child.pick(pickContext);
    }

    vfunc_map() {
        // Acknowledge all notifications once they are mapped
        this._notificationToMessage.forEach((_, notification) => {
            notification.acknowledged = true;
        });
        super.vfunc_map();
    }

    vfunc_get_focus_chain() {
        if (this.expanded)
            return this.get_children();
        else
            return [this.get_first_child()];
    }

    _moveMessage(message, index) {
        if (this.get_child_at_index(index) === message)
            return;

        const item = message.get_parent();
        item.ease({
            scale_x: 0,
            scale_y: 0,
            duration: MESSAGE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                // If we add a child below the top child we need to adjust index to skip the cover child
                if (index > 0)
                    index += 1;

                this.set_child_at_index(item, index);
                this._ensureCoverPosition();
                this._updateStackedMessagesFade();
                item.ease({
                    scale_x: 1,
                    scale_y: 1,
                    duration: MESSAGE_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                });
            },
        });
    }

    close() {
        // If the group is closed, close all messages in this group
        this._notificationToMessage.forEach(message => {
            message.disconnectObject(this);
            message.close();
        });
    }
});

const MessageGroupExpanderLayout = GObject.registerClass({
    Properties: {
        'expansion': GObject.ParamSpec.double(
            'expansion', null, null,
            GObject.ParamFlags.READWRITE,
            0, 1, 0),
    },
}, class MessageGroupExpanderLayout extends Clutter.LayoutManager {
    constructor(cover, header) {
        super();

        this._cover = cover;
        this._header = header;
        this._expansion = 0;
    }

    get expansion() {
        return this._expansion;
    }

    set expansion(v) {
        v = Math.clamp(v, 0, 1);

        if (v === this._expansion)
            return;
        this._expansion = v;
        this.notify('expansion');

        this.layout_changed();
    }

    getExpandedHeight(container, forWidth) {
        let [minExpanded, natExpanded] = [0, 0];

        container.get_children().forEach(child => {
            // We don't need to measure the cover
            if (child === this._cover)
                return;

            const [minChild, natChild] = child.get_preferred_height(forWidth);
            minExpanded += minChild;
            natExpanded += natChild;
        });

        // Add additional spacing after an expanded group
        minExpanded += ADDITIONAL_BOTTOM_MARGIN_EXPANDED_GROUP;
        natExpanded += ADDITIONAL_BOTTOM_MARGIN_EXPANDED_GROUP;

        return [minExpanded, natExpanded];
    }

    vfunc_get_preferred_width(container, forHeight) {
        return container.get_children().reduce((acc, child) => {
            // We don't need to measure the cover
            if (child === this._cover)
                return [0, 0];

            if (!child.visible)
                return acc;

            const [minChild, natChild] = child.get_preferred_width(forHeight);

            return [
                Math.max(minChild, acc[0]),
                Math.max(natChild, acc[1]),
            ];
        }, [0, 0]);
    }

    vfunc_get_preferred_height(container, forWidth) {
        let offset = HEIGHT_OFFSET_STACKED;
        let [min, nat] = [0, 0];
        let visibleCount = MAX_VISIBLE_STACKED_MESSAGES;

        for (const child of container.get_children()) {
            // We don't need to measure the cover and the header is behind the stacked messages
            if (child === this._cover || child === this._header)
                continue;

            if (!child.visible)
                continue;

            // The first message is always fully shown
            if (min === 0 || nat === 0) {
                [min, nat] = child.get_preferred_height(forWidth);
            } else {
                min += offset;
                nat += offset;
                offset /= HEIGHT_OFFSET_REDUCTION_STACKED;
            }

            visibleCount--;

            if (visibleCount === 0)
                break;
        }

        const [minExpanded, natExpanded] = this.getExpandedHeight(container, forWidth);

        [min, nat] = [
            min + this._expansion * (minExpanded - min),
            nat + this._expansion * (natExpanded - nat),
        ];

        return [min, nat];
    }

    vfunc_allocate(container, box) {
        const childWidth = box.x2 - box.x1;
        let fullY2 = box.y2;

        if (this._cover.visible)
            this._cover.allocate(box);

        if (this._header.visible) {
            const [min, nat_] = this._header.get_preferred_height(childWidth);
            box.y2 = box.y1 + min;
            this._header.allocate(box);
            box.y1 += this._expansion * (box.y2 - box.y1);
        }

        // The group doesn't have any messages
        if (container.get_n_children() < 3)
            return;

        let heightOffset = HEIGHT_OFFSET_STACKED;
        const [top, cover, ...stack] = container.get_children();
        const header = stack.pop();

        console.assert(cover === this._cover,
            'Cover has expected stack position');
        console.assert(header === this._header,
            'Header has expected stack position');

        if (top) {
            const [min, nat_] = top.get_preferred_height(childWidth);
            // The first message is always fully shown
            box.y2 = box.y1 + min;
            top.allocate(box);
        }

        stack.forEach(child => {
            const [min, nat_] = child.get_preferred_height(childWidth);

            // Reduce width of children when collapsed
            const widthOffset = (1.0 - this._expansion) * WIDTH_OFFSET_STACKED;
            box.x1 += widthOffset;
            box.x2 -= widthOffset;

            // Stack children with a small reveal when collapsed
            box.y2 += heightOffset + this._expansion * (min - heightOffset);
            // Ensure messages are not placed outside the widget
            if (box.y2 > fullY2)
                box.y2 = fullY2;
            else
                heightOffset /= HEIGHT_OFFSET_REDUCTION_STACKED;
            box.y1 = box.y2 - min;

            child.allocate(box);
        });
    }
});

const MessageViewLayout = GObject.registerClass({
}, class MessageViewLayout extends Clutter.LayoutManager {
    constructor(overlay) {
        super();
        this._overlay = overlay;
    }

    vfunc_get_preferred_width(container, forHeight) {
        const [min, nat] = container.get_children().reduce((acc, child) => {
            const [minChild, natChild] = child.get_preferred_width(forHeight);

            return [
                Math.max(minChild, acc[0]),
                Math.max(natChild, acc[1]),
            ];
        }, [0, 0]);

        return [
            min,
            nat,
        ];
    }

    vfunc_get_preferred_height(container, forWidth) {
        let [min, nat] = [0, 0];

        container.get_children().forEach(child => {
            const [minChild, natChild] = child.get_preferred_height(forWidth);

            min += minChild;
            nat += natChild;
        });

        return [
            min,
            nat,
        ];
    }

    vfunc_allocate(container, box) {
        if (this._overlay?.visible)
            this._overlay.allocate(box);

        const width = box.x2 - box.x1;
        // We need to use the order in messages since the children order is
        // the render order and the expanded group needs to be the top most child
        // and the overlay the child below it.
        container.messages.forEach(message => {
            const child = message.get_parent();

            const [min, _] = child.get_preferred_height(width);
            box.y2 = box.y1 + min;
            child.allocate(box);
            box.y1 = box.y2;
        });
    }
});

export const MessageView = GObject.registerClass({
    Properties: {
        'can-clear': GObject.ParamSpec.boolean(
            'can-clear', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'empty': GObject.ParamSpec.boolean(
            'empty', null, null,
            GObject.ParamFlags.READABLE,
            true),
        'expanded-group': GObject.ParamSpec.object(
            'expanded-group', null, null,
            GObject.ParamFlags.READABLE,
            Clutter.Actor),
    },
    Signals: {
        'message-focused': {param_types: [Message]},
    },
    Implements: [St.Scrollable],
}, class MessageView extends St.Viewport {
    messages = [];

    _notificationSourceToGroup = new Map();
    _nUrgent = 0;

    _playerToMessage = new Map();
    _mediaSource = new Mpris.MprisSource();

    constructor() {
        // Add an overlay that will be placed below the expanded group message
        // to block interaction with other messages.
        // Unfortunately there isn't a much better way to block
        // interaction with widgets for this use-case.
        const overlay = new Clutter.Actor({
            reactive: true,
            name: 'overlay',
            visible: false,
        });

        super({
            style_class: 'message-view',
            layout_manager: new MessageViewLayout(overlay),
            effect: new FadeEffect({name: 'highlight'}),
            x_expand: true,
            y_expand: true,
        });

        this._overlay = overlay;
        this.add_child(this._overlay);

        this._setupMpris();
        this._setupNotifications();
    }

    get empty() {
        return this.messages.length === 0;
    }

    get canClear() {
        return this.messages.some(msg => msg.canClose());
    }

    _onKeyFocusIn(messageActor) {
        this.emit('message-focused', messageActor);
    }

    vfunc_get_focus_chain() {
        if (this.expandedGroup)
            return [this.expandedGroup];
        else
            return this.messages.filter(m => m.visible).map(m => m.get_parent());
    }

    _addMessageAtIndex(message, index) {
        if (this.messages.includes(message))
            throw new Error('Message was already added previously');

        const wasEmpty = this.empty;
        const couldClear = this.canClear;

        const item = new St.Bin({
            child: message,
            canFocus: false,
            layout_manager: new ScaleLayout(),
            pivot_point: new Graphene.Point({x: .5, y: .5}),
            scale_x: 0,
            scale_y: 0,
        });

        message.connect('key-focus-in', this._onKeyFocusIn.bind(this));
        // Make sure that the messages array is updated even when
        // _removeMessage() isn't called.
        message.connect('destroy', () => {
            const indexLocal = this.messages.indexOf(message);
            if (indexLocal >= 0)
                this.messages.splice(indexLocal, 1);
        });

        this.add_child(item);
        this.messages.splice(index, 0, message);

        if (wasEmpty !== this.empty)
            this.notify('empty');

        if (couldClear !== this.canClear)
            this.notify('can-clear');

        item.ease({
            scale_x: 1,
            scale_y: 1,
            duration: MESSAGE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _moveMessage(message, index) {
        if (!this.messages.includes(message))
            throw new Error('Impossible to move untracked message');

        if (this.messages[index] === message)
            return;

        const item = message.get_parent();

        item.ease({
            scale_x: 0,
            scale_y: 0,
            duration: MESSAGE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this.messages.splice(this.messages.indexOf(message), 1);
                this.messages.splice(index, 0, message);
                this.queue_relayout();
                item.ease({
                    scale_x: 1,
                    scale_y: 1,
                    duration: MESSAGE_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                });
            },
        });
    }

    _removeMessage(message) {
        const messages = this.messages;

        if (!messages.includes(message))
            throw new Error('Impossible to remove untracked message');

        const item = message.get_parent();

        item.ease({
            scale_x: 0,
            scale_y: 0,
            duration: MESSAGE_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                const wasEmpty = this.empty;
                const couldClear = this.canClear;
                const index = this.messages.indexOf(message);
                if (message.has_key_focus()) {
                    const nextMessage =
                        this.messages[index + 1] ||
                        this.messages[index - 1] ||
                        this;
                    nextMessage?.grab_key_focus();
                }

                // The message is removed from the messages array in the
                // destroy signal handler
                item.destroy();

                if (wasEmpty !== this.empty)
                    this.notify('empty');

                if (couldClear !== this.canClear)
                    this.notify('can-clear');
            },
        });
    }

    clear() {
        const messages = this.messages.filter(msg => msg.canClose());

        // If there are few messages, letting them all zoom out looks OK
        if (messages.length < 2) {
            messages.forEach(message => {
                message.close();
            });
        } else {
            // Otherwise we slide them out one by one, and then zoom them
            // out "off-screen" in the end to smoothly shrink the parent
            const delay = MESSAGE_ANIMATION_TIME / Math.max(messages.length, 5);
            for (let i = 0; i < messages.length; i++) {
                const message = messages[i];
                message.get_parent().ease({
                    translation_x: this.width,
                    opacity: 0,
                    duration: MESSAGE_ANIMATION_TIME,
                    delay: i * delay,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    onComplete: () => message.close(),
                });
            }
        }
    }

    // When a group is expanded the user isn't allowed to scroll outside the expanded group,
    // therefore the adjustment used by the MessageView needs to be different then the external
    // adjustment used by the scrollbar and scrollview.
    vfunc_set_adjustments(hadjustment, vadjustment) {
        const internalAdjustment = new St.Adjustment({actor: vadjustment.actor});

        this._scrollViewAdjustment = vadjustment;
        this._adjValueOffset = 0;

        this._adjBinding = new GObject.BindingGroup();
        this._adjBinding.bind('lower',
            this._scrollViewAdjustment,
            'lower',
            GObject.BindingFlags.SYNC_CREATE);
        this._adjBinding.bind('upper',
            this._scrollViewAdjustment,
            'upper', GObject.BindingFlags.SYNC_CREATE);
        this._adjBinding.bind('step-increment',
            this._scrollViewAdjustment, 'step-increment',
            GObject.BindingFlags.SYNC_CREATE);
        this._adjBinding.bind('page-increment',
            this._scrollViewAdjustment, 'page-increment',
            GObject.BindingFlags.SYNC_CREATE);
        this._adjBinding.bind('page-size',
            this._scrollViewAdjustment, 'page-size',
            GObject.BindingFlags.SYNC_CREATE);

        internalAdjustment.bind_property_full('value',
            this._scrollViewAdjustment,
            'value',
            GObject.BindingFlags.BIDIRECTIONAL,
            (bind, value) => {
                return [true, value - this._adjValueOffset];
            },
            (bind, value) => {
                return [true, value + this._adjValueOffset];
            });

        super.vfunc_set_adjustments(hadjustment, internalAdjustment);
    }

    vfunc_allocate(box) {
        const group = this.expandedGroup;

        this.vadjustment.freeze_notify();

        const prevUpper = this.vadjustment.upper;
        const prevAdjValueOffset = this._adjValueOffset;

        super.vfunc_allocate(box);

        if (group) {
            // Decouple the internal adjustment from the external when there is an expanded group
            this._adjBinding.set_source(null);

            const pageHeight = this.vadjustment.pageSize;
            const position = group.apply_relative_transform_to_point(this, new Graphene.Point3D());
            const [groupHeight] = group.get_preferred_height(box.x2 - box.x1);

            this._adjValueOffset = Math.max(0, position.y - EXPANDED_GROUP_OVERSHOT_HEIGHT);

            // Limit the area the user can scroll to the expanded group with some extra space
            this._scrollViewAdjustment.freeze_notify();
            this._scrollViewAdjustment.upper =
                Math.max(groupHeight + EXPANDED_GROUP_OVERSHOT_HEIGHT * 2, pageHeight);
            this._scrollViewAdjustment.stepIncrement = pageHeight / 6;
            this._scrollViewAdjustment.pageIncrement = pageHeight - pageHeight / 6;
            this._scrollViewAdjustment.pageSize = pageHeight;
            this._scrollViewAdjustment.thaw_notify();

            // Adjust the value when new messages are added before the expanded group
            if (this._adjValueOffset > prevAdjValueOffset) {
                const offset = this.vadjustment.upper - prevUpper;
                if (offset > 0)
                    this.vadjustment.value += offset;
            }
        } else if (this._adjBinding.source === null) {
            this._adjValueOffset = 0;
            this._adjBinding.set_source(this.vadjustment);
            // We need to notify the 'value' property since it indirectly changed
            this.vadjustment.notify('value');
        }

        this.vadjustment.thaw_notify();
    }

    vfunc_style_changed() {
        // This widget doesn't use the normal st scroll view fade effect because
        // highlighting groups needs more control over the fade.
        const fadeOffset = this.get_theme_node().get_length('-st-vfade-offset');
        this.get_effect('highlight').fadeMargin = fadeOffset;

        super.vfunc_style_changed();
    }

    _setupMpris() {
        this._mediaSource.connectObject(
            'player-added', (_, player) => this._addPlayer(player),
            'player-removed', (_, player) => this._removePlayer(player),
            this);

        this._mediaSource.players.forEach(player => {
            this._addPlayer(player);
        });
    }

    _addPlayer(player) {
        const message = new MediaMessage(player);
        this._playerToMessage.set(player, message);
        this._addMessageAtIndex(message, 0);
    }

    _removePlayer(player) {
        const message = this._playerToMessage.get(player);
        this._removeMessage(message);
        this._playerToMessage.delete(player);
    }

    _setupNotifications() {
        Main.messageTray.connectObject(
            'source-added', (_, source) => this._addNotificationSource(source),
            'source-removed', (_, source) => this._removeNotificationSource(source),
            this);

        Main.messageTray.getSources().forEach(source => {
            this._addNotificationSource(source);
        });
    }

    _addNotificationSource(source) {
        const group = new NotificationMessageGroup(source);

        this._notificationSourceToGroup.set(source, group);

        group.connectObject(
            'notify::focus-child', () => this._onKeyFocusIn(group.focusChild),
            'expand-toggle-requested', () => {
                if (group.expanded)
                    this._setExpandedGroup(null).catch(logError);
                else
                    this._setExpandedGroup(group).catch(logError);
            },
            'notify::has-urgent', () => {
                if (group.hasUrgent)
                    this._nUrgent++;
                else
                    this._nUrgent--;

                const index = this._playerToMessage.size + (group.hasUrgent ? 0 : this._nUrgent);
                this._moveMessage(group, index);
            },
            'notification-added', () => {
                const index = this._playerToMessage.size + (group.hasUrgent ? 0 : this._nUrgent);
                this._moveMessage(group, index);
            }, this);

        if (group.hasUrgent)
            this._nUrgent++;

        const index = this._playerToMessage.size + (group.hasUrgent ? 0 : this._nUrgent);
        this._addMessageAtIndex(group, index);
    }

    _removeNotificationSource(source) {
        const group = this._notificationSourceToGroup.get(source);

        this._removeMessage(group);

        if (group.hasUrgent)
            this._nUrgent--;

        this._notificationSourceToGroup.delete(source);
    }

    // Try to center the expanded group in the available space
    _scrollToExpandedGroup() {
        if (!this._expandedGroup)
            return;

        const group = this._expandedGroup;
        const groupExpandedHeight = group.expandedHeight;
        const position = group.apply_relative_transform_to_point(this, new Graphene.Point3D());
        const groupCenter = position.y + (groupExpandedHeight / 2);
        const pageHeight = this.vadjustment.pageSize;
        const pageCenter = pageHeight / 2;
        const value = Math.min(position.y, groupCenter - pageCenter);

        this.vadjustment.ease(value, {
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: GROUP_EXPENSION_TIME,
        });
    }

    get expandedGroup() {
        return this._expandedGroup;
    }

    async _setExpandedGroup(group) {
        const prevGroup = this._expandedGroup;

        if (prevGroup === group)
            return;

        this._expandedGroup = group;
        this.notify('expanded-group');

        // Collapse the previously expanded group
        if (prevGroup) {
            this._unhighlightGroup(prevGroup);
            await prevGroup.collapse();
        }

        if (group) {
            // Make sure that the overlay is the child below the expanded group
            this.set_child_above_sibling(group.get_parent(), null);
            this.set_child_below_sibling(this._overlay, group.get_parent());
            this._overlay.show();
            this._scrollToExpandedGroup();

            this._highlightGroup(group);
            await group.expand();
        } else {
            this._overlay.hide();
        }
    }

    // Collapse expanded notification group
    collapse() {
        this._setExpandedGroup(null).catch(logError);
    }

    _highlightGroup(group) {
        const effect = this.get_effect('highlight');

        effect.opacity = 0.0;
        effect.highlightActor = group;
        this.ease_property('@effects.highlight.opacity', 1.0, {
            progress_mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: MESSAGE_ANIMATION_TIME,
        });
    }

    _unhighlightGroup() {
        this.ease_property('@effects.highlight.opacity', 0.0, {
            progress_mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: MESSAGE_ANIMATION_TIME,
            onStopped: () => {
                const effect = this.get_effect('highlight');
                effect.highlightActor = null;
            },
        });
    }
});

const FadeEffect = GObject.registerClass({
    Properties: {
        'fade-margin': GObject.ParamSpec.float(
            'fade-margin', null, null,
            GObject.ParamFlags.READWRITE,
            0, Infinity, 0),
        'opacity': GObject.ParamSpec.float(
            'opacity', null, null,
            GObject.ParamFlags.READWRITE,
            0, 1, 0),
        'highlight-actor': GObject.ParamSpec.object(
            'highlight-actor', null, null,
            GObject.ParamFlags.READWRITE,
            Clutter.Actor),
    },
}, class FadeEffect extends Shell.GLSLEffect {
    constructor(params) {
        super(params);

        this._heightLocation = this.get_uniform_location('height');
        this._topFadePositionLocation = this.get_uniform_location('top_fade_position');
        this._bottomFadePositionLocation = this.get_uniform_location('bottom_fade_position');
        this._opacityLocation = this.get_uniform_location('opacity');
        this._topEdgeFadeLocation = this.get_uniform_location('top_edge_fade');
        this._bottomEdgeFadeLocation = this.get_uniform_location('bottom_edge_fade');
    }

    _updateEnabled() {
        if (!this._vadjustment) {
            this.enabled = false;
            return;
        }

        const {upper, pageSize} = this._vadjustment;

        this.enabled = (upper > pageSize && this._fadeMargin > 0.0) || this._highlightActor;
    }

    get highlightActor() {
        return this._highlightActor;
    }

    set highlightActor(actor) {
        if (this._highlightActor === actor)
            return;

        this._highlightActor = actor;
        this.queue_repaint();

        this._updateEnabled();
        this.notify('highlight-actor');
    }

    get opacity() {
        return this._opacity;
    }

    set opacity(opacity) {
        if (this._opacity === opacity)
            return;

        this._opacity = opacity;
        this.set_uniform_float(this._opacityLocation, 1, [opacity]);
        this.queue_repaint();

        this.notify('opacity');
    }

    get fadeMargin() {
        return this._fadeMargin;
    }

    set fadeMargin(fadeMargin) {
        if (this._fadeMargin === fadeMargin)
            return;

        this._fadeMargin = fadeMargin;
        this.queue_repaint();

        this.notify('fade-margin');
    }

    _vadjustmentChanged() {
        const newAdj = this.actor.vadjustment;
        if (this._vadjustment === newAdj)
            return;

        this._vadjustment?.disconnectObject(this);
        this._vadjustment = newAdj;
        this._vadjustment?.connectObject('changed', this._updateEnabled.bind(this));
        this._updateEnabled();
    }

    vfunc_set_actor(actor) {
        if (this.actor === actor)
            return;

        this.actor?.disconnectObject(this);

        actor?.connectObject('notify::vadjustment', this._vadjustmentChanged.bind(this));
        super.vfunc_set_actor(actor);
        this._vadjustmentChanged();
    }

    vfunc_paint_target(node, paintContext) {
        const {pageSize, upper, value} = this._vadjustment ?? [this.actor.height, this.actor.height, 0];

        if (this._highlightActor) {
            const position = this._highlightActor.apply_relative_transform_to_point(this.actor, new Graphene.Point3D());

            this.set_uniform_float(this._topFadePositionLocation, 1, [position.y - value]);
            this.set_uniform_float(this._bottomFadePositionLocation, 1, [(position.y + this._highlightActor.height - 1) - value]);
        } else {
            this.set_uniform_float(this._topFadePositionLocation, 1, [0.0]);
            this.set_uniform_float(this._bottomFadePositionLocation, 1, [0.0]);
        }

        this.set_uniform_float(this._heightLocation, 1, [pageSize]);

        this.set_uniform_float(this._topEdgeFadeLocation, 1, [Math.min(value, this._fadeMargin)]);
        this.set_uniform_float(this._bottomEdgeFadeLocation, 1, [
            pageSize - Math.min(upper - pageSize - value, this._fadeMargin),
        ]);

        super.vfunc_paint_target(node, paintContext);
    }

    vfunc_build_pipeline() {
        const dec = `uniform sampler2D tex;                       \n
                     uniform float height;                        \n
                     uniform float opacity;                       \n
                     uniform float top_edge_fade;                 \n
                     uniform float bottom_edge_fade;              \n
                     uniform float top_fade_position;             \n
                     uniform float bottom_fade_position;          \n`;

        const src = `cogl_color_out = cogl_color_in * texture2D (tex, vec2 (cogl_tex_coord_in[0].xy));    \n
                     float fade_base = 300;                                                               \n
                     float fade_height = 400;                                                             \n
                     float y = height * cogl_tex_coord_in[0].y;                                           \n
                     float ratio = 1.0;                                                                   \n

                     if (y < top_fade_position && top_fade_position > 0.0) {                              \n
                         float edge1 = top_fade_position - fade_height;                                   \n
                         float edge2 = top_fade_position;                                                 \n
                         ratio = (smoothstep (edge1, edge2 + fade_base, y) - 1.0) * opacity + 1.0;        \n
                     } else if (y > bottom_fade_position && bottom_fade_position < height) {              \n
                         float edge1 = bottom_fade_position + fade_height;                                \n
                         float edge2 = bottom_fade_position;                                              \n
                         ratio = (smoothstep (edge1, edge2 - fade_base, y) - 1.0) * opacity + 1.0;        \n
                     }                                                                                    \n

                     if (top_edge_fade > 0.0)                                                             \n
                         ratio *= smoothstep (0.0, top_edge_fade, y);                                     \n
                     if (bottom_edge_fade > 0.0 && bottom_edge_fade < height)                             \n
                         ratio *= smoothstep (height, bottom_edge_fade, y);                               \n

                     cogl_color_out *= ratio;                                                             \n`;

        this.add_glsl_snippet(Cogl.SnippetHook.FRAGMENT, dec, src, true);
    }
});
