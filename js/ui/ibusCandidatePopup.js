// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const IBus = imports.gi.IBus;
const Lang = imports.lang;
const St = imports.gi.St;

const BoxPointer = imports.ui.boxpointer;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;

const MAX_CANDIDATES_PER_PAGE = 16;

const CandidateArea = new Lang.Class({
    Name: 'CandidateArea',
    Extends: PopupMenu.PopupBaseMenuItem,

    _init: function() {
        this.parent({ reactive: false });

        // St.Table exhibits some sizing problems so let's go with a
        // clutter layout manager for now.
        this._table = new Clutter.Actor();
        this.addActor(this._table);

        this._tableLayout = new Clutter.TableLayout();
        this._table.set_layout_manager(this._tableLayout);

        this._indexLabels = [];
        this._candidateLabels = [];
        for (let i = 0; i < MAX_CANDIDATES_PER_PAGE; ++i) {
            this._indexLabels.push(new St.Label({ style_class: 'candidate-index' }));
            this._candidateLabels.push(new St.Label({ style_class: 'candidate-label' }));
        }

        this._orientation = -1;
        this._cursorPosition = 0;
    },

    _setOrientation: function(orientation) {
        if (this._orientation == orientation)
            return;

        this._orientation = orientation;

        this._table.remove_all_children();

        if (this._orientation == IBus.Orientation.HORIZONTAL)
            for (let i = 0; i < MAX_CANDIDATES_PER_PAGE; ++i) {
                this._tableLayout.pack(this._indexLabels[i], i*2, 0);
                this._tableLayout.pack(this._candidateLabels[i], i*2 + 1, 0);
            }
        else                    // VERTICAL || SYSTEM
            for (let i = 0; i < MAX_CANDIDATES_PER_PAGE; ++i) {
                this._tableLayout.pack(this._indexLabels[i], 0, i);
                this._tableLayout.pack(this._candidateLabels[i], 1, i);
            }
    },

    setCandidates: function(indexes, candidates, orientation, cursorPosition, cursorVisible) {
        this._setOrientation(orientation);

        for (let i = 0; i < MAX_CANDIDATES_PER_PAGE; ++i) {
            let visible = i < candidates.length;
            this._indexLabels[i].visible = visible;
            this._candidateLabels[i].visible = visible;

            if (!visible)
                continue;

            this._indexLabels[i].text = ((indexes && indexes[i]) ? indexes[i] : '%x.'.format(i + 1));
            this._candidateLabels[i].text = candidates[i];
        }

        this._candidateLabels[this._cursorPosition].remove_style_pseudo_class('selected');
        this._cursorPosition = cursorPosition;
        if (cursorVisible)
            this._candidateLabels[cursorPosition].add_style_pseudo_class('selected');
    },
});

const CandidatePopup = new Lang.Class({
    Name: 'CandidatePopup',
    Extends: PopupMenu.PopupMenu,

    _init: function() {
        this._cursor = new St.Bin({ opacity: 0 });
        Main.uiGroup.add_actor(this._cursor);

        this.parent(this._cursor, 0, St.Side.TOP);
        this.actor.hide();
        Main.uiGroup.add_actor(this.actor);

        this._preeditTextItem = new PopupMenu.PopupMenuItem('', { reactive: false });
        this._preeditTextItem.actor.hide();
        this.addMenuItem(this._preeditTextItem);

        this._auxTextItem = new PopupMenu.PopupMenuItem('', { reactive: false });
        this._auxTextItem.actor.hide();
        this.addMenuItem(this._auxTextItem);

        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        this._lookupTableItem = new CandidateArea();
        this._lookupTableItem.actor.hide();
        this.addMenuItem(this._lookupTableItem);

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
                                 if (visible)
                                     this._preeditTextItem.actor.show();
                                 else
                                     this._preeditTextItem.actor.hide();
                                 this._updateVisibility();

                                 this._preeditTextItem.actor.label_actor.text = text.get_text();

                                 let attrs = text.get_attributes();
                                 if (attrs)
                                     this._setTextAttributes(this._preeditTextItem.actor.label_actor.clutter_text,
                                                             attrs);
                             }));
        panelService.connect('show-preedit-text',
                             Lang.bind(this, function(ps) {
                                 this._preeditTextItem.actor.show();
                                 this._updateVisibility();
                             }));
        panelService.connect('hide-preedit-text',
                             Lang.bind(this, function(ps) {
                                 this._preeditTextItem.actor.hide();
                                 this._updateVisibility();
                             }));
        panelService.connect('update-auxiliary-text',
                             Lang.bind(this, function(ps, text, visible) {
                                 if (visible)
                                     this._auxTextItem.actor.show();
                                 else
                                     this._auxTextItem.actor.hide();
                                 this._updateVisibility();

                                 this._auxTextItem.actor.label_actor.text = text.get_text();
                             }));
        panelService.connect('show-auxiliary-text',
                             Lang.bind(this, function(ps) {
                                 this._auxTextItem.actor.show();
                                 this._updateVisibility();
                             }));
        panelService.connect('hide-auxiliary-text',
                             Lang.bind(this, function(ps) {
                                 this._auxTextItem.actor.hide();
                                 this._updateVisibility();
                             }));
        panelService.connect('update-lookup-table',
                             Lang.bind(this, function(ps, lookupTable, visible) {
                                 if (visible)
                                     this._lookupTableItem.actor.show();
                                 else
                                     this._lookupTableItem.actor.hide();
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

                                 this._lookupTableItem.setCandidates(indexes,
                                                                     candidates,
                                                                     lookupTable.get_orientation(),
                                                                     cursorPos % pageSize,
                                                                     lookupTable.is_cursor_visible());
                             }));
        panelService.connect('show-lookup-table',
                             Lang.bind(this, function(ps) {
                                 this._lookupTableItem.actor.show();
                                 this._updateVisibility();
                             }));
        panelService.connect('hide-lookup-table',
                             Lang.bind(this, function(ps) {
                                 this._lookupTableItem.actor.hide();
                                 this._updateVisibility();
                             }));
        panelService.connect('focus-out',
                             Lang.bind(this, function(ps) {
                                 this.close(BoxPointer.PopupAnimation.NONE);
                             }));
    },

    _updateVisibility: function() {
        let isVisible = (this._preeditTextItem.actor.visible ||
                         this._auxTextItem.actor.visible ||
                         this._lookupTableItem.actor.visible);

        if (isVisible)
            this.open(BoxPointer.PopupAnimation.NONE);
        else
            this.close(BoxPointer.PopupAnimation.NONE);
    },

    _setTextAttributes: function(clutterText, ibusAttrList) {
        let attr;
        for (let i = 0; attr = ibusAttrList.get(i); ++i)
            if (attr.get_attr_type() == IBus.AttrType.BACKGROUND)
                clutterText.set_selection(attr.get_start_index(), attr.get_end_index());
    }
});
