// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ViewSelector */

const { Clutter, Gio, GObject, Meta, Shell, St } = imports.gi;
const Signals = imports.signals;

const AppDisplay = imports.ui.appDisplay;
const Main = imports.ui.main;
const OverviewControls = imports.ui.overviewControls;
const Params = imports.misc.params;
const Search = imports.ui.search;
const ShellEntry = imports.ui.shellEntry;
const WorkspacesView = imports.ui.workspacesView;
const EdgeDragAction = imports.ui.edgeDragAction;
const IconGrid = imports.ui.iconGrid;

const SHELL_KEYBINDINGS_SCHEMA = 'org.gnome.shell.keybindings';
var PINCH_GESTURE_THRESHOLD = 0.7;

var ViewPage = {
    WINDOWS: 1,
    APPS: 2,
    SEARCH: 3,
};

var FocusTrap = GObject.registerClass(
class FocusTrap extends St.Widget {
    vfunc_navigate_focus(from, direction) {
        if (direction == St.DirectionType.TAB_FORWARD ||
            direction == St.DirectionType.TAB_BACKWARD)
            return super.vfunc_navigate_focus(from, direction);
        return false;
    }
});

function getTermsForSearchString(searchString) {
    searchString = searchString.replace(/^\s+/g, '').replace(/\s+$/g, '');
    if (searchString == '')
        return [];

    let terms = searchString.split(/\s+/);
    return terms;
}

var TouchpadShowOverviewAction = class {
    constructor(actor) {
        actor.connect('captured-event::touchpad', this._handleEvent.bind(this));
    }

    _handleEvent(actor, event) {
        if (event.type() != Clutter.EventType.TOUCHPAD_PINCH)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_touchpad_gesture_finger_count() != 3)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.END)
            this.emit('activated', event.get_gesture_pinch_scale());

        return Clutter.EVENT_STOP;
    }
};
Signals.addSignalMethods(TouchpadShowOverviewAction.prototype);

var ShowOverviewAction = GObject.registerClass({
    Signals: { 'activated': { param_types: [GObject.TYPE_DOUBLE] } },
}, class ShowOverviewAction extends Clutter.GestureAction {
    _init() {
        super._init();
        this.set_n_touch_points(3);

        global.display.connect('grab-op-begin', () => {
            this.cancel();
        });
    }

    vfunc_gesture_prepare(_actor) {
        return Main.actionMode == Shell.ActionMode.NORMAL &&
               this.get_n_current_points() == this.get_n_touch_points();
    }

    _getBoundingRect(motion) {
        let minX, minY, maxX, maxY;

        for (let i = 0; i < this.get_n_current_points(); i++) {
            let x, y;

            if (motion == true)
                [x, y] = this.get_motion_coords(i);
            else
                [x, y] = this.get_press_coords(i);

            if (i == 0) {
                minX = maxX = x;
                minY = maxY = y;
            } else {
                minX = Math.min(minX, x);
                minY = Math.min(minY, y);
                maxX = Math.max(maxX, x);
                maxY = Math.max(maxY, y);
            }
        }

        return new Meta.Rectangle({ x: minX,
                                    y: minY,
                                    width: maxX - minX,
                                    height: maxY - minY });
    }

    vfunc_gesture_begin(_actor) {
        this._initialRect = this._getBoundingRect(false);
        return true;
    }

    vfunc_gesture_end(_actor) {
        let rect = this._getBoundingRect(true);
        let oldArea = this._initialRect.width * this._initialRect.height;
        let newArea = rect.width * rect.height;
        let areaDiff = newArea / oldArea;

        this.emit('activated', areaDiff);
    }
});

