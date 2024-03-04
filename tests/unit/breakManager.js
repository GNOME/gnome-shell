// Copyright 2024 GNOME Foundation, Inc.
//
// This is a GNOME Shell component to support break reminders and screen time
// statistics.
//
// Licensed under the GNU General Public License Version 2
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// SPDX-License-Identifier: GPL-2.0-or-later

import 'resource:///org/gnome/shell/ui/environment.js';
import GLib from 'gi://GLib';

import * as BreakManager from 'resource:///org/gnome/shell/misc/breakManager.js';

// Convenience alias
const {BreakState} = BreakManager;

// A harness for testing the BreakManager class. It simulates the passage of
// time, maintaining an internal ordered queue of events, and providing three
// groups of mock functions which the BreakManager uses to interact with it:
// a mock version of the IdleMonitor, mock versions of GLib’s clock and timeout
// functions, and a mock version of Gio.Settings.
//
// The internal ordered queue of events is sorted by time (in seconds since an
// arbitrary epoch; the tests arbitrarily start from 100s to avoid potential
// issues around time zero). On each _tick(), the next event is shifted off the
// head of the queue and processed. An event might be a simulated idle watch
// (mocking the user being idle), a simulated active watch, a scheduled timeout,
// or an assertion function for actually running test assertions.
//
// The simulated clock jumps from the scheduled time of one event to the
// scheduled time of the next. This way, we can simulate half an hour of active
// time (waiting for the next rest break to be due) on the computer instantly.
class TestHarness {
    constructor(settings) {
        this._currentTimeSecs = 100;
        this._nextSourceId = 1;
        this._events = [];
        this._idleWatch = null;
        this._activeWatch = null;
        this._settings = settings;
    }

    _allocateSourceId() {
        const sourceId = this._nextSourceId;
        this._nextSourceId++;
        return sourceId;
    }

    _removeEventBySourceId(sourceId) {
        const idx = this._events.findIndex(a => {
            return a.sourceId === sourceId;
        });
        console.assert(idx !== -1);
        this._events.splice(idx, 1);
    }

    _insertEvent(event) {
        this._events.push(event);
        this._events.sort((a, b) => {
            return a.time - b.time;
        });
        return event;
    }

    // Add a timeout event to the event queue. It will be scheduled at the
    // current simulated time plus `intervalSecs`. `callback` will be invoked
    // when the event is processed.
    addTimeoutEvent(intervalSecs, callback) {
        return this._insertEvent({
            type: 'timeout',
            time: this._currentTimeSecs + intervalSecs,
            callback,
            sourceId: this._allocateSourceId(),
            intervalSecs,
        });
    }

    // Add an idle watch event to the event queue. This simulates the user
    // becoming idle (no keyboard or mouse input) at time `timeSecs`.
    addIdleEvent(timeSecs) {
        return this._insertEvent({
            type: 'idle',
            time: timeSecs,
        });
    }

    // Add an active watch event to the event queue. This simulates the user
    // becoming active (using the keyboard or mouse after a period of
    // inactivity) at time `timeSecs`.
    addActiveEvent(timeSecs) {
        return this._insertEvent({
            type: 'active',
            time: timeSecs,
        });
    }

    // Add a delay action invocation to the event queue. This simulates the user
    // invoking the ‘delay’ action (typically via a notification) at time
    // `timeSecs`.
    addDelayAction(timeSecs, breakManager) {
        return this._insertEvent({
            type: 'action',
            time: timeSecs,
            callback: () => {
                breakManager.delayBreak();
            },
        });
    }

    // Add a skip action invocation to the event queue. This simulates the user
    // invoking the ‘skip’ action (typically via a notification) at time
    // `timeSecs`.
    addSkipAction(timeSecs, breakManager) {
        return this._insertEvent({
            type: 'action',
            time: timeSecs,
            callback: () => {
                breakManager.skipBreak();
            },
        });
    }

