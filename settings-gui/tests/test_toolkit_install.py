# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import re
import subprocess
import unittest

import fake_fcitx  # noqa: F401  (puts settings-gui on the path)
from test_core_toolkit_free import GUI_DIR

from core import toolkit_install

INSTALL_SH = GUI_DIR.parent / "install.sh"


def answering(outputs, returncode=0):
    """A stand-in for subprocess.run that prints outputs[program] and records each call."""
    calls = []

    def run(args, **kwargs):
        calls.append((args, kwargs))
        return subprocess.CompletedProcess(args, returncode, outputs.get(args[0], ""), "")

    run.calls = calls
    return run


def install_sh_lists(toolkit_case):
    """The package lists one branch of settings_window_packages in install.sh sets, by manager."""
    body = re.search(r"settings_window_packages\(\) \{(.*?)\n\}", INSTALL_SH.read_text(), re.S)[1]
    branch = body.split("elif")[0 if toolkit_case == "gtk" else 1]
    lists = re.findall(r"(\w+)\) [^(]*settings_packages=\(([^)]*)\)", branch)
    return {("apt-get" if m == "apt" else m): tuple(p.split()) for m, p in lists}


class PackageListTest(unittest.TestCase):
    def test_the_lists_match_install_sh(self):
        for toolkit in ("gtk", "qt"):
            from_script = install_sh_lists(toolkit)
            self.assertTrue(from_script, toolkit)
            for manager, packages in from_script.items():
                self.assertEqual(toolkit_install.PACKAGES[toolkit][manager], packages, manager)

    def test_the_command_is_shown_with_sudo(self):
        self.assertEqual(
            toolkit_install.shown_command("gtk", "dnf"),
            "sudo dnf install -y python3-gobject gobject-introspection libadwaita",
        )

    def test_the_first_manager_found_is_used(self):
        found = toolkit_install.package_manager(lambda name: name in ("dnf", "zypper"))
        self.assertEqual(found, "dnf")
        self.assertIsNone(toolkit_install.package_manager(lambda name: False))


class DownloadSizeTest(unittest.TestCase):
    def test_apt_adds_up_the_packages_it_would_install(self):
        run = answering(
            {
                "apt-get": "Inst gir1.2-adw-1 (1.5.0-1ubuntu2 Ubuntu:24.04/noble [amd64])\n"
                "Inst libadwaita-1-0 (1.5.0-1ubuntu2 Ubuntu:24.04/noble [amd64])\n"
                "Conf gir1.2-adw-1 (1.5.0-1ubuntu2 Ubuntu:24.04/noble [amd64])\n",
                "apt-cache": "Package: gir1.2-adw-1\nInstalled-Size: 167\nSize: 41778\n\n"
                "Package: libadwaita-1-0\nInstalled-Size: 2440\nSize: 408094\n",
            }
        )
        self.assertEqual(toolkit_install.download_size("gtk", "apt-get", run), 41778 + 408094)
        self.assertEqual(run.calls[1][0][-2:], ["gir1.2-adw-1", "libadwaita-1-0"])

    def test_the_labels_are_read_in_english(self):
        run = answering({})
        toolkit_install.download_size("gtk", "apt-get", run)
        self.assertEqual(run.calls[0][1]["env"]["LC_ALL"], "C")

    def test_dnf_reports_what_it_would_download(self):
        run = answering(
            {
                "dnf": "Total size of inbound packages is 199 MiB. Need to download 1.5 MiB.\n"
                "After this operation, 810 MiB extra will be used (install 810 MiB, remove 0 B).\n"
            },
            returncode=1,
        )
        self.assertEqual(toolkit_install.download_size("gtk", "dnf", run), round(1.5 * 1024**2))

    def test_pacman_adds_up_the_sizes(self):
        run = answering({"pacman": "151458\n34678\n329964\n"})
        self.assertEqual(toolkit_install.download_size("gtk", "pacman", run), 516100)

    def test_nothing_to_download_on_apt_is_zero(self):
        run = answering({"apt-get": "0 upgraded, 0 newly installed, 0 to remove.\n"})
        self.assertEqual(toolkit_install.download_size("gtk", "apt-get", run), 0)

    def test_apt_that_fails_gives_no_size(self):
        run = answering({"apt-get": "Reading package lists...\n"}, returncode=100)
        self.assertIsNone(toolkit_install.download_size("gtk", "apt-get", run))

    def test_pacman_with_everything_installed_is_zero(self):
        self.assertEqual(toolkit_install.download_size("gtk", "pacman", answering({})), 0)

    def test_pacman_that_fails_gives_no_size(self):
        run = answering({"pacman": "error: target not found: libadwaita\n"}, returncode=1)
        self.assertIsNone(toolkit_install.download_size("gtk", "pacman", run))

    def test_dnf_with_everything_installed_is_zero(self):
        run = answering({"dnf": "Package libadwaita is already installed.\nNothing to do.\n"})
        self.assertEqual(toolkit_install.download_size("gtk", "dnf", run), 0)

    def test_zypper_needs_root_to_tell(self):
        run = answering({"zypper": "Root privileges are required to run this command.\n"})
        self.assertIsNone(toolkit_install.download_size("gtk", "zypper", run))
        self.assertEqual(run.calls, [])

    def test_a_missing_program_gives_no_size(self):
        def run(args, **kwargs):
            raise FileNotFoundError(args[0])

        self.assertIsNone(toolkit_install.download_size("qt", "dnf", run))


