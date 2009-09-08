/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Lang = imports.lang;
const Mainloop = imports.mainloop;

const Tweener = imports.ui.tweener;
const Main = imports.ui.main;

const LG_BORDER_COLOR = new Clutter.Color();
LG_BORDER_COLOR.from_pixel(0x0000aca0);
const LG_BACKGROUND_COLOR = new Clutter.Color();
LG_BACKGROUND_COLOR.from_pixel(0x000000d5);
const GREY = new Clutter.Color();
GREY.from_pixel(0xAFAFAFFF);
const MATRIX_GREEN = new Clutter.Color();
MATRIX_GREEN.from_pixel(0x88ff66ff);
// FIXME pull from GConf
const MATRIX_FONT = 'Monospace 10';

                    /* Imports...feel free to add here as needed */
var commandHeader = "const Clutter = imports.gi.Clutter; " +
                    "const GLib = imports.gi.GLib; " +
                    "const Gtk = imports.gi.Gtk; " +
                    "const Mainloop = imports.mainloop; " +
                    "const Meta = imports.gi.Meta; " +
                    "const Shell = imports.gi.Shell; " +
                    "const Main = imports.ui.main; " +
                    "const Lang = imports.lang; " +
                    "const Tweener = imports.ui.tweener; " +
                    /* Utility functions...we should probably be able to use these
                     * in the shell core code too. */
                    "const global = Shell.Global.get(); " +
                    "const stage = global.stage; " +
                    "const color = function(pixel) { let c= new Clutter.Color(); c.from_pixel(pixel); return c; }; " +
                    /* Special lookingGlass functions */
                       "const it = Main.lookingGlass.getIt(); " +
                    "const r = Lang.bind(Main.lookingGlass, Main.lookingGlass.getResult); ";

function Notebook() {
    this._init();
}

Notebook.prototype = {
    _init: function() {
        this.actor = new Big.Box();

        this.tabControls = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                         spacing: 4, padding: 2 });

        this._selectedIndex = -1;
        this._tabs = [];
    },

    appendPage: function(name, child) {
        let labelOuterBox = new Big.Box({ padding: 2 });
        let labelBox = new Big.Box({ padding: 2, border_color: MATRIX_GREEN,
                                     reactive: true });
        labelOuterBox.append(labelBox, Big.BoxPackFlags.NONE);
        let label = new Clutter.Text({ color: MATRIX_GREEN,
                                       font_name: MATRIX_FONT,
                                       text: name });
        labelBox.connect('button-press-event', Lang.bind(this, function () {
            this.selectChild(child);
            return true;
        }));
        labelBox.append(label, Big.BoxPackFlags.EXPAND);
        this._tabs.push([child, labelBox]);
        child.hide();
        this.actor.append(child, Big.BoxPackFlags.EXPAND);
        this.tabControls.append(labelOuterBox, Big.BoxPackFlags.NONE);
        if (this._selectedIndex == -1)
            this.selectIndex(0);
    },

    _unselect: function() {
        if (this._selectedIndex < 0)
            return;
        let [child, labelBox] = this._tabs[this._selectedIndex];
        labelBox.padding = 2;
        labelBox.border = 0;
        child.hide();
        this._selectedIndex = -1;
    },

    selectIndex: function(index) {
        if (index == this._selectedIndex)
            return;
        this._unselect();
        if (index < 0) {
            this.emit('selection', null);
            return;
        }
        let [child, labelBox] = this._tabs[index];
        labelBox.padding = 1;
        labelBox.border = 1;
        child.show();
        this._selectedIndex = index;
        this.emit('selection', child);
    },

    selectChild: function(child) {
        if (child == null)
            this.selectIndex(-1);
        else {
            for (let i = 0; i < this._tabs.length; i++) {
                let [tabChild, labelBox] = this._tabs[i];
                if (tabChild == child) {
                    this.selectIndex(i);
                    return;
                }
            }
        }
    }
}
Signals.addSignalMethods(Notebook.prototype);

function Result(command, o, index) {
    this._init(command, o, index);
}

Result.prototype = {
    _init : function(command, o, index) {
        this.index = index;
        this.o = o;

        this.actor = new Big.Box();

        let cmdTxt = new Clutter.Text({ color: MATRIX_GREEN,
                                        font_name: MATRIX_FONT,
                                        ellipsize: Pango.EllipsizeMode.END,
                                        text: command });
        this.actor.append(cmdTxt, Big.BoxPackFlags.NONE);
        let resultTxt = new Clutter.Text({ color: MATRIX_GREEN,
                                           font_name: MATRIX_FONT,
                                           ellipsize: Pango.EllipsizeMode.END,
                                           text: "r(" + index + ") = " + o });
        this.actor.append(resultTxt, Big.BoxPackFlags.NONE);
        let line = new Big.Box({ border_color: GREY,
                                 border_bottom: 1,
                                 height: 8 });
        this.actor.append(line, Big.BoxPackFlags.NONE);
    }
}

