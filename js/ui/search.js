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
                    log('A ' + error.name + ' has occured in ' + provider.id + ': ' + error.message);
                }
            }
        } else {
            for (let i = 0; i < this._providers.length; i++) {
                let provider = this._providers[i];
                try {
                    results.push([provider, []]);
                    provider.getInitialResultSet(terms);
                } catch (error) {
                    log('A ' + error.name + ' has occured in ' + provider.id + ': ' + error.message);
                }
            }
        }
    },
});
Signals.addSignalMethods(SearchSystem.prototype);
