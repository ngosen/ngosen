// SPDX-License-Identifier: GPL-3.0-or-later
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import IBus from 'gi://IBus';
import Shell from 'gi://Shell';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Config from 'resource:///org/gnome/shell/misc/config.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

// A key we pressed stays here until it reaches the IM filter, which must hand it straight to the app.
const PENDING_LIMIT_US = 500000;
const PASS_THROUGH = Symbol('pass through');

const FCITX_SERVICE = 'org.fcitx.Fcitx5';
const OBJECT_PATH = '/io/github/ngosen/AppName';
const INTERFACE = 'io.github.ngosen.AppName';
const INTERFACE_XML = `<node>
  <interface name="${INTERFACE}">
    <method name="Sync"/>
    <signal name="FocusedApp"><arg type="s"/></signal>
  </interface>
</node>`;
// The name fcitx5 5.1.22 and later give the overview's search box.
const OVERVIEW_APP = 'gnome-shell-overview';

// mutter 50.0-50.3 drop the key events the stock forward_key builds (mutter#4853).
function shellDropsForwardedKeys() {
    return /^50\.[0-3](\D|$)/.test(Config.PACKAGE_VERSION);
}

export default class ForwardKeys extends Extension {
    enable() {
        if (shellDropsForwardedKeys())
            this._enableKeyForwarding();
        this._enableAppNames();
    }

    disable() {
        this._disableAppNames();
        if (this._im)
            this._disableKeyForwarding();
    }

    _enableKeyForwarding() {
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

    _disableKeyForwarding() {
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

    // fcitx5 before 5.1.22 names every app GNOME Shell relays after the shell; this tells it the focused one.
    _enableAppNames() {
        this._tracker = Shell.WindowTracker.get_default();
        this._trackerSignal = this._tracker.connect('notify::focus-app', () => this._updateApp());
        this._focusSignal = global.display.connect('notify::focus-window', () => this._updateApp());
        this._overviewSignals = ['showing', 'hiding'].map(name => Main.overview.connect(name, () => this._updateApp()));
        this._dbus = Gio.DBusExportedObject.wrapJSObject(INTERFACE_XML, this);
        this._dbus.export(Gio.DBus.session, OBJECT_PATH);
        this._updateApp();
    }

    _disableAppNames() {
        this._dbus.unexport();
        this._dbus = null;
        this._overviewSignals.forEach(id => Main.overview.disconnect(id));
        this._overviewSignals = null;
        global.display.disconnect(this._focusSignal);
        this._tracker.disconnect(this._trackerSignal);
        this._tracker = null;
        this._sendApp('');
        this._sentApp = null;
    }

    // Called when fcitx5 starts after the shell, which is what happens at every login.
    Sync() {
        this._sendApp(this._focusedApp());
    }

    _updateApp() {
        const app = this._focusedApp();
        if (app !== this._sentApp)
            this._sendApp(app);
    }

    _focusedApp() {
        if (Main.overview.visibleTarget)
            return OVERVIEW_APP;
        const id = this._tracker.focus_app?.get_id();
        // GNOME numbers an app without a desktop file anew each session, so a rule could not stick to it.
        if (id && !id.startsWith('window:'))
            return id;
        return global.display.focus_window?.get_wm_class() ?? '';
    }

    // Sent only to fcitx5: GNOME keeps the focused app from other clients on the session bus.
    _sendApp(app) {
        this._sentApp = app;
        Gio.DBus.session.emit_signal(FCITX_SERVICE, OBJECT_PATH, INTERFACE, 'FocusedApp', new GLib.Variant('(s)', [app]));
    }
}
