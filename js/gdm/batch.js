// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/*
 * Copyright 2011 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * In order for transformation animations to look good, they need to be
 * incremental and have some order to them (e.g., fade out hidden items,
 * then shrink to close the void left over). Chaining animations in this way can
 * be error-prone and wordy using just ease() callbacks.
 *
 * The classes in this file help with this:
 *
 * - Task.  encapsulates schedulable work to be run in a specific scope.
 *
 * - ConsecutiveBatch.  runs a series of tasks in order and completes
 *                      when the last in the series finishes.
 *
 * - ConcurrentBatch.  runs a set of tasks at the same time and completes
 *                     when the last to finish completes.
 *
 * - Hold.  prevents a batch from completing the pending task until
 *          the hold is released.
 *
 * The tasks associated with a batch are specified in a list at batch
 * construction time as either task objects or plain functions.
 * Batches are task objects, themselves, so they can be nested.
 *
 * These classes aren't specific to GDM, but were found to be unintuitive and so
 * are not used elsewhere. These APIs may ultimately get dropped entirely and
 * replaced by something else.
 */
/* exported ConcurrentBatch, ConsecutiveBatch */

const { GObject } = imports.gi;
const Signals = imports.misc.signals;

var Task = class extends Signals.EventEmitter {
    constructor(scope, handler) {
        super();

        if (scope)
            this.scope = scope;
        else
            this.scope = this;

        this.handler = handler;
    }

    run() {
        if (this.handler)
            return this.handler.call(this.scope);

        return null;
    }
};

var Hold = class extends Task {
    constructor() {
        super(null, () => this);

        this._acquisitions = 1;
    }

    acquire() {
        if (this._acquisitions <= 0)
            throw new Error("Cannot acquire hold after it's been released");
        this._acquisitions++;
    }

    acquireUntilAfter(hold) {
        if (!hold.isAcquired())
            return;

        this.acquire();
        let signalId = hold.connect('release', () => {
            hold.disconnect(signalId);
            this.release();
        });
    }

    release() {
        this._acquisitions--;

        if (this._acquisitions == 0)
            this.emit('release');
    }

    isAcquired() {
        return this._acquisitions > 0;
    }
};

var Batch = class extends Task {
    constructor(scope, tasks) {
        super();

        this.tasks = [];

        for (let i = 0; i < tasks.length; i++) {
            let task;

            if (tasks[i] instanceof Task)
                task = tasks[i];
            else if (typeof tasks[i] == 'function')
                task = new Task(scope, tasks[i]);
            else
                throw new Error('Batch tasks must be functions or Task, Hold or Batch objects');

            this.tasks.push(task);
        }
    }

    process() {
        throw new GObject.NotImplementedError(`process in ${this.constructor.name}`);
    }

    runTask() {
        if (!(this._currentTaskIndex in this.tasks))
            return null;

        return this.tasks[this._currentTaskIndex].run();
    }

    _finish() {
        this.hold.release();
    }

    nextTask() {
        this._currentTaskIndex++;

        // if the entire batch of tasks is finished, release
        // the hold and notify anyone waiting on the batch
        if (this._currentTaskIndex >= this.tasks.length) {
            this._finish();
            return;
        }

        this.process();
    }

    _start() {
        // acquire a hold to get released when the entire
        // batch of tasks is finished
        this.hold = new Hold();
        this._currentTaskIndex = 0;
        this.process();
    }

    run() {
        this._start();

        // hold may be destroyed at this point
        // if we're already done running
        return this.hold;
    }

    cancel() {
        this.tasks = this.tasks.splice(0, this._currentTaskIndex + 1);
    }
};

var ConcurrentBatch = class extends Batch {
    process() {
        let hold = this.runTask();

        if (hold)
            this.hold.acquireUntilAfter(hold);

        // Regardless of the state of the just run task,
        // fire off the next one, so all the tasks can run
        // concurrently.
        this.nextTask();
    }
};

var ConsecutiveBatch = class extends Batch {
    process() {
        let hold = this.runTask();

        if (hold && hold.isAcquired()) {
            // This task is inhibiting the batch. Wait on it
            // before processing the next one.
            let signalId = hold.connect('release', () => {
                hold.disconnect(signalId);
                this.nextTask();
            });
        } else {
            // This task finished, process the next one
            this.nextTask();
        }
    }
};
