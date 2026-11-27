"""Offline integration tests for tools/daily.sh (temporary repos, fake CLIs)."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

DAILY = Path(__file__).resolve().parents[1] / "tools" / "daily.sh"

SCORER = """\
import json, pathlib, sys
def verse_move(*args):
    return "held"

if __name__ == "__main__":
    assert sys.argv[1] == "score"
    assert len(sys.argv) >= 3
    pathlib.Path(sys.argv[sys.argv.index("--save") + 1]).write_text(
        json.dumps({"results": [{"file": "widget.h", "action": "review",
                      "health": 0.4, "concern": "undocumented",
                      "concern_confidence": 0.6, "quality_F": 31.0}]}))
"""

AGENT = r"""#!/usr/bin/env bash
set -eu
[[ ! -v TYPESAFE_API_KEY ]]
if [[ ${0##*/} == grok ]]; then
    [[ $1 == --cwd && $2 == "$PWD" && $3 == --no-alt-screen ]]
    provider=grok
elif [[ $* == *"daily review"* ]]; then
    [[ $1 == run && $2 == --dir && $3 == "$PWD" ]]
    provider=fallback
else
    printf 'editor\n' >> "$CALL_LOG"
    printf 'edited\n' >> widget.h
    case "$EDIT_MODE" in
      partial) printf 'partial\n' > test/test_new.c; exit 124 ;;
      ok) printf 'Improve widget docs\n' > .led-daily-what ;;
    esac
    exit 0
fi
printf '%s\n' "$provider" >> "$CALL_LOG"
[[ $PWD != "$LIVE_REPO" && ! -e .git && ! -e private-untracked && ! -e review-mutation ]]
printf '%s\n' "$provider" > review-mutation
case "$provider:${GROK_MODE}" in
  grok:timeout) exit 124 ;;
  grok:noise) printf 'Almost done\n' ;;
  grok:multi) printf 'SKIP\nCHANGE: not one verdict\n' ;;
  grok:pipe) printf 'CHANGE: no | pipe\n' ;;
  grok:empty) printf 'CHANGE:    \n' ;;
  grok:skip) printf '\r\nSKIP\r\n' ;;
  grok:ok) printf '\nCHANGE: Improve widget docs\n' ;;
  fallback:*) printf '%s\n' "$FALLBACK_VERDICT" ;;
