// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported LookingGlass */

const {
    Clutter, Cogl, Gio, GLib, GObject, Graphene, Meta, Pango, Shell, St,
} = imports.gi;
const Signals = imports.misc.signals;
const System = imports.system;

const History = imports.misc.history;
const ExtensionUtils = imports.misc.extensionUtils;
const PopupMenu = imports.ui.popupMenu;
const ShellEntry = imports.ui.shellEntry;
const Main = imports.ui.main;
const JsParse = imports.misc.jsParse;

const { ExtensionState } = ExtensionUtils;

const CHEVRON = '>>> ';

/* Imports...feel free to add here as needed */
var commandHeader = 'const { Clutter, Gio, GLib, GObject, Meta, Shell, St } = imports.gi; ' +
                    'const Main = imports.ui.main; ' +
                    /* Utility functions...we should probably be able to use these
                     * in the shell core code too. */
                    'const stage = global.stage; ' +
                    /* Special lookingGlass functions */
                    'const inspect = Main.lookingGlass.inspect.bind(Main.lookingGlass); ' +
                    'const it = Main.lookingGlass.getIt(); ' +
                    'const r = Main.lookingGlass.getResult.bind(Main.lookingGlass); ';

const HISTORY_KEY = 'looking-glass-history';
// Time between tabs for them to count as a double-tab event
var AUTO_COMPLETE_DOUBLE_TAB_DELAY = 500;
var AUTO_COMPLETE_SHOW_COMPLETION_ANIMATION_DURATION = 200;
var AUTO_COMPLETE_GLOBAL_KEYWORDS = _getAutoCompleteGlobalKeywords();

const LG_ANIMATION_TIME = 500;

const CLUTTER_DEBUG_FLAG_CATEGORIES = new Map([
    // Paint debugging can easily result in a non-responsive session
    ['DebugFlag', { argPos: 0, exclude: ['PAINT'] }],
    ['DrawDebugFlag', { argPos: 1, exclude: [] }],
    // Exluded due to the only current option likely to result in shooting ones
    // foot
    // ['PickDebugFlag', { argPos: 2, exclude: [] }],
]);

function _getAutoCompleteGlobalKeywords() {
    const keywords = ['true', 'false', 'null', 'new'];
    // Don't add the private properties of globalThis (i.e., ones starting with '_')
    const windowProperties = Object.getOwnPropertyNames(globalThis).filter(
        a => a.charAt(0) !== '_');
    const headerProperties = JsParse.getDeclaredConstants(commandHeader);

    return keywords.concat(windowProperties).concat(headerProperties);
}

var AutoComplete = class AutoComplete extends Signals.EventEmitter {
    constructor(entry) {
        super();

        this._entry = entry;
        this._entry.connect('key-press-event', this._entryKeyPressEvent.bind(this));
        this._lastTabTime = global.get_current_time();
    }

    _processCompletionRequest(event) {
        if (event.completions.length == 0)
            return;

        // Unique match = go ahead and complete; multiple matches + single tab = complete the common starting string;
        // multiple matches + double tab = emit a suggest event with all possible options
        if (event.completions.length == 1) {
            this.additionalCompletionText(event.completions[0], event.attrHead);
            this.emit('completion', { completion: event.completions[0], type: 'whole-word' });
        } else if (event.completions.length > 1 && event.tabType === 'single') {
            let commonPrefix = JsParse.getCommonPrefix(event.completions);

            if (commonPrefix.length > 0) {
                this.additionalCompletionText(commonPrefix, event.attrHead);
                this.emit('completion', { completion: commonPrefix, type: 'prefix' });
                this.emit('suggest', { completions: event.completions });
            }
        } else if (event.completions.length > 1 && event.tabType === 'double') {
            this.emit('suggest', { completions: event.completions });
        }
    }

    _entryKeyPressEvent(actor, event) {
        let cursorPos = this._entry.clutter_text.get_cursor_position();
        let text = this._entry.get_text();
        if (cursorPos != -1)
            text = text.slice(0, cursorPos);

        if (event.get_key_symbol() == Clutter.KEY_Tab) {
            let [completions, attrHead] = JsParse.getCompletions(text, commandHeader, AUTO_COMPLETE_GLOBAL_KEYWORDS);
            let currTime = global.get_current_time();
            if ((currTime - this._lastTabTime) < AUTO_COMPLETE_DOUBLE_TAB_DELAY) {
                this._processCompletionRequest({
                    tabType: 'double',
                    completions,
                    attrHead,
                });
            } else {
                this._processCompletionRequest({
                    tabType: 'single',
                    completions,
                    attrHead,
                });
            }
            this._lastTabTime = currTime;
        }
        return Clutter.EVENT_PROPAGATE;
    }

    // Insert characters of text not already included in head at cursor position.  i.e., if text="abc" and head="a",
    // the string "bc" will be appended to this._entry
    additionalCompletionText(text, head) {
        let additionalCompletionText = text.slice(head.length);
        let cursorPos = this._entry.clutter_text.get_cursor_position();

        this._entry.clutter_text.insert_text(additionalCompletionText, cursorPos);
    }
};


