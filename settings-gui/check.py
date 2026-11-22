#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prints Check my machine as text, for a terminal or an AI assistant. It changes nothing."""

import argparse
import contextlib
import sys

import dbus
from core.dbus_handler import NgoSenDBusHandler
from i18n import setup_i18n
from save_log import SERVICE

from core import system_check

try:
    from version import __version__
except ImportError:
    __version__ = ""


class RunningFcitxHandler(NgoSenDBusHandler):
    """Talks only to an fcitx5 already running: the well-known name would start one."""

    def __init__(self, bus):
        self.addon_name = "fcitx://config/addon/ngosen"
        self.bus = bus

    @property
    def iface(self):
        if not self.bus:
            return None
        try:
            owner = self.bus.get_name_owner(SERVICE)
            proxy = self.bus.get_object(owner, "/controller", introspect=False)
        except dbus.DBusException:
            return None
        return dbus.Interface(proxy, "org.fcitx.Fcitx.Controller1")


NO_BUS, NOT_RUNNING, NOT_LOADED = 1, 2, 3


def exit_code(bus, facts):
    """Lets a script tell why fcitx5 could not answer; the report is printed either way."""
    if bus is None:
        return NO_BUS
    if not facts.fcitx_running:
        return NOT_RUNNING
    if not facts.ngosen_loaded:
        return NOT_LOADED
    return 0


def check(bus, include_apps=False, env=None):
    """Returns the report and the exit code."""
    handler = RunningFcitxHandler(bus)
    # The handler reports D-Bus errors with print(); stdout carries only the report.
    with contextlib.redirect_stdout(sys.stderr):
        facts = system_check.collect(handler, __version__, env)
        found = system_check.steps(facts)
    return system_check.terminal_report(facts, found, include_apps), exit_code(bus, facts)


def main(argv=None):
    setup_i18n()
    parser = argparse.ArgumentParser(prog="ngosen-check", description=__doc__)
    parser.add_argument(
        "--include-apps",
        action="store_true",
        help="list the apps connected to fcitx5; read the list before sharing it",
    )
    args = parser.parse_args(argv)
    try:
        bus = dbus.SessionBus()
    except dbus.DBusException:
        # Under sudo or ssh; the rest of the check still says something.
        print("No desktop session bus: fcitx5 cannot be asked.", file=sys.stderr)
        bus = None
    report, code = check(bus, args.include_apps)
    print(report)
    return code


if __name__ == "__main__":
    sys.exit(main())
