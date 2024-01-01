import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

import './environment.js';
import {formatError} from '../misc/errorUtils.js';

// Run the Mutter main loop after
// GJS finishes resolving this module.
imports._promiseNative.setMainLoopHook(() => {
    // Queue starting the shell
    GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
        import('./main.js').then(main => main.start()).catch(e => {
            const error = new GLib.Error(
                Gio.IOErrorEnum, Gio.IOErrorEnum.FAILED, formatError(e));
            global.context.terminate_with_error(error);
        });
        return GLib.SOURCE_REMOVE;
    });

    // Run the meta context's main loop
    global.context.run_main_loop();
});
