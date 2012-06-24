// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;

const Calendar = imports.ui.calendar;
const UI = imports.testcommon.ui;

function test() {
    let stage = new Clutter.Stage({ width: 400, height: 400 });
    UI.init(stage);

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

    UI.main(stage);
}
test();