class CanInstallTest(unittest.TestCase):
    def policy(self, candidate, newer):
        def run(args, **kwargs):
            if args[0] == "dpkg":
                return subprocess.CompletedProcess(args, 0 if newer else 1, "", "")
            return subprocess.CompletedProcess(
                args, 0, f"gir1.2-adw-1:\n  Installed: (none)\n  Candidate: {candidate}\n", ""
            )

        return run

    def test_debian_12_cannot_get_the_gtk_window(self):
        self.assertFalse(
            toolkit_install.can_install("gtk", "apt-get", self.policy("1.2.2-1", False))
        )

    def test_ubuntu_24_04_can(self):
        self.assertTrue(toolkit_install.can_install("gtk", "apt-get", self.policy("1.5.0-1", True)))

    def test_no_candidate_means_no(self):
        self.assertFalse(toolkit_install.can_install("gtk", "apt-get", self.policy("(none)", True)))

    def test_qt_and_other_managers_need_no_check(self):
        self.assertTrue(toolkit_install.can_install("qt", "apt-get", answering({})))
        self.assertTrue(toolkit_install.can_install("gtk", "dnf", answering({})))
        self.assertFalse(toolkit_install.can_install("gtk", None, answering({})))


class FakeModel:
    def __init__(self, saves=True):
        self.values, self.saves = {}, saves

    def set(self, key, value):
        self.values[key] = value

    def save(self):
        return self.saves


class OfferTest(unittest.TestCase):
    def offer(self, gtk=False, qt=True, programs=("dnf", "pkexec")):
        return toolkit_install.Offer(
            "qt",
            installed={"qt": lambda: qt, "gtk": lambda: gtk},
            which=lambda name: name in programs,
            run=answering({}),
        )

    def test_an_installed_window_needs_nothing(self):
        offer = self.offer(gtk=True)
        self.assertEqual(offer.target, "gtk")
        self.assertTrue(offer.ready)
        self.assertFalse(offer.installable)

    def test_a_missing_one_is_installed_through_pkexec(self):
        offer = self.offer()
        self.assertTrue(offer.runnable)
        self.assertEqual(offer.install_argv[:3], ["pkexec", "dnf", "install"])

    def test_without_pkexec_only_the_command_is_offered(self):
        offer = self.offer(programs=("dnf",))
        self.assertTrue(offer.installable)
        self.assertFalse(offer.runnable)

    def test_sizes_read_in_megabytes(self):
        self.assertEqual(toolkit_install.format_size(450_000), "0.4 MB")
        self.assertEqual(toolkit_install.format_size(52 * 1024**2), "52 MB")


class SwitchTest(unittest.TestCase):
    def tearDown(self):
        toolkit_install._reopen_in.clear()

    def test_the_window_reopens_in_the_saved_choice(self):
        model = FakeModel()
        self.assertTrue(toolkit_install.switch(model, "gtk"))
        self.assertEqual(model.values, {"SettingsToolkit": "GTK"})
        calls = []
        toolkit_install.reopen_if_switched(lambda *args: calls.append(args))
        self.assertEqual(calls[0][2]["NGOSEN_SETTINGS_TOOLKIT"], "gtk")

    def test_nothing_reopens_when_saving_fails(self):
        self.assertFalse(toolkit_install.switch(FakeModel(saves=False), "gtk"))
        calls = []
        toolkit_install.reopen_if_switched(lambda *args: calls.append(args))
        self.assertEqual(calls, [])


if __name__ == "__main__":
    unittest.main()