var Notebook = GObject.registerClass({
    Signals: { 'selection': { param_types: [Clutter.Actor.$gtype] } },
}, class Notebook extends St.BoxLayout {
    _init() {
        super._init({
            vertical: true,
            y_expand: true,
        });

        this.tabControls = new St.BoxLayout({ style_class: 'labels' });

        this._selectedIndex = -1;
        this._tabs = [];
    }

    appendPage(name, child) {
        const labelBox = new St.BoxLayout({
            style_class: 'notebook-tab',
            reactive: true,
            track_hover: true,
        });
        let label = new St.Button({ label: name });
        label.connect('clicked', () => {
            this.selectChild(child);
            return true;
        });
        labelBox.add_child(label);
        this.tabControls.add(labelBox);

        let scrollview = new St.ScrollView({ y_expand: true });
        scrollview.get_hscroll_bar().hide();
        scrollview.add_actor(child);

        const tabData = {
            child,
            labelBox,
            label,
            scrollView: scrollview,
            _scrollToBottom: false,
        };
        this._tabs.push(tabData);
        scrollview.hide();
        this.add_child(scrollview);

        let vAdjust = scrollview.vscroll.adjustment;
        vAdjust.connect('changed', () => this._onAdjustScopeChanged(tabData));
        vAdjust.connect('notify::value', () => this._onAdjustValueChanged(tabData));

        if (this._selectedIndex == -1)
            this.selectIndex(0);
    }

    _unselect() {
        if (this._selectedIndex < 0)
            return;
        let tabData = this._tabs[this._selectedIndex];
        tabData.labelBox.remove_style_pseudo_class('selected');
        tabData.scrollView.hide();
        this._selectedIndex = -1;
    }

    selectIndex(index) {
        if (index == this._selectedIndex)
            return;
        if (index < 0) {
            this._unselect();
            this.emit('selection', null);
            return;
        }

        // Focus the new tab before unmapping the old one
        let tabData = this._tabs[index];
        if (!tabData.scrollView.navigate_focus(null, St.DirectionType.TAB_FORWARD, false))
            this.grab_key_focus();

        this._unselect();

        tabData.labelBox.add_style_pseudo_class('selected');
        tabData.scrollView.show();
        this._selectedIndex = index;
        this.emit('selection', tabData.child);
    }

    selectChild(child) {
        if (child == null) {
            this.selectIndex(-1);
        } else {
            for (let i = 0; i < this._tabs.length; i++) {
                let tabData = this._tabs[i];
                if (tabData.child == child) {
                    this.selectIndex(i);
                    return;
                }
            }
        }
    }

    scrollToBottom(index) {
        let tabData = this._tabs[index];
        tabData._scrollToBottom = true;
    }

    _onAdjustValueChanged(tabData) {
        let vAdjust = tabData.scrollView.vscroll.adjustment;
        if (vAdjust.value < (vAdjust.upper - vAdjust.lower - 0.5))
            tabData._scrolltoBottom = false;
    }

    _onAdjustScopeChanged(tabData) {
        if (!tabData._scrollToBottom)
            return;
        let vAdjust = tabData.scrollView.vscroll.adjustment;
        vAdjust.value = vAdjust.upper - vAdjust.page_size;
    }

    nextTab() {
        let nextIndex = this._selectedIndex;
        if (nextIndex < this._tabs.length - 1)
            ++nextIndex;

        this.selectIndex(nextIndex);
    }

    prevTab() {
        let prevIndex = this._selectedIndex;
        if (prevIndex > 0)
            --prevIndex;

        this.selectIndex(prevIndex);
    }
});

function objectToString(o) {
    if (typeof o == typeof objectToString) {
        // special case this since the default is way, way too verbose
        return '<js function>';
    } else {
        return `${o}`;
    }
}

var ObjLink = GObject.registerClass(
class ObjLink extends St.Button {
    _init(lookingGlass, o, title) {
        let text;
        if (title)
            text = title;
        else
            text = objectToString(o);
        text = GLib.markup_escape_text(text, -1);

        super._init({
            reactive: true,
            track_hover: true,
            style_class: 'shell-link',
            label: text,
            x_align: Clutter.ActorAlign.START,
        });
        this.get_child().single_line_mode = true;

        this._obj = o;
        this._lookingGlass = lookingGlass;
    }

    vfunc_clicked() {
        this._lookingGlass.inspectObject(this._obj, this);
    }
});

var Result = GObject.registerClass(
class Result extends St.BoxLayout {
    _init(lookingGlass, command, o, index) {
        super._init({ vertical: true });

        this.index = index;
        this.o = o;

        this._lookingGlass = lookingGlass;

        let cmdTxt = new St.Label({ text: command });
        cmdTxt.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        this.add(cmdTxt);
        let box = new St.BoxLayout({});
        this.add(box);
        let resultTxt = new St.Label({ text: `r(${index}) = ` });
        resultTxt.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        box.add(resultTxt);
        let objLink = new ObjLink(this._lookingGlass, o);
        box.add(objLink);
    }
});

var WindowList = GObject.registerClass({
}, class WindowList extends St.BoxLayout {
    _init(lookingGlass) {
        super._init({ name: 'Windows', vertical: true, style: 'spacing: 8px' });
        let tracker = Shell.WindowTracker.get_default();
        this._updateId = Main.initializeDeferredWork(this, this._updateWindowList.bind(this));
        global.display.connect('window-created', this._updateWindowList.bind(this));
        tracker.connect('tracked-windows-changed', this._updateWindowList.bind(this));

        this._lookingGlass = lookingGlass;
    }

    _updateWindowList() {
        if (!this._lookingGlass.isOpen)
            return;

        this.destroy_all_children();
        let windows = global.get_window_actors();
        let tracker = Shell.WindowTracker.get_default();
        for (let i = 0; i < windows.length; i++) {
            let metaWindow = windows[i].metaWindow;
            // Avoid multiple connections
            if (!metaWindow._lookingGlassManaged) {
                metaWindow.connect('unmanaged', this._updateWindowList.bind(this));
                metaWindow._lookingGlassManaged = true;
            }
            let box = new St.BoxLayout({ vertical: true });
            this.add(box);
            let windowLink = new ObjLink(this._lookingGlass, metaWindow, metaWindow.title);
            box.add_child(windowLink);
            let propsBox = new St.BoxLayout({ vertical: true, style: 'padding-left: 6px;' });
            box.add(propsBox);
            propsBox.add(new St.Label({ text: `wmclass: ${metaWindow.get_wm_class()}` }));
            let app = tracker.get_window_app(metaWindow);
            if (app != null && !app.is_window_backed()) {
                let icon = app.create_icon_texture(22);
                let propBox = new St.BoxLayout({ style: 'spacing: 6px; ' });
                propsBox.add(propBox);
                propBox.add_child(new St.Label({ text: 'app: ' }));
                let appLink = new ObjLink(this._lookingGlass, app, app.get_id());
                propBox.add_child(appLink);
                propBox.add_child(icon);
            } else {
                propsBox.add(new St.Label({ text: '<untracked>' }));
            }
        }
    }

    update() {
        this._updateWindowList();
    }
});

