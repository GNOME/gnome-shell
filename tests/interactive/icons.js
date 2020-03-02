// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const UI = imports.testcommon.ui;

const { Clutter, St } = imports.gi;

function test() {
    let stage = new Clutter.Stage();
    UI.init(stage);

    let b = new St.BoxLayout({ vertical: true,
                               width: stage.width,
                               height: stage.height });
    stage.add_actor(b);

    function addTest(label, icon_props) {
        if (b.get_children().length > 0)
            b.add (new St.BoxLayout({ style: 'background: #cccccc; border: 10px transparent white; height: 1px; ' }));

        let hb = new St.BoxLayout({ vertical: false,
                                    style: 'spacing: 10px;' });

        hb.add(new St.Label({ text: label }), { y_fill: false });
        hb.add(new St.Icon(icon_props));

        b.add(hb);
    }

    addTest("Symbolic",
            { icon_name: 'battery-full-symbolic',
              icon_size: 48 });
    addTest("Full color",
            { icon_name: 'battery-full',
              icon_size: 48 });
    addTest("Default size",
            { icon_name: 'battery-full-symbolic' });
    addTest("Size set by property",
            { icon_name: 'battery-full-symbolic',
              icon_size: 32 });
    addTest("Size set by style",
            { icon_name: 'battery-full-symbolic',
              style: 'icon-size: 1em;' });
    addTest("16px icon in 48px icon widget",
            { icon_name: 'battery-full-symbolic',
              style: 'icon-size: 16px; width: 48px; height: 48px; border: 1px solid black;' });

    function iconRow(icons, box_style) {
        let hb = new St.BoxLayout({ vertical: false, style: box_style });

        for (let iconName of icons) {
            hb.add(new St.Icon({ icon_name: iconName,
                                 icon_size: 48 }));
        }

        b.add(hb);
    }

    let normalCss = 'background: white; color: black; padding: 10px 10px;';
    let reversedCss = 'background: black; color: white; warning-color: #ffcc00; error-color: #ff0000; padding: 10px 10px;';

    let batteryIcons = ['battery-full-charging-symbolic',
                        'battery-full-symbolic',
                        'battery-good-symbolic',
                        'battery-low-symbolic',
                        'battery-caution-symbolic' ];

    let volumeIcons = ['audio-volume-high-symbolic',
                       'audio-volume-medium-symbolic',
                       'audio-volume-low-symbolic',
                       'audio-volume-muted-symbolic' ];

    iconRow(batteryIcons, normalCss);
    iconRow(batteryIcons, reversedCss);
    iconRow(volumeIcons, normalCss);
    iconRow(volumeIcons, reversedCss);

    UI.main(stage);
}
test();
