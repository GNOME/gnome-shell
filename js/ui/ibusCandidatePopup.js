// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const IBus = imports.gi.IBus;
const Lang = imports.lang;
const Signals = imports.signals;
const St = imports.gi.St;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;

var MAX_CANDIDATES_PER_PAGE = 16;

var DEFAULT_INDEX_LABELS = [ '1', '2', '3', '4', '5', '6', '7', '8',
                             '9', '0', 'a', 'b', 'c', 'd', 'e', 'f' ];

var CandidateArea = new Lang.Class({
    Name: 'CandidateArea',

    _init() {
        this.actor = new St.BoxLayout({ vertical: true,
                                        reactive: true,
                                        visible: false });
        this._candidateBoxes = [];
        for (let i = 0; i < MAX_CANDIDATES_PER_PAGE; ++i) {
            let box = new St.BoxLayout({ style_class: 'candidate-box',
                                         reactive: true,
                                         track_hover: true });
            box._indexLabel = new St.Label({ style_class: 'candidate-index' });
            box._candidateLabel = new St.Label({ style_class: 'candidate-label' });
            box.add(box._indexLabel, { y_fill: false });
            box.add(box._candidateLabel, { y_fill: false });
            this._candidateBoxes.push(box);
            this.actor.add(box);

            let j = i;
            box.connect('button-release-event', (actor, event) => {
                this.emit('candidate-clicked', j, event.get_button(), event.get_state());
                return Clutter.EVENT_PROPAGATE;
            });
        }

        this.actor.connect('scroll-event', (actor, event) => {
            let direction = event.get_scroll_direction();
            switch(direction) {
            case Clutter.ScrollDirection.UP:
                this.emit('cursor-up');
                break;
            case Clutter.ScrollDirection.DOWN:
                this.emit('cursor-down');
                break;
            };
            return Clutter.EVENT_PROPAGATE;
        });

        this._buttonBox = new St.BoxLayout({ style_class: 'candidate-page-button-box' });

        this._previousButton = new St.Button({ style_class: 'candidate-page-button candidate-page-button-previous button' });
        this._previousButton.child = new St.Icon({ style_class: 'candidate-page-button-icon' });
        this._buttonBox.add(this._previousButton, { expand: true });

        this._nextButton = new St.Button({ style_class: 'candidate-page-button candidate-page-button-next button' });
        this._nextButton.child = new St.Icon({ style_class: 'candidate-page-button-icon' });
        this._buttonBox.add(this._nextButton, { expand: true });

        this.actor.add(this._buttonBox);

        this._previousButton.connect('clicked', () => {
            this.emit('previous-page');
        });
        this._nextButton.connect('clicked', () => {
            this.emit('next-page');
        });

        this._orientation = -1;
        this._cursorPosition = 0;
    },

    setOrientation(orientation) {
        if (this._orientation == orientation)
            return;

        this._orientation = orientation;

        if (this._orientation == IBus.Orientation.HORIZONTAL) {
            this.actor.vertical = false;
            this.actor.remove_style_class_name('vertical');
            this.actor.add_style_class_name('horizontal');
            this._previousButton.child.icon_name = 'go-previous-symbolic';
            this._nextButton.child.icon_name = 'go-next-symbolic';
        } else {                // VERTICAL || SYSTEM
            this.actor.vertical = true;
            this.actor.add_style_class_name('vertical');
            this.actor.remove_style_class_name('horizontal');
            this._previousButton.child.icon_name = 'go-up-symbolic';
            this._nextButton.child.icon_name = 'go-down-symbolic';
        }
    },

    setCandidates(indexes, candidates, cursorPosition, cursorVisible) {
        for (let i = 0; i < MAX_CANDIDATES_PER_PAGE; ++i) {
            let visible = i < candidates.length;
            let box = this._candidateBoxes[i];
            box.visible = visible;

            if (!visible)
                continue;

            box._indexLabel.text = ((indexes && indexes[i]) ? indexes[i] : DEFAULT_INDEX_LABELS[i]);
            box._candidateLabel.text = candidates[i];
        }

        this._candidateBoxes[this._cursorPosition].remove_style_pseudo_class('selected');
        this._cursorPosition = cursorPosition;
        if (cursorVisible)
            this._candidateBoxes[cursorPosition].add_style_pseudo_class('selected');
    },

    updateButtons(wrapsAround, page, nPages) {
        if (nPages < 2) {
            this._buttonBox.hide();
            return;
        }
        this._buttonBox.show();
        this._previousButton.reactive = wrapsAround || page > 0;
        this._nextButton.reactive = wrapsAround || page < nPages - 1;
    },
});
Signals.addSignalMethods(CandidateArea.prototype);