var ObjInspector = GObject.registerClass(
class ObjInspector extends St.ScrollView {
    _init(lookingGlass) {
        super._init({
            pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
        });

        this._obj = null;
        this._previousObj = null;

        this._parentList = [];

        this.get_hscroll_bar().hide();
        this._container = new St.BoxLayout({
            name: 'LookingGlassPropertyInspector',
            style_class: 'lg-dialog',
            vertical: true,
            x_expand: true,
            y_expand: true,
        });
        this.add_actor(this._container);

        this._lookingGlass = lookingGlass;
    }

    selectObject(obj, skipPrevious) {
        if (!skipPrevious)
            this._previousObj = this._obj;
        else
            this._previousObj = null;
        this._obj = obj;

        this._container.destroy_all_children();

        let hbox = new St.BoxLayout({ style_class: 'lg-obj-inspector-title' });
        this._container.add_actor(hbox);
        let label = new St.Label({
            text: `Inspecting: ${typeof obj}: ${objectToString(obj)}`,
            x_expand: true,
        });
        label.single_line_mode = true;
        hbox.add_child(label);
        let button = new St.Button({ label: 'Insert', style_class: 'lg-obj-inspector-button' });
        button.connect('clicked', this._onInsert.bind(this));
        hbox.add(button);

        if (this._previousObj != null) {
            button = new St.Button({ label: 'Back', style_class: 'lg-obj-inspector-button' });
            button.connect('clicked', this._onBack.bind(this));
            hbox.add(button);
        }

        button = new St.Button({
            style_class: 'window-close',
            icon_name: 'window-close-symbolic',
        });
        button.connect('clicked', this.close.bind(this));
        hbox.add(button);
        if (typeof obj == typeof {}) {
            let properties = [];
            for (let propName in obj)
                properties.push(propName);
            properties.sort();

            for (let i = 0; i < properties.length; i++) {
                let propName = properties[i];
                let link;
                try {
                    let prop = obj[propName];
                    link = new ObjLink(this._lookingGlass, prop);
                } catch (e) {
                    link = new St.Label({ text: '<error>' });
                }
                let box = new St.BoxLayout();
                box.add(new St.Label({ text: `${propName}: ` }));
                box.add(link);
                this._container.add_actor(box);
            }
        }
    }

    open(sourceActor) {
        if (this._open)
            return;

        const grab = Main.pushModal(this, { actionMode: Shell.ActionMode.LOOKING_GLASS });
        if (grab.get_seat_state() !== Clutter.GrabState.ALL) {
            Main.popModal(grab);
            return;
        }

        this._grab = grab;
        this._previousObj = null;
        this._open = true;
        this.show();
        if (sourceActor) {
            this.set_scale(0, 0);
            this.ease({
                scale_x: 1,
                scale_y: 1,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                duration: 200,
            });
        } else {
            this.set_scale(1, 1);
        }
    }

    close() {
        if (!this._open)
            return;
        Main.popModal(this._grab);
        this._grab = null;
        this._open = false;
        this.hide();
        this._previousObj = null;
        this._obj = null;
    }

    vfunc_key_press_event(keyPressEvent) {
        const symbol = keyPressEvent.keyval;
        if (symbol === Clutter.KEY_Escape) {
            this.close();
            return Clutter.EVENT_STOP;
        }
        return super.vfunc_key_press_event(keyPressEvent);
    }

    _onInsert() {
        let obj = this._obj;
        this.close();
        this._lookingGlass.insertObject(obj);
    }

    _onBack() {
        this.selectObject(this._previousObj, true);
    }
});

var RedBorderEffect = GObject.registerClass(
class RedBorderEffect extends Clutter.Effect {
    _init() {
        super._init();
        this._pipeline = null;
    }

    vfunc_paint_node(node, paintContext) {
        let actor = this.get_actor();

        const actorNode = new Clutter.ActorNode(actor, -1);
        node.add_child(actorNode);

        if (!this._pipeline) {
            const framebuffer = paintContext.get_framebuffer();
            const coglContext = framebuffer.get_context();

            let color = new Cogl.Color();
            color.init_from_4ub(0xff, 0, 0, 0xc4);

            this._pipeline = new Cogl.Pipeline(coglContext);
            this._pipeline.set_color(color);
        }

        let alloc = actor.get_allocation_box();
        let width = 2;

        const pipelineNode = new Clutter.PipelineNode(this._pipeline);
        pipelineNode.set_name('Red Border');
        node.add_child(pipelineNode);

        const box = new Clutter.ActorBox();

        // clockwise order
        box.set_origin(0, 0);
        box.set_size(alloc.get_width(), width);
        pipelineNode.add_rectangle(box);

        box.set_origin(alloc.get_width() - width, width);
        box.set_size(width, alloc.get_height() - width);
        pipelineNode.add_rectangle(box);

        box.set_origin(0, alloc.get_height() - width);
        box.set_size(alloc.get_width() - width, width);
        pipelineNode.add_rectangle(box);

        box.set_origin(0, width);
        box.set_size(width, alloc.get_height() - width * 2);
        pipelineNode.add_rectangle(box);
    }
});

