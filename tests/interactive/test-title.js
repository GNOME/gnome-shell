#!/usr/bin/env gjs

imports.gi.versions.Gtk = '4.0';
const Gtk = imports.gi.Gtk;

function nextTitle() {
    let length = Math.random() * 20;
    let str = '';

    for (let i = 0; i < length; i++) {
        // 97 == 'a'
        str += String.fromCharCode(97 + Math.random() * 26);
    }

    return str;
}

const application = new Gtk.Application({application_id: 'org.gnome.TestTitle'});
application.connect('activate', () => {
    const win = new Gtk.Window({
        application,
        title: nextTitle(),
    });
    win.present();

    setInterval(() => (win.title = nextTitle()), 5000);
});
application.run(null);
