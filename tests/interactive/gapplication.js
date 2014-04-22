#!/usr/bin/env gjs
// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;

function do_action(action, parameter) {
    print ("Action '" + action.name + "' invoked");
}

function do_action_param(action, parameter) {
    print ("Action '" + action.name + "' invoked with parameter " + parameter.print(true));
}

function do_action_toggle(action) {
    action.set_state(GLib.Variant.new('b', !action.state.deep_unpack()));
    print ("Toggled");
}

function do_action_state_change(action) {
    print ("Action '" + action.name + "' has now state " + action.state.print(true));
}

function main() {
    Gtk.init(null);
    Gdk.set_program_class('test-gjsgapp');

    let app = new Gtk.Application({ application_id: 'org.gnome.Shell.GtkApplicationTest' });
    app.connect('activate', function() {
        print ("Activated");
    });

    let action = new Gio.SimpleAction({ name: 'one' });
    action.connect('activate', do_action);
    app.add_action(action);

    let action = new Gio.SimpleAction({ name: 'two' });
    action.connect('activate', do_action);
    app.add_action(action);

    let action = new Gio.SimpleAction({ name: 'toggle', state: GLib.Variant.new('b', false) });
    action.connect('activate', do_action_toggle);
    action.connect('notify::state', do_action_state_change);
    app.add_action(action);

    let action = new Gio.SimpleAction({ name: 'disable', enabled: false });
    action.set_enabled(false);
    action.connect('activate', do_action);
    app.add_action(action);

    let action = new Gio.SimpleAction({ name: 'parameter-int', parameter_type: GLib.VariantType.new('u') });
    action.connect('activate', do_action_param);
    app.add_action(action);

    let action = new Gio.SimpleAction({ name: 'parameter-string', parameter_type: GLib.VariantType.new('s') });
    action.connect('activate', do_action_param);
    app.add_action(action);

    let menu = new Gio.Menu();
    menu.append('An action', 'app.one');

    let section = new Gio.Menu();
    section.append('Another action', 'app.two');
    section.append('Same as above', 'app.two');
    menu.append_section(null, section);

    // another section, to check separators
    section = new Gio.Menu();
    section.append('Checkbox', 'app.toggle');
    section.append('Disabled', 'app.disable');
    menu.append_section('Subsection', section);

    // empty sections or submenus should be invisible
    menu.append_section('Empty section', new Gio.Menu());
    menu.append_submenu('Empty submenu', new Gio.Menu());

    let submenu = new Gio.Menu();
    submenu.append('Open c:\\', 'app.parameter-string::c:\\');
    submenu.append('Open /home', 'app.parameter-string::/home');
    menu.append_submenu('Recent files', submenu);

    let item = Gio.MenuItem.new('Say 42', null);
    item.set_action_and_target_value('app.parameter-int', GLib.Variant.new('u', 42));
    menu.append_item(item);

    let item = Gio.MenuItem.new('Say 43', null);
    item.set_action_and_target_value('app.parameter-int', GLib.Variant.new('u', 43));
    menu.append_item(item);

    let window = null;

    app.connect_after('startup', function(app) {
        app.set_app_menu(menu);
        window = new Gtk.ApplicationWindow({ title: "Test Application", application: app });
    });
    app.connect('activate', function(app) {
        window.present();
    });

    app.run(null);
}

main();
