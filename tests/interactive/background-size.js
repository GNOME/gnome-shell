// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Cogl = imports.gi.Cogl;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;

const UI = imports.testcommon.ui;

UI.init();
let stage = Clutter.Stage.get_default();
stage.user_resizable = true;
stage.width = 1024;
stage.height = 768;

let vbox = new St.BoxLayout({ style: 'background: #ffee88;' });
vbox.add_constraint(new Clutter.BindConstraint({ source: stage,
                                                 coordinate: Clutter.BindCoordinate.SIZE }));
stage.add_actor(vbox);

let scroll = new St.ScrollView();
vbox.add(scroll, { expand: true });

let vbox = new St.BoxLayout({ vertical: true,
                              style: 'padding: 10px;'
                                     + 'spacing: 20px;' });
scroll.add_actor(vbox);

let tbox = null;

function addTestCase(image, size, backgroundSize, useCairo) {
    // Using a border in CSS forces cairo rendering.
    // To get a border using cogl, we paint a border using
    // paint signal hacks.

    let obin = new St.Bin();
    if (useCairo)
        obin.style = 'border: 3px solid green;';
    else
        obin.connect_after('paint', function(actor) {
            Cogl.set_source_color4f(0, 1, 0, 1);

            let geom = actor.get_allocation_geometry();
            let width = 3;

            // clockwise order
            Cogl.rectangle(0, 0, geom.width, width);
            Cogl.rectangle(geom.width - width, width,
                           geom.width, geom.height);
            Cogl.rectangle(0, geom.height,
                           geom.width - width, geom.height - width);
            Cogl.rectangle(0, geom.height - width,
                           width, width);
        });
    tbox.add(obin);

    let [width, height] = size;
    let bin = new St.Bin({ style_class: 'background-image-' + image,
                           width: width,
                           height: height,
                           style: 'border: 1px solid transparent;'
                                  + 'background-size: ' + backgroundSize + ';',
                           x_fill: true,
                           y_fill: true
                         });
    obin.set_child(bin);

    bin.set_child(new St.Label({ text: backgroundSize + (useCairo ? ' (cairo)' : ' (cogl)'),
                                 style: 'font-size: 15px;'
                                        + 'text-align: center;'
                               }));
}

function addTestLine(image, size, useCairo) {
    const backgroundSizes = ["auto", "contain", "cover", "200px 200px", "100px 100px", "100px 200px"];

    let [width, height] = size;
    vbox.add(new St.Label({ text: image + '.svg / ' + width + '×' + height,
                            style: 'font-size: 15px;'
                                   + 'text-align: center;'
                          }));

    tbox = new St.BoxLayout({ style: 'spacing: 20px;' });
    vbox.add(tbox);

    for each (let s in backgroundSizes)
        addTestCase(image, size, s, false);
    for each (let s in backgroundSizes)
        addTestCase(image, size, s, true);
}

function addTestImage(image) {
    const containerSizes = [[100, 100], [200, 200], [250, 250], [100, 250], [250, 100]];

    for each (let size in containerSizes)
        addTestLine(image, size);
}

addTestImage ('200-200');
addTestImage ('200-100');
addTestImage ('100-200');

stage.show();
Clutter.main();
stage.destroy();
