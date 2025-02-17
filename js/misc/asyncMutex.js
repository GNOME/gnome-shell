export class AsyncMutex {
    #isHold = false;
    #pendingResolvers = [];

    hold() {
        if (this.#isHold) {
            const {promise, resolve} = Promise.withResolvers();
            this.#pendingResolvers.push(resolve);
            return promise;
        }
        this.#isHold = true;
        return Promise.resolve();
    }

    release() {
        console.assert(this.#isHold, 'release() called without matching hold()');
        if (this.#pendingResolvers.length === 0) {
            this.#isHold = false;
            return;
        }
        const resolve = this.#pendingResolvers.shift();
        resolve();
    }
}
