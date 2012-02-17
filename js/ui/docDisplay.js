// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const DocInfo = imports.misc.docInfo;
const Lang = imports.lang;
const Params = imports.misc.params;
const Search = imports.ui.search;

const DocSearchProvider = new Lang.Class({
    Name: 'DocSearchProvider',
    Extends: Search.SearchProvider,

    _init: function(name) {
        this.parent(_("RECENT ITEMS"));
        this._docManager = DocInfo.getDocManager();
    },

    getResultMetas: function(resultIds) {
        let metas = [];
        for (let i = 0; i < resultIds.length; i++) {
            let docInfo = this._docManager.lookupByUri(resultIds[i]);
            if (!docInfo)
                metas.push(null);
            else
                metas.push({ 'id': resultIds[i],
                             'name': docInfo.name,
                             'createIcon': function(size) {
                                 return docInfo.createIcon(size);
                             }
                           });
        }
        return metas;
    },

    activateResult: function(id, params) {
        params = Params.parse(params, { workspace: -1,
                                        timestamp: 0 });

        let docInfo = this._docManager.lookupByUri(id);
        docInfo.launch(params.workspace);
    },

    getInitialResultSet: function(terms) {
        return this._docManager.initialSearch(terms);
    },

    getSubsearchResultSet: function(previousResults, terms) {
        return this._docManager.subsearch(previousResults, terms);
    }
});
