// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

imports.gi.versions.Clutter = '1.0';
imports.gi.versions.Gio = '2.0';
imports.gi.versions.Gdk = '3.0';
imports.gi.versions.GdkPixbuf = '2.0';
imports.gi.versions.Gtk = '3.0';
imports.gi.versions.TelepathyGLib = '0.12';
imports.gi.versions.TelepathyLogger = '0.2';

const Clutter = imports.gi.Clutter;;
const Gettext = imports.gettext;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

// We can't import shell JS modules yet, because they may have
// variable initializations, etc, that depend on init() already having
// been run.


// "monkey patch" in some varargs ClutterContainer methods; we need
// to do this per-container class since there is no representation
// of interfaces in Javascript
function _patchContainerClass(containerClass) {
    // This one is a straightforward mapping of the C method
    containerClass.prototype.child_set = function(actor, props) {
        let meta = this.get_child_meta(actor);
        for (let prop in props)
            meta[prop] = props[prop];
    };

    // clutter_container_add() actually is a an add-many-actors
    // method. We conveniently, but somewhat dubiously, take the
    // this opportunity to make it do something more useful.
    containerClass.prototype.add = function(actor, props) {
        this.add_actor(actor);
        if (props)
            this.child_set(actor, props);
    };
}

function _patchLayoutClass(layoutClass, styleProps) {
    if (styleProps)
        layoutClass.prototype.hookup_style = function(container) {
            container.connect('style-changed', Lang.bind(this, function() {
                let node = container.get_theme_node();
                for (let prop in styleProps) {
                    let [found, length] = node.lookup_length(styleProps[prop], false);
                    if (found)
                        this[prop] = length;
                }
            }));
        };
    layoutClass.prototype.child_set = function(actor, props) {
        let meta = this.get_child_meta(actor.get_parent(), actor);
        for (let prop in props)
            meta[prop] = props[prop];
    };
}

function _makeLoggingFunc(func) {
    return function() {
        return func([].join.call(arguments, ', '));
    };
}

function init() {
    // Add some bindings to the global JS namespace; (gjs keeps the web
    // browser convention of having that namespace be called 'window'.)
    window.global = Shell.Global.get();

    window.log = _makeLoggingFunc(window.log);

    window._ = Gettext.gettext;
    window.C_ = Gettext.pgettext;
    window.ngettext = Gettext.ngettext;

    // Miscellaneous monkeypatching
    _patchContainerClass(St.BoxLayout);
    _patchContainerClass(St.Table);

    _patchLayoutClass(Clutter.TableLayout, { row_spacing: 'spacing-rows',
                                             column_spacing: 'spacing-columns' });
    _patchLayoutClass(Clutter.GridLayout, { row_spacing: 'spacing-rows',
                                            column_spacing: 'spacing-columns' });
    _patchLayoutClass(Clutter.BoxLayout, { spacing: 'spacing' });

    Clutter.Actor.prototype.toString = function() {
        return St.describe_actor(this);
    };

    let origToString = Object.prototype.toString;
    Object.prototype.toString = function() {
        let base = origToString.call(this);
        try {
            if ('actor' in this && this.actor instanceof Clutter.Actor)
                return base.replace(/\]$/, ' delegate for ' + this.actor.toString().substring(1));
            else
                return base;
        } catch(e) {
            return base;
        }
    };

    // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=508783
    Date.prototype.toLocaleFormat = function(format) {
        return Shell.util_format_date(format, this.getTime());
    };

    let slowdownEnv = GLib.getenv('GNOME_SHELL_SLOWDOWN_FACTOR');
    if (slowdownEnv) {
        let factor = parseFloat(slowdownEnv);
        if (!isNaN(factor) && factor > 0.0)
            St.set_slow_down_factor(factor);
    }

    // OK, now things are initialized enough that we can import shell JS
    const Format = imports.format;
    const Tweener = imports.ui.tweener;

    Tweener.init();
    String.prototype.format = Format.format;
}
