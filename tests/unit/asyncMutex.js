import GLib from 'gi://GLib';

import * as Mutex from 'resource:///org/gnome/shell/misc/asyncMutex.js';

/**
 * A helper that asynchronously waits an specified amount
 * of time in milliseconds.
 *
 * @param {Int} ms Time, in milliseconds, to wait before resolving the promise.
 * @returns A promise that will be resolved after the specified time.
 */
function waitMs(ms) {
    return new Promise((resolve, _reject) => {
        setTimeout(resolve, ms);
    });
}

const STATE_UNINITIALIZED = -1;
const STATE_HOLD = 0;
const STATE_RUN = 1;
const STATE_RELEASE = 2;
const STATE_DONE = 3;

class AsyncMutexTest {
    #counterWithMutex = 0;
    #mutex;
    #loop;
    #promiseList = [];
    #messages = [];
    #workercounter = 0;

    constructor() {
        this.#mutex = new Mutex.AsyncMutex();
        this.#loop = new GLib.MainLoop(null, false);
    }

    async _workerWithMutex(index) {
        this.#messages.push({'state': STATE_HOLD, 'id': index});
        await this.#mutex.hold();
        this.#messages.push({'state': STATE_RUN, 'id': index});
        const value = this.#counterWithMutex;
        await waitMs(200);
        this.#messages.push({'state': STATE_RELEASE, 'id': index});
        this.#counterWithMutex = value + 1;
        this.#mutex.release();
        this.#messages.push({'state': STATE_DONE, 'id': index});
    }

    _appendWorker() {
        this.#promiseList.push(this._workerWithMutex(this.#workercounter++));
    }

    _runLoop() {
        Promise.all(this.#promiseList).then(() => {
            this.#loop.quit();
        });
        this.#loop.run();
    }

    get counter() {
        return this.#counterWithMutex;
    }

    #checkWorkerCompletedFine(workerId) {
        // Ensures that the specified worker passes in order by all the states.
        let lastState = STATE_UNINITIALIZED;
        for (const msg of this.#messages) {
            if (msg.id !== workerId)
                continue;
            if (msg.state !== (lastState + 1))
                return false;
            lastState = msg.state;
        }
        return lastState === STATE_DONE;
    }

    checkWorkersCompletedFine() {
        // Ensures that all the workers pass in order by all the states.
        for (let i = 0; i < this.#workercounter; i++) {
            if (!this.#checkWorkerCompletedFine(i))
                return false;
        }
        return true;
    }

    checkWorkerStates() {
        // Ensures that no worker has STATE_RUN or STATE_RELEASE between the STATE_HOLD and the
        // STATE_RELEASE of other worker, thus ensuring that the mutex works as expected.
        let currentWorker = -1;
        const onHold = [];
        for (const msg of this.#messages) {
            switch (msg.state) {
            case STATE_HOLD:
                if (currentWorker === -1)
                    currentWorker = msg.id;
                else
                    onHold.push(msg.id);
                break;
            case STATE_RUN:
                if (currentWorker !== -1) {
                    // Ensure that the worker that enters RUN state is the one that owns the mutex
                    expect(msg.id).toBe(currentWorker);
                } else {
                    // the mutex is released and a new worker has taken ownership of it
                    currentWorker = msg.id;

                    // Ensure that the worker did a HOLD before
                    const idx = onHold.indexOf(msg.id);
                    expect(idx).not.toBe(-1);

                    // remove the id from the onHold list
                    onHold.splice(onHold.indexOf(msg.id), 1);
                }
                break;
            case STATE_RELEASE:
                // Ensure that the worker that releases the mutex is the one that owns it
                expect(currentWorker).toBe(msg.id);
                currentWorker = -1;
                break;
            }
        }
        return true;
    }

    run(numWorkers) {
        for (let i = 0; i < numWorkers; i++)
            this._appendWorker();

        this._runLoop();

        expect(this.checkWorkersCompletedFine()).toBe(true);
        expect(this.counter).toBe(numWorkers);
        expect(this.checkWorkerStates()).toBe(true);
    }
}


describe('AsyncMutex', () => {
    it('works with one worker', () => {
        const test = new AsyncMutexTest();
        test.run(1);
    });
    it('works with several concurrent workers', () => {
        const test = new AsyncMutexTest();
        test.run(4);
    });
});
