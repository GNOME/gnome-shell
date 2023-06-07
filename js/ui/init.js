import {setConsoleLogDomain} from 'console';
import GLib from 'gi://GLib';
import {exit} from 'system';

setConsoleLogDomain('GNOME Shell');

imports.ui.environment.init();

// Run the Mutter main loop after
// GJS finishes resolving this module.
imports._promiseNative.setMainLoopHook(() => {
    // Queue starting the shell
    GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
        try {
            imports.ui.main.start();
        } catch (e) {
            logError(e);
            exit(1);
        }
        return GLib.SOURCE_REMOVE;
    });

    // Run the meta context's main loop
    global.context.run_main_loop();
});
