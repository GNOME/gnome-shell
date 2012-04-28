// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/*
 * Copyright 2012 Red Hat, Inc.
 * Copyright 2012 Peng Huang <shawn.p.huang@gmail.com>
 * Copyright 2012 Takao Fujiwara <tfujiwar@redhat.com>
 * Copyright 2012 Tiger Soldier <tigersoldi@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

const Cairo = imports.cairo;
const St = imports.gi.St;
const GLib = imports.gi.GLib;
const IBus = imports.gi.IBus;
const Lang = imports.lang;
const Signals = imports.signals;
const Main = imports.ui.main;
const Shell = imports.gi.Shell;
const BoxPointer = imports.ui.boxpointer;

const ORIENTATION_HORIZONTAL  = 0;
const ORIENTATION_VERTICAL    = 1;
const ORIENTATION_SYSTEM      = 2;

const topLabelStyle = 'padding: .1em 0em';
const candidateLabelStyle = 'padding: .1em 0em .1em 0em';
const candidateTextStyle = 'padding: .1em 0em .1em 0em';
const separatorStyle = 'height: 2px; padding: 0em';

function StCandidateArea(orientation) {
    this._init(orientation);
}

StCandidateArea.prototype = {
    _init: function(orientation) {
        this.actor = new St.BoxLayout({ style_class: 'candidate-area' });
        this._orientation = orientation;
        this._labels = [];
        this._labelBoxes = [];
        this._createUI();
    },

    _removeOldWidgets: function() {
        this.actor.destroy_all_children();
        this._labels = [];
        this._labelBoxes = [];
    },

    _createUI: function() {
        let vbox = null;
        let hbox = null;
        if (this._orientation == ORIENTATION_VERTICAL) {
            vbox = new St.BoxLayout({ vertical: true,
                                      style_class: 'candidate-vertical' });
            this.actor.add_child(vbox,
                                 { expand: true,
                                   x_fill: true,
                                   y_fill: true
                                 });
        } else {
            hbox = new St.BoxLayout({ vertical: false,
                                      style_class: 'candidate-horizontal' });
            this.actor.add_child(hbox,
                                 { expand: true,
                                   x_fill: true,
                                   y_fill: true
                                 });
        }
        for (let i = 0; i < 16; i++) {
            let label1 = new St.Label({ text: '1234567890abcdef'.charAt(i) + '.',
                                        style_class: 'popup-menu-item',
                                        style: candidateLabelStyle,
                                        reactive: true });

            let label2 = new St.Label({ text: '' ,
                                        style_class: 'popup-menu-item',
                                        style: candidateTextStyle,
                                        reactive: true });

            if (this._orientation == ORIENTATION_VERTICAL) {
                let candidateHBox = new St.BoxLayout({vertical: false});
                let labelBox = new St.Bin({ style_class: 'candidate-hlabel-content' });
                labelBox.set_child(label1);
                labelBox.set_fill(true, true);
                let textBox = new St.Bin({ style_class: 'candidate-htext-content' });

                textBox.set_child(label2);
                textBox.set_fill(true, true);
                candidateHBox.add_child(labelBox,
                                        { expand: false,
                                          x_fill: false,
                                          y_fill: true
                                        });
                candidateHBox.add_child(textBox,
                                        { expand: true,
                                          x_fill: true,
                                          y_fill: true
                                        });
                vbox.add_child(candidateHBox);
                this._labelBoxes.push(candidateHBox);
            } else {
                let candidateHBox = new St.BoxLayout({ style_class: 'candidate-vcontent',
                                                       vertical: false });
                candidateHBox.add_child(label1);
                candidateHBox.add_child(label2);
                hbox.add_child(candidateHBox);
                this._labelBoxes.push(candidateHBox);
            }

            this._labels.push([label1, label2]);
        }

        for (let i = 0; i < this._labels.length; i++) {
            for(let j = 0; j < this._labels[i].length; j++) {
                let widget = this._labels[i][j];
                widget.candidateIndex = i;
                widget.connect('button-press-event',
                               Lang.bind(this, function (widget, event) {
                                   this._candidateClickedCB(widget, event);
                               }));
                widget.connect('enter-event',
                               function(widget, event) {
                                   widget.add_style_pseudo_class('hover');
                               });
                widget.connect('leave-event',
                               function(widget, event) {
                                   widget.remove_style_pseudo_class('hover');
                               });
            }
        }
    },

    _recreateUI: function() {
        this._removeOldWidgets();
        this._createUI();
    },

    _candidateClickedCB: function(widget, event) {
        this.emit('candidate-clicked',
                  widget.candidateIndex,
                  event.get_button(),
                  event.get_state());
    },

    setLabels: function(labels) {
        if (!labels || labels.length == 0) {
            for (let i = 0; i < 16; i++) {
                this._labels[i][0].set_text('1234567890abcdef'.charAt(i) + '.');
            }
            return;
        }

        for (let i = 0; i < labels.length && i < this._labels.length; i++) {
            /* Use a ClutterActor attribute of Shell's theme instead of
             * Pango.AttrList for the lookup window GUI and
             * can ignore 'attrs' simply from IBus engines?
             */
            let [text, attrs] = labels[i];
            this._labels[i][0].set_text(text);
        }
    },

    setCandidates: function(candidates, focusCandidate, showCursor) {
        if (focusCandidate == undefined) {
            focusCandidate = 0;
        }
        if (showCursor == undefined) {
            showCursor = true;
        }
        if (candidates.length > this._labels.length) {
            assert();
        }

        for (let i = 0; i < candidates.length; i++) {
            /* Use a ClutterActor attribute of Shell's theme instead of
             * Pango.AttrList for the lookup window GUI and
             * can ignore 'attrs' simply from IBus engines?
             */
            let [text, attrs] = candidates[i];
            if (i == focusCandidate && showCursor) {
                this._labels[i][1].add_style_pseudo_class('active');
            } else {
                this._labels[i][1].remove_style_pseudo_class('active');
            }
            this._labels[i][1].set_text(text);
            this._labelBoxes[i].show();
        }

        for (let i = this._labelBoxes.length - 1; i >= candidates.length; i--) {
            this._labelBoxes[i].hide();
        }
    },

    setOrientation: function(orientation) {
        if (orientation == this._orientation)
            return;
        this._orientation = orientation;
        this._recreateUI();
    },

    showAll: function() {
        this.actor.show();
    },

    hideAll: function() {
        this.actor.hide();
    },
};

