/* exported MessageListSection */
const {
    Atk, Clutter, Gio, GLib, GObject, Graphene, Meta, Pango, St,
} = imports.gi;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

const Util = imports.misc.util;

var MESSAGE_ANIMATION_TIME = 100;

var DEFAULT_EXPAND_LINES = 6;

function _fixMarkup(text, allowMarkup) {
    if (allowMarkup) {
        // Support &amp;, &quot;, &apos;, &lt; and &gt;, escape all other
        // occurrences of '&'.
        let _text = text.replace(/&(?!amp;|quot;|apos;|lt;|gt;)/g, '&amp;');

        // Support <b>, <i>, and <u>, escape anything else
        // so it displays as raw markup.
        // Ref: https://developer.gnome.org/notification-spec/#markup
        _text = _text.replace(/<(?!\/?[biu]>)/g, '&lt;');

        try {
            Pango.parse_markup(_text, -1, '');
            return _text;
        } catch (e) {}
    }

    // !allowMarkup, or invalid markup
    return GLib.markup_escape_text(text, -1);
}

var URLHighlighter = GObject.registerClass(
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
                let linkColor = color.to_string().substr(0, 7);
                if (linkColor != this._linkColor) {
                    this._linkColor = linkColor;
                    this._highlightUrls();
                }
            }
        });
        this.clutter_text.line_wrap = lineWrap;
        this.clutter_text.line_wrap_mode = Pango.WrapMode.WORD_CHAR;

        this.setMarkup(text, allowMarkup);
    }

    vfunc_button_press_event(buttonEvent) {
        // Don't try to URL highlight when invisible.
        // The MessageTray doesn't actually hide us, so
        // we need to check for paint opacities as well.
        if (!this.visible || this.get_paint_opacity() == 0)
            return Clutter.EVENT_PROPAGATE;

        // Keep Notification from seeing this and taking
        // a pointer grab, which would block our button-release-event
        // handler, if an URL is clicked
        return this._findUrlAtPos(buttonEvent) != -1;
    }

    vfunc_button_release_event(buttonEvent) {
        if (!this.visible || this.get_paint_opacity() == 0)
            return Clutter.EVENT_PROPAGATE;

        let urlId = this._findUrlAtPos(buttonEvent);
        if (urlId != -1) {
            let url = this._urls[urlId].url;
            if (!url.includes(':'))
                url = `http://${url}`;

            Gio.app_info_launch_default_for_uri(
                url, global.create_app_launch_context(0, -1));
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_motion_event(motionEvent) {
        if (!this.visible || this.get_paint_opacity() == 0)
            return Clutter.EVENT_PROPAGATE;

        let urlId = this._findUrlAtPos(motionEvent);
        if (urlId != -1 && !this._cursorChanged) {
            global.display.set_cursor(Meta.Cursor.POINTING_HAND);
            this._cursorChanged = true;
        } else if (urlId == -1) {
            global.display.set_cursor(Meta.Cursor.DEFAULT);
            this._cursorChanged = false;
        }
        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_leave_event(crossingEvent) {
        if (!this.visible || this.get_paint_opacity() == 0)
            return Clutter.EVENT_PROPAGATE;

        if (this._cursorChanged) {
            this._cursorChanged = false;
            global.display.set_cursor(Meta.Cursor.DEFAULT);
        }
        return super.vfunc_leave_event(crossingEvent);
    }

    setMarkup(text, allowMarkup) {
        text = text ? _fixMarkup(text, allowMarkup) : '';
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
            let str = this._text.substr(pos, url.pos - pos);
            markup += `${str}<span foreground="${this._linkColor}"><u>${url.url}</u></span>`;
            pos = url.pos + url.url.length;
        }
        markup += this._text.substr(pos);
        this.clutter_text.set_markup(markup);
    }

    _findUrlAtPos(event) {
        let { x, y } = event;
        [, x, y] = this.transform_stage_point(x, y);
        let findPos = -1;
        for (let i = 0; i < this.clutter_text.text.length; i++) {
            let [, px, py, lineHeight] = this.clutter_text.position_to_coords(i);
            if (py > y || py + lineHeight < y || x < px)
                continue;
            findPos = i;
        }
        if (findPos != -1) {
            for (let i = 0; i < this._urls.length; i++) {
                if (findPos >= this._urls[i].pos &&
                    this._urls[i].pos + this._urls[i].url.length > findPos)
                    return i;
            }
        }
        return -1;
    }
});

