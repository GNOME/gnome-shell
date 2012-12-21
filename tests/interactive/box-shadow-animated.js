// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Mainloop = imports.mainloop;
const GLib = imports.gi.GLib;
const Lang = imports.lang;

const UI = imports.testcommon.ui;

const DELAY = 2000;

function resize_animated(label) {
    if (label.width == 100) {
        label.save_easing_state();
        label.set_easing_mode(Clutter.AnimationMode.EASE_OUT_QUAD);
        label.set_easing_duration(DELAY - 50);
        label.set_size(500, 500);
        label.restore_easing_state();
    } else {
        label.save_easing_state();
        label.set_easing_mode(Clutter.AnimationMode.EASE_OUT_QUAD);
        label.set_easing_duration(DELAY - 50);
        label.set_size(100, 100);
        label.restore_easing_state();
    }
}

function get_css_style(shadow_style)
{
    return 'border: 20px solid black;' +
        'border-radius: 20px;' +
        'background-color: white; ' +
        'padding: 5px;' + shadow_style;
}

function test() {
    let stage = new Clutter.Stage({ width: 1000, height: 600 });
    UI.init(stage);

    let iter = 0;
    let shadowStyles = [ 'box-shadow: 3px 50px 0px 4px rgba(0,0,0,0.5);',
                         'box-shadow: 3px 4px 10px 4px rgba(0,0,0,0.5);',
                         'box-shadow: 0px 50px 0px 0px rgba(0,0,0,0.5);',
                         'box-shadow: 100px 100px 20px 4px rgba(0,0,0,0.5);'];
    let label1 = new St.Label({ style: get_css_style(shadowStyles[iter]),
                                text: shadowStyles[iter],
                                x: 20,
                                y: 20,
                                width: 100,
                                height: 100
                             });
    stage.add_actor(label1);
    let label2 = new St.Label({ style: get_css_style(shadowStyles[iter]),
                                text: shadowStyles[iter],
                                x: 500,
                                y: 20,
                                width: 100,
                                height: 100
                              });
    stage.add_actor(label2);

    resize_animated(label1);
    resize_animated(label2);
    Mainloop.timeout_add(DELAY, Lang.bind(this, function() {
        log(label1 + label1.get_size());
        resize_animated(label1);
        resize_animated(label2);
        return true;
    }));

    Mainloop.timeout_add(2 * DELAY, Lang.bind(this, function() {
        iter += 1;
        iter %= shadowStyles.length;
        label1.set_style(get_css_style(shadowStyles[iter]));
        label1.set_text(shadowStyles[iter]);
        label2.set_style(get_css_style(shadowStyles[iter]));
        label2.set_text(shadowStyles[iter]);
        return true;
    }));

    UI.main(stage);
}
test();
