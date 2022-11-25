// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported SearchController */

const { Clutter, GObject, St } = imports.gi;

const Main = imports.ui.main;
const Search = imports.ui.search;
const ShellEntry = imports.ui.shellEntry;

var FocusTrap = GObject.registerClass(
class FocusTrap extends St.Widget {
    vfunc_navigate_focus(from, direction) {
        if (direction === St.DirectionType.TAB_FORWARD ||
            direction === St.DirectionType.TAB_BACKWARD)
            return super.vfunc_navigate_focus(from, direction);
        return false;
    }
});

function getTermsForSearchString(searchString) {
    searchString = searchString.replace(/^\s+/g, '').replace(/\s+$/g, '');
    if (searchString === '')
        return [];
    return searchString.split(/\s+/);
}

var SearchController = GObject.registerClass({
    Properties: {
        'search-active': GObject.ParamSpec.boolean(
            'search-active', 'search-active', 'search-active',
            GObject.ParamFlags.READABLE,
            false),
    },
}, class SearchController extends St.Widget {
    _init(searchEntry, showAppsButton) {
        super._init({
            name: 'searchController',
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
            visible: false,
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
        global.stage.connectObject('notify::key-focus',
            this._onStageKeyFocusChanged.bind(this), this);

        this._entry.set_primary_icon(new St.Icon({
            style_class: 'search-entry-icon',
            icon_name: 'edit-find-symbolic',
        }));
        this._clearIcon = new St.Icon({
            style_class: 'search-entry-icon',
            icon_name: 'edit-clear-symbolic',
        });

        this._iconClickedId = 0;
        this._capturedEventId = 0;

        this._searchResults = new Search.SearchResultsView();
        this.add_child(this._searchResults);
        Main.ctrlAltTabManager.addGroup(this._entry, _('Search'), 'edit-find-symbolic');

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
            this._stageKeyPressId =
                global.stage.connect('key-press-event', this._onStageKeyPress.bind(this));
        });
        Main.overview.connect('hiding', () => {
            if (this._stageKeyPressId !== 0) {
                global.stage.disconnect(this._stageKeyPressId);
                this._stageKeyPressId = 0;
            }
        });
    }

    prepareToEnterOverview() {
        this.reset();
        this._setSearchActive(false);
    }

    vfunc_unmap() {
        this.reset();

        super.vfunc_unmap();
    }

    _setSearchActive(searchActive) {
        if (this._searchActive === searchActive)
            return;

        this._searchActive = searchActive;
        this.notify('search-active');
    }

    _onShowAppsButtonToggled() {
        this._setSearchActive(false);
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
        }
        return Clutter.EVENT_PROPAGATE;
    }

    _searchCancelled() {
        this._setSearchActive(false);

        // Leave the entry focused when it doesn't have any text;
        // when replacing a selected search term, Clutter emits
        // two 'text-changed' signals, one for deleting the previous
        // text and one for the new one - the second one is handled
        // incorrectly when we remove focus
        // (https://bugzilla.gnome.org/show_bug.cgi?id=636341) */
        if (this._text.text !== '')
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
            this._capturedEventId =
                global.stage.connect('captured-event', this._onCapturedEvent.bind(this));
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
        if (unicode === 0)
            return false;

        if (getTermsForSearchString(String.fromCharCode(unicode)).length > 0)
            return true;

        return false;
    }

    startSearch(event) {
        global.stage.set_key_focus(this._text);
        this._text.event(event, false);
    }

    // the entry does not show the hint
    _isActivated() {
        return this._text.text === this._entry.get_text();
    }

    _onTextChanged() {
        let terms = getTermsForSearchString(this._entry.get_text());

        const searchActive = terms.length > 0;
        this._searchResults.setTerms(terms);

        if (searchActive) {
            this._setSearchActive(true);

            this._entry.set_secondary_icon(this._clearIcon);

            if (this._iconClickedId === 0) {
                this._iconClickedId =
                    this._entry.connect('secondary-icon-clicked', this.reset.bind(this));
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
            if (entry.get_text_direction() === Clutter.TextDirection.RTL) {
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
            } else if (symbol === arrowNext && this._text.position === -1) {
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
        if (event.type() === Clutter.EventType.BUTTON_PRESS) {
            const targetActor = global.stage.get_event_actor(event);
            if (targetActor !== this._text &&
                this._text.has_key_focus() &&
                this._text.text === '' &&
                !this._text.has_preedit() &&
                !Main.layoutManager.keyboardBox.contains(targetActor)) {
                // the user clicked outside after activating the entry, but
                // with no search term entered and no keyboard button pressed
                // - cancel the search
                this.reset();
            }
        }

        return Clutter.EVENT_PROPAGATE;
    }

    get searchActive() {
        return this._searchActive;
    }
});