var ScaleLayout = GObject.registerClass(
class ScaleLayout extends Clutter.BinLayout {
    _init(params) {
        this._container = null;
        super._init(params);
    }

    _connectContainer(container) {
        if (this._container == container)
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

var LabelExpanderLayout = GObject.registerClass({
    Properties: {
        'expansion': GObject.ParamSpec.double('expansion',
                                              'Expansion',
                                              'Expansion of the layout, between 0 (collapsed) ' +
                                              'and 1 (fully expanded',
                                              GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
                                              0, 1, 0),
    },
}, class LabelExpanderLayout extends Clutter.LayoutManager {
    _init(params) {
        this._expansion = 0;
        this._expandLines = DEFAULT_EXPAND_LINES;

        super._init(params);
    }

    get expansion() {
        return this._expansion;
    }

    set expansion(v) {
        if (v == this._expansion)
            return;
        this._expansion = v;
        this.notify('expansion');

        let visibleIndex = this._expansion > 0 ? 1 : 0;
        for (let i = 0; this._container && i < this._container.get_n_children(); i++)
            this._container.get_child_at_index(i).visible = i == visibleIndex;

        this.layout_changed();
    }

    set expandLines(v) {
        if (v == this._expandLines)
            return;
        this._expandLines = v;
        if (this._expansion > 0)
            this.layout_changed();
    }

    vfunc_set_container(container) {
        this._container = container;
    }

    vfunc_get_preferred_width(container, forHeight) {
        let [min, nat] = [0, 0];

        for (let i = 0; i < container.get_n_children(); i++) {
            if (i > 1)
                break; // we support one unexpanded + one expanded child

            let child = container.get_child_at_index(i);
            let [childMin, childNat] = child.get_preferred_width(forHeight);
            [min, nat] = [Math.max(min, childMin), Math.max(nat, childNat)];
        }

        return [min, nat];
    }

    vfunc_get_preferred_height(container, forWidth) {
        let [min, nat] = [0, 0];

        let children = container.get_children();
        if (children[0])
            [min, nat] = children[0].get_preferred_height(forWidth);

        if (children[1]) {
            let [min2, nat2] = children[1].get_preferred_height(forWidth);
            const [expMin, expNat] = [
                Math.min(min2, min * this._expandLines),
                Math.min(nat2, nat * this._expandLines),
            ];
            [min, nat] = [
                min + this._expansion * (expMin - min),
                nat + this._expansion * (expNat - nat),
            ];
        }

        return [min, nat];
    }

    vfunc_allocate(container, box) {
        for (let i = 0; i < container.get_n_children(); i++) {
            let child = container.get_child_at_index(i);

            if (child.visible)
                child.allocate(box);
        }
    }
});


var Message = GObject.registerClass({
    Signals: {
        'close': {},
        'expanded': {},
        'unexpanded': {},
    },
}, class Message extends St.Button {
    _init(title, body) {
        super._init({
            style_class: 'message',
            accessible_role: Atk.Role.NOTIFICATION,
            can_focus: true,
            x_expand: true,
            y_expand: true,
        });

        this.expanded = false;
        this._useBodyMarkup = false;

        let vbox = new St.BoxLayout({
            vertical: true,
            x_expand: true,
        });
        this.set_child(vbox);

        let hbox = new St.BoxLayout();
        vbox.add_actor(hbox);

        this._actionBin = new St.Widget({
            layout_manager: new ScaleLayout(),
            visible: false,
        });
        vbox.add_actor(this._actionBin);

        this._iconBin = new St.Bin({
            style_class: 'message-icon-bin',
            y_expand: true,
            y_align: Clutter.ActorAlign.START,
            visible: false,
        });
        hbox.add_actor(this._iconBin);

        const contentBox = new St.BoxLayout({
            style_class: 'message-content',
            vertical: true,
            x_expand: true,
        });
        hbox.add_actor(contentBox);

        this._mediaControls = new St.BoxLayout();
        hbox.add_actor(this._mediaControls);

        let titleBox = new St.BoxLayout();
        contentBox.add_actor(titleBox);

        this.titleLabel = new St.Label({ style_class: 'message-title' });
        this.setTitle(title);
        titleBox.add_actor(this.titleLabel);

        this._secondaryBin = new St.Bin({
            style_class: 'message-secondary-bin',
            x_expand: true, y_expand: true,
        });
        titleBox.add_actor(this._secondaryBin);

        this._closeButton = new St.Button({
            style_class: 'message-close-button',
            icon_name: 'window-close-symbolic',
            y_align: Clutter.ActorAlign.CENTER,
            opacity: 0,
        });
        titleBox.add_actor(this._closeButton);

        this._bodyStack = new St.Widget({ x_expand: true });
        this._bodyStack.layout_manager = new LabelExpanderLayout();
        contentBox.add_actor(this._bodyStack);

        this.bodyLabel = new URLHighlighter('', false, this._useBodyMarkup);
        this.bodyLabel.add_style_class_name('message-body');
        this._bodyStack.add_actor(this.bodyLabel);
        this.setBody(body);

        this._closeButton.connect('clicked', this.close.bind(this));
        let actorHoverId = this.connect('notify::hover', this._sync.bind(this));
        this._closeButton.connect('destroy', this.disconnect.bind(this, actorHoverId));
        this.connect('destroy', this._onDestroy.bind(this));
        this._sync();
    }

    close() {
        this.emit('close');
    }

    setIcon(actor) {
        this._iconBin.child = actor;
        this._iconBin.visible = actor != null;
    }

    setSecondaryActor(actor) {
        this._secondaryBin.child = actor;
    }

    setTitle(text) {
        let title = text ? _fixMarkup(text.replace(/\n/g, ' '), false) : '';
        this.titleLabel.clutter_text.set_markup(title);
    }

    setBody(text) {
        this._bodyText = text;
        this.bodyLabel.setMarkup(text ? text.replace(/\n/g, ' ') : '',
                                 this._useBodyMarkup);
        if (this._expandedLabel)
            this._expandedLabel.setMarkup(text, this._useBodyMarkup);
    }

    setUseBodyMarkup(enable) {
        if (this._useBodyMarkup === enable)
            return;
        this._useBodyMarkup = enable;
        if (this.bodyLabel)
            this.setBody(this._bodyText);
    }

    setActionArea(actor) {
        if (actor == null) {
            if (this._actionBin.get_n_children() > 0)
                this._actionBin.get_child_at_index(0).destroy();
            return;
        }

        if (this._actionBin.get_n_children() > 0)
            throw new Error('Message already has an action area');

        this._actionBin.add_actor(actor);
        this._actionBin.visible = this.expanded;
    }

    addMediaControl(iconName, callback) {
        const button = new St.Button({
            style_class: 'message-media-control',
            iconName,
        });
        button.connect('clicked', callback);
        this._mediaControls.add_actor(button);
        return button;
    }

    setExpandedBody(actor) {
        if (actor == null) {
            if (this._bodyStack.get_n_children() > 1)
                this._bodyStack.get_child_at_index(1).destroy();
            return;
        }

        if (this._bodyStack.get_n_children() > 1)
            throw new Error('Message already has an expanded body actor');

        this._bodyStack.insert_child_at_index(actor, 1);
    }

    setExpandedLines(nLines) {
        this._bodyStack.layout_manager.expandLines = nLines;
    }

    expand(animate) {
        this.expanded = true;

        this._actionBin.visible = this._actionBin.get_n_children() > 0;

        if (this._bodyStack.get_n_children() < 2) {
            this._expandedLabel = new URLHighlighter(this._bodyText,
                                                     true, this._useBodyMarkup);
            this.setExpandedBody(this._expandedLabel);
        }

        if (animate) {
            this._bodyStack.ease_property('@layout.expansion', 1, {
                progress_mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                duration: MessageTray.ANIMATION_TIME,
            });

            this._actionBin.scale_y = 0;
            this._actionBin.ease({
                scale_y: 1,
                duration: MessageTray.ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        } else {
            this._bodyStack.layout_manager.expansion = 1;
            this._actionBin.scale_y = 1;
        }

        this.emit('expanded');
    }

    unexpand(animate) {
        if (animate) {
            this._bodyStack.ease_property('@layout.expansion', 0, {
                progress_mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                duration: MessageTray.ANIMATION_TIME,
            });

            this._actionBin.ease({
                scale_y: 0,
                duration: MessageTray.ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._actionBin.hide();
                    this.expanded = false;
                },
            });
        } else {
            this._bodyStack.layout_manager.expansion = 0;
            this._actionBin.scale_y = 0;
            this.expanded = false;
        }

        this.emit('unexpanded');
    }

    canClose() {
        return false;
    }

    _sync() {
        let visible = this.hover && this.canClose();
        this._closeButton.opacity = visible ? 255 : 0;
        this._closeButton.reactive = visible;
    }

    _onDestroy() {
    }

    vfunc_key_press_event(keyEvent) {
        let keysym = keyEvent.keyval;

        if (keysym == Clutter.KEY_Delete ||
            keysym == Clutter.KEY_KP_Delete) {
            if (this.canClose()) {
                this.close();
                return Clutter.EVENT_STOP;
            }
        }
        return super.vfunc_key_press_event(keyEvent);
    }
});

var MessageListSection = GObject.registerClass({
    Properties: {
        'can-clear': GObject.ParamSpec.boolean(
            'can-clear', 'can-clear', 'can-clear',
            GObject.ParamFlags.READABLE,
            false),
        'empty': GObject.ParamSpec.boolean(
            'empty', 'empty', 'empty',
            GObject.ParamFlags.READABLE,
            true),
    },
    Signals: {
        'can-clear-changed': {},
        'empty-changed': {},
        'message-focused': { param_types: [Message.$gtype] },
    },
}, class MessageListSection extends St.BoxLayout {
    _init() {
        super._init({
            style_class: 'message-list-section',
            clip_to_allocation: true,
            vertical: true,
            x_expand: true,
        });

        this._list = new St.BoxLayout({
            style_class: 'message-list-section-list',
            vertical: true,
        });
        this.add_actor(this._list);

        this._list.connect('actor-added', this._sync.bind(this));
        this._list.connect('actor-removed', this._sync.bind(this));

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
            pivot_point: new Graphene.Point({ x: .5, y: .5 }),
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

        if (animate) {
            listItem.set({ scale_x: 0, scale_y: 0 });
            listItem.ease({
                scale_x: 1,
                scale_y: 1,
                duration: MESSAGE_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        }
    }

    moveMessage(message, index, animate) {
        if (!this._messages.includes(message))
            throw new Error(`Impossible to move untracked message`);

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
            throw new Error(`Impossible to remove untracked message`);

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

        if (animate) {
            listItem.ease({
                scale_x: 0,
                scale_y: 0,
                duration: MESSAGE_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    listItem.destroy();
                    nextMessage?.grab_key_focus();
                },
            });
        } else {
            listItem.destroy();
            nextMessage?.grab_key_focus();
        }
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
        let empty = messages.length == 0;

        if (this._empty != empty) {
            this._empty = empty;
            this.notify('empty');
        }

        let canClear = messages.some(m => m.canClose());
        if (this._canClear != canClear) {
            this._canClear = canClear;
            this.notify('can-clear');
        }

        this.visible = this.allowed && this._shouldShow();
    }
});
