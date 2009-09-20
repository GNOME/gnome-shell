/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Nbtk = imports.gi.Nbtk;

const UI = imports.testcommon.ui;

UI.init();
let stage = Clutter.Stage.get_default();

let v = new Nbtk.ScrollView({});
stage.add_actor(v);
let b = new Nbtk.BoxLayout({ vertical: true,
			     width: stage.width,
			     height: stage.height });
v.add_actor(b);

let cc_a = "a".charCodeAt(0);
let s = "";
for (let i = 0; i < 26 * 3; i++) {
    s += String.fromCharCode(cc_a + i % 26);

    let t = new Nbtk.Label({ "text": s});
    b.add(t);
}

stage.show();
Clutter.main();
