// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const UI = imports.testcommon.ui;

const { Clutter, GLib, St } = imports.gi;

function test() {
    let stage = new Clutter.Stage({ width: 400, height: 400 });
    UI.init(stage);

    let vbox = new St.BoxLayout({ vertical: true,
                                  width: stage.width,
                                  height: stage.height,
                                  style: 'padding: 10px; spacing: 10px; font: 32px sans-serif;' });
    stage.add_actor(vbox);

    let entry = new St.Entry({ style: 'border: 1px solid black; text-shadow: 0 2px red;',
                               text: 'Example text' });
    vbox.add(entry,
             { expand: true,
               y_fill: false, y_align: St.Align.MIDDLE });
    entry.grab_key_focus();

    let entryTextHint = new St.Entry({ style: 'border: 1px solid black; text-shadow: 0 2px red;',
                                       hint_text: 'Hint text' });
    vbox.add(entryTextHint,
             { expand: true,
               y_fill: false, y_align: St.Align.MIDDLE });

    let hintActor = new St.Label({ text: 'Hint actor' });
    let entryHintActor = new St.Entry({ style: 'border: 1px solid black; text-shadow: 0 2px red;',
                                        hint_actor: hintActor });
    vbox.add(entryHintActor,
             { expand: true,
               y_fill: false, y_align: St.Align.MIDDLE });

    let hintActor2 = new St.Label({ text: 'Hint both (actor)' });
    let entryHintBoth = new St.Entry({ style: 'border: 1px solid black; text-shadow: 0 2px red;',
                                       hint_actor: hintActor2 });
    let idx = 0;
    GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 1, function() {
        idx++;

        if (idx % 2 == 0)
            entryHintBoth.hint_actor = hintActor2;
        else
            entryHintBoth.hint_text = 'Hint both (text)';

        return true;
    });
    vbox.add(entryHintBoth,
             { expand: true,
               y_fill: false, y_align: St.Align.MIDDLE });

    UI.main(stage);
}
test();
