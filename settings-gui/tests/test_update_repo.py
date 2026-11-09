# SPDX-FileCopyrightText: 2026 Ngó Sen contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import shutil
import tempfile
import unittest

# support puts the settings window on sys.path, so it has to come first.
# isort: off
from support import app  # noqa: F401
from core import update_repo

# isort: on

TOP = os.path.join(os.path.dirname(__file__), "..", "..")
APT = os.path.join(TOP, "test", "data", "apt-sources")
FILES = os.path.join(TOP, "test", "data", "repo-files")
# What README tells Arch users to append to pacman.conf.
PACMAN_README = "\n[ngosen]\nServer = https://ngosen.github.io/ngosen/arch/$arch\n"


def read(*path):
    with open(os.path.join(*path), encoding="utf-8") as f:
        return f.read()


class AptRuleTest(unittest.TestCase):
    def test_matches_what_install_sh_decides_for_each_sample(self):
        # The same samples and answers as test/install-apt-repo.sh, so both readers agree.
        for line in read(APT, "expected").splitlines():
            if not line or line.startswith("#"):
                continue
            name, want = line.split()
            text = read(APT, name)
            reader = (
                update_repo.apt_sources_enables
                if name.endswith(".sources")
                else update_repo.apt_list_enables
            )
            with self.subTest(name):
                self.assertEqual(reader(text), want == "yes")


class RpmRepoTest(unittest.TestCase):
    def test_copr_file_from_dnf_copr_enable(self):
        text = read(FILES, "copr.repo")
        self.assertTrue(update_repo.rpm_repo_enables(text, update_repo.COPR_URL))
        disabled = text.replace("enabled=1", "enabled=0")
        self.assertFalse(update_repo.rpm_repo_enables(disabled, update_repo.COPR_URL))

    def test_zypper_file_from_the_published_repository(self):
        text = read(FILES, "ngosen.repo")
        self.assertTrue(update_repo.rpm_repo_enables(text, update_repo.ZYPPER_URL))
        self.assertFalse(
            update_repo.rpm_repo_enables(
                text.replace("enabled=1", "enabled=0"), update_repo.ZYPPER_URL
            )
        )

    def test_a_section_without_enabled_is_on(self):
        text = read(FILES, "copr.repo").replace("enabled=1\n", "")
        self.assertTrue(update_repo.rpm_repo_enables(text, update_repo.COPR_URL))

    def test_another_repository_is_not_ours(self):
        text = read(FILES, "ngosen.repo")
        self.assertFalse(update_repo.rpm_repo_enables(text, update_repo.COPR_URL))


class PacmanTest(unittest.TestCase):
    def test_stock_config_has_no_ngosen_and_readme_lines_add_it(self):
        text = read(FILES, "pacman.conf")
        self.assertNotIn("ngosen", update_repo.pacman_sections(text)[0])
        self.assertIn("ngosen", update_repo.pacman_sections(text + PACMAN_README)[0])

    def test_commented_section_does_not_count(self):
        text = read(FILES, "pacman.conf") + PACMAN_README.replace("[ngosen]", "#[ngosen]")
        self.assertNotIn("ngosen", update_repo.pacman_sections(text)[0])


class StateTest(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.root)

    def put(self, path, text):
        full = os.path.join(self.root, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as f:
            f.write(text)
        return full

    def state(self):
        return update_repo.state(self.root, program=__file__)

    def test_nothing_to_read_means_off(self):
        self.assertEqual(self.state(), update_repo.OFF)

    def test_apt_file_from_install_sh(self):
        self.put("etc/apt/sources.list", read(APT, "other.list"))
        self.put("etc/apt/sources.list.d/ngosen.list", read(APT, "installer.list"))
        self.assertEqual(self.state(), update_repo.ON)

    def test_apt_skips_files_with_other_characters_in_their_names(self):
        self.put("etc/apt/sources.list.d/ngosen (copy).list", read(APT, "readme.list"))
        self.assertEqual(self.state(), update_repo.OFF)

    def test_dnf_zypper_and_pacman(self):
        cases = {
            "etc/yum.repos.d/copr.repo": read(FILES, "copr.repo"),
            "etc/zypp/repos.d/ngosen.repo": read(FILES, "ngosen.repo"),
            "etc/pacman.conf": read(FILES, "pacman.conf") + PACMAN_README,
        }
        for path, text in cases.items():
            with self.subTest(path):
                full = self.put(path, text)
                self.assertEqual(self.state(), update_repo.ON)
                os.remove(full)

    def test_pacman_section_in_an_included_file(self):
        conf = read(FILES, "pacman.conf") + "\nInclude = /etc/pacman.d/ngosen.conf\n"
        self.put("etc/pacman.conf", conf)
        self.put("etc/pacman.d/ngosen.conf", PACMAN_README)
        self.assertEqual(self.state(), update_repo.ON)

    @unittest.skipIf(os.geteuid() == 0, "root reads every file")
    def test_unreadable_file_makes_it_unknown(self):
        full = self.put("etc/apt/sources.list.d/a.list", read(APT, "other.list"))
        os.chmod(full, 0)
        self.assertEqual(self.state(), update_repo.UNKNOWN)

    @unittest.skipIf(os.geteuid() == 0, "root reads every file")
    def test_one_enabling_file_wins_over_an_unreadable_one(self):
        os.chmod(self.put("etc/apt/sources.list.d/a.list", read(APT, "other.list")), 0)
        self.put("etc/apt/sources.list.d/b.list", read(APT, "installer.list"))
        self.assertEqual(self.state(), update_repo.ON)

    def test_nix_store_install_has_no_repository_line(self):
        self.put("etc/apt/sources.list.d/ngosen.list", read(APT, "installer.list"))
        program = "/nix/store/abc-fcitx5-ngosen/share/ngosen/core/update_repo.py"
        self.assertEqual(update_repo.state(self.root, program=program), update_repo.NONE)


if __name__ == "__main__":
    unittest.main()