var Inspector = GObject.registerClass({
    Signals: {
        'closed': {},
        'target': { param_types: [Clutter.Actor.$gtype, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE] },
    },
}, class Inspector extends Clutter.Actor {
    _init(lookingGlass) {
        super._init({ width: 0, height: 0 });

        Main.uiGroup.add_actor(this);

        const eventHandler = new St.BoxLayout({
            name: 'LookingGlassDialog',
            vertical: false,
            reactive: true,
        });
        this._eventHandler = eventHandler;
        this.add_actor(eventHandler);
        this._displayText = new St.Label({ x_expand: true });
        eventHandler.add_child(this._displayText);

        eventHandler.connect('key-press-event', this._onKeyPressEvent.bind(this));
        eventHandler.connect('button-press-event', this._onButtonPressEvent.bind(this));
        eventHandler.connect('scroll-event', this._onScrollEvent.bind(this));
        eventHandler.connect('motion-event', this._onMotionEvent.bind(this));

        this._grab = global.stage.grab(eventHandler);

        // this._target is the actor currently shown by the inspector.
        // this._pointerTarget is the actor directly under the pointer.
        // Normally these are the same, but if you use the scroll wheel
        // to drill down, they'll diverge until you either scroll back
        // out, or move the pointer outside of _pointerTarget.
        this._target = null;
        this._pointerTarget = null;

        this._lookingGlass = lookingGlass;
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        if (!this._eventHandler)
            return;

        let primary = Main.layoutManager.primaryMonitor;

        let [, , natWidth, natHeight] =
            this._eventHandler.get_preferred_size();

        let childBox = new Clutter.ActorBox();
        childBox.x1 = primary.x + Math.floor((primary.width - natWidth) / 2);
        childBox.x2 = childBox.x1 + natWidth;
        childBox.y1 = primary.y + Math.floor((primary.height - natHeight) / 2);
        childBox.y2 = childBox.y1 + natHeight;
        this._eventHandler.allocate(childBox);
    }

    _close() {
        if (this._grab) {
            this._grab.dismiss();
            this._grab = null;
        }
        this._eventHandler.destroy();
        this._eventHandler = null;
        this.emit('closed');
    }

    _onKeyPressEvent(actor, event) {
        if (event.get_key_symbol() === Clutter.KEY_Escape)
            this._close();
        return Clutter.EVENT_STOP;
    }

    _onButtonPressEvent(actor, event) {
        if (this._target) {
            let [stageX, stageY] = event.get_coords();
            this.emit('target', this._target, stageX, stageY);
        }
        this._close();
        return Clutter.EVENT_STOP;
    }

    _onScrollEvent(actor, event) {
        switch (event.get_scroll_direction()) {
        case Clutter.ScrollDirection.UP: {
            // select parent
            let parent = this._target.get_parent();
            if (parent != null) {
                this._target = parent;
                this._update(event);
            }
            break;
        }

        case Clutter.ScrollDirection.DOWN:
            // select child
            if (this._target != this._pointerTarget) {
                let child = this._pointerTarget;
                while (child) {
                    let parent = child.get_parent();
                    if (parent == this._target)
                        break;
                    child = parent;
                }
                if (child) {
                    this._target = child;
                    this._update(event);
                }
            }
            break;

        default:
            break;
        }
        return Clutter.EVENT_STOP;
    }

    _onMotionEvent(actor, event) {
        this._update(event);
        return Clutter.EVENT_STOP;
    }

    _update(event) {
        let [stageX, stageY] = event.get_coords();
        let target = global.stage.get_actor_at_pos(Clutter.PickMode.ALL,
                                                   stageX,
                                                   stageY);

        if (target != this._pointerTarget)
            this._target = target;
        this._pointerTarget = target;

        let position = `[inspect x: ${stageX} y: ${stageY}]`;
        this._displayText.text = '';
        this._displayText.text = `${position} ${this._target}`;

        this._lookingGlass.setBorderPaintTarget(this._target);
    }
});

var Extensions = GObject.registerClass({
}, class Extensions extends St.BoxLayout {
    _init(lookingGlass) {
        super._init({ vertical: true, name: 'lookingGlassExtensions' });

        this._lookingGlass = lookingGlass;
        this._noExtensions = new St.Label({
            style_class: 'lg-extensions-none',
            text: _('No extensions installed'),
        });
        this._numExtensions = 0;
        this._extensionsList = new St.BoxLayout({
            vertical: true,
            style_class: 'lg-extensions-list',
        });
        this._extensionsList.add(this._noExtensions);
        this.add(this._extensionsList);

        Main.extensionManager.getUuids().forEach(uuid => {
            this._loadExtension(null, uuid);
        });

        Main.extensionManager.connect('extension-loaded',
                                      this._loadExtension.bind(this));
    }

    _loadExtension(o, uuid) {
        let extension = Main.extensionManager.lookup(uuid);
        // There can be cases where we create dummy extension metadata
        // that's not really a proper extension. Don't bother with these.
        if (!extension.metadata.name)
            return;

        let extensionDisplay = this._createExtensionDisplay(extension);
        if (this._numExtensions == 0)
            this._extensionsList.remove_actor(this._noExtensions);

        this._numExtensions++;
        const { name } = extension.metadata;
        const pos = [...this._extensionsList].findIndex(
            dsp => dsp._extension.metadata.name.localeCompare(name) > 0);
        this._extensionsList.insert_child_at_index(extensionDisplay, pos);
    }

    _onViewSource(actor) {
        let extension = actor._extension;
        let uri = extension.dir.get_uri();
        Gio.app_info_launch_default_for_uri(uri, global.create_app_launch_context(0, -1));
        this._lookingGlass.close();
    }

    _onWebPage(actor) {
        let extension = actor._extension;
        Gio.app_info_launch_default_for_uri(extension.metadata.url, global.create_app_launch_context(0, -1));
        this._lookingGlass.close();
    }

    _onViewErrors(actor) {
        let extension = actor._extension;
        let shouldShow = !actor._isShowing;

        if (shouldShow) {
            let errors = extension.errors;
            let errorDisplay = new St.BoxLayout({ vertical: true });
            if (errors && errors.length) {
                for (let i = 0; i < errors.length; i++)
                    errorDisplay.add(new St.Label({ text: errors[i] }));
            } else {
                /* Translators: argument is an extension UUID. */
                let message = _("%s has not emitted any errors.").format(extension.uuid);
                errorDisplay.add(new St.Label({ text: message }));
            }

            actor._errorDisplay = errorDisplay;
            actor._parentBox.add(errorDisplay);
            actor.label = _("Hide Errors");
        } else {
            actor._errorDisplay.destroy();
            actor._errorDisplay = null;
            actor.label = _("Show Errors");
        }

        actor._isShowing = shouldShow;
    }

    _stateToString(extensionState) {
        switch (extensionState) {
        case ExtensionState.ENABLED:
            return _('Enabled');
        case ExtensionState.DISABLED:
        case ExtensionState.INITIALIZED:
            return _('Disabled');
        case ExtensionState.ERROR:
            return _('Error');
        case ExtensionState.OUT_OF_DATE:
            return _('Out of date');
        case ExtensionState.DOWNLOADING:
            return _('Downloading');
        case ExtensionState.DISABLING:
            return _('Disabling');
        case ExtensionState.ENABLING:
            return _('Enabling');
        }
        return 'Unknown'; // Not translated, shouldn't appear
    }

    _createExtensionDisplay(extension) {
        let box = new St.BoxLayout({ style_class: 'lg-extension', vertical: true });
        box._extension = extension;
        let name = new St.Label({
            style_class: 'lg-extension-name',
            text: extension.metadata.name,
            x_expand: true,
        });
        box.add_child(name);
        let description = new St.Label({
            style_class: 'lg-extension-description',
            text: extension.metadata.description || 'No description',
            x_expand: true,
        });
        box.add_child(description);

        let metaBox = new St.BoxLayout({ style_class: 'lg-extension-meta' });
        box.add(metaBox);
        const state = new St.Label({
            style_class: 'lg-extension-state',
            text: this._stateToString(extension.state),
        });
        metaBox.add(state);

        const viewsource = new St.Button({
            reactive: true,
            track_hover: true,
            style_class: 'shell-link',
            label: _('View Source'),
        });
        viewsource._extension = extension;
        viewsource.connect('clicked', this._onViewSource.bind(this));
        metaBox.add(viewsource);

        if (extension.metadata.url) {
            const webpage = new St.Button({
                reactive: true,
                track_hover: true,
                style_class: 'shell-link',
                label: _('Web Page'),
            });
            webpage._extension = extension;
            webpage.connect('clicked', this._onWebPage.bind(this));
            metaBox.add(webpage);
        }

        const viewerrors = new St.Button({
            reactive: true,
            track_hover: true,
            style_class: 'shell-link',
            label: _('Show Errors'),
        });
        viewerrors._extension = extension;
        viewerrors._parentBox = box;
        viewerrors._isShowing = false;
        viewerrors.connect('clicked', this._onViewErrors.bind(this));
        metaBox.add(viewerrors);

        return box;
    }
});


