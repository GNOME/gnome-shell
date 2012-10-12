// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;

const CenterLayout = imports.ui.centerLayout;
const UI = imports.testcommon.ui;

function test() {
    let stage = new Clutter.Stage({ user_resizable: true });
    UI.init(stage);

    ////////////////////////////////////////////////////////////////////////////////

    let container = new St.Widget({ style: 'border: 2px solid black;',
                                    layout_manager: new CenterLayout.CenterLayout() });
    container.add_constraint(new Clutter.BindConstraint({ coordinate: Clutter.BindCoordinate.SIZE, source: stage }));
    stage.add_actor(container);

    let left = new Clutter.Actor({ background_color: Clutter.Color.get_static(Clutter.StaticColor.RED), width: 300 });
    let center = new Clutter.Actor({ background_color: Clutter.Color.get_static(Clutter.StaticColor.BLUE), width: 100 });
    let right = new Clutter.Actor({ background_color: Clutter.Color.get_static(Clutter.StaticColor.YELLOW), width: 200 });

    container.add_actor(left);
    container.add_actor(center);
    container.add_actor(right);

    UI.main(stage);
}
test();
