import MetaTest from 'gi://MetaTest';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as Scripting from 'resource:///org/gnome/shell/ui/scripting.js';

function checkAllocation(actor) {
    const box = actor.get_allocation_box();
    if (box.get_width() !== global.stage.width ||
        box.get_height() !== global.stage.height) {
        throw new Error(`${actor.name} allocation is ${box.get_width()}x${box.get_height()}, ` +
            `expected ${global.stage.width}x${global.stage.height}`);
    }
}

async function changeMonitors(change) {
    Main.overview.hide();
    await Scripting.waitLeisure();

    const manager = global.backend.get_monitor_manager();
    await new Promise(resolve => {
        const id = manager.connect('monitors-changed', () => {
            manager.disconnect(id);
            resolve();
        });
        change();
    });
    await Scripting.waitLeisure();
    Main.overview.show();
    await Scripting.waitLeisure();
}

/**
 * run:
 */
export async function run() {
    Main.overview.show();
    await Scripting.waitLeisure();
    checkAllocation(Main.layoutManager.uiGroup);
    checkAllocation(Main.layoutManager.overviewGroup);

    let monitor = null;
    try {
        await changeMonitors(() => {
            monitor = MetaTest.TestMonitor.new(global.context, 1920, 1080, 60.0);
        });
        checkAllocation(Main.layoutManager.uiGroup);
        checkAllocation(Main.layoutManager.overviewGroup);

        await changeMonitors(() => {
            monitor.destroy();
            monitor = null;
        });
        checkAllocation(Main.layoutManager.uiGroup);
        checkAllocation(Main.layoutManager.overviewGroup);
    } finally {
        monitor?.destroy();
    }
}
