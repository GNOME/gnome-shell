// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const UI = imports.testcommon.ui;

const { Clutter, St } = imports.gi;

function test() {
    let stage = new Clutter.Stage({ width: 640, height: 480 });
    UI.init(stage);

    let vbox = new St.BoxLayout({ width: stage.width,
                                  height: stage.height,
                                  style: 'background: #ffee88;' });
    stage.add_actor(vbox);

    let scroll = new St.ScrollView();
    vbox.add(scroll, { expand: true });

    let box = new St.BoxLayout({ vertical: true });
    scroll.add_actor(box);

    let contents = new St.Widget({ width: 1000, height: 1000,
                                   style_class: 'background-image background-repeat' });
    box.add_actor(contents);

    UI.main(stage);
}
test();
