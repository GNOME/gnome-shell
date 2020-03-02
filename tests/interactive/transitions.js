// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const UI = imports.testcommon.ui;

const { Clutter, St } = imports.gi;

function test() {
    let stage = new Clutter.Stage();
    UI.init(stage);

    let hbox = new St.BoxLayout({ name: 'transition-container',
                                  reactive: true,
                                  track_hover: true,
                                  width: stage.width,
                                  height: stage.height,
                                  style: 'padding: 10px;'
                                  + 'spacing: 10px;' });
    stage.add_actor(hbox);

    for (let i = 0; i < 5; i ++) {
        let label = new St.Label({ text: (i+1).toString(),
                                   name: "label" + i,
                                   style_class: 'transition-label',
                                   reactive: true,
                                   track_hover: true });

        hbox.add(label, { x_fill: false,
                          y_fill: false });
    }

    ////////////////////////////////////////////////////////////////////////////////

    UI.main(stage);
}
test();