function ActorHierarchy() {
    this._init();
}

ActorHierarchy.prototype = {
    _init : function () {
        this._previousTarget = null;
        this._target = null;

        this._parentList = [];

        this.actor = new Big.Box({ spacing: 4,
                                   border: 1,
                                   padding: 4,
                                   border_color: GREY });
    },

    setTarget: function(actor) {
        this._previousTarget = this._target;
        this.target = actor;

        this.actor.remove_all();

        if (!(actor instanceof Clutter.Actor))
            return;

        if (this.target == null)
            return;

        this._parentList = [];
        let parent = actor;
        while ((parent = parent.get_parent()) != null) {
            this._parentList.push(parent);

            let link = new Clutter.Text({ color: MATRIX_GREEN,
                                          font_name: MATRIX_FONT,
                                          reactive: true,
                                          text: "" + parent });
            this.actor.append(link, Big.BoxPackFlags.IF_FITS);
            let parentTarget = parent;
            link.connect('button-press-event', Lang.bind(this, function () {
                this._selectByActor(parentTarget);
                return true;
            }));
        }
        this.emit('selection', actor);
    },

    _selectByActor: function(actor) {
        let idx = this._parentList.indexOf(actor);
        let children = this.actor.get_children();
        let link = children[idx];
        this.emit('selection', actor);
    }
}
Signals.addSignalMethods(ActorHierarchy.prototype);

function PropertyInspector() {
    this._init();
}

PropertyInspector.prototype = {
    _init : function () {
        this._target = null;

        this._parentList = [];

        this.actor = new Big.Box({ spacing: 4,
                                   border: 1,
                                   padding: 4,
                                   border_color: GREY });
    },

    setTarget: function(actor) {
        this.target = actor;

        this.actor.remove_all();

        for (let propName in actor) {
            let valueStr;
            try {
                valueStr = "" + actor[propName];
            } catch (e) {
                valueStr = '<error>';
            }
            let propText = propName + ": " + valueStr;
            let propDisplay = new Clutter.Text({ color: MATRIX_GREEN,
                                                 font_name: MATRIX_FONT,
                                                 reactive: true,
                                                 text: propText });
            this.actor.append(propDisplay, Big.BoxPackFlags.IF_FITS);
        }
    }
}

function Inspector() {
    this._init();
}

Inspector.prototype = {
    _init: function() {
        let width = 150;
        let eventHandler = new Big.Box({ background_color: LG_BACKGROUND_COLOR,
                                         border: 1,
                                         border_color: LG_BORDER_COLOR,
                                         corner_radius: 4,
                                         y: global.stage.height/2,
                                         reactive: true
                                      });
        eventHandler.connect('notify::allocation', Lang.bind(this, function () {
            eventHandler.x = Math.floor((global.stage.width)/2 - (eventHandler.width)/2);
        }));
        global.stage.add_actor(eventHandler);
        let displayText = new Clutter.Text({ color: MATRIX_GREEN,
                                             font_name: MATRIX_FONT, text: '' });
        eventHandler.append(displayText, Big.BoxPackFlags.EXPAND);

        let borderPaintTarget = null;
        let borderPaintId = null;
        eventHandler.connect('destroy', Lang.bind(this, function() {
            if (borderPaintTarget != null)
                borderPaintTarget.disconnect(borderPaintId);
        }));

        eventHandler.connect('button-press-event', Lang.bind(this, function (actor, event) {
            Clutter.ungrab_pointer(eventHandler);

            let [stageX, stageY] = event.get_coords();
            let target = global.stage.get_actor_at_pos(Clutter.PickMode.ALL,
                                                       stageX,
                                                       stageY);
            this.emit('target', target, stageX, stageY);
            eventHandler.destroy();
            this.emit('closed');
            return true;
        }));

        eventHandler.connect('motion-event', Lang.bind(this, function (actor, event) {
            let [stageX, stageY] = event.get_coords();
            let target = global.stage.get_actor_at_pos(Clutter.PickMode.ALL,
                                                       stageX,
                                                       stageY);
            displayText.text = '<inspect x: ' + stageX + ' y: ' + stageY + '> ' + target;
            if (borderPaintTarget != null)
                borderPaintTarget.disconnect(borderPaintId);
            borderPaintTarget = target;
            borderPaintId = Shell.add_hook_paint_red_border(target);
            return true;
        }));
        Clutter.grab_pointer(eventHandler);
    }
}

