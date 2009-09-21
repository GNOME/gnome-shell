/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Nbtk = imports.gi.Nbtk;

const UI = imports.testcommon.ui;

UI.init();
let stage = Clutter.Stage.get_default();

let vbox = new Nbtk.BoxLayout({ vertical: true,
                                width: stage.width,
                                height: stage.height,
                                spacing: 10,
                                style: 'padding: 10px' });
stage.add_actor(vbox);

////////////////////////////////////////////////////////////////////////////////

let colored_boxes = new Nbtk.BoxLayout({ vertical: true,
                                         width:  200,
                                         height: 200,
                                         style: 'border: 2px solid black;' });
vbox.add(colored_boxes, { x_fill: false,
                          x_align: Nbtk.Align.MIDDLE });

let b2 = new Nbtk.BoxLayout({ style: 'border: 2px solid #666666' });
colored_boxes.add(b2, { expand: true });

b2.add(new Nbtk.Label({ text: "Expand",
                        style: 'border: 1px solid #aaaaaa; '
                               + 'background: #ffeecc' }),
       { expand: true });
b2.add(new Nbtk.Label({ text: "Expand\nNo Fill",
                        style: 'border: 1px solid #aaaaaa; '
                               + 'background: #ccffaa' }),
       { expand: true,
         x_fill: false,
         x_align: Nbtk.Align.MIDDLE,
         y_fill: false,
         y_align: Nbtk.Align.MIDDLE });

colored_boxes.add(new Nbtk.Label({ text: "Default",
                                   style: 'border: 1px solid #aaaaaa; '
                                          + 'background: #cceeff' }));

////////////////////////////////////////////////////////////////////////////////

function createCollapsableBox(width) {
    let b = new Nbtk.BoxLayout({ width: width,
                                 style: 'border: 1px solid black;'
                                        + 'font: 13px Sans;' });
    b.add(new Nbtk.Label({ text: "Very Very Very Long",
                           style: 'background: #ffaacc;'
                                  + 'padding: 5px; '
                                  + 'border: 1px solid #666666;' }),
          { expand: true });
    b.add(new Nbtk.Label({ text: "Very Very Long",
                           style: 'background: #ffeecc; '
                                  + 'padding: 5px; '
                                  + 'border: 1px solid #666666;' }),
          { expand: true });
    b.add(new Nbtk.Label({ text: "Very Long",
                           style: 'background: #ccffaa; '
                                  + 'padding: 5px; '
                                  + 'border: 1px solid #666666;' }),
          { expand: true });
    b.add(new Nbtk.Label({ text: "Short",
                           style: 'background: #cceeff; '
                                  + 'padding: 5px; '
                                  + 'border: 1px solid #666666;' }),
          { expand: true });

    return b;
}

for (let width = 200; width <= 500; width += 60 ) {
    vbox.add(createCollapsableBox (width),
             { x_fill: false,
               x_align: Nbtk.Align.MIDDLE });
}

////////////////////////////////////////////////////////////////////////////////


stage.show();
Clutter.main();
