import MetaTest from 'gi://MetaTest';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';

/**
 * init:
 */
export function init() {}

/**
 * run:
 */
export async function run() {
    const monitorsChangedPromise = new Promise(resolve => {
        const monitorsChangedHandlerId = Main.layoutManager.connect('monitors-changed',
            () => {
                Main.layoutManager.disconnect(monitorsChangedHandlerId);
                resolve();
            });
    });

    MetaTest.TestMonitor.new(global.context, 1280, 720, 60.0);
    await monitorsChangedPromise;
}

function expectEqual(a, b, desc = null) {
    if (a !== b)
        throw new Error(`${desc ? `${desc} ` : ''}${a} does not match ${b}`);
}

/**
 * finish:
 */
export function finish() {
    const monitors = [];

    expectEqual(global.display.get_n_monitors(), 2, 'Configured monitors');

    for (let i = 0; i < global.display.get_n_monitors(); ++i)
        monitors.push(global.display.get_monitor_geometry(i));

    const expectedScreenWidth = monitors.reduce((sum, m) => sum + m.width, 0);
    const expectedScreenHeight = monitors.reduce((max, m) => Math.max(max, m.height), 0);
    console.log(`Expecting screen to have size of: ${expectedScreenWidth}x${expectedScreenHeight}`);

    expectEqual(global.screen_width, expectedScreenWidth, 'Screen Width');
    expectEqual(global.screen_height, expectedScreenHeight, 'Screen Height');

    expectEqual(global.stage.width, global.screen_width, 'Stage Width');
    expectEqual(global.stage.height, global.screen_height, 'Stage Height');

    expectEqual(Main.layoutManager.monitors.length, monitors.length, 'Monitors length');
    expectEqual(Main.layoutManager.primaryMonitor, Main.layoutManager.monitors[0], 'Primary Monitor');
    expectEqual(Main.layoutManager._bgManagers.length, monitors.length, 'Background Managers length');

    for (let i = 0; i < monitors.length; ++i) {
        const monitor = Main.layoutManager.monitors[i];
        expectEqual(monitor.index, i, `Monitor ${i} index`);
        expectEqual(monitor.x, monitors[i].x, `Monitor ${i} x`);
        expectEqual(monitor.y, monitors[i].y, `Monitor ${i} y`);
        expectEqual(monitor.width, monitors[i].width, `Monitor ${i} width`);
        expectEqual(monitor.height, monitors[i].height, `Monitor ${i} height`);
    }
}
