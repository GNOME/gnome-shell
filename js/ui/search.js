/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Signals = imports.signals;
const St = imports.gi.St;

const RESULT_ICON_SIZE = 24;

// Not currently referenced by the search API, but
// this enumeration can be useful for provider
// implementations.
const MatchType = {
    NONE: 0,
    MULTIPLE: 1,
    PREFIX: 2,
    SUBSTRING: 3
};

function SearchResultDisplay(provider) {
    this._init(provider);
}

SearchResultDisplay.prototype = {
    _init: function(provider) {
        this.provider = provider;
        this.actor = null;
        this.selectionIndex = -1;
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
        throw new Error("not implemented");
    },

    /**
     * clear:
     * Remove all results from this display and reset the selection index.
     */
    clear: function() {
        this.actor.get_children().forEach(function (actor) { actor.destroy(); });
        this.selectionIndex = -1;
    },

    /**
     * getSelectionIndex:
     *
     * Returns the index of the selected actor, or -1 if none.
     */
    getSelectionIndex: function() {
        return this.selectionIndex;
    },

    /**
     * getVisibleResultCount:
     *
     * Returns: The number of actors visible.
     */
    getVisibleResultCount: function() {
        throw new Error("not implemented");
    },

    /**
     * selectIndex:
     * @index: Integer index
     *
     * Move selection to the given index.
     * Return true if successful, false if no more results
     * available.
     */
    selectIndex: function() {
        throw new Error("not implemented");
    }
};

/**
 * SearchProvider:
 *
 * Subclass this object to add a new result type
 * to the search system, then call registerProvider()
 * in SearchSystem with an instance.
 */
function SearchProvider(title) {
    this._init(title);
}

SearchProvider.prototype = {
    _init: function(title) {
        this.title = title;
    },

    /**
     * getInitialResultSet:
     * @terms: Array of search terms, treated as logical OR
     *
     * Called when the user first begins a search (most likely
     * therefore a single term of length one or two), or when
     * a new term is added.
     *
     * Should return an array of result identifier strings representing
     * items which match the given search terms.  This
     * is expected to be a substring match on the metadata for a given
     * item.  Ordering of returned results is up to the discretion of the provider,
     * but you should follow these heruistics:
     *
     *  * Put items which match multiple search terms before single matches
     *  * Put items which match on a prefix before non-prefix substring matches
     *
     * This function should be fast; do not perform unindexed full-text searches
     * or network queries.
     */
    getInitialResultSet: function(terms) {
        throw new Error("not implemented");
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
     */
    getSubsearchResultSet: function(previousResults, newTerms) {
        throw new Error("not implemented");
    },

    /**
     * getResultInfo:
     * @id: Result identifier string
     *
     * Return an object with 'id', 'name', (both strings) and 'icon' (Clutter.Texture)
     * properties which describe the given search result.
     */
    getResultMeta: function(id) {
        throw new Error("not implemented");
    },

    /**
     * createResultContainer:
     *
     * Search providers may optionally override this to render their
     * results in a custom fashion.  The default implementation
     * will create a vertical list.
     *
     * Returns: An instance of SearchResultDisplay.
     */
    createResultContainerActor: function() {
        return null;
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
     * 'dash-search-result-content'.
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
        throw new Error("not implemented");
    },

    /**
     * expandSearch:
     *
     * Called when the user clicks on the header for this
     * search section.  Should typically launch an external program
     * displaying search results for that item type.
     */
    expandSearch: function(terms) {
        throw new Error("not implemented");
    }
}
Signals.addSignalMethods(SearchProvider.prototype);

function SearchSystem() {
    this._init();
}

SearchSystem.prototype = {
    _init: function() {
        this._providers = [];
        this.reset();
    },

    registerProvider: function (provider) {
        this._providers.push(provider);
    },

    getProviders: function() {
        return this._providers;
    },

    getTerms: function() {
        return this._previousTerms;
    },

    reset: function() {
        this._previousTerms = [];
        this._previousResults = [];
    },

    updateSearch: function(searchString) {
        searchString = searchString.replace(/^\s+/g, "").replace(/\s+$/g, "");
        if (searchString == '')
            return null;

        let terms = searchString.split(/\s+/);
        let isSubSearch = terms.length == this._previousTerms.length;
        if (isSubSearch) {
            for (let i = 0; i < terms.length; i++) {
                if (terms[i].indexOf(this._previousTerms[i]) != 0) {
                    isSubSearch = false;
                    break;
                }
            }
        }

        let results = [];
        if (isSubSearch) {
            for (let i = 0; i < this._previousResults.length; i++) {
                let [provider, previousResults] = this._previousResults[i];
                let providerResults = provider.getSubsearchResultSet(previousResults, terms);
                if (providerResults.length > 0)
                    results.push([provider, providerResults]);
            }
        } else {
            for (let i = 0; i < this._providers.length; i++) {
                let provider = this._providers[i];
                let providerResults = provider.getInitialResultSet(terms);
                if (providerResults.length > 0)
                    results.push([provider, providerResults]);
            }
        }

        this._previousTerms = terms;
        this._previousResults = results;

        return results;
    }
}
Signals.addSignalMethods(SearchSystem.prototype);
