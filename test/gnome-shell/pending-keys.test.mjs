// SPDX-License-Identifier: GPL-3.0-or-later
import assert from 'node:assert/strict';
import test from 'node:test';

import {PendingKeys} from '../../gnome-shell/forward-keys@ngosen.github.io/pending-keys.js';

const BACKSPACE = 0xff08;

test('a forwarded key that comes back goes to the app once', () => {
    const keys = new PendingKeys();
    keys.add(BACKSPACE, true, 0);
    assert.equal(keys.take(BACKSPACE, true, 10000), true);
    assert.equal(keys.take(BACKSPACE, true, 20000), false);
});

test('a release is not taken for a press', () => {
    const keys = new PendingKeys();
    keys.add(BACKSPACE, false, 0);
    assert.equal(keys.take(BACKSPACE, true, 10000), false);
    assert.equal(keys.take(BACKSPACE, false, 10000), true);
});

// The shell can drop forwarded keys, and nothing else may be forwarded before the user types the same key.
test('a forwarded key that never came back does not take a key typed later', () => {
    const keys = new PendingKeys();
    for (let i = 0; i < 4; i++) {
        keys.add(BACKSPACE, true, i * 1000);
        keys.add(BACKSPACE, false, i * 1000 + 500);
    }
    assert.equal(keys.take(BACKSPACE, true, 6000000), false);
});
