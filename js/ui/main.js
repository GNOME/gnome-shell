const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;

function start() {
    let global = Shell.global_get();

    let message = new Clutter.Label({font_name: "Sans Bold 64px", text: "DRAFT"});
    message.set_opacity(75);
    // Not working for unclear reasons
    // message.set_rotation(Clutter.RotateAxis.Z_AXIS, - 45, 0, 0, 0);
    message.set_position(100, 100);
    global.get_overlay_group().add_actor(message);
}
