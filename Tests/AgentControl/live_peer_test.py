"""Exercise both production peer checks using signed, app-scoped fixtures."""

import filecmp
import pathlib
import plistlib
import re
import subprocess
import sys
import unittest

APP_ID = "com.audioforge.studio"
SERVICE_ID = APP_ID + ".agent-control"
WRONG_APP_ID = APP_ID + ".f1-test"
WRONG_SERVICE_ID = SERVICE_ID + ".f1-test"


def executable(bundle):
    with (bundle / "Contents/Info.plist").open("rb") as info:
        return bundle / "Contents/MacOS" / plistlib.load(info)["CFBundleExecutable"]


def helper(app):
    return app / "Contents/XPCServices" / (SERVICE_ID + ".xpc")


def run(*args):
    return subprocess.run(args, check=False, capture_output=True, text=True, timeout=10)


def signing_team(bundle):
    display = run("/usr/bin/codesign", "-dv", bundle)
    match = re.search(r"^TeamIdentifier=([A-Z0-9]{10})$", display.stderr, re.MULTILINE)
    if display.returncode or not match:
        raise RuntimeError(f"{bundle}: cannot read a signing team: {display.stderr}")
    return match[1]


def requirement(identifier, team):
    return f'anchor apple generic and identifier "{identifier}" and certificate leaf[subject.OU] = "{team}"'


class LivePeerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.team = signing_team(app)
        cls.service_requirement = requirement(SERVICE_ID, cls.team)
        for bundle in (app, wrong_service_app, wrong_client_app, matching_client_app):
            verification = run("/usr/bin/codesign", "--verify", "--deep", "--strict", bundle)
            if verification.returncode:
                raise RuntimeError(f"{bundle}: {verification.stderr}")

    def assertSignature(self, bundle, identifier):
        verification = run("/usr/bin/codesign", "--verify", "-R=" + requirement(identifier, self.team), bundle)
        self.assertEqual(verification.returncode, 0, verification.stderr)

    def assertOutcome(self, host, arguments, output):
        result = run(executable(host), *arguments)
        self.assertEqual((result.returncode, result.stdout.strip()), (0, output), result.stderr)
        print(f"{host.stem} {arguments[0]}: {result.stdout.strip()} (exit {result.returncode})", flush=True)

    def helper_signing_denials(self):
        result = run(
            "/usr/bin/log", "show", "--last", "2m", "--style", "compact",
            "--predicate",
            'process == "com.audioforge.studio.agent-control" AND '
            'eventMessage CONTAINS "Received message forbidden due to code signing requirement"',
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return {line for line in result.stdout.splitlines() if "Received message forbidden" in line}

    def test_client_refuses_wrong_helper(self):
        self.assertSignature(wrong_service_app, APP_ID)
        self.assertSignature(helper(wrong_service_app), WRONG_SERVICE_ID)
        # A control pins the fixture's actual identity and gets a valid reply.
        self.assertOutcome(wrong_service_app, ("exchange", requirement(WRONG_SERVICE_ID, self.team)), "reply: 0")
        self.assertOutcome(wrong_service_app, ("discover",), "discovery: 4")

    def test_helper_refuses_wrong_client(self):
        self.assertSignature(app, APP_ID)
        self.assertSignature(helper(app), SERVICE_ID)
        self.assertSignature(matching_client_app, APP_ID)
        self.assertSignature(wrong_client_app, WRONG_APP_ID)
        production_helper = executable(helper(app))
        for host in (matching_client_app, wrong_client_app):
            self.assertSignature(helper(host), SERVICE_ID)
            self.assertTrue(filecmp.cmp(production_helper, executable(helper(host)), shallow=False),
                            f"{host}: expected a byte-identical copy of the production helper")
        self.assertOutcome(app, ("--agent-control-discovery-probe",), "XPC discovery status: 0")
        exchange = ("exchange", self.service_requirement)
        self.assertOutcome(matching_client_app, exchange, "reply: 0")
        # XPC reports this rejection to the sender as an interruption. Correlate
        # it with the helper's signing-denial log in the request's time window;
        # interruption alone also covers crashes and unrelated transport loss.
        previous_denials = self.helper_signing_denials()
        result = run(executable(wrong_client_app), *exchange)
        self.assertEqual((result.returncode, result.stdout.strip()), (0, "connection interrupted"), result.stderr)
        new_denials = self.helper_signing_denials() - previous_denials
        self.assertTrue(new_denials, "no new helper code-signing denial log correlated with wrong-client request")
        print("wrong-client rejection: XPC interruption correlated with helper signing-denial log", flush=True)


if __name__ == "__main__":
    app, wrong_service_app, wrong_client_app, matching_client_app = map(pathlib.Path, sys.argv[1:])
    unittest.main(argv=sys.argv[:1], verbosity=2)