var ActorLink = GObject.registerClass({
    Signals: {
        'inspect-actor': {},
    },
}, class ActorLink extends St.Button {
    _init(actor) {
        this._arrow = new St.Icon({
            icon_name: 'pan-end-symbolic',
            icon_size: 8,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
            pivot_point: new Graphene.Point({ x: 0.5, y: 0.5 }),
        });

        const label = new St.Label({
            text: actor.toString(),
            x_align: Clutter.ActorAlign.START,
        });

        const inspectButton = new St.Button({
            icon_name: 'insert-object-symbolic',
            reactive: true,
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.CENTER,
        });
        inspectButton.connect('clicked', () => this.emit('inspect-actor'));

        const box = new St.BoxLayout();
        box.add_child(this._arrow);
        box.add_child(label);
        box.add_child(inspectButton);

        super._init({
            reactive: true,
            track_hover: true,
            toggle_mode: true,
            style_class: 'actor-link',
            child: box,
            x_align: Clutter.ActorAlign.START,
        });

        this._actor = actor;
    }

    vfunc_clicked() {
        this._arrow.ease({
            rotation_angle_z: this.checked ? 90 : 0,
            duration: 250,
        });
    }
});

var ActorTreeViewer = GObject.registerClass(
class ActorTreeViewer extends St.BoxLayout {
    _init(lookingGlass) {
        super._init();

        this._lookingGlass = lookingGlass;
        this._actorData = new Map();
    }

    _showActorChildren(actor) {
        const data = this._actorData.get(actor);
        if (!data || data.visible)
            return;

        data.visible = true;
        data.actorAddedId = actor.connect('actor-added', (container, child) => {
            this._addActor(data.children, child);
        });
        data.actorRemovedId = actor.connect('actor-removed', (container, child) => {
            this._removeActor(child);
        });

        for (let child of actor)
            this._addActor(data.children, child);
    }

    _hideActorChildren(actor) {
        const data = this._actorData.get(actor);
        if (!data || !data.visible)
            return;

        for (let child of actor)
            this._removeActor(child);

        data.visible = false;
        if (data.actorAddedId > 0) {
            actor.disconnect(data.actorAddedId);
            data.actorAddedId = 0;
        }
        if (data.actorRemovedId > 0) {
            actor.disconnect(data.actorRemovedId);
            data.actorRemovedId = 0;
        }
        data.children.remove_all_children();
    }

    _addActor(container, actor) {
        if (this._actorData.has(actor))
            return;

        if (actor === this._lookingGlass)
            return;

        const button = new ActorLink(actor);
        button.connect('notify::checked', () => {
            this._lookingGlass.setBorderPaintTarget(actor);
            if (button.checked)
                this._showActorChildren(actor);
            else
                this._hideActorChildren(actor);
        });
        button.connect('inspect-actor', () => {
            this._lookingGlass.inspectObject(actor, button);
        });

        const mainContainer = new St.BoxLayout({ vertical: true });
        const childrenContainer = new St.BoxLayout({
            vertical: true,
            style: 'padding: 0 0 0 18px',
        });

        mainContainer.add_child(button);
        mainContainer.add_child(childrenContainer);

        this._actorData.set(actor, {
            button,
            container: mainContainer,
            children: childrenContainer,
            visible: false,
            actorAddedId: 0,
            actorRemovedId: 0,
            actorDestroyedId: actor.connect('destroy', () => this._removeActor(actor)),
        });

        let belowChild = null;
        const nextSibling = actor.get_next_sibling();
        if (nextSibling && this._actorData.has(nextSibling))
            belowChild = this._actorData.get(nextSibling).container;

        container.insert_child_above(mainContainer, belowChild);
    }

    _removeActor(actor) {
        const data = this._actorData.get(actor);
        if (!data)
            return;

        for (let child of actor)
            this._removeActor(child);

        if (data.actorAddedId > 0) {
            actor.disconnect(data.actorAddedId);
            data.actorAddedId = 0;
        }
        if (data.actorRemovedId > 0) {
            actor.disconnect(data.actorRemovedId);
            data.actorRemovedId = 0;
        }
        if (data.actorDestroyedId > 0) {
            actor.disconnect(data.actorDestroyedId);
            data.actorDestroyedId = 0;
        }
        data.container.destroy();
        this._actorData.delete(actor);
    }

    vfunc_map() {
        super.vfunc_map();
        this._addActor(this, global.stage);
    }

    vfunc_unmap() {
        super.vfunc_unmap();
        this._removeActor(global.stage);
    }
});