    // Add a take action invocation to the event queue. This simulates the user
    // invoking the ‘take’ action (typically via a notification) at time
    // `timeSecs`.
    addTakeAction(timeSecs, breakManager) {
        return this._insertEvent({
            type: 'action',
            time: timeSecs,
            callback: () => {
                breakManager.takeBreak();
            },
        });
    }

    // Add an assertion event to the event queue. This is a callback which is
    // invoked when the simulated clock reaches `timeSecs`. The callback can
    // contain whatever test assertions you like.
    addAssertionEvent(timeSecs, callback) {
        return this._insertEvent({
            type: 'assertion',
            time: timeSecs,
            callback,
        });
    }

    // Add a state assertion event to the event queue. This is a specialised
    // form of `addAssertionEvent()` which asserts that the `BreakManager.state`
    // equals `state` at time `timeSecs`.
    expectState(timeSecs, breakManager, expectedState) {
        return this.addAssertionEvent(timeSecs, () => {
            expect(BreakManager.breakStateToString(breakManager.state))
                .withContext(`${timeSecs}s state`)
                .toEqual(BreakManager.breakStateToString(expectedState));
        });
    }

    // Add a state assertion event to the event queue. This is a specialised
    // form of `addAssertionEvent()` which asserts that the given `BreakManager`
    // properties equal the expected values at time `timeSecs`.
    expectProperties(timeSecs, breakManager, expectedProperties) {
        return this.addAssertionEvent(timeSecs, () => {
            for (const [name, expectedValue] of Object.entries(expectedProperties)) {
                expect(breakManager[name])
                    .withContext(`${timeSecs}s ${name}`)
                    .toEqual(expectedValue);
            }
        });
    }

    _popEvent() {
        return this._events.shift();
    }

    // Get a mock clock object for use in the `BreakManager` under test.
    // This provides a basic implementation of GLib’s clock and timeout
    // functions which use the simulated clock and event queue.
    get mockClock() {
        return {
            getRealTimeSecs: () => {
                return this._currentTimeSecs;
            },
            timeoutAddSeconds: (priority, intervalSecs, callback) => {
                return this.addTimeoutEvent(intervalSecs, callback).sourceId;
            },
            sourceRemove: sourceId => {
                this._removeEventBySourceId(sourceId);
            },
        };
    }

    // Get a mock idle monitor object for use in the `BreakManager` under test.
    // This provides a basic implementation of the `IdleMonitor` which uses the
    // simulated clock and event queue.
    get mockIdleMonitor() {
        return {
            add_idle_watch: (waitMsec, callback) => {
                console.assert(this._idleWatch === null);
                this._idleWatch = {
                    waitMsec,
                    callback,
                };
                return 1;
            },

            add_user_active_watch: callback => {
                console.assert(this._activeWatch === null);
                this._activeWatch = callback;
                return 2;
            },

            remove_watch: id => {
                console.assert(id === 1 || id === 2);
                if (id === 1)
                    this._idleWatch = null;
                else if (id === 2)
                    this._activeWatch = null;
            },
        };
    }

    // Get a mock settings factory for use in the `BreakManager` under test.
    // This is an object providing a couple of constructors for `Gio.Settings`
    // objects. Each constructor returns a basic implementation of
    // `Gio.Settings` which uses the settings dictionary passed to `TestHarness`
    // in its constructor.
    // This necessarily has an extra layer of indirection because there are
    // multiple ways to construct a `Gio.Settings`.
    get mockSettingsFactory() {
        return {
            new: schemaId => {
                return {
                    connect: (unusedSignalName, unusedCallback) => {
                        /* no-op for mock purposes */
                        return 1;
                    },
                    get_boolean: key => {
                        return this._settings[schemaId][key];
                    },
                    get_strv: key => {
                        return this._settings[schemaId][key];
                    },
                    get_uint: key => {
                        return this._settings[schemaId][key];
                    },
                };
            },

            newWithPath: (schemaId, unusedPath) => {
                return {
                    connect: (unusedSignalName, unusedCallback) => {
                        /* no-op for mock purposes */
                        return 1;
                    },
                    get_boolean: key => {
                        return this._settings[schemaId][key];
                    },
                    get_strv: key => {
                        return this._settings[schemaId][key];
                    },
                    get_uint: key => {
                        return this._settings[schemaId][key];
                    },
                };
            },
        };
    }

