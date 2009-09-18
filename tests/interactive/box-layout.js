/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;

const UI = imports.testcommon.ui;

UI.init();
let stage = Clutter.Stage.get_default();

let b = new St.BoxLayout({ vertical: true,
			     width: stage.width,
			     height: stage.height });
stage.add_actor(b);

let b2 = new St.BoxLayout();
b.add(b2, { expand: true, fill: true });

let r1 = new St.BoxLayout({ style_class: 'red',
                              width: 10,
                              height: 10 });
b2.add(r1, { expand: true });

let r2 = new St.BoxLayout({ style_class: 'green',
                              width: 10,
                              height: 10 });
b2.add(r2, { expand: true,
             x_fill: false,
             x_align: St.Align.MIDDLE,
             y_fill: false,
             y_align: St.Align.MIDDLE });

let r3 = new St.BoxLayout({ style_class: 'blue',
                              width: 10,
                              height: 10 });
b.add(r3);

stage.show();
Clutter.main();