Signals.addSignalMethods(StCandidateArea.prototype);

function CandidatePanel() {
    this._init();
}

CandidatePanel.prototype = {
    _init: function() {
        this._orientation = ORIENTATION_VERTICAL;
        this._currentOrientation = this._orientation;
        this._preeditVisible = false;
        this._auxStringVisible = false;
        this._lookupTableVisible = false;
        this._lookupTable = null;

        this._cursorLocation = [0, 0, 0, 0];
        this._movedCursorLocation = null;

        this._initSt();

    },

    _initSt: function() {
        this._arrowSide = St.Side.TOP;
        this._arrowAlignment = 0.0;
        this._boxPointer = new BoxPointer.BoxPointer(this._arrowSide,
                                                     { x_fill: true,
                                                       y_fill: true,
                                                       x_align: St.Align.START });
        this.actor = this._boxPointer.actor;
        this.actor._delegate = this;
        this.actor.style_class = 'popup-menu-boxpointer';
        this.actor.add_style_class_name('popup-menu');
        this.actor.add_style_class_name('candidate-panel');
        this._cursorActor = new Shell.GenericContainer();
        Main.uiGroup.add_actor(this.actor);
        Main.uiGroup.add_actor(this._cursorActor);

        this._stCandidatePanel = new St.BoxLayout({ style_class: 'candidate-panel',
                                                    vertical: true });
        this._boxPointer.bin.set_child(this._stCandidatePanel);

        this._stPreeditLabel = new St.Label({ style_class: 'popup-menu-item',
                                              style: topLabelStyle,
                                              text: '' });
        if (!this._preeditVisible) {
            this._stPreeditLabel.hide();
        }
        this._stAuxLabel = new St.Label({ style_class: 'popup-menu-item',
                                          style: topLabelStyle,
                                          text: '' });
        if (!this._auxVisible) {
            this._stAuxLabel.hide();
        }

        this._separator = new Separator();
        if (!this._preeditVisible && !this._auxVisible) {
            this._separator.actor.hide();
        }
        // create candidates area
        this._stCandidateArea = new StCandidateArea(this._currentOrientation);
        this._stCandidateArea.connect('candidate-clicked',
                                      Lang.bind(this, function(x, i, b, s) {
                                                this.emit('candidate-clicked', i, b, s);}));
        this.updateLookupTable(this._lookupTable, this._lookupTableVisible);

        // TODO: page up/down GUI

        this._packAllStWidgets();
        this._isVisible = true;
        this.hideAll();
        this._checkShowStates();
    },

    _packAllStWidgets: function() {
        this._stCandidatePanel.add_child(this._stPreeditLabel,
                                         { x_fill: true,
                                           y_fill: false,
                                           x_align: St.Align.MIDDLE,
                                           y_align: St.Align.START });
        this._stCandidatePanel.add_child(this._stAuxLabel,
                                         { x_fill: true,
                                           y_fill: false,
                                           x_align: St.Align.MIDDLE,
                                           y_align: St.Align.MIDDLE });
        this._stCandidatePanel.add_child(this._separator.actor,
                                         { x_fill: true,
                                           y_fill: false,
                                           x_align: St.Align.MIDDLE,
                                           y_align: St.Align.MIDDLE });
        this._stCandidatePanel.add_child(this._stCandidateArea.actor,
                                         { x_fill: true,
                                           y_fill: false,
                                           x_align: St.Align.MIDDLE,
                                           y_align: St.Align.END });
    },

    showPreeditText: function() {
        this._preeditVisible = true;
        this._stPreeditLabel.show();
        this._checkShowStates();
    },

    hidePreeditText: function() {
        this._preeditVisible = false;
        this._checkShowStates();
        this._stPreeditLabel.hide();
    },

    updatePreeditText: function(text, cursorPos, visible) {
        if (visible) {
            this.showPreeditText();
        } else {
            this.hidePreeditText();
        }
        let str = text.get_text();
        this._stPreeditLabel.set_text(str);

        let attrs = text.get_attributes();
        for (let i = 0; attrs != null && attrs.get(i) != null; i++) {
            let attr = attrs.get(i);
            if (attr.get_attr_type() == IBus.AttrType.BACKGROUND) {
                let startIndex = attr.get_start_index();
                let endIndex = attr.get_end_index();
                let len = GLib.utf8_strlen(str, -1);
                let markup = '';
                if (startIndex == 0 &&
                    endIndex == GLib.utf8_strlen(str, -1)) {
                    markup = markup.concat(str);
                } else {
                    if (startIndex > 0) {
                        markup = markup.concat(GLib.utf8_substring(str,
                                                                   0,
                                                                   startIndex));
                    }
                    if (startIndex != endIndex) {
                        markup = markup.concat('<span background=\"#555555\">');
                        markup = markup.concat(GLib.utf8_substring(str,
                                                                   startIndex,
                                                                   endIndex));
                        markup = markup.concat('</span>');
                    }
                    if (endIndex < len) {
                        markup = markup.concat(GLib.utf8_substring(str,
                                                                   endIndex,
                                                                   len));
                    }
                }
                let clutter_text = this._stPreeditLabel.get_clutter_text();
                clutter_text.set_markup(markup);
                clutter_text.queue_redraw();
            }
        }
    },

    showAuxiliaryText: function() {
        this._auxStringVisible = true;
        this._stAuxLabel.show();
        this._checkShowStates();
    },

    hideAuxiliaryText: function() {
        this._auxStringVisible = false;
        this._checkShowStates();
        this._stAuxLabel.hide();
    },

    updateAuxiliaryText: function(text, show) {
        if (show) {
            this.showAuxiliaryText();
        } else {
            this.hideAuxiliaryText();
        }

        this._stAuxLabel.set_text(text.get_text());
    },

    _refreshLabels: function() {
        let newLabels = [];
        for (let i = 0; this._lookupTable.get_label(i) != null; i++) {
            let label = this._lookupTable.get_label(i);
            newLabels.push([label.get_text(), label.get_attributes()]);
        }
        this._stCandidateArea.setLabels(newLabels);
    },


    _getCandidatesInCurrentPage: function() {
        let cursorPos = this._lookupTable.get_cursor_pos();
        let pageSize = this._lookupTable.get_page_size();
        let page = ((cursorPos == 0) ? 0 : Math.floor(cursorPos / pageSize));
        let startIndex = page * pageSize;
        let endIndex = Math.min((page + 1) * pageSize,
                                this._lookupTable.get_number_of_candidates());
        let candidates = [];
        for (let i = startIndex; i < endIndex; i++) {
            candidates.push(this._lookupTable.get_candidate(i));
        }
        return candidates;
    },

    _getCursorPosInCurrentPage: function() {
        let cursorPos = this._lookupTable.get_cursor_pos();
        let pageSize = this._lookupTable.get_page_size();
        let posInPage = cursorPos % pageSize;
        return posInPage;
    },

    _refreshCandidates: function() {
        let candidates = this._getCandidatesInCurrentPage();
        let newCandidates = [];
        for (let i = 0; i < candidates.length; i++) {
            let candidate = candidates[i];
            newCandidates.push([candidate.get_text(),
                                candidate.get_attributes()]);
        }
        this._stCandidateArea.setCandidates(newCandidates,
                                            this._getCursorPosInCurrentPage(),
                                            this._lookupTable.is_cursor_visible());
    },

    updateLookupTable: function(lookupTable, visible) {
        // hide lookup table
        if (!visible) {
            this.hideLookupTable();
        }

        this._lookupTable = lookupTable || new IBus.LookupTable();
        let orientation = this._lookupTable.get_orientation();
        if (orientation != ORIENTATION_HORIZONTAL &&
            orientation != ORIENTATION_VERTICAL) {
            orientation = this._orientation;
        }
        this.setCurrentOrientation(orientation);
        this._refreshCandidates();
        this._refreshLabels();

        // show lookup table
        if (visible) {
            this.showLookupTable();
        }
    },

    showLookupTable: function() {
        this._lookupTableVisible = true;
        this._stCandidateArea.showAll();
        this._checkShowStates();
    },

    hideLookupTable: function() {
        this._lookupTableVisible = false;
        this._checkShowStates();
        this._stCandidateArea.hideAll();
    },

    pageUpLookupTable: function() {
        this._lookupTable.page_up();
        this._refreshCandidates();
    },

    pageDownLookup_table: function() {
        this._lookupTable.page_down();
        this._refreshCandidates();
    },

    cursorUpLookupTable: function() {
        this._lookupTable.cursor_up();
        this._refreshCandidates();
    },

    cursorDownLookupTable: function() {
        this._lookupTable.cursor_down();
        this._refreshCandidates();
    },

    setCursorLocation: function(x, y, w, h) {
        // if cursor location is changed, we reset the moved cursor location
        if (this._cursorLocation.join() != [x, y, w, h].join()) {
            this._cursorLocation = [x, y, w, h];
            this._movedCursorLocation = null;
            this._checkPosition();
        }
    },

    _checkShowStates: function() {
        this._checkSeparatorShowStates();
        if (this._preeditVisible ||
            this._auxStringVisible ||
            this._lookupTableVisible) {
            this._checkPosition();
            this.showAll();
            this.emit('show');
        } else {
            this.hideAll();
            this.emit('hide');
        }
    },

    _checkSeparatorShowStates: function() {
        if (this._preeditVisible || this._auxStringVisible) {
            this._separator.actor.show();
        }
        else
            this._separator.actor.hide();
    },

    reset: function() {
        let text = IBus.Text.new_from_string('');
        this.updatePreeditText(text, 0, false);
        text = IBus.Text.new_from_string('');
        this.updateAuxiliaryText(text, false);
        this.updateLookupTable(null, false);
        this.hideAll();
    },

    setCurrentOrientation: function(orientation) {
        if (this._currentOrientation == orientation) {
            return;
        }
        this._currentOrientation = orientation;
        this._stCandidateArea.setOrientation(orientation);
    },

    setOrientation: function(orientation) {
        this._orientation = orientation;
        this.updateLookupTable(this._lookupTable, this._lookupTableVisible);
    },

    getCurrentOrientation: function() {
        return this._currentOrientation;
    },

    _checkPosition: function() {
        let cursorLocation = this._movedCursorLocation || this._cursorLocation;
        let [cursorX, cursorY, cursorWidth, cursorHeight] = cursorLocation;

        let windowRight = cursorX + cursorWidth + this.actor.get_width();
        let windowBottom = cursorY + cursorHeight + this.actor.get_height();

        this._cursorActor.set_position(cursorX, cursorY);
        this._cursorActor.set_size(cursorWidth, cursorHeight);

        let monitor = Main.layoutManager.findMonitorForActor(this._cursorActor);
        let [sx, sy] = [monitor.x + monitor.width, monitor.y + monitor.height];

        if (windowBottom > sy) {
            this._arrowSide = St.Side.BOTTOM;
        } else {
            this._arrowSide = St.Side.TOP;
        }

        this._boxPointer._arrowSide = this._arrowSide;
        this._boxPointer.setArrowOrigin(this._arrowSide);
        this._boxPointer.setPosition(this._cursorActor, this._arrowAlignment);
    },

    showAll: function() {
        if (!this._isVisible) {
            this.actor.opacity = 255;
            this.actor.show();
            this._isVisible = true;
        }
    },

    hideAll: function() {
        if (this._isVisible) {
            this.actor.opacity = 0;
            this.actor.hide();
            this._isVisible = false;
        }
    },

    move: function(x, y) {
        this.actor.set_position(x, y);
    }
};