    _tick() {
        const event = this._popEvent();
        if (!event)
            return false;
        this._currentTimeSecs = event.time;

        switch (event.type) {
        case 'timeout':
            if (event.callback()) {
                event.time += event.intervalSecs;
                this._insertEvent(event);
            }
            break;
        case 'idle':
            if (this._idleWatch)
                this._idleWatch.callback();
            break;
        case 'active':
            if (this._activeWatch) {
                this._activeWatch();
                this._activeWatch = null;  // one-shot
            }
            break;
        case 'action':
            event.callback();
            break;
        case 'assertion':
            event.callback();
            break;
        default:
            console.assert(false, 'not reached');
        }

        return true;
    }

    // Run the test in a loop, blocking until all events are processed or an
    // exception is raised.
    run() {
        const loop = new GLib.MainLoop(null, false);
        let innerException = null;

        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            try {
                if (this._tick())
                    return GLib.SOURCE_CONTINUE;
                loop.quit();
                return GLib.SOURCE_REMOVE;
            } catch (e) {
                // Quit the main loop then re-raise the exception
                loop.quit();
                innerException = e;
                return GLib.SOURCE_REMOVE;
            }
        });

        loop.run();

        // Did we exit with an exception?
        if (innerException)
            throw innerException;
    }
}

describe('Break manager', () => {
    it('can be disabled via GSettings', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.break-reminders': {
                'selected-breaks': [],
            },
        });
        const breakManager = new BreakManager.BreakManager(harness.mockClock, harness.mockIdleMonitor, harness.mockSettingsFactory);

        harness.addActiveEvent(101);
        harness.expectState(102, breakManager, BreakState.DISABLED);
        harness.addIdleEvent(130);
        harness.expectState(135, breakManager, BreakState.DISABLED);

        harness.run();
    });

    // A simple test which simulates the user being active briefly, taking a short
    // break before one is due, and then being active again until their next break
    // is overdue.
    it('tracks a single break type', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.break-reminders': {
                'selected-breaks': ['movement'],
            },
            'org.gnome.desktop.break-reminders.movement': {
                'duration-seconds': 300,  /* 5 minutes */
                'interval-seconds': 1800,  /* 30 minutes */
                'delay-seconds': 300,  /* 5 minutes */
                'notify': true,
                'play-sound': false,
                'fade-screen': false,
            },
        });
        const breakManager = new BreakManager.BreakManager(harness.mockClock, harness.mockIdleMonitor, harness.mockSettingsFactory);

        harness.addActiveEvent(101);
        harness.expectState(102, breakManager, BreakState.ACTIVE);
        harness.addIdleEvent(130);
        harness.expectState(135, breakManager, BreakState.IDLE);
        harness.addActiveEvent(200);  // cut the break short before its duration
        harness.expectState(201, breakManager, BreakState.ACTIVE);
        harness.expectProperties(2001, breakManager, {  // break is due after 30 mins
            'state': BreakState.BREAK_DUE,
            'currentBreakType': 'movement',
            'currentBreakStartTime': 0,
            'lastBreakEndTime': 100,
        });
        harness.addIdleEvent(2005);
        harness.expectProperties(2006, breakManager, {
            'state': BreakState.IN_BREAK,
            'currentBreakType': 'movement',
            'currentBreakStartTime': 1900,
            'lastBreakEndTime': 0,
        });
        harness.expectState(2195, breakManager, BreakState.IN_BREAK);  // near the end of the break
        harness.expectState(2210, breakManager, BreakState.IDLE);  // just after the end of the break
        harness.addActiveEvent(2320);
        harness.expectProperties(2321, breakManager, {
            'state': BreakState.ACTIVE,
            'currentBreakType': null,
            'currentBreakStartTime': 0,
            'lastBreakEndTime': 2320,
        });
        harness.addIdleEvent(4100);  // start the next break a little early
        harness.expectState(4101, breakManager, BreakState.IDLE);
        harness.expectState(4121, breakManager, BreakState.IN_BREAK);
        harness.addActiveEvent(4420);
        harness.expectProperties(4421, breakManager, {
            'state': BreakState.ACTIVE,
            'currentBreakType': null,
            'currentBreakStartTime': 0,
            'lastBreakEndTime': 4420,
        });

        harness.run();
    });

    // Test requesting to delay a break.
    it('supports delaying a break', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.break-reminders': {
                'selected-breaks': ['movement'],
            },
            'org.gnome.desktop.break-reminders.movement': {
                'duration-seconds': 300,  /* 5 minutes */
                'interval-seconds': 1800,  /* 30 minutes */
                'delay-seconds': 300,  /* 5 minutes */
                'notify': true,
                'play-sound': false,
                'fade-screen': false,
            },
        });
        const breakManager = new BreakManager.BreakManager(harness.mockClock, harness.mockIdleMonitor, harness.mockSettingsFactory);

        harness.addActiveEvent(101);
        harness.expectState(102, breakManager, BreakState.ACTIVE);
        harness.expectProperties(1901, breakManager, {  // break is due after 30 mins
            'state': BreakState.BREAK_DUE,
            'currentBreakType': 'movement',
            'currentBreakStartTime': 0,
            'lastBreakEndTime': 100,
        });
        harness.addDelayAction(1902, breakManager);
        harness.expectProperties(1903, breakManager, {  // break is delayed by 5 mins
            'state': BreakState.ACTIVE,
            'currentBreakType': null,
            'currentBreakStartTime': 0,
            'lastBreakEndTime': 400,
        });
        harness.expectProperties(2201, breakManager, {  // break is due after another 5 mins
            'state': BreakState.BREAK_DUE,
            'currentBreakType': 'movement',
            'currentBreakStartTime': 0,
            'lastBreakEndTime': 400,
        });
        harness.addIdleEvent(2202);
        harness.expectState(2203, breakManager, BreakState.IN_BREAK);

        harness.run();
    });

    // Test requesting to skip a break.
    it('supports skipping a break', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.break-reminders': {
                'selected-breaks': ['movement'],
            },
            'org.gnome.desktop.break-reminders.movement': {
                'duration-seconds': 300,  /* 5 minutes */
                'interval-seconds': 1800,  /* 30 minutes */
                'delay-seconds': 300,  /* 5 minutes */
                'notify': true,
                'play-sound': false,
                'fade-screen': false,
            },
        });
        const breakManager = new BreakManager.BreakManager(harness.mockClock, harness.mockIdleMonitor, harness.mockSettingsFactory);

        harness.addActiveEvent(101);
        harness.expectState(102, breakManager, BreakState.ACTIVE);
        harness.expectProperties(1901, breakManager, {  // break is due after 30 mins
            'state': BreakState.BREAK_DUE,
            'currentBreakType': 'movement',
            'currentBreakStartTime': 0,
            'lastBreakEndTime': 100,
        });
        harness.addSkipAction(1902, breakManager);
        harness.expectProperties(1903, breakManager, {  // break is skipped for 30 mins
            'state': BreakState.ACTIVE,
            'currentBreakType': null,
            'currentBreakStartTime': 0,
            'lastBreakEndTime': 1902,
        });
        harness.expectProperties(3703, breakManager, {  // break is due after another 30 mins
            'state': BreakState.BREAK_DUE,
            'currentBreakType': 'movement',
            'currentBreakStartTime': 0,
            'lastBreakEndTime': 1902,
        });
        harness.addIdleEvent(3704);
        harness.expectState(3704, breakManager, BreakState.IN_BREAK);

        harness.run();
    });
});