esac
"""


class DailyTest(unittest.TestCase):
    def git(self, *args):
        return subprocess.check_output(
            ["git", *args], cwd=self.repo, env=self.env, text=True, stderr=subprocess.DEVNULL
        ).strip()

    def setUp(self):
        tmp = tempfile.TemporaryDirectory(prefix="led-daily-test-")
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.repo = self.root / "local copy"
        self.repo.mkdir()
        self.home = self.root / "home"
        self.home.mkdir()
        cli = self.root / "bin"
        cli.mkdir()
        self.state = self.root / "state" / "led-daily"
        self.call_log = self.root / "calls"
        self.env = {**os.environ, "HOME": str(self.home),
                    "PATH": str(cli) + os.pathsep + os.environ["PATH"],
                    "XDG_CONFIG_HOME": str(self.root / "config"),
                    "XDG_STATE_HOME": str(self.state.parent),
                    "XDG_DATA_HOME": str(self.root / "data"),
                    "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null",
                    "GROK": str(cli / "grok"), "OPENCODE": str(cli / "opencode"),
                    "GROK_MODE": "noise", "EDIT_MODE": "ok",
                    "FALLBACK_VERDICT": "CHANGE: Improve widget docs",
                    "CALL_LOG": str(self.call_log), "LIVE_REPO": str(self.repo)}
        self.env.pop("TYPESAFE_API_KEY", None)
        config = Path(self.env["XDG_CONFIG_HOME"]) / "led"
        config.mkdir(parents=True)
        (config / "daily.env").write_text("TYPESAFE_API_KEY=fake-scorer-only\n")
        for name in ("grok", "opencode"):
            (cli / name).write_text(AGENT)
            (cli / name).chmod(0o755)
        (cli / "thunderbird").write_text("#!/bin/sh\nexit 1\n")
        (cli / "systemd-run").write_text("#!/bin/sh\nexit 1\n")
        for f in (cli / "thunderbird", cli / "systemd-run"):
            f.chmod(0o755)
        (self.repo / "tools").mkdir()
        (self.repo / "test").mkdir()
        shutil.copy2(DAILY, self.repo / "tools/daily.sh")
        (self.repo / "tools/quality_jev.py").write_text(SCORER)
        (self.repo / "widget.h").write_text("original\n")
        (self.repo / "path.log").write_text("")
        (self.repo / ".gitignore").write_text("led\n__pycache__/\n")
        (self.repo / "Makefile").write_text(
            "all:\n\tcp test/fake-led led\nclean:\n\trm -f led\n")
        runner = self.repo / "test/fake-led"
        runner.write_text("#!/bin/sh\nprintf 'Test summary: 4 passed, 0 failed\\nALL TESTS PASSED\\n'\n")
        runner.chmod(0o755)
        self.git("init", "-b", "master")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "user.name", "Workflow test")
        self.git("add", ".")
        self.git("commit", "-m", "fixture")
        self.head = self.git("rev-parse", "HEAD")
        self.git("clone", "--bare", str(self.repo), str(self.root / "origin.git"))
        self.git("remote", "add", "origin", str(self.root / "origin.git"))
        (self.repo / "private-untracked").touch()

    def run_daily(self, *args, **env):
        p = subprocess.run(["bash", str(self.repo / "tools/daily.sh"), *args],
                           cwd=self.root, env={**self.env, **env},
                           capture_output=True, text=True, timeout=12)
        self.assertFalse(any(p.is_dir() for p in self.state.glob("review.*")))
        self.assertFalse((self.repo / "review-mutation").exists())
        self.assertTrue((self.repo / "private-untracked").exists())
        return p

    def calls(self):
        return self.call_log.read_text().splitlines() if self.call_log.exists() else []

    def log(self):
        return (self.repo / "path.log").read_text()

    def test_noise_falls_back_and_records_passing_edit(self):
        p = self.run_daily()
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(self.calls(), ["grok", "fallback", "editor"])
        self.assertIn("tests 4/4", self.log())
        self.assertEqual(self.git("rev-parse", "HEAD"), self.git("rev-parse", "origin/master"))
        self.assertEqual((self.repo / "widget.h").read_text(), "original\nedited\n")

    def test_timeout_falls_back(self):
        p = self.run_daily(GROK_MODE="timeout", FALLBACK_VERDICT="SKIP")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(self.calls(), ["grok", "fallback"])
        self.assertIn("review declined", self.log())

    def test_valid_skip_does_not_fall_back(self):
        p = self.run_daily(GROK_MODE="skip")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(self.calls(), ["grok"])
        self.assertIn("review declined", self.log())

    def test_missing_grok_falls_back(self):
        p = self.run_daily(GROK=str(self.root / "missing"), FALLBACK_VERDICT="SKIP")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(self.calls(), ["fallback"])
        self.assertIn("review declined", self.log())

    def test_bad_verdicts_do_not_edit(self):
        for kind in ("multi", "pipe", "empty"):
            with self.subTest(kind=kind):
                p = self.run_daily(GROK_MODE=kind, FALLBACK_VERDICT="not a verdict")
                self.assertNotEqual(p.returncode, 0, p.stderr)
                self.assertIn("no valid review verdict", self.log())
        self.assertEqual(self.calls(), ["grok", "fallback"] * 3)
        self.assertEqual((self.repo / "widget.h").read_text(), "original\n")

    def test_partial_edit_after_nonzero_exit_is_rolled_back(self):
        p = self.run_daily(GROK_MODE="ok", EDIT_MODE="partial")
        self.assertNotEqual(p.returncode, 0, p.stderr)
        self.assertEqual(self.calls(), ["grok", "editor"])
        self.assertIn("opencode exit 124", self.log())
        self.assertEqual((self.repo / "widget.h").read_text(), "original\n")
        self.assertFalse((self.repo / "test/test_new.c").exists())

    def test_dry_does_not_need_origin_or_touch_tree(self):
        self.git("remote", "remove", "origin")
        before = self.git("status", "--porcelain")
        p = self.run_daily("dry")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn("target widget.h", p.stderr)
        self.assertEqual(self.calls(), [])
        self.assertEqual(self.git("rev-parse", "HEAD"), self.head)
        self.assertEqual(self.git("status", "--porcelain"), before)
        self.assertEqual(self.log(), "")


if __name__ == "__main__":
    unittest.main()
