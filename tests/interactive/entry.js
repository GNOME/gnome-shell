// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const UI = imports.testcommon.ui;

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;

function test() {
    let stage = new Clutter.Stage({ width: 400, height: 400 });
    UI.init(stage);

    let vbox = new St.BoxLayout({ vertical: true,
                                  width: stage.width,
                                  height: stage.height,
                                  style: 'padding: 10px; spacing: 10px; font: 32px sans-serif;' });
    stage.add_actor(vbox);

    let entry = new St.Entry({ style: 'border: 1px solid black; text-shadow: 0 2px red;',
                               text: 'Example text' });
    vbox.add(entry,
             { expand: true,
               y_fill: false, y_align: St.Align.MIDDLE });
    entry.grab_key_focus();

    UI.main(stage);
}
test();
