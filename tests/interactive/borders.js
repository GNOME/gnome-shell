/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;

const UI = imports.testcommon.ui;

UI.init();
let stage = Clutter.Stage.get_default();
stage.width = 600;
stage.height = 600;

let vbox = new St.BoxLayout({ vertical: true,
                              width: stage.width,
                              height: stage.height,
                              spacing: 20,
                              style: 'padding: 10px; background: #ffee88' });
stage.add_actor(vbox);

vbox.add(new St.Label({ text: "Hello World",
                        style: 'border: 1px solid black; '
                               + 'padding: 5px;' }));

vbox.add(new St.Label({ text: "Hello Round World",
                        style: 'border: 3px solid green; '
                               + 'border-radius: 8px; '
                               + 'padding: 5px;' }));

vbox.add(new St.Label({ text: "Hello Background",
                        style: 'border: 3px solid green; '
                               + 'border-radius: 8px; '
                               + 'background: white; '
                               + 'padding: 5px;' }));

vbox.add(new St.Label({ text: "Border, Padding, Content: 20px" }));

let b1 = new St.BoxLayout({ vertical: true,
                            style: 'border: 20px solid black; '
                                   + 'background: white; '
                                   + 'padding: 20px;' });
vbox.add(b1);

b1.add(new St.BoxLayout({ width: 20, height: 20,
                          style: 'background: black' }));

vbox.add(new St.Label({ text: "Translucent blue border",
                        style: 'border: 20px solid rgba(0, 0, 255, 0.2); '
                               + 'background: white; '
                               + 'padding: 10px;' }));

vbox.add(new St.Label({ text: "Border Image",
                        style_class: "border-image",
                        style: "padding: 10px;" }));

stage.show();
Clutter.main();
stage.destroy();
