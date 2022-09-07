// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const UI = imports.testcommon.ui;

const { Cogl, Clutter, Meta, St } = imports.gi;


function test() {
    Meta.init();

    let stage = global.backend.get_stage();
    UI.init(stage);

    let vbox = new St.BoxLayout({ style: 'background: #ffee88;' });
    vbox.add_constraint(new Clutter.BindConstraint({ source: stage,
                                                     coordinate: Clutter.BindCoordinate.SIZE }));
    stage.add_actor(vbox);

    let scroll = new St.ScrollView();
    vbox.add(scroll, { expand: true });

    vbox = new St.BoxLayout({ vertical: true,
                              style: 'padding: 10px;'
                              + 'spacing: 20px;' });
    scroll.add_actor(vbox);

    let tbox = null;

    function addTestCase(image, size, backgroundSize, useCairo) {
        let obin = new St.Bin({ style: 'border: 3px solid green;' });
        tbox.add(obin);

        let [width, height] = size;
        let bin = new St.Bin({ style_class: 'background-image-' + image,
                               width: width,
                               height: height,
                               style: `${useCairo
                                   ? 'border: 1px solid transparent;' : ''
                               } background-size: ${backgroundSize};`,
                               x_fill: true,
                               y_fill: true
                             });
        obin.set_child(bin);

        bin.set_child(new St.Label({ text: backgroundSize + (useCairo ? ' (cairo)' : ' (cogl)'),
                                     style: 'font-size: 15px;'
                                     + 'text-align: center;'
                                   }));
    }

    function addTestLine(image, size) {
        const backgroundSizes = ["auto", "contain", "cover", "200px 200px", "100px 100px", "100px 200px"];

        let [width, height] = size;
        vbox.add(new St.Label({ text: image + '.svg / ' + width + '×' + height,
                                style: 'font-size: 15px;'
                                + 'text-align: center;'
                              }));

        tbox = new St.BoxLayout({ style: 'spacing: 20px;' });
        vbox.add(tbox);

        for (let s of backgroundSizes)
            addTestCase(image, size, s, false);
        for (let s of backgroundSizes)
            addTestCase(image, size, s, true);
    }

    function addTestImage(image) {
        const containerSizes = [[100, 100], [200, 200], [250, 250], [100, 250], [250, 100]];

        for (let size of containerSizes)
            addTestLine(image, size);
    }

    addTestImage ('200-200');
    addTestImage ('200-100');
    addTestImage ('100-200');

    UI.main(stage);
}
test();
