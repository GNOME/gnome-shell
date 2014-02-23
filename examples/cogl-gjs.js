const Cogl = imports.gi.Cogl;
const GLib = imports.gi.GLib;
const Lang = imports.lang;

let renderer = new Cogl.Renderer();
let display = new Cogl.Display(renderer, new Cogl.OnscreenTemplate(new Cogl.SwapChain()));
let ctx = new Cogl.Context(display);

// Should be able to replace the 3 previous lines with :
// let ctx = new Cogl.Context(null);
// But crashing for some reason.

// GLib mainloop integration
let gsource = Cogl.glib_renderer_source_new(renderer, 0);
let loop = GLib.MainLoop.new(null, false);
gsource.attach(loop.get_context());

// Onscreen creation
let onscreen = new Cogl.Onscreen(ctx, 800, 600);
onscreen.show();

// Drawing pipeline
let crate = Cogl.Texture2D.new_from_file(ctx, 'crate.jpg');
let pipeline = new Cogl.Pipeline(ctx);
pipeline.set_layer_texture(0, crate);
let clearColor = new Cogl.Color();
clearColor.init_from_4f(0, 0, 0, 1.0);

// Redraw callback
let closure = onscreen.add_dirty_callback(Lang.bind(this, function() {
    onscreen.clear(Cogl.BufferBit.COLOR, clearColor);
    onscreen.draw_rectangle(pipeline, -1, -1, 1, 1);
    onscreen.swap_buffers();
    return true;
}), null);

// Quit after 5s
let tm = GLib.timeout_source_new(5000);
tm.set_callback(Lang.bind(this, function() {
    loop.quit();
    return false;
}), null);
tm.attach(loop.get_context());

// Run!
loop.run();
