/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Mainloop = imports.mainloop;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;
const Widget = imports.ui.widget;

const WIDGETBOX_BG_COLOR = new Clutter.Color();
WIDGETBOX_BG_COLOR.from_pixel(0xf0f0f0ff);
const BLACK = new Clutter.Color();
BLACK.from_pixel(0x000000ff);

const WIDGETBOX_PADDING = 2;
const ANIMATION_TIME = 0.5;
const POP_IN_LAG = 250; /* milliseconds */

function WidgetBox(widget, expanded) {
    this._init(widget, expanded);
}

WidgetBox.prototype = {
    _init: function(widget, expanded) {
        this.state = expanded ? Widget.STATE_EXPANDED : Widget.STATE_COLLAPSED;

	if (widget instanceof Widget.Widget) {
	    this._widget = widget;
            this._widget.state = this.state;
	} else {
	    let ctor = this._ctorFromName(widget);
            this._widget = new ctor(this.state);
	}

        if (!this._widget.actor)
            throw new Error("widget has no actor");
        else if (!this._widget.title && !this._widget.collapsedActor)
            throw new Error("widget has neither title nor collapsedActor");

        this.state = expanded ? Widget.STATE_EXPANDED : Widget.STATE_COLLAPSED;

        // The structure of a WidgetBox:
        //
        // The top level is a Clutter.Group, which exists to make
        // pop-out work correctly; when another widget pops out, its
        // width will increase, which will in turn cause the sidebar's
        // width to increase, which will cause the sidebar to increase
        // the width of each of its children (the WidgetBoxes). But we
        // don't want the non-popped-out widgets to expand, so we make
        // the top-level actor be a Clutter.Group, which will accept
        // the new width from the Sidebar, but not impose it on its
        // own child.
        //
        // Inside the toplevel group is a horizontal Big.Box
        // containing 2 Clutter.Groups; one for the collapsed state
        // (cgroup) and one for the expanded state (egroup). Each
        // group contains a single vertical Big.Box (cbox and ebox
        // respectively), which have the appropriate fixed width. The
        // cbox contains either the collapsed widget actor or else the
        // rotated title. The ebox contains the horizontal title (if
        // any), separator line, and the expanded widget actor. (If
        // the widget doesn't have a collapsed actor, and therefore
        // supports pop-out, then it will also have a vertical line
        // between the two groups, which will only be shown during
        // pop-out.)
        //
        // In the expanded view, cgroup is hidden and egroup is shown.
        // When animating to the collapsed view, first the ebox is
        // slid offscreen by giving it increasingly negative x
        // coordinates within egroup. Then once it's fully offscreen,
        // we hide egroup, show cgroup, and slide cbox back in in the
        // same way.
        //
        // The pop-out view works similarly to the second half of the
        // collapsed-to-expanded transition, except that the
        // horizontal title gets hidden to avoid duplication.

        this.actor = new Clutter.Group();
        this._hbox = new Big.Box({ background_color: WIDGETBOX_BG_COLOR,
                                   padding_top: WIDGETBOX_PADDING,
                                   padding_bottom: WIDGETBOX_PADDING,
                                   padding_right: WIDGETBOX_PADDING,
                                   // Left padding is here to make up for
                                   // the X offset used for the sidebar
                                   // to hide its rounded corners
                                   padding_left: 2 * WIDGETBOX_PADDING,
                                   spacing: WIDGETBOX_PADDING,
                                   corner_radius: WIDGETBOX_PADDING,
                                   orientation: Big.BoxOrientation.HORIZONTAL,
                                   reactive: true });
        this.actor.add_actor(this._hbox);

        this._cgroup = new Clutter.Group({ clip_to_allocation: true });
        this._hbox.append(this._cgroup, Big.BoxPackFlags.NONE);

        this._cbox = new Big.Box({ width: Widget.COLLAPSED_WIDTH,
                                   clip_to_allocation: true });
        this._cgroup.add_actor(this._cbox);

        if (this._widget.collapsedActor) {
            if (this._widget.collapsedActor == this._widget.actor)
                this._singleActor = true;
            else {
                this._cbox.append(this._widget.collapsedActor,
                                  Big.BoxPackFlags.NONE);
            }
        } else {
            let vtitle = new Clutter.Text({ font_name: "Sans 16px",
                                            text: this._widget.title,
                                            rotation_angle_z: -90.0 });
            let signalId = vtitle.connect('notify::allocation',
                                          function () {
                                              vtitle.disconnect(signalId);
                                              vtitle.set_anchor_point(vtitle.natural_width, 0);
                                              vtitle.set_size(vtitle.natural_height,
                                                              vtitle.natural_width);
                                          });
            this._vtitle = vtitle;
            this._cbox.append(this._vtitle, Big.BoxPackFlags.NONE);

            this._vline = new Clutter.Rectangle({ color: BLACK, width: 1 });
            this._hbox.append(this._vline, Big.BoxPackFlags.NONE);
            this._vline.hide();

            // Set up pop-out
            this._eventHandler = this._hbox.connect('captured-event',
                                                    Lang.bind(this, this._popEventHandler));
            this._activationHandler = this._widget.connect('activated',
                                                           Lang.bind(this, this._activationHandler));
        }

        this._egroup = new Clutter.Group({ clip_to_allocation: true });
        this._hbox.append(this._egroup, Big.BoxPackFlags.NONE);

        this._ebox = new Big.Box({ spacing: WIDGETBOX_PADDING,
                                   width: Widget.EXPANDED_WIDTH,
                                   clip_to_allocation: true });
        this._egroup.add_actor(this._ebox);

        if (this._widget.title) {
            this._htitle = new Clutter.Text({ font_name: "Sans 16px",
                                              text: this._widget.title });
            this._ebox.append(this._htitle, Big.BoxPackFlags.NONE);

            this._hline = new Clutter.Rectangle({ color: BLACK, height: 1 });
            this._ebox.append(this._hline, Big.BoxPackFlags.NONE);
        }

        this._ebox.append(this._widget.actor, Big.BoxPackFlags.NONE);

        if (expanded)
            this._setWidgetExpanded();
        else
            this._setWidgetCollapsed();
    },

    // Given a name like "imports.ui.widget.ClockWidget", turn that
    // into a constructor function
    _ctorFromName: function(name) {
        // Make sure it's a valid import
        if (!name.match(/^imports(\.[a-zA-Z0-9_]+)+$/))
            throw new Error("widget name must start with 'imports.'");
        if (name.match(/^imports\.gi\./))
            throw new Error("cannot import widget from GIR");

        let ctor = eval(name);

        // Make sure it's really a constructor
        if (!ctor || typeof(ctor) != "function")
            throw new Error("widget name is not a constructor");

        // Make sure it's a widget
        let proto = ctor.prototype;
        while (proto && proto != Widget.Widget.prototype)
            proto = proto.__proto__;
        if (!proto)
            throw new Error("widget does not inherit from Widget prototype");

        return ctor;
    },

    expand: function() {
        Tweener.addTween(this._cbox, { x: -Widget.COLLAPSED_WIDTH,
                                       time: ANIMATION_TIME / 2,
                                       transition: "easeOutQuad",
                                       onComplete: this._expandPart1Complete,
                                       onCompleteScope: this });
        this.state = this._widget.state = Widget.STATE_EXPANDING;
    },

    _setWidgetExpanded: function() {
        this._cgroup.hide();
        this._egroup.show();

        if (this._singleActor) {
            this._widget.actor.unparent();
            this._ebox.append(this._widget.actor, Big.BoxPackFlags.NONE);
        }

        if (this._htitle) {
            this._htitle.show();
            this._hline.show();
        }
    },

    _expandPart1Complete: function() {
        this._cbox.x = 0;
        this._setWidgetExpanded();

        if (this._widget.expand) {
            try {
                this._widget.expand();
            } catch (e) {
                logError(e, 'Widget failed to expand');
            }
        }

        this._ebox.x = -Widget.EXPANDED_WIDTH;
        Tweener.addTween(this._ebox, { x: 0,
                                       time: ANIMATION_TIME / 2,
                                       transition: "easeOutQuad",
                                       onComplete: this._expandComplete,
                                       onCompleteScope: this });
    },

    _expandComplete: function() {
        this.state = this._widget.state = Widget.STATE_EXPANDED;
    },

    collapse: function() {
        Tweener.addTween(this._ebox, { x: -Widget.EXPANDED_WIDTH,
                                       time: ANIMATION_TIME / 2,
                                       transition: "easeOutQuad",
                                       onComplete: this._collapsePart1Complete,
                                       onCompleteScope: this });
        this.state = this._widget.state = Widget.STATE_COLLAPSING;
    },

    _setWidgetCollapsed: function() {
        this._egroup.hide();
        this._cgroup.show();

        if (this._singleActor) {
            this._widget.actor.unparent();
            this._cbox.append(this._widget.actor, Big.BoxPackFlags.NONE);
        }

        if (this._htitle) {
            this._htitle.hide();
            this._hline.hide();
        }

        if (this._vtitle)
            this._cbox.height = this._ebox.height;
    },

    _collapsePart1Complete: function() {
        this._ebox.x = 0;
        this._setWidgetCollapsed();

        if (this._widget.collapse) {
            try {
                this._widget.collapse();
            } catch (e) {
                logError(e, 'Widget failed to collapse');
            }
        }

        this._cbox.x = -Widget.COLLAPSED_WIDTH;
        Tweener.addTween(this._cbox, { x: 0,
                                       time: ANIMATION_TIME / 2,
                                       transition: "easeOutQuad",
                                       onComplete: this._collapseComplete,
                                       onCompleteScope: this });
    },

    _collapseComplete: function() {
        this.state = this._widget.state = Widget.STATE_COLLAPSED;
    },

    _popEventHandler: function(actor, event) {
        let type = event.type();

        if (type == Clutter.EventType.ENTER) {
            this._clearPopInTimeout();
            if (this.state == Widget.STATE_COLLAPSED ||
                this.state == Widget.STATE_COLLAPSING) {
                this._popOut();
                return false;
            }
        } else if (type == Clutter.EventType.LEAVE &&
                   (this.state == Widget.STATE_POPPED_OUT ||
                    this.state == Widget.STATE_POPPING_OUT)) {
            // If moving into another actor within this._hbox, let the
            // event be propagated
            let into = event.get_related();
            while (into) {
                if (into == this._hbox)
                    return false;
                into = into.get_parent();
            }

            // Else, moving out of this._hbox
            this._setPopInTimeout();
            return false;
        }

        return false;
    },

    _activationHandler: function() {
        if (this.state == Widget.STATE_POPPED_OUT)
            this._popIn();
    },

    _popOut: function() {
        if (this.state != Widget.STATE_COLLAPSED &&
            this.state != Widget.STATE_COLLAPSING)
            return;

        this._vline.show();
        this._egroup.show();
        this._ebox.x = -Widget.EXPANDED_WIDTH;
        Tweener.addTween(this._ebox, { x: 0,
                                       time: ANIMATION_TIME / 2,
                                       transition: "easeOutQuad",
                                       onComplete: this._popOutComplete,
                                       onCompleteScope: this });
        this.state = this._widget.state = Widget.STATE_POPPING_OUT;

        Main.chrome.addInputRegionActor(this._hbox);
    },

    _popOutComplete: function() {
        this.state = this._widget.state = Widget.STATE_POPPED_OUT;
    },

    _setPopInTimeout: function() {
        this._clearPopInTimeout();
        this._popInTimeout = Mainloop.timeout_add(POP_IN_LAG, Lang.bind(this, function () { this._popIn(); return false; }));
    },

    _clearPopInTimeout: function() {
        if (this._popInTimeout) {
            Mainloop.source_remove(this._popInTimeout);
            delete this._popInTimeout;
        }
    },

    _popIn: function() {
        this._clearPopInTimeout();

        if (this.state != Widget.STATE_POPPED_OUT &&
            this.state != Widget.STATE_POPPING_OUT)
            return;

        Tweener.addTween(this._ebox, { x: -Widget.EXPANDED_WIDTH,
                                       time: ANIMATION_TIME / 2,
                                       transition: "easeOutQuad",
                                       onComplete: this._popInComplete,
                                       onCompleteScope: this });
    },

    _popInComplete: function() {
        this.state = this._widget.state = Widget.STATE_COLLAPSED;
        this._vline.hide();
        this._egroup.hide();
        this._ebox.x = 0;

        Main.chrome.removeInputRegionActor(this._hbox);
    },

    destroy: function() {
        if (this._widget.destroy)
            this._widget.destroy();
    }
};

