// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported getInternetSearchProvider */

const { GLib, Gio } = imports.gi;

const Util = imports.misc.util;

// Returns a plain URI if the user types in
// something like "facebook.com"
function getURIForSearch(terms) {
    let searchedUris = Util.findSearchUrls(terms);
    // Make sure search contains only a uri
    // Avoid cases like "what is github.com"
    if (searchedUris.length === 1 && terms.length === 1) {
        let uri = searchedUris[0];
        // Ensure all uri has a scheme name
        if (!GLib.uri_parse_scheme(uri))
            uri = 'http://'.format(uri);

        return uri;
    } else {
        return null;
    }
}

function getInternetSearchProvider() {
    let browserApp = Util.getBrowserApp();
    if (browserApp)
        return new InternetSearchProvider(browserApp);

    return null;
}

var InternetSearchProvider = class {
    constructor(browserApp) {
        this.id = 'internet';
        this.appInfo = browserApp.get_app_info();
        this.canLaunchSearch = true;
        this.isRemoteProvider = false;

        this._engineNameParsed = false;
        this._engineName = null;

        this._networkMonitor = Gio.NetworkMonitor.get_default();
    }

    _getEngineName() {
        if (!this._engineNameParsed) {
            this._engineNameParsed = true;
            this._engineName = Util.getSearchEngineName();
        }

        return this._engineName;
    }

    _launchURI(uri) {
        try {
            this.appInfo.launch_uris([uri], null);
        } catch (e) {
            logError(e, 'error while launching browser for uri: %s'.format(uri));
        }
    }

    getResultMetas(results, callback) {
        let metas = results.map(resultId => {
            let name;
            if (resultId.startsWith('uri:')) {
                let uri = resultId.slice('uri:'.length);
                name = _('Open "%s" in browser').format(uri);
            } else if (resultId.startsWith('search:')) {
                let query = resultId.slice('search:'.length);
                let engineName = this._getEngineName();

                if (engineName) {
                    /* Translators: the first %s is the search engine name, and the second
                     * is the search string. For instance, 'Search Google for "hello"'.
                     */
                    name = _('Search %s for "%s"').format(engineName, query);
                } else {
                    name = _('Search the internet for "%s"').format(query);
                }
            }

            return {
                id: resultId,
                name,
                // We will already have an app icon next to our result,
                // so we don't need an individual result icon.
                createIcon() {
                    return null;
                },
            };
        });
        callback(metas);
    }

    filterResults(results, maxNumber) {
        return results.slice(0, maxNumber);
    }

    getInitialResultSet(terms, callback, _cancellable) {
        let results = [];

        if (this._networkMonitor.network_available) {
            let uri = getURIForSearch(terms);
            let query = terms.join(' ');
            if (uri)
                results.push('uri:%s'.format(query));
            else
                results.push('search:'.format(query));
        }

        callback(results);
    }

    getSubsearchResultSet(previousResults, terms, callback, cancellable) {
        this.getInitialResultSet(terms, callback, cancellable);
    }

    activateResult(metaId) {
        if (metaId.startsWith('uri:')) {
            let uri = metaId.slice('uri:'.length);
            uri = getURIForSearch([uri]);
            this._launchURI(uri);
        } else if (metaId.startsWith('search:')) {
            let query = metaId.slice('search:'.length);
            this._launchURI('? '.concat(query));
        }
    }

    launchSearch(terms) {
        this.getInitialResultSet(terms, results => {
            if (results)
                this.activateResult(results[0]);
        });
    }
};
