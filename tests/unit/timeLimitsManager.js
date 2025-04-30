// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// Copyright 2024, 2025 GNOME Foundation, Inc.
//
// This is a GNOME Shell component to support screen time limits and statistics.
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
import Gio from 'gi://Gio';

import * as TimeLimitsManager from 'resource:///org/gnome/shell/misc/timeLimitsManager.js';

// Convenience aliases
const {TimeLimitsState, UserState} = TimeLimitsManager;

/**
 * A harness for testing the `TimeLimitsManager` class. It simulates the passage
 * of time, maintaining an internal ordered queue of events, and providing three
 * groups of mock functions which the `TimeLimitsManager` uses to interact with
 * it: mock versions of GLib’s clock and timeout functions, a mock proxy of the
 * logind `User` D-Bus object, and a mock version of `Gio.Settings`.
 *
 * The internal ordered queue of events is sorted by time (in real/wall clock
 * seconds, i.e. UNIX timestamps). On each _tick(), the next event is shifted
 * off the head of the queue and processed. An event might be a simulated user
 * state change (mocking the user starting or stopping a session), a scheduled
 * timeout, or an assertion function for actually running test assertions.
 *
 * The simulated clock jumps from the scheduled time of one event to the
 * scheduled time of the next. This way, we can simulate half an hour of active
 * time (simulating usage) on the device instantly.
 *
 * Times are provided as ISO 8601 date/time strings, to allow tests which span
 * multiple days to be written more easily. This simplifies things because the
 * daily time limit is reset at a specific time each morning.
 */
class TestHarness {
    constructor(settings, historyFileContents = null) {
        this._currentTimeSecs = 0;
        this._clockOffset = 100;  // make the monotonic clock lag by 100s, arbitrarily
        this._nextSourceId = 1;
        this._events = [];
        this._timeChangeNotify = null;
        this._timeChangeNotifySourceId = 0;
        this._settings = settings;
        this._settingsChangedCallback = null;
        this._settingsChangedDailyLimitSecondsCallback = null;
        this._settingsChangedDailyLimitEnabledCallback = null;
        this._settingsChangedGrayscaleCallback = null;

        // These two emulate relevant bits of the o.fdo.login1.User API
        // See https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html#User%20Objects
        this._currentUserState = 'active';
        this._currentUserIdleHint = false;

        this._loginUserPropertiesChangedCallback = null;

        this._currentPreparingForSleepState = false;
        this._loginManagerPrepareForSleepCallback = null;

        // Create a fake history file containing the given contents. Or, if no
        // contents are given, reserve a distinct new history file name but then
        // delete it so it doesn’t exist for the manager.
        const [file, stream] = Gio.File.new_tmp('gnome-shell-time-limits-manager-test-XXXXXX.json');
        if (historyFileContents)
            stream.output_stream.write_bytes(new GLib.Bytes(historyFileContents), null);
        stream.close(null);
        if (!historyFileContents)
            file.delete(null);

        this._historyFile = file;

        // And a mock D-Bus proxy for logind.
        const harness = this;

        class MockInhibitor {
            close(unusedCancellable) {
                // No-op for mock purposes
            }
        }

        class MockLoginManager {
            connectObject(signalName, callback, unusedObject) {
                if (signalName === 'prepare-for-sleep') {
                    if (harness._loginManagerPrepareForSleepCallback !== null)
                        fail('Duplicate prepare-for-sleep connection');
                    harness._loginManagerPrepareForSleepCallback = callback;
                } else {
                    // No-op for mock purposes
                }
            }

            disconnectObject(unused) {
                // Very simple implementation for mock purposes
                harness._loginManagerPrepareForSleepCallback = null;
            }

            /* eslint-disable-next-line require-await */
            async inhibit(unusedReason, unusedCancellable) {
                // Basically a no-op for mock purposes
                return new MockInhibitor();
            }

            get preparingForSleep() {
                return harness._currentPreparingForSleepState;
            }
        }

        class MockLoginUser {
            connectObject(signalName, callback, unusedObject) {
                if (signalName === 'g-properties-changed') {
                    if (harness._loginUserPropertiesChangedCallback !== null)
                        fail('Duplicate g-properties-changed connection');
                    harness._loginUserPropertiesChangedCallback = callback;
                } else {
                    // No-op for mock purposes
                }
            }

            disconnectObject(unused) {
                // Very simple implementation for mock purposes
                harness._loginUserPropertiesChangedCallback = null;
            }

            get State() {
                return harness._currentUserState;
            }

            get IdleHint() {
                return harness._currentUserIdleHint;
            }
        }

        this._mockLoginManager = new MockLoginManager();
        this._mockLoginUser = new MockLoginUser();
    }

