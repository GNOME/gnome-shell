// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const UI = imports.testcommon.ui;

const { Clutter, St } = imports.gi;

function test() {
    let stage = new Clutter.Stage({ width: 400, height: 400 });
    UI.init(stage);

    let vbox = new St.BoxLayout({ vertical: true,
                                  width: stage.width,
                                  height: stage.height,
                                  style: 'padding: 10px; spacing: 10px; font: 15px sans-serif;' });
    stage.add_actor(vbox);

    // Calendar can only be imported after Environment.init()
    const Calendar = imports.ui.calendar;
    let calendar = new Calendar.Calendar();
    vbox.add(calendar,
             { expand: true,
               x_fill: false, x_align: St.Align.MIDDLE,
               y_fill: false, y_align: St.Align.START });
    calendar.setEventSource(new Calendar.EmptyEventSource());

    UI.main(stage);
}
test();
