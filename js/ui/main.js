const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;

function start() {
    let global = Shell.global_get();

    let message = new Clutter.Label({font_name: "Sans Bold 64px", text: "DRAFT"});
    message.set_opacity(75);
    message.set_anchor_point_from_gravity (Clutter.Gravity.CENTER);
    message.set_rotation(Clutter.RotateAxis.Z_AXIS, - 45, 0, 0, 0);
    message.set_position(global.screen_width / 2, global.screen_height / 2);
    global.overlay_group.add_actor(message);
}
