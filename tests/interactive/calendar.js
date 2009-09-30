/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;

const Calendar =imports.ui.calendar;
const UI = imports.testcommon.ui;

const Gettext_gtk20 = imports.gettext.domain('gtk20');

UI.init();
let stage = Clutter.Stage.get_default();
stage.width = stage.height = 400;
stage.show();

let vbox = new St.BoxLayout({ vertical: true,
                              width: stage.width,
                              height: stage.height,
                              style: 'padding: 10px; spacing: 10px; font: 15px sans-serif;' });
stage.add_actor(vbox);

let calendar = new Calendar.Calendar();
vbox.add(calendar.actor,
         { expand: true,
           x_fill: false, x_align: St.Align.MIDDLE,
           y_fill: false, y_align: St.Align.START });

stage.show();
Clutter.main();
stage.destroy();
