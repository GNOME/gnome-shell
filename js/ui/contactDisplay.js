// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Folks = imports.gi.Folks
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const Util = imports.misc.util;
const IconGrid = imports.ui.iconGrid;
const Search = imports.ui.search;
const SearchDisplay = imports.ui.searchDisplay;

const MAX_SEARCH_RESULTS_ROWS = 1;
const ICON_SIZE = 81;

function launchContact(id) {
    Util.spawn(['gnome-contacts', '-i', id]);
}


/* This class represents a shown contact search result in the overview */
const Contact = new Lang.Class({
    Name: 'Contact',

    _init: function(id) {
        this._contactSys = Shell.ContactSystem.get_default();
        this.individual = this._contactSys.get_individual(id);

        this.actor = new St.Bin({ style_class: 'contact',
                                  reactive: true,
                                  can_focus: true,
                                  track_hover: true,
                                  accessible_role: Atk.Role.PUSH_BUTTON });

        let content = new St.BoxLayout( { style_class: 'contact-content',
                                          vertical: false });
        this.actor.set_child(content);

        let icon = new St.Icon({ icon_type: St.IconType.FULLCOLOR,
                                 icon_size: ICON_SIZE,
                                 style_class: 'contact-icon' });
        if (this.individual.avatar != null)
            icon.gicon = this.individual.avatar;
        else
            icon.icon_name = 'avatar-default';

        content.add(icon, { x_fill: true,
                            y_fill: false,
                            x_align: St.Align.START,
                            y_align: St.Align.MIDDLE });

        let details = new St.BoxLayout({ style_class: 'contact-details',
                                         vertical: true });
        content.add(details, { x_fill: true,
                               y_fill: false,
                               x_align: St.Align.START,
                               y_align: St.Align.MIDDLE });

        let email = this._contactSys.get_email_for_display(this.individual);
        let aliasText = this.individual.alias     ||
                        this.individual.full_name ||
                        this.individual.nickname  ||
                        email                     ||
                        _("Unknown");
        let aliasLabel = new St.Label({ text: aliasText,
                                        style_class: 'contact-details-alias' });
        details.add(aliasLabel, { x_fill: true,
                                  y_fill: false,
                                  x_align: St.Align.START,
                                  y_align: St.Align.START });

        this.actor.label_actor = aliasLabel;

        let presence = this._createPresence(this.individual.presence_type);
        details.add(presence, { x_fill: false,
                                y_fill: true,
                                x_align: St.Align.START,
                                y_align: St.Align.END });
    },

    _createPresence: function(presence) {
        let text;
        let iconName;

        switch(presence) {
          case Folks.PresenceType.AVAILABLE:
            text = _("Available");
            iconName = 'user-available';
            break;
          case Folks.PresenceType.AWAY:
          case Folks.PresenceType.EXTENDED_AWAY:
            text = _("Away");
            iconName = 'user-away';
            break;
          case Folks.PresenceType.BUSY:
            text = _("Busy");
            iconName = 'user-busy';
            break;
          case Folks.PresenceType.OFFLINE:
            text = _("Offline");
            iconName = 'user-offline';
            break;
          default:
            text = '';
            iconName = null;
          }

        let box = new St.BoxLayout({ vertical: false,
                                     style_class: 'contact-details-status' });

        if (iconName) {
            let icon = new St.Icon({ icon_name: iconName,
                                     icon_type: St.IconType.FULLCOLOR,
                                     icon_size: 16,
                                     style_class: 'contact-details-status-icon' });
            box.add(icon, { x_fill: true,
                            y_fill: false,
                            x_align: St.Align.START,
                            y_align: St.Align.START });
        }

        let label = new St.Label({ text: text });

        box.add(label, { x_fill: true,
                         y_fill: false,
                         x_align: St.Align.END,
                         y_align: St.Align.START });

        return box;
    },

    createIcon: function(size) {
        let tc = St.TextureCache.get_default();
        let icon = this.individual.avatar;

        if (icon != null) {
            return tc.load_gicon(null, icon, size);
        } else {
            return tc.load_icon_name(null, 'avatar-default', St.IconType.FULLCOLOR, size);
        }
    },
});


/* Searches for and returns contacts */
const ContactSearchProvider = new Lang.Class({
    Name: 'ContactSearchProvider',
    Extends: Search.SearchProvider,

    _init: function() {
        this.parent(_("CONTACTS"));
        this._contactSys = Shell.ContactSystem.get_default();
    },

    getResultMetas: function(ids) {
        let metas = [];
        for (let i = 0; i < ids.length; i++) {
            let contact = new Contact(ids[i]);
            metas.push({ 'id': ids[i],
                         'name': contact.alias,
                         'createIcon': function(size) {
                             return contact.createIcon(size);
                         }
                       });
        }
        return metas;
    },

    getInitialResultSet: function(terms) {
        return this._contactSys.initial_search(terms);
    },

    getSubsearchResultSet: function(previousResults, terms) {
        return this._contactSys.subsearch(previousResults, terms);
    },

    createResultActor: function(resultMeta, terms) {
        let contact = new Contact(resultMeta.id);
        return contact.actor;
    },

    createResultContainerActor: function() {
        let grid = new IconGrid.IconGrid({ rowLimit: MAX_SEARCH_RESULTS_ROWS,
                                             xAlign: St.Align.START });
        grid.actor.style_class = 'contact-grid';

        let actor = new SearchDisplay.GridSearchResults(this, grid);
        return actor;
    },

    activateResult: function(id, params) {
        launchContact(id);
    }
});
