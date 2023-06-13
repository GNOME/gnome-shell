import {setConsoleLogDomain} from 'console';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

setConsoleLogDomain('GNOME Shell');

imports.ui.environment.init();

// Run the Mutter main loop after
// GJS finishes resolving this module.
imports._promiseNative.setMainLoopHook(() => {
    // Queue starting the shell
    GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
        imports.ui.main.start().catch(e => {
            const error = new GLib.Error(
                Gio.IOErrorEnum, Gio.IOErrorEnum.FAILED,
                e.message);
            global.context.terminate_with_error(error);
        });
        return GLib.SOURCE_REMOVE;
    });

    // Run the meta context's main loop
    global.context.run_main_loop();
});
