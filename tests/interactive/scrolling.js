/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;

const UI = imports.testcommon.ui;

UI.init();
let stage = Clutter.Stage.get_default();

let vbox = new St.BoxLayout({ vertical: true,
                              width: stage.width,
                              height: stage.height,
                              style: "padding: 10px;" });
stage.add_actor(vbox);

let toggle = new St.Button({ label: 'Horizontal Scrolling',
                         toggle_mode: true });
vbox.add(toggle);

let v = new St.ScrollView();
vbox.add(v, { expand: true });

toggle.connect('notify::checked', function () {
    v.set_policy(toggle.checked ? St.ScrollPolicy.AUTOMATIC
                                : St.ScrollPolicy.NEVER,
                 St.ScrollPolicy.AUTOMATIC);
});

let b = new St.BoxLayout({ vertical: true,
                           style: "border: 2px solid #880000; border-radius: 10px; padding: 0px 5px;" });
v.add_actor(b);

let cc_a = "a".charCodeAt(0);
let s = "";
for (let i = 0; i < 26 * 3; i++) {
    s += String.fromCharCode(cc_a + i % 26);

    let t = new St.Label({ text: s,
                           reactive: true });
    let line = i + 1;
    t.connect('button-press-event',
              function() {
                  log("Click on line " + line);
              });
    b.add(t);
}

stage.show();
Clutter.main();
stage.destroy();
