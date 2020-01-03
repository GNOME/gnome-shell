
Token = class {
    constructor() {
        this._token = null;
        this._authenticatedSignalId = null;
    }

    _onUserAuthenticated(proxy, sender, [token]) {
        this._token = token;
        this.emit('user-authenticated', token);
    }

    _onVMUserAuthenticated(proxy, sender, [token]) {
        this._token = token;
        this.emit('vm-authenticated', token);
    }

    hasToken() {
        return this._token != null;
    }

    getToken() {
        return this._token;
    }

    resetToken() {
        this._token = null;
    }
};
