/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const DocInfo = imports.misc.docInfo;
const Search = imports.ui.search;


function DocSearchProvider() {
    this._init();
}

DocSearchProvider.prototype = {
    __proto__: Search.SearchProvider.prototype,

    _init: function(name) {
        Search.SearchProvider.prototype._init.call(this, _("RECENT ITEMS"));
        this._docManager = DocInfo.getDocManager();
    },

    getResultMeta: function(resultId) {
        let docInfo = this._docManager.lookupByUri(resultId);
        if (!docInfo)
            return null;
        return { 'id': resultId,
                 'name': docInfo.name,
                 'icon': docInfo.createIcon(Search.RESULT_ICON_SIZE)};
    },

    activateResult: function(id) {
        let docInfo = this._docManager.lookupByUri(id);
        docInfo.launch();
    },

    getInitialResultSet: function(terms) {
        return this._docManager.initialSearch(terms);
    },

    getSubsearchResultSet: function(previousResults, terms) {
        return this._docManager.subsearch(previousResults, terms);
    },

    expandSearch: function(terms) {
        log('TODO expand docs search');
    }
};