var ViewSelector = GObject.registerClass({
    Signals: {
        'page-changed': {},
        'page-empty': {},
    },
}, class ViewSelector extends Shell.Stack {
    _init(searchEntry, workspaceAdjustment, showAppsButton) {
        super._init({
            name: 'viewSelector',
            x_expand: true,
        });

        this._showAppsButton = showAppsButton;
        this._showAppsButton.connect('notify::checked', this._onShowAppsButtonToggled.bind(this));

        this._activePage = null;

        this._searchActive = false;

        this._entry = searchEntry;
        ShellEntry.addContextMenu(this._entry);

        this._text = this._entry.clutter_text;
        this._text.connect('text-changed', this._onTextChanged.bind(this));
        this._text.connect('key-press-event', this._onKeyPress.bind(this));
        this._text.connect('key-focus-in', () => {
            this._searchResults.highlightDefault(true);
        });
        this._text.connect('key-focus-out', () => {
            this._searchResults.highlightDefault(false);
        });
        this._entry.connect('popup-menu', () => {
            if (!this._searchActive)
                return;

            this._entry.menu.close();
            this._searchResults.popupMenuDefault();
        });
        this._entry.connect('notify::mapped', this._onMapped.bind(this));
        global.stage.connect('notify::key-focus', this._onStageKeyFocusChanged.bind(this));

        this._entry.set_primary_icon(new St.Icon({ style_class: 'search-entry-icon',
                                                   icon_name: 'edit-find-symbolic' }));
        this._clearIcon = new St.Icon({ style_class: 'search-entry-icon',
                                        icon_name: 'edit-clear-symbolic' });

        this._iconClickedId = 0;
        this._capturedEventId = 0;

        this._workspacesDisplay =
            new WorkspacesView.WorkspacesDisplay(workspaceAdjustment);
        this._workspacesPage = this._addPage(this._workspacesDisplay,
                                             _("Windows"), 'focus-windows-symbolic');

        this.appDisplay = new AppDisplay.AppDisplay();
        this._appsPage = this._addPage(this.appDisplay,
                                       _("Applications"), 'view-app-grid-symbolic');

        this._searchResults = new Search.SearchResultsView();
        this._searchPage = this._addPage(this._searchResults,
                                         _("Search"), 'edit-find-symbolic',
                                         { a11yFocus: this._entry });

        // Since the entry isn't inside the results container we install this
        // dummy widget as the last results container child so that we can
        // include the entry in the keynav tab path
        this._focusTrap = new FocusTrap({ can_focus: true });
        this._focusTrap.connect('key-focus-in', () => {
            this._entry.grab_key_focus();
        });
        this._searchResults.add_actor(this._focusTrap);

        global.focus_manager.add_group(this._searchResults);

        this._stageKeyPressId = 0;
        Main.overview.connect('showing', () => {
            this._stageKeyPressId = global.stage.connect('key-press-event',
                                                         this._onStageKeyPress.bind(this));
        });
        Main.overview.connect('hiding', () => {
            if (this._stageKeyPressId != 0) {
                global.stage.disconnect(this._stageKeyPressId);
                this._stageKeyPressId = 0;
            }
        });
        Main.overview.connect('shown', () => {
            // If we were animating from the desktop view to the
            // apps page the workspace page was visible, allowing
            // the windows to animate, but now we no longer want to
            // show it given that we are now on the apps page or
            // search page.
            if (this._activePage != this._workspacesPage) {
                this._workspacesPage.opacity = 0;
                this._workspacesPage.hide();
            }
        });

        Main.wm.addKeybinding('toggle-application-view',
                              new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                              Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                              Shell.ActionMode.NORMAL |
                              Shell.ActionMode.OVERVIEW,
                              this._toggleAppsPage.bind(this));

        Main.wm.addKeybinding('toggle-overview',
                              new Gio.Settings({ schema_id: SHELL_KEYBINDINGS_SCHEMA }),
                              Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                              Shell.ActionMode.NORMAL |
                              Shell.ActionMode.OVERVIEW,
                              Main.overview.toggle.bind(Main.overview));

        let side;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            side = St.Side.RIGHT;
        else
            side = St.Side.LEFT;
        let gesture = new EdgeDragAction.EdgeDragAction(side,
                                                        Shell.ActionMode.NORMAL);
        gesture.connect('activated', () => {
            if (Main.overview.visible)
                Main.overview.hide();
            else
                this.showApps();
        });
        global.stage.add_action(gesture);

        gesture = new ShowOverviewAction();
        gesture.connect('activated', this._pinchGestureActivated.bind(this));
        global.stage.add_action(gesture);

        gesture = new TouchpadShowOverviewAction(global.stage);
        gesture.connect('activated', this._pinchGestureActivated.bind(this));
    }

    _pinchGestureActivated(action, scale) {
        if (scale < PINCH_GESTURE_THRESHOLD)
            Main.overview.show();
    }

    _toggleAppsPage() {
        this._showAppsButton.checked = !this._showAppsButton.checked;
        Main.overview.show();
    }

    showApps() {
        this._showAppsButton.checked = true;
        Main.overview.show();
    }

    show() {
        this.reset();
        this._workspacesDisplay.show(this._showAppsButton.checked);
        this._activePage = null;
        if (this._showAppsButton.checked)
            this._showPage(this._appsPage);
        else
            this._showPage(this._workspacesPage);

        if (!this._workspacesDisplay.activeWorkspaceHasMaximizedWindows())
            Main.overview.fadeOutDesktop();
    }

    animateFromOverview() {
        // Make sure workspace page is fully visible to allow
        // workspace.js do the animation of the windows
        this._workspacesPage.opacity = 255;

        this._workspacesDisplay.animateFromOverview(this._activePage != this._workspacesPage);

        this._showAppsButton.checked = false;

        if (!this._workspacesDisplay.activeWorkspaceHasMaximizedWindows())
            Main.overview.fadeInDesktop();
    }

    setWorkspacesFullGeometry(geom) {
        this._workspacesDisplay.setWorkspacesFullGeometry(geom);
    }

    hide() {
        this.reset();
        this._workspacesDisplay.hide();
    }

    _addPage(actor, name, a11yIcon, params) {
        params = Params.parse(params, { a11yFocus: null });

        let page = new St.Bin({ child: actor });

        if (params.a11yFocus) {
            Main.ctrlAltTabManager.addGroup(params.a11yFocus, name, a11yIcon);
        } else {
            Main.ctrlAltTabManager.addGroup(actor, name, a11yIcon, {
                proxy: this,
                focusCallback: () => this._a11yFocusPage(page),
            });
        }
        page.hide();
        this.add_actor(page);
        return page;
    }

    _fadePageIn() {
        this._activePage.ease({
            opacity: 255,
            duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    _fadePageOut(page) {
        let oldPage = page;
        page.ease({
            opacity: 0,
            duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onStopped: () => this._animateIn(oldPage),
        });
    }

    _animateIn(oldPage) {
        if (oldPage)
            oldPage.hide();

        this.emit('page-empty');

        this._activePage.show();

        if (this._activePage == this._appsPage && oldPage == this._workspacesPage) {
            // Restore opacity, in case we animated via _fadePageOut
            this._activePage.opacity = 255;
            this.appDisplay.animate(IconGrid.AnimationDirection.IN);
        } else {
            this._fadePageIn();
        }
    }

    _animateOut(page) {
        let oldPage = page;
        if (page == this._appsPage &&
            this._activePage == this._workspacesPage &&
            !Main.overview.animationInProgress) {
            this.appDisplay.animate(IconGrid.AnimationDirection.OUT, () => {
                this._animateIn(oldPage);
            });
        } else {
            this._fadePageOut(page);
        }
    }

    _showPage(page) {
        if (!Main.overview.visible)
            return;

        if (page == this._activePage)
            return;

        let oldPage = this._activePage;
        this._activePage = page;
        this.emit('page-changed');

        if (oldPage)
            this._animateOut(oldPage);
        else
            this._animateIn();
    }

    _a11yFocusPage(page) {
        this._showAppsButton.checked = page == this._appsPage;
        page.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
    }

    _onShowAppsButtonToggled() {
        this._showPage(this._showAppsButton.checked
            ? this._appsPage : this._workspacesPage);
    }

    _onStageKeyPress(actor, event) {
        // Ignore events while anything but the overview has
        // pushed a modal (system modals, looking glass, ...)
        if (Main.modalCount > 1)
            return Clutter.EVENT_PROPAGATE;

        let symbol = event.get_key_symbol();

        if (symbol === Clutter.KEY_Escape) {
            if (this._searchActive)
                this.reset();
            else if (this._showAppsButton.checked)
                this._showAppsButton.checked = false;
            else
                Main.overview.hide();
            return Clutter.EVENT_STOP;
        } else if (this._shouldTriggerSearch(symbol)) {
            this.startSearch(event);
        } else if (!this._searchActive && !global.stage.key_focus) {
            if (symbol === Clutter.KEY_Tab || symbol === Clutter.KEY_Down) {
                this._activePage.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
                return Clutter.EVENT_STOP;
            } else if (symbol === Clutter.KEY_ISO_Left_Tab) {
                this._activePage.navigate_focus(null, St.DirectionType.TAB_BACKWARD, false);
                return Clutter.EVENT_STOP;
            }
        }
        return Clutter.EVENT_PROPAGATE;
    }

    _searchCancelled() {
        this._showPage(this._showAppsButton.checked
            ? this._appsPage
            : this._workspacesPage);

        // Leave the entry focused when it doesn't have any text;
        // when replacing a selected search term, Clutter emits
        // two 'text-changed' signals, one for deleting the previous
        // text and one for the new one - the second one is handled
        // incorrectly when we remove focus
        // (https://bugzilla.gnome.org/show_bug.cgi?id=636341) */
        if (this._text.text != '')
            this.reset();
    }

    reset() {
        // Don't drop the key focus on Clutter's side if anything but the
        // overview has pushed a modal (e.g. system modals when activated using
        // the overview).
        if (Main.modalCount <= 1)
            global.stage.set_key_focus(null);

        this._entry.text = '';

        this._text.set_cursor_visible(true);
        this._text.set_selection(0, 0);
    }

    _onStageKeyFocusChanged() {
        let focus = global.stage.get_key_focus();
        let appearFocused = this._entry.contains(focus) ||
                             this._searchResults.contains(focus);

        this._text.set_cursor_visible(appearFocused);

        if (appearFocused)
            this._entry.add_style_pseudo_class('focus');
        else
            this._entry.remove_style_pseudo_class('focus');
    }

    _onMapped() {
        if (this._entry.mapped) {
            // Enable 'find-as-you-type'
            this._capturedEventId = global.stage.connect('captured-event',
                                                         this._onCapturedEvent.bind(this));
            this._text.set_cursor_visible(true);
            this._text.set_selection(0, 0);
        } else {
            // Disable 'find-as-you-type'
            if (this._capturedEventId > 0)
                global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }
    }

    _shouldTriggerSearch(symbol) {
        if (symbol === Clutter.KEY_Multi_key)
            return true;

        if (symbol === Clutter.KEY_BackSpace && this._searchActive)
            return true;

        let unicode = Clutter.keysym_to_unicode(symbol);
        if (unicode == 0)
            return false;

        if (getTermsForSearchString(String.fromCharCode(unicode)).length > 0)
            return true;

        return false;
    }

    startSearch(event) {
        global.stage.set_key_focus(this._text);

        let synthEvent = event.copy();
        synthEvent.set_source(this._text);
        this._text.event(synthEvent, false);
    }

    // the entry does not show the hint
    _isActivated() {
        return this._text.text == this._entry.get_text();
    }

    _onTextChanged() {
        let terms = getTermsForSearchString(this._entry.get_text());

        this._searchActive = terms.length > 0;
        this._searchResults.setTerms(terms);

        if (this._searchActive) {
            this._showPage(this._searchPage);

            this._entry.set_secondary_icon(this._clearIcon);

            if (this._iconClickedId == 0) {
                this._iconClickedId = this._entry.connect('secondary-icon-clicked',
                                                          this.reset.bind(this));
            }
        } else {
            if (this._iconClickedId > 0) {
                this._entry.disconnect(this._iconClickedId);
                this._iconClickedId = 0;
            }

            this._entry.set_secondary_icon(null);
            this._searchCancelled();
        }
    }

    _onKeyPress(entry, event) {
        let symbol = event.get_key_symbol();
        if (symbol === Clutter.KEY_Escape) {
            if (this._isActivated()) {
                this.reset();
                return Clutter.EVENT_STOP;
            }
        } else if (this._searchActive) {
            let arrowNext, nextDirection;
            if (entry.get_text_direction() == Clutter.TextDirection.RTL) {
                arrowNext = Clutter.KEY_Left;
                nextDirection = St.DirectionType.LEFT;
            } else {
                arrowNext = Clutter.KEY_Right;
                nextDirection = St.DirectionType.RIGHT;
            }

            if (symbol === Clutter.KEY_Tab) {
                this._searchResults.navigateFocus(St.DirectionType.TAB_FORWARD);
                return Clutter.EVENT_STOP;
            } else if (symbol === Clutter.KEY_ISO_Left_Tab) {
                this._focusTrap.can_focus = false;
                this._searchResults.navigateFocus(St.DirectionType.TAB_BACKWARD);
                this._focusTrap.can_focus = true;
                return Clutter.EVENT_STOP;
            } else if (symbol === Clutter.KEY_Down) {
                this._searchResults.navigateFocus(St.DirectionType.DOWN);
                return Clutter.EVENT_STOP;
            } else if (symbol == arrowNext && this._text.position == -1) {
                this._searchResults.navigateFocus(nextDirection);
                return Clutter.EVENT_STOP;
            } else if (symbol === Clutter.KEY_Return || symbol === Clutter.KEY_KP_Enter) {
                this._searchResults.activateDefault();
                return Clutter.EVENT_STOP;
            }
        }
        return Clutter.EVENT_PROPAGATE;
    }

    _onCapturedEvent(actor, event) {
        if (event.type() == Clutter.EventType.BUTTON_PRESS) {
            let source = event.get_source();
            if (source != this._text &&
                this._text.has_key_focus() &&
                this._text.text == '' &&
                !this._text.has_preedit() &&
                !Main.layoutManager.keyboardBox.contains(source)) {
                // the user clicked outside after activating the entry, but
                // with no search term entered and no keyboard button pressed
                // - cancel the search
                this.reset();
            }
        }

        return Clutter.EVENT_PROPAGATE;
    }

    getActivePage() {
        if (this._activePage == this._workspacesPage)
            return ViewPage.WINDOWS;
        else if (this._activePage == this._appsPage)
            return ViewPage.APPS;
        else
            return ViewPage.SEARCH;
    }
});
