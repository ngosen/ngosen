// SPDX-License-Identifier: GPL-3.0-or-later
import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import IBus from 'gi://IBus';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

// A key we pressed stays here until it reaches the IM filter, which must hand it straight to the app.
const PENDING_LIMIT_US = 500000;
const PASS_THROUGH = Symbol('pass through');

export default class ForwardKeys extends Extension {
    enable() {
        const seat = global.stage.context.get_backend().get_default_seat();
        this._keyboard = seat.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);
        this._pending = [];

        // The stock forward_key builds a key event without a source device, which mutter 50.0-50.3 drops.
        this._im = Main.inputMethod;
        this._im.forward_key = (keyval, keycode, _state, _time, press) => this._press(keyval, press);

        // Every IBus reconnect replaces the context, so wrap each one as it is stored.
        let context = this._im._context;
        Object.defineProperty(this._im, '_context', {
            configurable: true,
            get: () => context,
            set: value => {
                context = value;
                this._wrap(value);
            },
        });
        this._wrap(context);
    }

    disable() {
        const context = this._im._context;
        delete this._im._context;
        this._im._context = context;
        this._unwrap(context);
        delete this._im.forward_key;
        this._im = null;
        this._keyboard = null;
        this._pending = null;
    }

    _press(keyval, press) {
        const now = GLib.get_monotonic_time();
        this._pending = this._pending.filter(key => now - key.time < PENDING_LIMIT_US);
        this._pending.push({keyval, press, time: now});
        this._keyboard.notify_keyval(now, keyval, press ? Clutter.KeyState.PRESSED : Clutter.KeyState.RELEASED);
    }

    _takePending(keyval, press) {
        const i = this._pending.findIndex(key => key.keyval === keyval && key.press === press);
        if (i < 0)
            return false;
        this._pending.splice(i, 1);
        return true;
    }

    _wrap(context) {
        if (!context || context._ngosenProcess)
            return;
        context._ngosenProcess = context.process_key_event_async;
        context._ngosenFinish = context.process_key_event_async_finish;
        context.process_key_event_async = (keyval, keycode, state, timeout, cancellable, callback) => {
            const press = (state & IBus.ModifierType.RELEASE_MASK) === 0;
            if (this._takePending(keyval, press)) {
                // Reporting "not handled" makes GNOME Shell deliver the key to the app.
                callback(context, PASS_THROUGH);
                return;
            }
            context._ngosenProcess.call(context, keyval, keycode, state, timeout, cancellable, callback);
        };
        context.process_key_event_async_finish = result =>
            result === PASS_THROUGH ? false : context._ngosenFinish.call(context, result);
    }

    _unwrap(context) {
        if (!context || !context._ngosenProcess)
            return;
        delete context.process_key_event_async;
        delete context.process_key_event_async_finish;
        delete context._ngosenProcess;
        delete context._ngosenFinish;
    }
}
