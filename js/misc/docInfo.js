/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const St = imports.gi.St;
const Search = imports.ui.search;

function ZeitgeistItemInfo(event) {
    this._init(event);
}

ZeitgeistItemInfo.prototype = {
    _init : function(event) {
        this.event = event;
        this.subject = event.subjects[0];
        this.timestamp = event.timestamp;
        this.name = this.subject.text;
        this._lowerName = this.name.toLowerCase();
        this.uri = this.subject.uri;
        this.mimeType = this.subject.mimetype;
        this.interpretation = this.subject.interpretation;
    },

    createIcon : function(size) {
        return St.TextureCache.get_default().load_thumbnail(size, this.uri, this.subject.mimetype);
        // FIXME: We should consider caching icons
    },

    launch : function() {
        Gio.app_info_launch_default_for_uri(this.uri,
                                            global.create_app_launch_context());
    },

    matchTerms: function(terms) {
        let mtype = Search.MatchType.NONE;
        for (let i = 0; i < terms.length; i++) {
            let term = terms[i];
            let idx = this._lowerName.indexOf(term);
            if (idx == 0) {
                mtype = Search.MatchType.PREFIX;
            } else if (idx > 0) {
                if (mtype == Search.MatchType.NONE)
                    mtype = Search.MatchType.SUBSTRING;
            } else {
                return Search.MatchType.NONE;
            }
        }
        return mtype;
    },
};