var CandidatePopup = new Lang.Class({
    Name: 'CandidatePopup',

    _init() {
        this._boxPointer = new BoxPointer.BoxPointer(St.Side.TOP);
        this._boxPointer.actor.visible = false;
        this._boxPointer.actor.style_class = 'candidate-popup-boxpointer';
        Main.layoutManager.addChrome(this._boxPointer.actor);

        let box = new St.BoxLayout({ style_class: 'candidate-popup-content',
                                     vertical: true });
        this._boxPointer.bin.set_child(box);

        this._preeditText = new St.Label({ style_class: 'candidate-popup-text',
                                           visible: false });
        box.add(this._preeditText);

        this._auxText = new St.Label({ style_class: 'candidate-popup-text',
                                       visible: false });
        box.add(this._auxText);

        this._candidateArea = new CandidateArea();
        box.add(this._candidateArea.actor);

        this._candidateArea.connect('previous-page', () => {
            this._panelService.page_up();
        });
        this._candidateArea.connect('next-page', () => {
            this._panelService.page_down();
        });

        this._candidateArea.connect('cursor-up', () => {
            this._panelService.cursor_up();
        });
        this._candidateArea.connect('cursor-down', () => {
            this._panelService.cursor_down();
        });

        this._candidateArea.connect('candidate-clicked', () => {
            this._panelService.candidate_clicked(index, button, state);
        });

        this._panelService = null;
    },

    setPanelService(panelService) {
        this._panelService = panelService;
        if (!panelService)
            return;

        panelService.connect('set-cursor-location', (ps, x, y, w, h) => {
            this._setDummyCursorGeometry(x, y, w, h);
        });
        try {
            panelService.connect('set-cursor-location-relative', (ps, x, y, w, h) => {
                if (!global.display.focus_window)
                    return;
                let window = global.display.focus_window.get_compositor_private();
                this._setDummyCursorGeometry(window.x + x, window.y + y, w, h);
            });
        } catch(e) {
            // Only recent IBus versions have support for this signal
            // which is used for wayland clients. In order to work
            // with older IBus versions we can silently ignore the
            // signal's absence.
        }
        panelService.connect('update-preedit-text', (ps, text, cursorPosition, visible) => {
            this._preeditText.visible = visible;
            this._updateVisibility();

            this._preeditText.text = text.get_text();

            let attrs = text.get_attributes();
            if (attrs)
                this._setTextAttributes(this._preeditText.clutter_text,
                                        attrs);
        });
        panelService.connect('show-preedit-text', ps => {
            this._preeditText.show();
            this._updateVisibility();
        });
        panelService.connect('hide-preedit-text', ps => {
            this._preeditText.hide();
            this._updateVisibility();
        });
        panelService.connect('update-auxiliary-text', (ps, text, visible) => {
            this._auxText.visible = visible;
            this._updateVisibility();

            this._auxText.text = text.get_text();
        });
        panelService.connect('show-auxiliary-text', ps => {
            this._auxText.show();
            this._updateVisibility();
        });
        panelService.connect('hide-auxiliary-text', ps => {
            this._auxText.hide();
            this._updateVisibility();
        });
        panelService.connect('update-lookup-table', (ps, lookupTable, visible) => {
            this._candidateArea.actor.visible = visible;
            this._updateVisibility();

            let nCandidates = lookupTable.get_number_of_candidates();
            let cursorPos = lookupTable.get_cursor_pos();
            let pageSize = lookupTable.get_page_size();
            let nPages = Math.ceil(nCandidates / pageSize);
            let page = ((cursorPos == 0) ? 0 : Math.floor(cursorPos / pageSize));
            let startIndex = page * pageSize;
            let endIndex = Math.min((page + 1) * pageSize, nCandidates);

            let indexes = [];
            let indexLabel;
            for (let i = 0; indexLabel = lookupTable.get_label(i); ++i)
                 indexes.push(indexLabel.get_text());

            Main.keyboard.resetSuggestions();

            let candidates = [];
            for (let i = startIndex; i < endIndex; ++i) {
                candidates.push(lookupTable.get_candidate(i).get_text());

                Main.keyboard.addSuggestion(lookupTable.get_candidate(i).get_text(), () => {
                    let index = i;
                    this._panelService.candidate_clicked(index, 1, 0);
                });
            }

            this._candidateArea.setCandidates(indexes,
                                              candidates,
                                              cursorPos % pageSize,
                                              lookupTable.is_cursor_visible());
            this._candidateArea.setOrientation(lookupTable.get_orientation());
            this._candidateArea.updateButtons(lookupTable.is_round(), page, nPages);
        });
        panelService.connect('show-lookup-table', ps => {
            this._candidateArea.actor.show();
            this._updateVisibility();
        });
        panelService.connect('hide-lookup-table', ps => {
            this._candidateArea.actor.hide();
            this._updateVisibility();
        });
        panelService.connect('focus-out', ps => {
            this._boxPointer.hide(BoxPointer.PopupAnimation.NONE);
            Main.keyboard.resetSuggestions();
        });
    },

    _setDummyCursorGeometry(x, y, w, h) {
        Main.layoutManager.setDummyCursorGeometry(x, y, w, h);
        if (this._boxPointer.actor.visible)
            this._boxPointer.setPosition(Main.layoutManager.dummyCursor, 0);
    },

    _updateVisibility() {
        let isVisible = (!Main.keyboard.visible &&
                         (this._preeditText.visible ||
                          this._auxText.visible ||
                          this._candidateArea.actor.visible));

        if (isVisible) {
            this._boxPointer.setPosition(Main.layoutManager.dummyCursor, 0);
            this._boxPointer.show(BoxPointer.PopupAnimation.NONE);
            this._boxPointer.actor.raise_top();
        } else {
            this._boxPointer.hide(BoxPointer.PopupAnimation.NONE);
        }
    },

    _setTextAttributes(clutterText, ibusAttrList) {
        let attr;
        for (let i = 0; attr = ibusAttrList.get(i); ++i)
            if (attr.get_attr_type() == IBus.AttrType.BACKGROUND)
                clutterText.set_selection(attr.get_start_index(), attr.get_end_index());
    }
});
