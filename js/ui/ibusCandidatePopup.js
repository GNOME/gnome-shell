// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const IBus = imports.gi.IBus;
const Lang = imports.lang;
const St = imports.gi.St;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;

const MAX_CANDIDATES_PER_PAGE = 16;

const CandidateArea = new Lang.Class({
    Name: 'CandidateArea',

    _init: function() {
        this.actor = new St.BoxLayout({ vertical: true,
                                        visible: false });
        this._candidateBoxes = [];
        for (let i = 0; i < MAX_CANDIDATES_PER_PAGE; ++i) {
            let box = new St.BoxLayout({ style_class: 'candidate-box' });
            box._indexLabel = new St.Label({ style_class: 'candidate-index' });
            box._candidateLabel = new St.Label({ style_class: 'candidate-label' });
            box.add(box._indexLabel, { y_fill: false });
            box.add(box._candidateLabel, { y_fill: false });
            this._candidateBoxes.push(box);
            this.actor.add(box);
        }

        this._orientation = -1;
        this._cursorPosition = 0;
    },

    _setOrientation: function(orientation) {
        if (this._orientation == orientation)
            return;

        this._orientation = orientation;

        if (this._orientation == IBus.Orientation.HORIZONTAL)
            this.actor.vertical = false;
        else                    // VERTICAL || SYSTEM
            this.actor.vertical = true;
    },

    setCandidates: function(indexes, candidates, orientation, cursorPosition, cursorVisible) {
        this._setOrientation(orientation);

        for (let i = 0; i < MAX_CANDIDATES_PER_PAGE; ++i) {
            let visible = i < candidates.length;
            let box = this._candidateBoxes[i];
            box.visible = visible;

            if (!visible)
                continue;

            box._indexLabel.text = ((indexes && indexes[i]) ? indexes[i] : '%x'.format(i + 1));
            box._candidateLabel.text = candidates[i];
        }

        this._candidateBoxes[this._cursorPosition].remove_style_pseudo_class('selected');
        this._cursorPosition = cursorPosition;
        if (cursorVisible)
            this._candidateBoxes[cursorPosition].add_style_pseudo_class('selected');
    },
});

const CandidatePopup = new Lang.Class({
    Name: 'CandidatePopup',

    _init: function() {
        this._cursor = new St.Bin({ opacity: 0 });
        Main.uiGroup.add_actor(this._cursor);

        this._boxPointer = new BoxPointer.BoxPointer(St.Side.TOP);
        this._boxPointer.actor.visible = false;
        this._boxPointer.actor.style_class = 'candidate-popup-boxpointer';
        Main.uiGroup.add_actor(this._boxPointer.actor);

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

        this._panelService = null;
    },

    setPanelService: function(panelService) {
        this._panelService = panelService;
        if (!panelService)
            return;

        panelService.connect('set-cursor-location',
                             Lang.bind(this, function(ps, x, y, w, h) {
                                 this._cursor.set_position(x, y);
                                 this._cursor.set_size(w, h);
                             }));
        panelService.connect('update-preedit-text',
                             Lang.bind(this, function(ps, text, cursorPosition, visible) {
                                 this._preeditText.visible = visible;
                                 this._updateVisibility();

                                 this._preeditText.text = text.get_text();

                                 let attrs = text.get_attributes();
                                 if (attrs)
                                     this._setTextAttributes(this._preeditText.clutter_text,
                                                             attrs);
                             }));
        panelService.connect('show-preedit-text',
                             Lang.bind(this, function(ps) {
                                 this._preeditText.show();
                                 this._updateVisibility();
                             }));
        panelService.connect('hide-preedit-text',
                             Lang.bind(this, function(ps) {
                                 this._preeditText.hide();
                                 this._updateVisibility();
                             }));
        panelService.connect('update-auxiliary-text',
                             Lang.bind(this, function(ps, text, visible) {
                                 this._auxText.visible = visible;
                                 this._updateVisibility();

                                 this._auxText.text = text.get_text();
                             }));
        panelService.connect('show-auxiliary-text',
                             Lang.bind(this, function(ps) {
                                 this._auxText.show();
                                 this._updateVisibility();
                             }));
        panelService.connect('hide-auxiliary-text',
                             Lang.bind(this, function(ps) {
                                 this._auxText.hide();
                                 this._updateVisibility();
                             }));
        panelService.connect('update-lookup-table',
                             Lang.bind(this, function(ps, lookupTable, visible) {
                                 this._candidateArea.actor.visible = visible;
                                 this._updateVisibility();

                                 let cursorPos = lookupTable.get_cursor_pos();
                                 let pageSize = lookupTable.get_page_size();
                                 let page = ((cursorPos == 0) ? 0 : Math.floor(cursorPos / pageSize));
                                 let startIndex = page * pageSize;
                                 let endIndex = Math.min((page + 1) * pageSize,
                                                         lookupTable.get_number_of_candidates());
                                 let indexes = [];
                                 let indexLabel;
                                 for (let i = 0; indexLabel = lookupTable.get_label(i); ++i)
                                      indexes.push(indexLabel.get_text());

                                 let candidates = [];
                                 for (let i = startIndex; i < endIndex; ++i)
                                     candidates.push(lookupTable.get_candidate(i).get_text());

                                 this._candidateArea.setCandidates(indexes,
                                                                   candidates,
                                                                   lookupTable.get_orientation(),
                                                                   cursorPos % pageSize,
                                                                   lookupTable.is_cursor_visible());
                             }));
        panelService.connect('show-lookup-table',
                             Lang.bind(this, function(ps) {
                                 this._candidateArea.actor.show();
                                 this._updateVisibility();
                             }));
        panelService.connect('hide-lookup-table',
                             Lang.bind(this, function(ps) {
                                 this._candidateArea.actor.hide();
                                 this._updateVisibility();
                             }));
        panelService.connect('focus-out',
                             Lang.bind(this, function(ps) {
                                 this._boxPointer.hide(BoxPointer.PopupAnimation.NONE);
                             }));
    },

    _updateVisibility: function() {
        let isVisible = (this._preeditText.visible ||
                         this._auxText.visible ||
                         this._candidateArea.actor.visible);

        if (isVisible) {
            this._boxPointer.setPosition(this._cursor, 0);
            this._boxPointer.show(BoxPointer.PopupAnimation.NONE);
            this._boxPointer.actor.raise_top();
        } else {
            this._boxPointer.hide(BoxPointer.PopupAnimation.NONE);
        }
    },

    _setTextAttributes: function(clutterText, ibusAttrList) {
        let attr;
        for (let i = 0; attr = ibusAttrList.get(i); ++i)
            if (attr.get_attr_type() == IBus.AttrType.BACKGROUND)
                clutterText.set_selection(attr.get_start_index(), attr.get_end_index());
    }
});