var DebugFlag = GObject.registerClass({
    GTypeFlags: GObject.TypeFlags.ABSTRACT,
}, class DebugFlag extends St.Button {
    _init(label) {
        const box = new St.BoxLayout();

        const flagLabel = new St.Label({
            text: label,
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(flagLabel);

        this._flagSwitch = new PopupMenu.Switch(false);
        this._stateHandler = this._flagSwitch.connect('notify::state', () => {
            if (this._flagSwitch.state)
                this._enable();
            else
                this._disable();
        });

        // Update state whenever the switch is mapped, because most debug flags
        // don't have a way of notifying us of changes.
        this._flagSwitch.connect('notify::mapped', () => {
            if (!this._flagSwitch.is_mapped())
                return;

            const state = this._isEnabled();
            if (state === this._flagSwitch.state)
                return;

            this._flagSwitch.block_signal_handler(this._stateHandler);
            this._flagSwitch.state = state;
            this._flagSwitch.unblock_signal_handler(this._stateHandler);
        });

        box.add_child(this._flagSwitch);

        super._init({
            style_class: 'lg-debug-flag-button',
            can_focus: true,
            toggleMode: true,
            child: box,
            label_actor: flagLabel,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this.connect('clicked', () => this._flagSwitch.toggle());
    }

    _isEnabled() {
        throw new Error('Method not implemented');
    }

    _enable() {
        throw new Error('Method not implemented');
    }

    _disable() {
        throw new Error('Method not implemented');
    }
});


var ClutterDebugFlag = GObject.registerClass(
class ClutterDebugFlag extends DebugFlag {
    _init(categoryName, flagName) {
        super._init(flagName);

        this._argPos = CLUTTER_DEBUG_FLAG_CATEGORIES.get(categoryName).argPos;
        this._enumValue = Clutter[categoryName][flagName];
    }

    _isEnabled() {
        const enabledFlags = Meta.get_clutter_debug_flags();
        return !!(enabledFlags[this._argPos] & this._enumValue);
    }

    _getArgs() {
        const args = [0, 0, 0];
        args[this._argPos] = this._enumValue;
        return args;
    }

    _enable() {
        Meta.add_clutter_debug_flags(...this._getArgs());
    }

    _disable() {
        Meta.remove_clutter_debug_flags(...this._getArgs());
    }
});

var MutterPaintDebugFlag = GObject.registerClass(
class MutterPaintDebugFlag extends DebugFlag {
    _init(flagName) {
        super._init(flagName);

        this._enumValue = Meta.DebugPaintFlag[flagName];
    }

    _isEnabled() {
        return !!(Meta.get_debug_paint_flags() & this._enumValue);
    }

    _enable() {
        Meta.add_debug_paint_flag(this._enumValue);
    }

    _disable() {
        Meta.remove_debug_paint_flag(this._enumValue);
    }
});

var MutterTopicDebugFlag = GObject.registerClass(
class MutterTopicDebugFlag extends DebugFlag {
    _init(flagName) {
        super._init(flagName);

        this._enumValue = Meta.DebugTopic[flagName];
    }

    _isEnabled() {
        return Meta.is_topic_enabled(this._enumValue);
    }

    _enable() {
        Meta.add_verbose_topic(this._enumValue);
    }

    _disable() {
        Meta.remove_verbose_topic(this._enumValue);
    }
});

var UnsafeModeDebugFlag = GObject.registerClass(
class UnsafeModeDebugFlag extends DebugFlag {
    _init() {
        super._init('unsafe-mode');
    }

    _isEnabled() {
        return global.context.unsafe_mode;
    }

    _enable() {
        global.context.unsafe_mode = true;
    }

    _disable() {
        global.context.unsafe_mode = false;
    }
});

var DebugFlags = GObject.registerClass(
class DebugFlags extends St.BoxLayout {
    _init() {
        super._init({
            name: 'lookingGlassDebugFlags',
            vertical: true,
            x_align: Clutter.ActorAlign.CENTER,
        });

        // Clutter debug flags
        for (const [categoryName, props] of CLUTTER_DEBUG_FLAG_CATEGORIES.entries()) {
            this._addHeader(`Clutter${categoryName}`);
            for (const flagName of this._getFlagNames(Clutter[categoryName])) {
                if (props.exclude.includes(flagName))
                    continue;
                this.add_child(new ClutterDebugFlag(categoryName, flagName));
            }
        }

        // Meta paint flags
        this._addHeader('MetaDebugPaintFlag');
        for (const flagName of this._getFlagNames(Meta.DebugPaintFlag))
            this.add_child(new MutterPaintDebugFlag(flagName));

        // Meta debug topics
        this._addHeader('MetaDebugTopic');
        for (const flagName of this._getFlagNames(Meta.DebugTopic))
            this.add_child(new MutterTopicDebugFlag(flagName));

        // MetaContext::unsafe-mode
        this._addHeader('MetaContext');
        this.add_child(new UnsafeModeDebugFlag());
    }

    _addHeader(title) {
        const header = new St.Label({
            text: title,
            style_class: 'lg-debug-flags-header',
            x_align: Clutter.ActorAlign.START,
        });
        this.add_child(header);
    }

    *_getFlagNames(enumObject) {
        for (const flagName of Object.getOwnPropertyNames(enumObject)) {
            if (typeof enumObject[flagName] !== 'number')
                continue;

            if (enumObject[flagName] <= 0)
                continue;

            yield flagName;
        }
    }
});


var LookingGlass = GObject.registerClass(
class LookingGlass extends St.BoxLayout {
    _init() {
        super._init({
            name: 'LookingGlassDialog',
            style_class: 'lg-dialog',
            vertical: true,
            visible: false,
            reactive: true,
        });

        this._borderPaintTarget = null;
        this._redBorderEffect = new RedBorderEffect();

        this._open = false;

        this._it = null;
        this._offset = 0;

        // Sort of magic, but...eh.
        this._maxItems = 150;

        this._interfaceSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.interface' });
        this._interfaceSettings.connect('changed::monospace-font-name',
                                        this._updateFont.bind(this));
        this._updateFont();

        // We want it to appear to slide out from underneath the panel
        Main.uiGroup.add_actor(this);
        Main.uiGroup.set_child_below_sibling(this,
                                             Main.layoutManager.panelBox);
        Main.layoutManager.panelBox.connect('notify::allocation',
                                            this._queueResize.bind(this));
        Main.layoutManager.keyboardBox.connect('notify::allocation',
                                               this._queueResize.bind(this));

        this._objInspector = new ObjInspector(this);
        Main.uiGroup.add_actor(this._objInspector);
        this._objInspector.hide();

        let toolbar = new St.BoxLayout({ name: 'Toolbar' });
        this.add_actor(toolbar);
        const inspectButton = new St.Button({
            style_class: 'lg-toolbar-button',
            icon_name: 'find-location-symbolic',
        });
        toolbar.add_actor(inspectButton);
        inspectButton.connect('clicked', () => {
            let inspector = new Inspector(this);
            inspector.connect('target', (i, target, stageX, stageY) => {
                this._pushResult(`inspect(${Math.round(stageX)}, ${Math.round(stageY)})`, target);
            });
            inspector.connect('closed', () => {
                this.show();
                global.stage.set_key_focus(this._entry);
            });
            this.hide();
            return Clutter.EVENT_STOP;
        });

        const gcButton = new St.Button({
            style_class: 'lg-toolbar-button',
            icon_name: 'user-trash-full-symbolic',
        });
        toolbar.add_actor(gcButton);
        gcButton.connect('clicked', () => {
            gcButton.child.icon_name = 'user-trash-symbolic';
            System.gc();
            this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 500, () => {
                gcButton.child.icon_name = 'user-trash-full-symbolic';
                this._timeoutId = 0;
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(
                this._timeoutId,
                '[gnome-shell] gcButton.child.icon_name = \'user-trash-full-symbolic\''
            );
            return Clutter.EVENT_PROPAGATE;
        });

        let notebook = new Notebook();
        this._notebook = notebook;
        this.add_child(notebook);

        let emptyBox = new St.Bin({ x_expand: true });
        toolbar.add_child(emptyBox);
        toolbar.add_actor(notebook.tabControls);

        this._evalBox = new St.BoxLayout({ name: 'EvalBox', vertical: true });
        notebook.appendPage('Evaluator', this._evalBox);

        this._resultsArea = new St.BoxLayout({
            name: 'ResultsArea',
            vertical: true,
            y_expand: true,
        });
        this._evalBox.add_child(this._resultsArea);

        this._entryArea = new St.BoxLayout({
            name: 'EntryArea',
            y_align: Clutter.ActorAlign.END,
        });
        this._evalBox.add_actor(this._entryArea);

        let label = new St.Label({ text: CHEVRON });
        this._entryArea.add(label);

        this._entry = new St.Entry({
            can_focus: true,
            x_expand: true,
        });
        ShellEntry.addContextMenu(this._entry);
        this._entryArea.add_child(this._entry);

        this._windowList = new WindowList(this);
        notebook.appendPage('Windows', this._windowList);

        this._extensions = new Extensions(this);
        notebook.appendPage('Extensions', this._extensions);

        this._actorTreeViewer = new ActorTreeViewer(this);
        notebook.appendPage('Actors', this._actorTreeViewer);

        this._debugFlags = new DebugFlags();
        notebook.appendPage('Flags', this._debugFlags);

        this._entry.clutter_text.connect('activate', (o, _e) => {
            // Hide any completions we are currently showing
            this._hideCompletions();

            let text = o.get_text();
            // Ensure we don't get newlines in the command; the history file is
            // newline-separated.
            text = text.replace('\n', ' ');
            this._evaluate(text);
            return true;
        });

        this._history = new History.HistoryManager({
            gsettingsKey: HISTORY_KEY,
            entry: this._entry.clutter_text,
        });

        this._autoComplete = new AutoComplete(this._entry);
        this._autoComplete.connect('suggest', (a, e) => {
            this._showCompletions(e.completions);
        });
        // If a completion is completed unambiguously, the currently-displayed completion
        // suggestions become irrelevant.
        this._autoComplete.connect('completion', (a, e) => {
            if (e.type == 'whole-word')
                this._hideCompletions();
        });

        this._resize();
    }

    vfunc_captured_event(event) {
        if (Main.keyboard.maybeHandleEvent(event))
            return Clutter.EVENT_STOP;

        return Clutter.EVENT_PROPAGATE;
    }

    _updateFont() {
        let fontName = this._interfaceSettings.get_string('monospace-font-name');
        let fontDesc = Pango.FontDescription.from_string(fontName);
        // We ignore everything but size and style; you'd be crazy to set your system-wide
        // monospace font to be bold/oblique/etc. Could easily be added here.
        let size = fontDesc.get_size() / 1024.;
        let unit = fontDesc.get_size_is_absolute() ? 'px' : 'pt';
        this.style = `
            font-size: ${size}${unit};
            font-family: "${fontDesc.get_family()}";`;
    }

    setBorderPaintTarget(obj) {
        if (this._borderPaintTarget != null)
            this._borderPaintTarget.remove_effect(this._redBorderEffect);
        this._borderPaintTarget = obj;
        if (this._borderPaintTarget != null)
            this._borderPaintTarget.add_effect(this._redBorderEffect);
    }

    _pushResult(command, obj) {
        let index = this._resultsArea.get_n_children() + this._offset;
        let result = new Result(this, CHEVRON + command, obj, index);
        this._resultsArea.add(result);
        if (obj instanceof Clutter.Actor)
            this.setBorderPaintTarget(obj);

        if (this._resultsArea.get_n_children() > this._maxItems) {
            this._resultsArea.get_first_child().destroy();
            this._offset++;
        }
        this._it = obj;

        // Scroll to bottom
        this._notebook.scrollToBottom(0);
    }

    _showCompletions(completions) {
        if (!this._completionActor) {
            this._completionActor = new St.Label({ name: 'LookingGlassAutoCompletionText', style_class: 'lg-completions-text' });
            this._completionActor.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
            this._completionActor.clutter_text.line_wrap = true;
            this._evalBox.insert_child_below(this._completionActor, this._entryArea);
        }

        this._completionActor.set_text(completions.join(', '));

        // Setting the height to -1 allows us to get its actual preferred height rather than
        // whatever was last set when animating
        this._completionActor.set_height(-1);
        let [, naturalHeight] = this._completionActor.get_preferred_height(this._resultsArea.get_width());

        // Don't reanimate if we are already visible
        if (this._completionActor.visible) {
            this._completionActor.height = naturalHeight;
        } else {
            let settings = St.Settings.get();
            let duration = AUTO_COMPLETE_SHOW_COMPLETION_ANIMATION_DURATION / settings.slow_down_factor;
            this._completionActor.show();
            this._completionActor.remove_all_transitions();
            this._completionActor.ease({
                height: naturalHeight,
                opacity: 255,
                duration,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        }
    }

    _hideCompletions() {
        if (this._completionActor) {
            let settings = St.Settings.get();
            let duration = AUTO_COMPLETE_SHOW_COMPLETION_ANIMATION_DURATION / settings.slow_down_factor;
            this._completionActor.remove_all_transitions();
            this._completionActor.ease({
                height: 0,
                opacity: 0,
                duration,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._completionActor.hide();
                },
            });
        }
    }

    _evaluate(command) {
        command = this._history.addItem(command); // trims command
        if (!command)
            return;

        let lines = command.split(';');
        lines.push(`return ${lines.pop()}`);

        let fullCmd = commandHeader + lines.join(';');

        let resultObj;
        try {
            resultObj = Function(fullCmd)();
        } catch (e) {
            resultObj = `<exception ${e}>`;
        }

        this._pushResult(command, resultObj);
        this._entry.text = '';
    }

    inspect(x, y) {
        return global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, x, y);
    }

    getIt() {
        return this._it;
    }

    getResult(idx) {
        try {
            return this._resultsArea.get_child_at_index(idx - this._offset).o;
        } catch (e) {
            throw new Error(`Unknown result at index ${idx}`);
        }
    }

    toggle() {
        if (this._open)
            this.close();
        else
            this.open();
    }

    _queueResize() {
        const laters = global.compositor.get_laters();
        laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
            this._resize();
            return GLib.SOURCE_REMOVE;
        });
    }

    _resize() {
        let primary = Main.layoutManager.primaryMonitor;
        let myWidth = primary.width * 0.7;
        let availableHeight = primary.height - Main.layoutManager.keyboardBox.height;
        let myHeight = Math.min(primary.height * 0.7, availableHeight * 0.9);
        this.x = primary.x + (primary.width - myWidth) / 2;
        this._hiddenY = primary.y + Main.layoutManager.panelBox.height - myHeight;
        this._targetY = this._hiddenY + myHeight;
        this.y = this._hiddenY;
        this.width = myWidth;
        this.height = myHeight;
        this._objInspector.set_size(Math.floor(myWidth * 0.8), Math.floor(myHeight * 0.8));
        this._objInspector.set_position(this.x + Math.floor(myWidth * 0.1),
                                        this._targetY + Math.floor(myHeight * 0.1));
    }

    insertObject(obj) {
        this._pushResult('<insert>', obj);
    }

    inspectObject(obj, sourceActor) {
        this._objInspector.open(sourceActor);
        this._objInspector.selectObject(obj);
    }

    // Handle key events which are relevant for all tabs of the LookingGlass
    vfunc_key_press_event(keyPressEvent) {
        let symbol = keyPressEvent.keyval;
        if (symbol == Clutter.KEY_Escape) {
            this.close();
            return Clutter.EVENT_STOP;
        }
        // Ctrl+PgUp and Ctrl+PgDown switches tabs in the notebook view
        if (keyPressEvent.modifier_state & Clutter.ModifierType.CONTROL_MASK) {
            if (symbol == Clutter.KEY_Page_Up)
                this._notebook.prevTab();
            else if (symbol == Clutter.KEY_Page_Down)
                this._notebook.nextTab();
        }
        return super.vfunc_key_press_event(keyPressEvent);
    }

    open() {
        if (this._open)
            return;

        let grab = Main.pushModal(this, { actionMode: Shell.ActionMode.LOOKING_GLASS });
        if (grab.get_seat_state() !== Clutter.GrabState.ALL) {
            Main.popModal(grab);
            return;
        }

        this._grab = grab;
        this._notebook.selectIndex(0);
        this.show();
        this._open = true;
        this._history.lastItem();

        this.remove_all_transitions();

        // We inverse compensate for the slow-down so you can change the factor
        // through LookingGlass without long waits.
        let duration = LG_ANIMATION_TIME / St.Settings.get().slow_down_factor;
        this.ease({
            y: this._targetY,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });

        this._windowList.update();
        this._entry.grab_key_focus();
    }

    close() {
        if (!this._open)
            return;

        this._objInspector.hide();

        this._open = false;
        this.remove_all_transitions();

        this.setBorderPaintTarget(null);

        let settings = St.Settings.get();
        let duration = Math.min(LG_ANIMATION_TIME / settings.slow_down_factor,
                                LG_ANIMATION_TIME);
        this.ease({
            y: this._hiddenY,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                Main.popModal(this._grab);
                this._grab = null;
                this.hide();
            },
        });
    }

    get isOpen() {
        return this._open;
    }
});
