// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const Util = imports.misc.util;

const FileUtils = imports.misc.fileUtils;
const Main = imports.ui.main;

const SEARCH_PROVIDERS_SCHEMA = 'org.gnome.desktop.search-providers';

// Not currently referenced by the search API, but
// this enumeration can be useful for provider
// implementations.
const MatchType = {
    NONE: 0,
    SUBSTRING: 1,
    PREFIX: 2
};

const SearchResultDisplay = new Lang.Class({
    Name: 'SearchResultDisplay',

    _init: function(provider) {
        this.provider = provider;
        this.actor = null;
    },

    /**
     * renderResults:
     * @results: List of identifier strings
     * @terms: List of search term strings
     *
     * Display the given search matches which resulted
     * from the given terms.  It's expected that not
     * all results will fit in the space for the container
     * actor; in this case, show as many as makes sense
     * for your result type.
     *
     * The terms are useful for search match highlighting.
     */
    renderResults: function(results, terms) {
        throw new Error('Not implemented');
    },

    /**
     * clear:
     * Remove all results from this display.
     */
    clear: function() {
        this.actor.destroy_all_children();
    },

    /**
     * getVisibleResultCount:
     *
     * Returns: The number of actors visible.
     */
    getVisibleResultCount: function() {
        throw new Error('Not implemented');
    },
});

/**
 * SearchProvider:
 *
 * Subclass this object to add a new result type
 * to the search system, then call registerProvider()
 * in SearchSystem with an instance.
 * Search is asynchronous and uses the
 * getInitialResultSet()/getSubsearchResultSet() methods.
 */
const SearchProvider = new Lang.Class({
    Name: 'SearchProvider',

    _init: function(title, appInfo, isRemoteProvider) {
        this.title = title;
        this.appInfo = appInfo;
        this.searchSystem = null;
        this.isRemoteProvider = !!isRemoteProvider;
    },

    /**
     * getInitialResultSet:
     * @terms: Array of search terms, treated as logical AND
     *
     * Called when the user first begins a search (most likely
     * therefore a single term of length one or two), or when
     * a new term is added.
     *
     * Should "return" an array of result identifier strings representing
     * items which match the given search terms.  This
     * is expected to be a substring match on the metadata for a given
     * item.  Ordering of returned results is up to the discretion of the provider,
     * but you should follow these heruistics:
     *
     *  * Put items where the term matches multiple criteria (e.g. name and
     *    description) before single matches
     *  * Put items which match on a prefix before non-prefix substring matches
     *
     * We say "return" above, but in order to make the query asynchronous, use
     * this.searchSystem.pushResults();. The return value should be ignored.
     *
     * This function should be fast; do not perform unindexed full-text searches
     * or network queries.
     */
    getInitialResultSet: function(terms) {
        throw new Error('Not implemented');
    },

    /**
     * getSubsearchResultSet:
     * @previousResults: Array of item identifiers
     * @newTerms: Updated search terms
     *
     * Called when a search is performed which is a "subsearch" of
     * the previous search; i.e. when every search term has exactly
     * one corresponding term in the previous search which is a prefix
     * of the new term.
     *
     * This allows search providers to only search through the previous
     * result set, rather than possibly performing a full re-query.
     *
     * Similar to getInitialResultSet, the return value for this will
     * be ignored; use this.searchSystem.pushResults();.
     */
    getSubsearchResultSet: function(previousResults, newTerms) {
        throw new Error('Not implemented');
    },

    /**
     * getResultMetas:
     * @ids: Result identifier strings
     *
     * Call callback with array of objects with 'id', 'name', (both strings) and
     * 'createIcon' (function(size) returning a Clutter.Texture) properties
     * with the same number of members as @ids
     */
    getResultMetas: function(ids, callback) {
        throw new Error('Not implemented');
    },

    /**
     * createResultActor:
     * @resultMeta: Object with result metadata
     * @terms: Array of search terms, should be used for highlighting
     *
     * Search providers may optionally override this to render a
     * particular serch result in a custom fashion.  The default
     * implementation will show the icon next to the name.
     *
     * The actor should be an instance of St.Widget, with the style class
     * 'search-result-content'.
     */
    createResultActor: function(resultMeta, terms) {
        return null;
    },

    /**
     * activateResult:
     * @id: Result identifier string
     *
     * Called when the user chooses a given result.
     */
    activateResult: function(id) {
        throw new Error('Not implemented');
    }
});
Signals.addSignalMethods(SearchProvider.prototype);

const SearchSystem = new Lang.Class({
    Name: 'SearchSystem',

    _init: function() {
        this._providers = [];
        this._remoteProviders = [];
        this.reset();
    },

    registerProvider: function (provider) {
        provider.searchSystem = this;
        this._providers.push(provider);

        if (provider.isRemoteProvider)
            this._remoteProviders.push(provider);
    },

    unregisterProvider: function (provider) {
        let index = this._providers.indexOf(provider);
        if (index == -1)
            return;
        provider.searchSystem = null;
        this._providers.splice(index, 1);

        let remoteIndex = this._remoteProviders.indexOf(provider);
        if (remoteIndex != -1)
            this._remoteProviders.splice(index, 1);
    },

    getProviders: function() {
        return this._providers;
    },

    getRemoteProviders: function() {
        return this._remoteProviders;
    },

    getTerms: function() {
        return this._previousTerms;
    },

    reset: function() {
        this._previousTerms = [];
        this._previousResults = [];
    },

    pushResults: function(provider, results) {
        let i = this._providers.indexOf(provider);
        if (i == -1)
            return;

        this._previousResults[i] = [provider, results];
        this.emit('search-updated', this._previousResults[i]);
    },

    updateSearch: function(searchString) {
        searchString = searchString.replace(/^\s+/g, '').replace(/\s+$/g, '');
        if (searchString == '')
            return;

        let terms = searchString.split(/\s+/);
        this.updateSearchResults(terms);
    },

    updateSearchResults: function(terms) {
        if (!terms)
            return;

        let isSubSearch = terms.length == this._previousTerms.length;
        if (isSubSearch) {
            for (let i = 0; i < terms.length; i++) {
                if (terms[i].indexOf(this._previousTerms[i]) != 0) {
                    isSubSearch = false;
                    break;
                }
            }
        }

        let previousResultsArr = this._previousResults;

        let results = [];
        this._previousTerms = terms;
        this._previousResults = results;

        if (isSubSearch) {
            for (let i = 0; i < this._providers.length; i++) {
                let [provider, previousResults] = previousResultsArr[i];
                try {
                    results.push([provider, []]);
                    provider.getSubsearchResultSet(previousResults, terms);
                } catch (error) {
                    log('A ' + error.name + ' has occured in ' + provider.title + ': ' + error.message);
                }
            }
        } else {
            for (let i = 0; i < this._providers.length; i++) {
                let provider = this._providers[i];
                try {
                    results.push([provider, []]);
                    provider.getInitialResultSet(terms);
                } catch (error) {
                    log('A ' + error.name + ' has occured in ' + provider.title + ': ' + error.message);
                }
            }
        }
    },
});
Signals.addSignalMethods(SearchSystem.prototype);
