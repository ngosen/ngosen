// SPDX-License-Identifier: GPL-3.0-or-later
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import IBus from 'gi://IBus';
import Meta from 'gi://Meta';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

// A key we pressed stays here until it reaches the IM filter, which must hand it straight to the app.
const PENDING_LIMIT_US = 500000;
const PASS_THROUGH = Symbol('pass through');

Gio._promisify(Gio.DBusConnection.prototype, 'call');

// WPS Office drops what the input method sends, so the Ngó Sen IBus engine asks for real key presses.
// Only that engine may ask, only for BackSpace, and only into an X11 window.
const KEYS_NAME = 'io.github.ngosen.Shell';
const KEYS_PATH = '/io/github/ngosen/Shell';
const KEYS_IFACE = `<node><interface name="io.github.ngosen.Shell">
  <method name="FocusedX11Class"><arg type="s" direction="out"/></method>
  <method name="PressBackSpace"><arg type="u" name="count" direction="in"/><arg type="b" direction="out"/></method>
</interface></node>`;
const ENGINE = 'ibus-engine-ngosen';
const MAX_BACKSPACES = 16;

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

        this._callers = new Map();
        this._service = Gio.DBusExportedObject.wrapJSObject(KEYS_IFACE, this);
        this._service.export(Gio.DBus.session, KEYS_PATH);
        this._nameId = Gio.bus_own_name_on_connection(Gio.DBus.session, KEYS_NAME, Gio.BusNameOwnerFlags.NONE, null, null);
    }

    disable() {
        Gio.bus_unown_name(this._nameId);
        this._service.unexport();
        this._service = null;
        this._callers = null;

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

    FocusedX11ClassAsync(_params, invocation) {
        this._fromEngine(invocation).then(ok =>
            invocation.return_value(new GLib.Variant('(s)', [ok ? this._focusedX11Class() : ''])));
    }

    PressBackSpaceAsync([count], invocation) {
        this._fromEngine(invocation).then(ok => {
            const press = ok && this._keyboard !== null && count > 0 && count <= MAX_BACKSPACES && this._focusedX11Class() !== '';
            for (let i = 0; press && i < count; i++) {
                const now = GLib.get_monotonic_time();
                this._keyboard.notify_keyval(now, Clutter.KEY_BackSpace, Clutter.KeyState.PRESSED);
                this._keyboard.notify_keyval(now, Clutter.KEY_BackSpace, Clutter.KeyState.RELEASED);
            }
            invocation.return_value(new GLib.Variant('(b)', [press]));
        });
    }

    _focusedX11Class() {
        const window = global.display.focus_window;
        if (!window || window.get_client_type() !== Meta.WindowClientType.X11)
            return '';
        return window.get_wm_class() ?? '';
    }

    // Unique bus names are never reused, so each caller is checked once.
    _fromEngine(invocation) {
        const sender = invocation.get_sender();
        if (!this._callers.has(sender))
            this._callers.set(sender, this._isEngine(sender));
        return this._callers.get(sender);
    }

    async _isEngine(sender) {
        try {
            const reply = await Gio.DBus.session.call('org.freedesktop.DBus', '/org/freedesktop/DBus',
                'org.freedesktop.DBus', 'GetConnectionUnixProcessID', new GLib.Variant('(s)', [sender]),
                new GLib.VariantType('(u)'), Gio.DBusCallFlags.NONE, -1, null);
            const [pid] = reply.deepUnpack();
            return GLib.path_get_basename(GLib.file_read_link(`/proc/${pid}/exe`)) === ENGINE;
        } catch {
            return false;
        }
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