    _cleanup() {
        try {
            this._historyFile?.delete(null);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND))
                throw e;
        }
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

        if (idx === -1)
            fail(`Removing non-existent source with ID ${sourceId}`);

        this._events.splice(idx, 1);
    }

    _insertEvent(event) {
        if (event.time < this._currentTimeSecs)
            fail(`Event ${event} cannot be before current mock clock time (${event.time} vs ${this._currentTimeSecs}`);

        this._events.push(event);
        this._events.sort((a, b) => {
            return a.time - b.time;
        });
        return event;
    }

    /**
     * Convert an ISO 8601 string to a UNIX timestamp for use in tests.
     *
     * Internally, the tests are all based on UNIX timestamps using wall clock
     * time. Those aren’t very easy to reason about when reading or writing
     * tests though, so we allow the tests to be written using ISO 8601 strings.
     *
     * @param {string} timeStr Date/Time in ISO 8601 format.
     */
    static timeStrToSecs(timeStr) {
        const dt = GLib.DateTime.new_from_iso8601(timeStr, null);
        if (dt === null)
            fail(`Time string ‘${timeStr}’ could not be parsed`);
        return dt.to_unix();
    }

    /**
     * Inverse of `timeStrToSecs()`.
     *
     * @param {number} timeSecs UNIX real/wall clock time in seconds.
     */
    _timeSecsToStr(timeSecs) {
        const dt = GLib.DateTime.new_from_unix_utc(timeSecs);
        if (dt === null)
            fail(`Time ‘${timeSecs}’ could not be represented`);
        return dt.format_iso8601();
    }

    /**
     * Add a timeout event to the event queue. It will be scheduled at the
     * current simulated time plus `intervalSecs`. `callback` will be invoked
     * when the event is processed.
     *
     * @param {number} intervalSecs Number of seconds in the future to schedule the event.
     * @param {Function} callback Callback to invoke when timeout is reached.
     */
    addTimeoutEvent(intervalSecs, callback) {
        return this._insertEvent({
            type: 'timeout',
            time: this._currentTimeSecs + intervalSecs,
            callback,
            sourceId: this._allocateSourceId(),
            intervalSecs,
        });
    }

    /**
     * Add a time change event to the event queue. This simulates the machine’s
     * real time clock changing relative to its monotonic clock, at date/time
     * `timeStr`. Such a change can happen as the result of an NTP sync, for
     * example.
     *
     * When the event is reached, the mock real/wall clock will have its time
     * set to `newTimeStr`, and then `callback` will be invoked. `callback`
     * should be used to enqueue any events *after* the time change event. If
     * they are enqueued in the same scope as `addTimeChangeEvent()`, they will
     * be mis-ordered as the event queue is sorted by mock real/wall clock time.
     *
     * @param {string} timeStr ISO 8601 date/time string to change the clock at.
     * @param {string} newTimeStr ISO 8601 date/time string to change the clock to.
     * @param {Function} callback Callback to invoke when time change is reached.
     */
    addTimeChangeEvent(timeStr, newTimeStr, callback) {
        return this._insertEvent({
            type: 'time-change',
            time: TestHarness.timeStrToSecs(timeStr),
            newTime: TestHarness.timeStrToSecs(newTimeStr),
            callback,
        });
    }

    /**
     * Add a pair of sleep and resume events to the event queue. This simulates
     * the machine being asleep (suspended) and then resumed after a period of
     * time. No other events should be inserted into the queue between these
     * two.
     *
     * This simulates the [D-Bus API for logind](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html#The%20Manager%20Object)
     * notifying to prepare for sleep at date/time `timeStr`, then sleeping,
     * then notifying that the machine has resumed from sleep at date/time
     * `timeStr` plus `duration` (in seconds).
     *
     * @param {string} timeStr Date/Time the prepare-for-sleep event happens,
     *    in ISO 8601 format.
     * @param {number} duration Duration of the sleep/suspend period, in seconds
     */
    addSleepAndResumeEvent(timeStr, duration) {
        this._insertEvent({
            type: 'preparing-for-sleep-state-change',
            time: TestHarness.timeStrToSecs(timeStr),
            newPreparingForSleepState: true,
        });
        this._insertEvent({
            type: 'time-change',
            time: TestHarness.timeStrToSecs(timeStr) + 1,
            newTime: TestHarness.timeStrToSecs(timeStr) + duration - 1,
            callback: null,
        });
        this._insertEvent({
            type: 'preparing-for-sleep-state-change',
            time: TestHarness.timeStrToSecs(timeStr) + duration,
            newPreparingForSleepState: false,
        });
    }

    /**
     * Add a login user state change event to the event queue. This simulates
     * the [D-Bus API for logind](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html#User%20Objects)
     * notifying that the user has changed state at date/time `timeStr`. For
     * example, this could represent the user logging out.
     *
     * @param {string} timeStr Date/Time the event happens, in ISO 8601 format.
     * @param {string} newState New user state as if returned by.
     *    [`sd_ui_get_state()`](https://www.freedesktop.org/software/systemd/man/latest/sd_uid_get_state.html).
     * @param {boolean} newIdleHint New user idle hint as per
     *    [the logind API](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html#User%20Objects).
     */
    addLoginUserStateChangeEvent(timeStr, newState, newIdleHint) {
        return this._insertEvent({
            type: 'login-user-state-change',
            time: TestHarness.timeStrToSecs(timeStr),
            newUserState: newState,
            newUserIdleHint: newIdleHint,
        });
    }

    /**
     * Add a settings change event to the event queue. This simulates dconf
     * notifying gnome-shell that the user has changed a setting, for example
     * from gnome-control-center.
     *
     * @param {string} timeStr Date/Time the event happens, in ISO 8601 format.
     * @param {string} schemaId ID of the schema of the setting which changed.
     * @param {string} key Key of the setting which changed.
     * @param {(number|boolean|string)} newValue New value of the setting.
     */
    addSettingsChangeEvent(timeStr, schemaId, key, newValue) {
        return this._insertEvent({
            type: 'settings-change',
            time: TestHarness.timeStrToSecs(timeStr),
            schemaId,
            key,
            newValue,
        });
    }

    /**
     * Add an assertion event to the event queue. This is a callback which is
     * invoked when the simulated clock reaches `timeStr`. The callback can
     * contain whatever test assertions you like.
     *
     * @param {string} timeStr Date/Time the event happens, in ISO 8601 format.
     * @param {Function} callback Callback with test assertions to run when the
     *    time is reached.
     */
    addAssertionEvent(timeStr, callback) {
        return this._insertEvent({
            type: 'assertion',
            time: TestHarness.timeStrToSecs(timeStr),
            callback,
        });
    }

    /**
     * Add a shutdown action to the event queue. This shuts down the
     * `timeLimitsManager` at date/time `timeStr`, and asserts that the state
     * after shutdown is as expected.
     *
     * @param {string} timeStr Date/Time to shut down the manager at, in
     *    ISO 8601 format.
     * @param {TimeLimitsManager} timeLimitsManager Manager to shut down.
     */
    shutdownManager(timeStr, timeLimitsManager) {
        return this._insertEvent({
            type: 'shutdown',
            time: TestHarness.timeStrToSecs(timeStr),
            timeLimitsManager,
        });
    }

    /**
     * Add a state assertion event to the event queue. This is a specialised
     * form of `addAssertionEvent()` which asserts that the
     * `TimeLimitsManager.state` equals `state` at date/time `timeStr`.
     *
     * @param {string} timeStr Date/Time to check the state at, in ISO 8601
     *    format.
     * @param {TimeLimitsManager} timeLimitsManager Manager to check the state of.
     * @param {TimeLimitsState} expectedState Expected state at that time.
     */
    expectState(timeStr, timeLimitsManager, expectedState) {
        return this.addAssertionEvent(timeStr, () => {
            expect(TimeLimitsManager.timeLimitsStateToString(timeLimitsManager.state))
                .withContext(`${timeStr} state`)
                .toEqual(TimeLimitsManager.timeLimitsStateToString(expectedState));
        });
    }

    /**
     * Add a state assertion event to the event queue. This is a specialised
     * form of `addAssertionEvent()` which asserts that the given
     * `TimeLimitsManager` properties equal the expected values at date/time
     * `timeStr`.
     *
     * @param {string} timeStr Date/Time to check the state at, in ISO 8601
     *    format.
     * @param {TimeLimitsManager} timeLimitsManager Manager to check the state of.
     * @param {object} expectedProperties Map of property names to expected
     *    values at that time.
     */
    expectProperties(timeStr, timeLimitsManager, expectedProperties) {
        return this.addAssertionEvent(timeStr, () => {
            for (const [name, expectedValue] of Object.entries(expectedProperties)) {
                expect(timeLimitsManager[name])
                    .withContext(`${timeStr} ${name}`)
                    .toEqual(expectedValue);
            }
        });
    }

    _popEvent() {
        return this._events.shift();
    }

    /**
     * Get a `Gio.File` for the mock history file.
     *
     * This file is populated when the `TestHarness` is created, and deleted
     * (as it’s a temporary file) after the harness is `run()`.
     *
     * @returns {Gio.File}
     */
    get mockHistoryFile() {
        return this._historyFile;
    }

    /**
     * Get a mock clock object for use in the `TimeLimitsManager` under test.
     * This provides a basic implementation of GLib’s clock and timeout
     * functions which use the simulated clock and event queue.
     */
    get mockClock() {
        return {
            getRealTimeSecs: () => {
                return this._currentTimeSecs;
            },
            getMonotonicTimeSecs: () => {
                return this._currentTimeSecs - this._clockOffset;
            },
            timeoutAddSeconds: (priority, intervalSecs, callback) => {
                return this.addTimeoutEvent(intervalSecs, callback).sourceId;
            },
            sourceRemove: sourceId => {
                if (this._timeChangeNotify !== null &&
                    sourceId === this._timeChangeNotifySourceId) {
                    this._timeChangeNotify = null;
                    this._timeChangeNotifySourceId = 0;
                    return;
                }

                this._removeEventBySourceId(sourceId);
            },
            timeChangeNotify: callback => {
                if (this._timeChangeNotify !== null)
                    fail('Duplicate time_change_notify() call');

                this._timeChangeNotify = callback;
                this._timeChangeNotifySourceId = this._nextSourceId;
                this._nextSourceId++;
                return this._timeChangeNotifySourceId;
            },
        };
    }

    /**
     * Set the initial time for the mock real/wall clock.
     *
     * This will typically become the time that the mock user first becomes
     * active, when the `TimeLimitManager` is created.
     *
     * @param {string} timeStr Date/Time to initialise the clock to, in ISO 8601
     *    format.
     */
    initializeMockClock(timeStr) {
        if (this._currentTimeSecs !== 0)
            fail('mock clock already used');

        this._currentTimeSecs = TestHarness.timeStrToSecs(timeStr);
    }

    /**
     * Get a mock login manager factory for use in the `TimeLimitsManager` under
     * test. This is an object providing a constructor for the objects returned
     * by `LoginManager.getLoginManager()`. This is roughly a wrapper around the
     * [`org.freedesktop.login1.Manager` D-Bus API](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html#The%20Manager%20Object).
     * Each constructor returns a basic implementation of the manager which uses
     * the current state from `TestHarness`.
     *
     * This has an extra layer of indirection to match `mockSettingsFactory`.
     */
    get mockLoginManagerFactory() {
        return {
            new: () => {
                return this._mockLoginManager;
            },
        };
    }

    /**
     * Get a mock login user factory for use in the `TimeLimitsManager` under
     * test. This is an object providing constructors for `LoginUser` objects,
     * which are proxies around the
     * [`org.freedesktop.login1.User` D-Bus API](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html#User%20Objects).
     * Each constructor returns a basic implementation of `LoginUser` which uses
     * the current state from `TestHarness`.
     *
     * This has an extra layer of indirection to match `mockSettingsFactory`.
     */
    get mockLoginUserFactory() {
        return {
            newAsync: () => {
                return this._mockLoginUser;
            },
        };
    }

    /**
     * Get a mock settings factory for use in the `TimeLimitsManager` under test.
     * This is an object providing constructors for `Gio.Settings` objects. Each
     * constructor returns a basic implementation of `Gio.Settings` which uses
     * the settings dictionary passed to `TestHarness` in its constructor.
     *
     * This necessarily has an extra layer of indirection because there are
     * multiple ways to construct a `Gio.Settings`.
     */
    get mockSettingsFactory() {
        return {
            new: schemaId => {
                return {
                    connectObject: (...args) => {
                        // This is very much hardcoded to how the
                        // TimeLimitsManager currently uses connectObject(), to
                        // avoid having to reimplement all the argument parsing
                        // code from SignalTracker.
                        const [
                            changedStr, changedCallback,
                            changedDailyLimitSecondsStr, changedDailyLimitSecondsCallback,
                            changedDailyLimitEnabledStr, changedDailyLimitEnabledCallback,
                            changedGrayscaleStr, changedGrayscaleCallback,
                            obj,
                        ] = args;

                        if (changedStr !== 'changed' ||
                            changedDailyLimitSecondsStr !== 'changed::daily-limit-seconds' ||
                            changedDailyLimitEnabledStr !== 'changed::daily-limit-enabled' ||
                            changedGrayscaleStr !== 'changed::grayscale' ||
                            typeof obj !== 'object')
                            fail('Gio.Settings.connectObject() not called in expected way');
                        if (this._settingsChangedCallback !== null)
                            fail('Settings signals already connected');

                        this._settingsChangedCallback = changedCallback;
                        this._settingsChangedDailyLimitSecondsCallback = changedDailyLimitSecondsCallback;
                        this._settingsChangedDailyLimitEnabledCallback = changedDailyLimitEnabledCallback;
                        this._settingsChangedGrayscaleCallback = changedGrayscaleCallback;
                    },
                    get_boolean: key => {
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

        console.debug(`Test tick: ${event.type} at ${this._timeSecsToStr(event.time)}`);

        this._currentTimeSecs = event.time;

        switch (event.type) {
        case 'timeout':
            if (event.callback()) {
                event.time += event.intervalSecs;
                this._insertEvent(event);
            }
            break;
        case 'time-change':
            this._clockOffset += event.newTime - this._currentTimeSecs;
            this._currentTimeSecs = event.newTime;

            if (event.callback !== null)
                event.callback();

            if (this._timeChangeNotify)
                this._timeChangeNotify();
            break;
        case 'preparing-for-sleep-state-change':
            this._currentPreparingForSleepState = event.newPreparingForSleepState;

            if (this._loginManagerPrepareForSleepCallback) {
                this._loginManagerPrepareForSleepCallback(
                    this._mockLoginManager, this._currentPreparingForSleepState);
            }
            break;
        case 'login-user-state-change':
            this._currentUserState = event.newUserState;
            this._currentUserIdleHint = event.newUserIdleHint;

            if (this._loginUserPropertiesChangedCallback)
                this._loginUserPropertiesChangedCallback();
            break;
        case 'settings-change':
            this._settings[event.schemaId][event.key] = event.newValue;

            if (this._settingsChangedCallback)
                this._settingsChangedCallback(event.key);
            if (event.key === 'daily-limit-seconds' &&
                this._settingsChangedDailyLimitSecondsCallback)
                this._settingsChangedDailyLimitSecondsCallback(event.key);
            if (event.key === 'daily-limit-enabled' &&
                this._settingsChangedDailyLimitEnabledCallback)
                this._settingsChangedDailyLimitEnabledCallback(event.key);
            if (event.key === 'grayscale' &&
                this._settingsChangedGrayscaleCallback)
                this._settingsChangedGrayscaleCallback(event.key);

            break;
        case 'assertion':
            event.callback();
            break;
        case 'shutdown':
            event.timeLimitsManager.shutdown().catch(() => {});

            // FIXME: This doesn’t actually properly synchronise with the
            // completion of the shutdown() call
            this._insertEvent({
                type: 'assertion',
                time: event.time + 1,
                callback: () => {
                    expect(TimeLimitsManager.timeLimitsStateToString(event.timeLimitsManager.state))
                        .withContext('Post-shutdown state')
                        .toEqual(TimeLimitsManager.timeLimitsStateToString(TimeLimitsState.DISABLED));
                    expect(event.timeLimitsManager.dailyLimitTime)
                        .withContext('Post-shutdown dailyLimitTime')
                        .toEqual(0);
                },
            });
            break;
        default:
            fail('not reached');
        }

        return true;
    }

    /**
     * Run the test in a loop, blocking until all events are processed or an
     * exception is raised.
     */
    run() {
        console.debug('Starting new unit test');

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

        this._cleanup();

        // Did we exit with an exception?
        if (innerException)
            throw innerException;
    }
}

describe('Time limits manager', () => {
    it('can be disabled via GSettings', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': false,
                'daily-limit-enabled': false,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.DISABLED);
        harness.expectState('2024-06-01T15:00:00Z', timeLimitsManager, TimeLimitsState.DISABLED);
        harness.addLoginUserStateChangeEvent('2024-06-01T15:00:10Z', 'active', false);
        harness.addLoginUserStateChangeEvent('2024-06-01T15:00:20Z', 'lingering', true);
        harness.expectProperties('2024-06-01T15:00:30Z', timeLimitsManager, {
            'state': TimeLimitsState.DISABLED,
            'dailyLimitTime': 0,
        });
        harness.shutdownManager('2024-06-01T15:10:00Z', timeLimitsManager);

        harness.run();
    });

    it('can be toggled on and off via GSettings', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.addSettingsChangeEvent('2024-06-01T11:00:00Z',
            'org.gnome.desktop.screen-time-limits', 'history-enabled', false);
        harness.expectProperties('2024-06-01T11:00:01Z', timeLimitsManager, {
            'state': TimeLimitsState.DISABLED,
            'dailyLimitTime': 0,
        });

        // Test that toggling it on and off fast is handled OK
        for (var i = 0; i < 3; i++) {
            harness.addSettingsChangeEvent('2024-06-01T11:00:02Z',
                'org.gnome.desktop.screen-time-limits', 'history-enabled', true);
            harness.addSettingsChangeEvent('2024-06-01T11:00:02Z',
                'org.gnome.desktop.screen-time-limits', 'history-enabled', false);
        }

        harness.addSettingsChangeEvent('2024-06-01T11:00:03Z',
            'org.gnome.desktop.screen-time-limits', 'history-enabled', true);
        harness.expectState('2024-06-01T11:00:04Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.shutdownManager('2024-06-01T15:10:00Z', timeLimitsManager);

        harness.run();
    });

    it('tracks a single day’s usage', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T13:59:59Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T14:00:00Z'),
        });
        harness.expectState('2024-06-01T14:00:01Z', timeLimitsManager, TimeLimitsState.LIMIT_REACHED);
        harness.shutdownManager('2024-06-01T14:10:00Z', timeLimitsManager);

        harness.run();
    });

    it('tracks a single day’s usage early in the morning', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 1 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T00:30:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        harness.expectState('2024-06-01T00:30:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T01:29:59Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T01:30:00Z'),
        });
        harness.expectState('2024-06-01T01:30:01Z', timeLimitsManager, TimeLimitsState.LIMIT_REACHED);
        harness.shutdownManager('2024-06-01T01:40:00Z', timeLimitsManager);

        harness.run();
    });

    it('tracks a single day’s usage with limits disabled', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T13:59:59Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T14:00:00Z'),
        });
        harness.expectState('2024-06-01T14:00:01Z', timeLimitsManager, TimeLimitsState.LIMIT_REACHED);

        // Now disable limits and the state should change back to ACTIVE:
        harness.addSettingsChangeEvent('2024-06-01T14:10:00Z',
            'org.gnome.desktop.screen-time-limits', 'daily-limit-enabled', false);
        harness.expectState('2024-06-01T14:10:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);

        // And try enabling it again:
        harness.addSettingsChangeEvent('2024-06-01T14:20:00Z',
            'org.gnome.desktop.screen-time-limits', 'daily-limit-enabled', true);
        harness.expectState('2024-06-01T14:20:01Z', timeLimitsManager, TimeLimitsState.LIMIT_REACHED);

        harness.shutdownManager('2024-06-01T14:30:00Z', timeLimitsManager);

        harness.run();
    });

    it('resets usage at the end of the day', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T15:00:00Z', timeLimitsManager, {
            'state': TimeLimitsState.LIMIT_REACHED,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T14:00:00Z'),
        });
        harness.addLoginUserStateChangeEvent('2024-06-01T15:00:10Z', 'offline', true);

        // the next day (after 03:00 in the morning) usage should be reset:
        harness.expectProperties('2024-06-02T13:59:59Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': 0,
        });
        harness.addLoginUserStateChangeEvent('2024-06-02T14:00:00Z', 'active', false);
        harness.expectProperties('2024-06-02T14:00:00Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-02T18:00:00Z'),
        });

        // and that limit should be reached eventually
        harness.expectProperties('2024-06-02T18:00:01Z', timeLimitsManager, {
            'state': TimeLimitsState.LIMIT_REACHED,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-02T18:00:00Z'),
        });

        harness.shutdownManager('2024-06-02T18:10:00Z', timeLimitsManager);

        harness.run();
    });

    it('resets usage if the time limit is changed', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        // Run until the limit is reached.
        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T15:00:00Z', timeLimitsManager, {
            'state': TimeLimitsState.LIMIT_REACHED,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T14:00:00Z'),
        });

        // Increase the limit.
        harness.addSettingsChangeEvent('2024-06-01T15:10:00Z',
            'org.gnome.desktop.screen-time-limits', 'daily-limit-seconds', 8 * 60 * 60);
        harness.expectProperties('2024-06-01T15:10:01Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T18:00:00Z'),
        });

        // The new limit should be reached eventually
        harness.expectProperties('2024-06-01T18:00:01Z', timeLimitsManager, {
            'state': TimeLimitsState.LIMIT_REACHED,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T18:00:00Z'),
        });

        harness.shutdownManager('2024-06-01T18:10:00Z', timeLimitsManager);

        harness.run();
    });

    it('tracks usage correctly from an existing history file', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        }, JSON.stringify([
            {
                'oldState': UserState.INACTIVE,
                'newState': UserState.ACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T07:30:00Z'),
            },
            {
                'oldState': UserState.ACTIVE,
                'newState': UserState.INACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T08:00:00Z'),
            },
            {
                'oldState': UserState.INACTIVE,
                'newState': UserState.ACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T08:30:00Z'),
            },
            {
                'oldState': UserState.ACTIVE,
                'newState': UserState.INACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T09:30:00Z'),
            },
        ]));
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        // The existing history file (above) lists two active periods,
        // 07:30–08:00 and 08:30–09:30 that morning. So the user should have
        // 2.5h left today.
        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T12:29:59Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T12:30:00Z'),
        });
        harness.expectState('2024-06-01T12:30:01Z', timeLimitsManager, TimeLimitsState.LIMIT_REACHED);
        harness.shutdownManager('2024-06-01T12:40:00Z', timeLimitsManager);

        harness.run();
    });

    it('immediately limits usage from an existing history file', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        }, JSON.stringify([
            {
                'oldState': UserState.INACTIVE,
                'newState': UserState.ACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T04:30:00Z'),
            },
            {
                'oldState': UserState.ACTIVE,
                'newState': UserState.INACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T08:50:00Z'),
            },
        ]));
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        // The existing history file (above) lists one active period,
        // 04:30–08:50 that morning. So the user should have no time left today.
        harness.expectProperties('2024-06-01T10:00:01Z', timeLimitsManager, {
            'state': TimeLimitsState.LIMIT_REACHED,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T08:30:00Z'),
        });
        harness.shutdownManager('2024-06-01T10:10:00Z', timeLimitsManager);

        harness.run();
    });

    [
        '',
        'not valid JSON',
        '[]',
        '[{}]',
        '[{"newState": 1, "wallTimeSecs": 123}]',
        '[{"oldState": 0, "wallTimeSecs": 123}]',
        '[{"oldState": 0, "newState": 1}]',
        '[{"oldState": "not a number", "newState": 1, "wallTimeSecs": 123}]',
        '[{"oldState": 0, "newState": "not a number", "wallTimeSecs": 123}]',
        '[{"oldState": 0, "newState": 1, "wallTimeSecs": "not a number"}]',
        '[{"oldState": 0, "newState": 1, "wallTimeSecs": 123.456}]',
        '[{"oldState": 666, "newState": 1, "wallTimeSecs": 123}]',
        '[{"oldState": 0, "newState": 666, "wallTimeSecs": 123}]',
        '[{"oldState": 0, "newState": 0, "wallTimeSecs": 123}]',
        '[{"oldState": 0, "newState": 1, "wallTimeSecs": 123},{"oldState": 1, "newState": 0, "wallTimeSecs": 1}]',
    ].forEach((invalidHistoryFileContents, idx) => {
        it(`ignores invalid history file syntax (test case ${idx + 1})`, () => {
            const harness = new TestHarness({
                'org.gnome.desktop.screen-time-limits': {
                    'history-enabled': true,
                    'daily-limit-enabled': true,
                    'daily-limit-seconds': 4 * 60 * 60,
                },
            }, invalidHistoryFileContents);
            harness.initializeMockClock('2024-06-01T10:00:00Z');
            const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

            // The existing history file (above) is invalid or a no-op and
            // should be ignored.
            harness.expectProperties('2024-06-01T10:00:01Z', timeLimitsManager, {
                'state': TimeLimitsState.ACTIVE,
                'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T14:00:00Z'),
            });
            harness.shutdownManager('2024-06-01T10:10:00Z', timeLimitsManager);

            harness.run();
        });
    });

    it('expires old entries from an existing history file', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        }, JSON.stringify([
            // Old entries
            {
                'oldState': UserState.INACTIVE,
                'newState': UserState.ACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T07:30:00Z') - 2 * TimeLimitsManager.HISTORY_THRESHOLD_SECONDS,
            },
            {
                'oldState': UserState.ACTIVE,
                'newState': UserState.INACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T08:00:00Z') - 2 * TimeLimitsManager.HISTORY_THRESHOLD_SECONDS,
            },
            // Recent entries
            {
                'oldState': UserState.INACTIVE,
                'newState': UserState.ACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T08:30:00Z'),
            },
            {
                'oldState': UserState.ACTIVE,
                'newState': UserState.INACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T09:30:00Z'),
            },
        ]));
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        // The existing history file (above) lists two active periods,
        // one of which is a long time ago and the other is ‘this’ morning in
        // June. After the manager is shut down and the history file stored
        // again, the older entry should have been expired.
        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T12:29:59Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T13:00:00Z'),
        });
        harness.shutdownManager('2024-06-01T12:40:00Z', timeLimitsManager);
        harness.addAssertionEvent('2024-06-01T12:50:00Z', () => {
            const [, historyContents] = harness.mockHistoryFile.load_contents(null);
            expect(JSON.parse(new TextDecoder().decode(historyContents)))
                .withContext('History file contents')
                .toEqual([
                    // Recent entries
                    {
                        'oldState': UserState.INACTIVE,
                        'newState': UserState.ACTIVE,
                        'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T08:30:00Z'),
                    },
                    {
                        'oldState': UserState.ACTIVE,
                        'newState': UserState.INACTIVE,
                        'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T09:30:00Z'),
                    },
                    // New entries
                    {
                        'oldState': UserState.INACTIVE,
                        'newState': UserState.ACTIVE,
                        'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T10:00:00Z'),
                    },
                    {
                        'oldState': UserState.ACTIVE,
                        'newState': UserState.INACTIVE,
                        'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T12:40:00Z'),
                    },
                ]);
        });

        harness.run();
    });

    it('expires future entries from an existing history file', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        }, JSON.stringify([
            {
                'oldState': UserState.INACTIVE,
                'newState': UserState.ACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('3000-06-01T04:30:00Z'),
            },
            {
                'oldState': UserState.ACTIVE,
                'newState': UserState.INACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('3000-06-01T08:50:00Z'),
            },
        ]));
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        // The existing history file (above) lists one active period,
        // 04:30–08:50 that morning IN THE YEAR 3000. This could have resulted
        // from the clock offset changing while offline. Ignore it; the user
        // should still have their full limit for the day.
        harness.expectProperties('2024-06-01T10:00:01Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T14:00:00Z'),
        });
        harness.shutdownManager('2024-06-01T10:10:00Z', timeLimitsManager);

        harness.run();
    });

    it('removes an existing history file when history is disabled', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        }, JSON.stringify([
            {
                'oldState': UserState.INACTIVE,
                'newState': UserState.ACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T08:30:00Z'),
            },
            {
                'oldState': UserState.ACTIVE,
                'newState': UserState.INACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T09:30:00Z'),
            },
        ]));
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.addSettingsChangeEvent('2024-06-01T10:00:02Z',
            'org.gnome.desktop.screen-time-limits', 'history-enabled', false);
        harness.expectState('2024-06-01T10:00:03Z', timeLimitsManager, TimeLimitsState.DISABLED);
        harness.addAssertionEvent('2024-06-01T10:00:04Z', () => {
            expect(harness.mockHistoryFile.query_exists(null))
                .withContext('History file is deleted')
                .toEqual(false);
        });
        harness.shutdownManager('2024-06-01T10:10:00Z', timeLimitsManager);

        harness.run();
    });

    it('removes an existing history file when history is disabled after limit is reached', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        }, JSON.stringify([
            {
                'oldState': UserState.INACTIVE,
                'newState': UserState.ACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T08:30:00Z'),
            },
            {
                'oldState': UserState.ACTIVE,
                'newState': UserState.INACTIVE,
                'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T09:30:00Z'),
            },
        ]));
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectState('2024-06-01T14:00:01Z', timeLimitsManager, TimeLimitsState.LIMIT_REACHED);

        // Disable history storage and the history file should be removed and
        // the state changed to DISABLED:
        harness.addSettingsChangeEvent('2024-06-01T14:00:02Z',
            'org.gnome.desktop.screen-time-limits', 'history-enabled', false);
        harness.expectState('2024-06-01T14:00:03Z', timeLimitsManager, TimeLimitsState.DISABLED);
        harness.addAssertionEvent('2024-06-01T14:00:04Z', () => {
            expect(harness.mockHistoryFile.query_exists(null))
                .withContext('History file is deleted')
                .toEqual(false);
        });

        // Re-enable history storage and things should start afresh:
        harness.addSettingsChangeEvent('2024-06-01T14:10:02Z',
            'org.gnome.desktop.screen-time-limits', 'history-enabled', true);
        harness.expectProperties('2024-06-01T14:10:03Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T18:10:02Z'),
        });

        harness.shutdownManager('2024-06-01T14:20:00Z', timeLimitsManager);

        harness.run();
    });

    it('doesn’t count usage across time change events forwards', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        // Use up 2h of the daily limit.
        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T12:00:00Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T14:00:00Z'),
        });

        harness.addTimeChangeEvent('2024-06-01T12:00:01Z', '2024-06-01T16:00:00Z', () => {
            // The following events are in the new time epoch. There should be
            // 2h of time limit left for the day.
            harness.expectProperties('2024-06-01T16:00:01Z', timeLimitsManager, {
                'state': TimeLimitsState.ACTIVE,
                'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T17:59:59Z'),
            });

            harness.expectProperties('2024-06-01T18:00:00Z', timeLimitsManager, {
                'state': TimeLimitsState.LIMIT_REACHED,
                'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T17:59:59Z'),
            });

            harness.shutdownManager('2024-06-01T18:10:00Z', timeLimitsManager);
        });

        harness.run();
    });

    it('doesn’t count usage across time change events backwards', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        // Use up 2h of the daily limit.
        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T12:00:00Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T14:00:00Z'),
        });

        harness.addTimeChangeEvent('2024-06-01T12:00:01Z', '2024-06-01T09:00:00Z', () => {
            // The following events are in the new time epoch. There should be
            // 2h of time limit left for the day.
            harness.expectProperties('2024-06-01T09:00:01Z', timeLimitsManager, {
                'state': TimeLimitsState.ACTIVE,
                'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T10:59:59Z'),
            });

            harness.expectProperties('2024-06-01T11:00:00Z', timeLimitsManager, {
                'state': TimeLimitsState.LIMIT_REACHED,
                'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T10:59:59Z'),
            });

            harness.shutdownManager('2024-06-01T11:10:00Z', timeLimitsManager);
        });

        harness.run();
    });

    it('doesn’t count usage when asleep/suspended', () => {
        const harness = new TestHarness({
            'org.gnome.desktop.screen-time-limits': {
                'history-enabled': true,
                'daily-limit-enabled': true,
                'daily-limit-seconds': 4 * 60 * 60,
            },
        });
        harness.initializeMockClock('2024-06-01T10:00:00Z');
        const timeLimitsManager = new TimeLimitsManager.TimeLimitsManager(harness.mockHistoryFile, harness.mockClock, harness.mockLoginManagerFactory, harness.mockLoginUserFactory, harness.mockSettingsFactory);

        // Use up 2h of the daily limit.
        harness.expectState('2024-06-01T10:00:01Z', timeLimitsManager, TimeLimitsState.ACTIVE);
        harness.expectProperties('2024-06-01T12:00:00Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T14:00:00Z'),
        });

        // Sleep for 3h and that shouldn’t use up limit time.
        harness.addSleepAndResumeEvent('2024-06-01T12:00:01Z', 3 * 60 * 60);
        harness.expectProperties('2024-06-01T15:00:02Z', timeLimitsManager, {
            'state': TimeLimitsState.ACTIVE,
            'dailyLimitTime': TestHarness.timeStrToSecs('2024-06-01T17:00:00Z'),
        });

        // Ensure the suspend event caused a state change to inactive and its
        // time was not adjusted due to offset changes on resume
        harness.addAssertionEvent('2024-06-01T15:00:02Z', () => {
            const [, historyContents] = harness.mockHistoryFile.load_contents(null);
            expect(JSON.parse(new TextDecoder().decode(historyContents)))
                .withContext('History file contents')
                .toContain({
                    'oldState': UserState.ACTIVE,
                    'newState': UserState.INACTIVE,
                    'wallTimeSecs': TestHarness.timeStrToSecs('2024-06-01T12:00:01Z'),
                });
        });

        harness.shutdownManager('2024-06-01T15:20:00Z', timeLimitsManager);

        harness.run();
    });
});