Signals.addSignalMethods(CandidatePanel.prototype);

function Separator() {
    this._init();
}

Separator.prototype = {
    _init: function() {
        this.actor = new St.DrawingArea({ style_class: 'popup-separator-menu-item',
                                          style: separatorStyle });
        this.actor.connect('repaint', Lang.bind(this, this._onRepaint));
    },

    _onRepaint: function(area) {
        let cr = area.get_context();
        let themeNode = area.get_theme_node();
        let [width, height] = area.get_surface_size();
        let margin = 0;
        let gradientHeight = themeNode.get_length('-gradient-height');
        let startColor = themeNode.get_color('-gradient-start');
        let endColor = themeNode.get_color('-gradient-end');

        let gradientWidth = (width - margin * 2);
        let gradientOffset = (height - gradientHeight) / 2;
        let pattern = new Cairo.LinearGradient(margin, gradientOffset, width - margin, gradientOffset + gradientHeight);
        pattern.addColorStopRGBA(0, startColor.red / 255, startColor.green / 255, startColor.blue / 255, startColor.alpha / 255);
        pattern.addColorStopRGBA(0.5, endColor.red / 255, endColor.green / 255, endColor.blue / 255, endColor.alpha / 255);
        pattern.addColorStopRGBA(1, startColor.red / 255, startColor.green / 255, startColor.blue / 255, startColor.alpha / 255);
        cr.setSource(pattern);
        cr.rectangle(margin, gradientOffset, gradientWidth, gradientHeight);
        cr.fill();
    },
};
