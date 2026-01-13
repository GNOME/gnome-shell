import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import St from 'gi://St';

export const MetaWindowEmbedded = GObject.registerClass({
}, class MetaWindowEmbedded extends St.Widget {
    _init(params) {
        super._init({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
            reactive: true,
            can_focus: true,
            ...params,
        });

        this._connectEventHandlers();
        this._setupFocusManagement();

        this.connect('destroy', () => {
            if (this._buttonPressId)
                global.stage.disconnect(this._buttonPressId);

            if (this._keyFocusId)
                global.stage.disconnect(this._keyFocusId);

            this.setMetaWindow(null);
        });
    }

    _connectEventHandlers() {
        // Forward button press events to embedded window and grab keyboard focus
        this.connect('button-press-event', (_actor, event) => {
            this.grab_key_focus();
            return this._forwardEvent(event);
        });

        // Forward all other input events to the embedded window
        this.connect('button-release-event', (_actor, event) => this._forwardEvent(event));
        this.connect('motion-event', (_actor, event) => this._forwardEvent(event));
        this.connect('scroll-event', (_actor, event) => this._forwardEvent(event));
        this.connect('key-press-event', (_actor, event) => this._forwardEvent(event));
        this.connect('key-release-event', (_actor, event) => this._forwardEvent(event));
    }

    _setupFocusManagement() {
        this._keyFocusId = global.stage.connect('notify::key-focus', () => {
            const focusedActor = global.stage.get_key_focus();
            if (!focusedActor)
                return;

            // When focus enters this widget, give input focus to the embedded window
            if (this.contains(focusedActor)) {
                this._setFocus();
            } else {
                // Focus moved outside - save it so we can restore it later
                this._previousKeyFocus = focusedActor;
            }
        });

        // Restore previous focus when clicking outside this widget
        this._buttonPressId = global.stage.connect('button-press-event', () =>
            this._previousKeyFocus?.grab_key_focus());
    }

    _forwardEvent(event) {
        if (!this._metaWindow)
            return Clutter.EVENT_PROPAGATE;

        const context = global.get_context();
        const compositor = context.get_wayland_compositor();
        if (!compositor)
            return Clutter.EVENT_PROPAGATE;

        compositor.handle_event(event);

        return Clutter.EVENT_STOP;
    }

    _setFocus() {
        if (!this._metaWindow)
            return Clutter.EVENT_PROPAGATE;

        const context = global.get_context();
        const compositor = context.get_wayland_compositor();
        if (!compositor)
            return Clutter.EVENT_PROPAGATE;

        compositor.set_input_focus(this._metaWindow);

        return Clutter.EVENT_STOP;
    }

    setMetaWindow(metaWindow) {
        if (this._metaWindow === metaWindow)
            return;

        log(`MetaWindowEmbedded: Setting MetaWindow`);

        // Unparent old window actor
        if (this._windowActor) {
            const parent = this._windowActor.get_parent();
            if (parent === this)
                this.remove_child(this._windowActor);

            global.stage.remove_actor_from_bypass_grab(this._windowActor);

            this._windowActor = null;
        }

        this._metaWindow = metaWindow;
        if (!this._metaWindow)
            return;

        this._windowActor = this._metaWindow.get_compositor_private();
        if (!this._windowActor) {
            log(`MetaWindowEmbedded: Invalid windowActor`);
            return;
        }

        global.stage.add_actor_to_bypass_grab(this._windowActor);

        this._reparentWindowActor();
    }

    _reparentWindowActor() {
        log(`MetaWindowEmbedded: Reparenting WindowActor`);

        const parent = this._windowActor.get_parent();
        if (parent)
            parent.remove_child(this._windowActor);

        this.add_child(this._windowActor);

        // Set initial size and position
        try {
            const [width, height] = this.get_size();
            if (width > 0 && height > 0)
                this._metaWindow.configure(0, 0, width, height);
        } catch (e) {
            log(`Error configuring window: ${e}`);
        }

        log(`MetaWindowEmbedded: Window actor reparented`);
    }

    vfunc_allocate(box) {
        super.vfunc_allocate(box);

        // Update MetaWindow position/size based on allocation
        if (this._metaWindow) {
            const width = box.get_width();
            const height = box.get_height();

            this._metaWindow.configure(0, 0, width, height);
        }
    }
});