Signals.addSignalMethods(Inspector.prototype);

function LookingGlass() {
    this._init();
}

LookingGlass.prototype = {
    _init : function() {
        this._idleHistorySaveId = 0;
        let historyPath = global.configdir + "/lookingglass-history.txt";
        this._historyFile = Gio.file_new_for_path(historyPath);
        this._savedText = null;
        this._historyNavIndex = -1;
        this._history = [];
        this._readHistory();

        this._open = false;

        this._offset = 0;
        this._results = [];

        // TODO replace with scrolling or something better
        this._maxItems = 10;

        this.actor = new Big.Box({ background_color: LG_BACKGROUND_COLOR,
                                   border: 1,
                                   border_color: LG_BORDER_COLOR,
                                   corner_radius: 4,
                                   padding_top: 8,
                                   padding_left: 4,
                                   padding_right: 4,
                                   padding_bottom: 4,
                                   spacing: 4,
                                   visible: false
                                });
        global.stage.add_actor(this.actor);

        let toolbar = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                    border: 1, border_color: GREY,
                                    corner_radius: 4 });
        this.actor.append(toolbar, Big.BoxPackFlags.NONE);
        let inspectIcon = Shell.TextureCache.get_default().load_gicon(new Gio.ThemedIcon({ name: 'gtk-color-picker' }),
                                                                      24);
        toolbar.append(inspectIcon, Big.BoxPackFlags.NONE);
        inspectIcon.reactive = true;
        inspectIcon.connect('button-press-event', Lang.bind(this, function () {
            let inspector = new Inspector();
            inspector.connect('target', Lang.bind(this, function(i, target, stageX, stageY) {
                this._pushResult('<inspect x:' + stageX + ' y:' + stageY + '>',
                                 target);
                this._hierarchy.setTarget(target);
            }));
            inspector.connect('closed', Lang.bind(this, function() {
                this.actor.show();
                global.stage.set_key_focus(this._entry);
            }));
            this.actor.hide();
            return true;
        }));

        let notebook = new Notebook();
        this.actor.append(notebook.actor, Big.BoxPackFlags.EXPAND);
        toolbar.append(notebook.tabControls, Big.BoxPackFlags.END);

        this._evalBox = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                      spacing: 4 });
        notebook.appendPage('Evaluator', this._evalBox);

        this._resultsArea = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                          spacing: 4 });
        this._evalBox.append(this._resultsArea, Big.BoxPackFlags.EXPAND);

        let entryArea = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
        this._evalBox.append(entryArea, Big.BoxPackFlags.NONE);

        let label = new Clutter.Text({ color: MATRIX_GREEN,
                                       font_name: MATRIX_FONT,
                                       text: 'js>>> ' });
        entryArea.append(label, Big.BoxPackFlags.NONE);

        this._entry = new Clutter.Text({ color: MATRIX_GREEN,
                                         font_name: MATRIX_FONT,
                                         editable: true,
                                         activatable: true,
                                         singleLineMode: true,
                                         text: ''});
        /* kind of a hack */
        notebook.connect('selection', Lang.bind(this, function (nb, child) {
            if (child == this._evalBox)
                global.stage.set_key_focus(this._entry);
        }));
        entryArea.append(this._entry, Big.BoxPackFlags.EXPAND);

        this._hierarchy = new ActorHierarchy();
        notebook.appendPage('Hierarchy', this._hierarchy.actor);

        this._propInspector = new PropertyInspector();
        notebook.appendPage('Properties', this._propInspector.actor);
        this._hierarchy.connect('selection', Lang.bind(this, function (h, actor) {
            this._pushResult('<parent selection>', actor);
            notebook.selectIndex(0);
        }));

        this._entry.connect('activate', Lang.bind(this, function (o, e) {
            let text = o.get_text();
            // Ensure we don't get newlines in the command; the history file is
            // newline-separated.
            text.replace('\n', ' ');
            // Strip leading and trailing whitespace
            text = text.replace(/^\s+/g, "").replace(/\s+$/g, "");
            if (text == '')
                return true;
            this._evaluate(text);
            this._historyNavIndex = -1;
            return true;
        }));
        this._entry.connect('key-press-event', Lang.bind(this, function(o, e) {
            let symbol = e.get_key_symbol();
            if (symbol == Clutter.Escape) {
                this.close();
                return true;
            } else if (symbol == Clutter.Up) {
                if (this._historyNavIndex >= this._history.length - 1)
                    return true;
                this._historyNavIndex++;
                if (this._historyNavIndex == 0)
                    this._savedText = this._entry.text;
                this._entry.text = this._history[this._history.length - this._historyNavIndex - 1];
                return true;
            } else if (symbol == Clutter.Down) {
                if (this._historyNavIndex <= 0)
                    return true;
                this._historyNavIndex--;
                if (this._historyNavIndex < 0)
                    this._entry.text = this._savedText;
                else
                    this._entry.text = this._history[this._history.length - this._historyNavIndex - 1];
                return true;
            } else {
                this._historyNavIndex = -1;
                this._savedText = null;
                return false;
            }
        }));
    },

    _readHistory: function () {
        if (!this._historyFile.query_exists(null))
            return;
        let [result, contents, length, etag] = this._historyFile.load_contents(null);
        this._history = contents.split('\n').filter(function (e) { return e != ''; });
    },

    _queueHistorySave: function() {
        if (this._idleHistorySaveId > 0)
            return;
        this._idleHistorySaveId = Mainloop.timeout_add_seconds(5, Lang.bind(this, this._doSaveHistory));
    },

    _doSaveHistory: function () {
        this._idleHistorySaveId = false;
        let output = this._historyFile.replace(null, true, Gio.FileCreateFlags.NONE, null);
        let dataOut = new Gio.DataOutputStream({ base_stream: output });
        dataOut.put_string(this._history.join('\n'), null);
        dataOut.put_string('\n', null);
        dataOut.close(null);
        return false;
    },

    _pushResult: function(command, obj) {
        let index = this._results.length + this._offset;
        let result = new Result('>>> ' + command, obj, index);
        this._results.push(result);
        this._resultsArea.append(result.actor, Big.BoxPackFlags.NONE);
        this._propInspector.setTarget(obj);
        let children = this._resultsArea.get_children();
        if (children.length > this._maxItems) {
            this._results.shift();
            children[0].destroy();
            this._offset++;
        }
        this._it = obj;
    },

    _evaluate : function(command) {
        this._history.push(command);
        this._queueHistorySave();

        let fullCmd = commandHeader + command;

        let resultObj;
        try {
            resultObj = eval(fullCmd);
        } catch (e) {
            resultObj = "<exception " + e + ">";
        }

        this._pushResult(command, resultObj);
        this._hierarchy.setTarget(null);
        this._entry.text = '';
    },

    getIt: function () {
        return this._it;
    },

    getResult: function(idx) {
        return this._results[idx - this._offset].o;
    },

    toggle: function() {
        if (this._open)
            this.close();
        else
            this.open();
    },

    _resizeTo: function(actor) {
        let stage = Shell.Global.get().stage;
        let stageWidth = stage.width;
        let myWidth = stage.width * 0.7;
        let myHeight = stage.height * 0.7;
        let [srcX, srcY] = actor.get_transformed_position();
        this.actor.x = srcX + (stage.width-myWidth)/2;
        this._hiddenY = srcY + actor.height - myHeight - 4; // -4 to hide the top corners
        this._targetY = this._hiddenY + myHeight;
        this.actor.y = this._hiddenY;
        this.actor.width = myWidth;
        this.actor.height = myHeight;
    },

    slaveTo: function(actor) {
        this._slaveTo = actor;
        actor.connect('notify::allocation', Lang.bind(this, function () {
            this._resizeTo(actor);
        }));
        this._resizeTo(actor);
    },

    open : function() {
        if (this._open)
            return;

        this.actor.show();
        this.actor.lower(Main.chrome.actor);
        this._open = true;

        Tweener.removeTweens(this.actor);

        if (!Main.beginModal())
            return;

        global.stage.set_key_focus(this._entry);

        Tweener.addTween(this.actor, { time: 0.5,
                                       transition: "easeOutQuad",
                                       y: this._targetY
                                     });
    },

    close : function() {
        if (!this._open)
            return;

        this._historyNavIndex = -1;
        this._open = false;
        Tweener.removeTweens(this.actor);

        Main.endModal();

        Tweener.addTween(this.actor, { time: 0.5,
                                       transition: "easeOutQuad",
                                       y: this._hiddenY,
                                       onComplete: Lang.bind(this, function () {
                                           this.actor.hide();
                                       })
                                     });
    }
};
Signals.addSignalMethods(LookingGlass.prototype);
