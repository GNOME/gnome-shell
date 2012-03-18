// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Cogl = imports.gi.Cogl;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;

const UI = imports.testcommon.ui;

UI.init();
let stage = Clutter.Stage.get_default();
stage.width = 1024;
stage.height = 768;

let vbox = new St.BoxLayout({ width: stage.width,
                              height: stage.height,
                              style: 'background: #ffee88;' });
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

    let bin = new St.Bin({ style_class: 'background-image-' + image,
                           width: size.width,
                           height: size.height,
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

function addTestLine(image, size, backgroundSizes, useCairo) {
    vbox.add(new St.Label({ text: image + '.svg / ' + size.width + '×' + size.height,
                            style: 'font-size: 15px;'
                                   + 'text-align: center;'
                          }));

    tbox = new St.BoxLayout({ style: 'spacing: 20px;' });
    vbox.add(tbox);

    if (backgroundSizes.length == 2)
        addTestCase(image, size, "auto");
    for each (let s in backgroundSizes)
        addTestCase(image, size, s, false);
    for each (let s in backgroundSizes)
        addTestCase(image, size, s, true);
}

let size1 = { width: 200, height: 200 }
let size2 = { width: 250, height: 250 }
let size3 = { width: 100, height: 100 }

// fixed size
addTestLine('200-200', size1, ["200px 200px", "100px 100px", "100px 200px"]);

// same size
addTestLine('200-200', size1, ["contain", "cover"]);
// smaller
addTestLine('200-200', size2, ["contain", "cover"]);
// larger
addTestLine('200-200', size3, ["contain", "cover"]);


addTestLine('200-100', size1, ["contain", "cover"]);
addTestLine('200-100', size2, ["contain", "cover"]);
addTestLine('200-100', size3, ["contain", "cover"]);


addTestLine('100-200', size1, ["contain", "cover"]);
addTestLine('100-200', size2, ["contain", "cover"]);
addTestLine('100-200', size3, ["contain", "cover"]);

stage.show();
Clutter.main();
stage.destroy();
