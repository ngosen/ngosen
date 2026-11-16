// SPDX-License-Identifier: GPL-3.0-or-later

// A key we pressed stays here until it reaches the IM filter, which must hand it straight to the app.
const PENDING_LIMIT_US = 500000;

export class PendingKeys {
    constructor() {
        this._keys = [];
    }

    add(keyval, press, now) {
        this._dropStale(now);
        this._keys.push({keyval, press, time: now});
    }

    take(keyval, press, now) {
        // A key the shell dropped never comes back, and must not swallow the same key typed later.
        this._dropStale(now);
        const i = this._keys.findIndex(key => key.keyval === keyval && key.press === press);
        if (i < 0)
            return false;
        this._keys.splice(i, 1);
        return true;
    }

    _dropStale(now) {
        this._keys = this._keys.filter(key => now - key.time < PENDING_LIMIT_US);
    }
}
