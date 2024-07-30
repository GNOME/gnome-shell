import * as Signals from '../misc/signals.js';

export class CredentialManager extends Signals.EventEmitter {
    constructor(service) {
        super();

        this._token = null;
        this._service = service;
    }

    get token() {
        return this._token;
    }

    set token(t) {
        this._token = t;
        if (this._token)
            this.emit('user-authenticated', this._token);
    }

    get service() {
        return this._service;
    }
}
