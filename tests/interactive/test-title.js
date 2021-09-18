#!/usr/bin/env gjs

import 'gi://Gtk?version=3.0';

import GLib from 'gi://GLib';
import Gtk from 'gi://Gtk';

function nextTitle() {
    let length = Math.random() * 20;
    let str = '';

    for (let i = 0; i < length; i++) {
        // 97 == 'a'
        str += String.fromCharCode(97 + Math.random() * 26);
    }

    return str;
}

function main() {
    Gtk.init(null);

    let win = new Gtk.Window({ title: nextTitle() });
    win.connect('destroy', () => {
        Gtk.main_quit();
    });
    win.present();

    GLib.timeout_add(GLib.PRIORITY_DEFAULT, 5000, function() {
        win.title = nextTitle();
        return true;
    });

    Gtk.main();
}

main();

