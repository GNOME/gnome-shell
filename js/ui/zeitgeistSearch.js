/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*-
 *
 * Copyright (C) 2010 Seif Lotfy <seif@lotfy.com>
 * Copyright (C) 2011 Siegfried-Angel Gevatter Pujals <siegfried@gevatter.com>
 * Copyright (C) 2010-2011 Collabora Ltd.
 *     Authored by: Seif Lotfy <seif@lotfy.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

const Lang = imports.lang;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio
const Semantic = imports.misc.semantic;
const Zeitgeist = imports.misc.zeitgeist;

const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const DocInfo = imports.misc.docInfo;
const Search = imports.ui.search;

// FIXME: The subject cache is never being emptied.
let ZeitgeistSubjectCache = {};

function ZeitgeistAsyncSearchProvider(title, interpretations) {
    this._init(title, interpretations);
}

ZeitgeistAsyncSearchProvider.prototype = {
    __proto__: Search.SearchProvider.prototype,

    _init: function(title, interpretations) {
        Search.SearchProvider.prototype._init.call(this, title);
        this._buildTemplates(interpretations);
    },

    _buildTemplates: function(interpretations) {
        this.templates = [];
        for (let i = 0; i < interpretations.length; i++) {
            let subject = new Zeitgeist.Subject('', interpretations[i], '', '', '', '', '');
            let event = new Zeitgeist.Event('', '', '', [subject], []);
            this.templates.push(event);
        }
    },

    _search: function(terms) {
        this._search_terms = terms;
        Zeitgeist.fullTextSearch(terms[0]+'*',
                                 this.templates,
                                 Lang.bind(this, function(events) {
                                     if (terms == this._search_terms)
                                         this._asyncCallback(events);
                                 }));
    },

    _asyncCancelled: function() {
        this._search_terms = null;
    },

    getInitialResultSet: function(terms) {
        this._search(terms);
        return [];
    },

    getSubsearchResultSet: function(previousResults, terms) {
        this.tryCancelAsync();
        return this.getInitialResultSet(terms);
    },

    getResultMeta: function(resultId) {
        return { 'id': ZeitgeistSubjectCache[resultId].uri,
                 'name': ZeitgeistSubjectCache[resultId].name,
                 'createIcon': function (size) {
                                   return ZeitgeistSubjectCache[resultId].createIcon(size);
                               },
               };
    },

    activateResult: function(resultId) {
        Gio.app_info_launch_default_for_uri(resultId,
                                            global.create_app_launch_context());
    },

    _asyncCallback: function(events) {
        let items = [];
        for (let i = 0; i < events.length; i++) {
            let event = events[i];
            let subject = event.subjects[0];
            let uri = subject.uri.replace('file://', '');
            uri = GLib.uri_unescape_string(uri, '');
            if (GLib.file_test(uri, GLib.FileTest.EXISTS)) {
                if (!ZeitgeistSubjectCache.hasOwnProperty(subject.uri)) {
                    let info = new DocInfo.ZeitgeistItemInfo(event);
                    ZeitgeistSubjectCache[info.uri] = info;
                }
                items.push(subject.uri);
            }
        }
        this.addItems(items);
    }
};

function DocumentsAsyncSearchProvider() {
    this._init();
}

DocumentsAsyncSearchProvider.prototype = {
    __proto__: ZeitgeistAsyncSearchProvider.prototype,

    _init: function() {
        let interpretations = [Semantic.NFO_DOCUMENT];
        ZeitgeistAsyncSearchProvider.prototype._init.call(this, _("DOCUMENTS"), interpretations);
    }
};

function VideosAsyncSearchProvider() {
    this._init();
}

VideosAsyncSearchProvider.prototype = {
    __proto__: ZeitgeistAsyncSearchProvider.prototype,

    _init: function() {
        let interpretations = [Semantic.NFO_VIDEO];
        ZeitgeistAsyncSearchProvider.prototype._init.call(this, _("VIDEOS"), interpretations);
    }
};

function MusicAsyncSearchProvider() {
    this._init();
}

MusicAsyncSearchProvider.prototype = {
    __proto__: ZeitgeistAsyncSearchProvider.prototype,

    _init: function() {
        let interpretations = [
            Semantic.NFO_AUDIO,
            Semantic.NMM_MUSIC_PIECE];
        ZeitgeistAsyncSearchProvider.prototype._init.call(this, _("MUSIC"), interpretations);
    }
};

function PicturesAsyncSearchProvider() {
    this._init();
}

PicturesAsyncSearchProvider.prototype = {
    __proto__: ZeitgeistAsyncSearchProvider.prototype,

    _init: function() {
        let interpretations = [Semantic.NFO_IMAGE];
        ZeitgeistAsyncSearchProvider.prototype._init.call(this, _("PICTURES"), interpretations);
    }
};

function OtherAsyncSearchProvider() {
    this._init();
}

OtherAsyncSearchProvider.prototype = {
    __proto__: ZeitgeistAsyncSearchProvider.prototype,

    _init: function() {
        let interpretations = [
            '!' + Semantic.NFO_IMAGE,
            '!' + Semantic.NFO_DOCUMENT,
            '!' + Semantic.NFO_VIDEO,
            '!' + Semantic.NFO_AUDIO,
            '!' + Semantic.NMM_MUSIC_PIECE];
        ZeitgeistAsyncSearchProvider.prototype._init.call(this, _("OTHER"), interpretations);
    },

    _buildTemplates: function(interpretations) {
        // Here we want to get everything matching all of the templates, and
        // not just any of them. Therefore we need to AND the interpretations
        // instead of OR'ing them; this is done by having an Event with
        // different Subjects.
        this.templates = [];
        let subjects = [];
        for (let i = 0; i < interpretations.length; i++) {
            let subject = new Zeitgeist.Subject('', interpretations[i], '', '', '', '', '');
            subjects.push(subject);
        }
        let event = new Zeitgeist.Event('', '', '', subjects, []);
        this.templates.push(event);
    }
};
