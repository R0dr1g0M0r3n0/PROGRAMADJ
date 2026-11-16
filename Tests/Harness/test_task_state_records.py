"""Structural contract tests for PD-69 task transition records.

These tests use only the Python standard library. They inspect the schema and
representative records; they do not perform full JSON Schema validation or
exercise a production task-state runtime.
"""

import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCHEMA_PATH = ROOT / "docs" / "task-transition-record-v1.schema.json"
FIXTURE_PATH = ROOT / "TestFixtures" / "task_transition_records_v1.json"
CONTRACT_PATH = ROOT / "docs" / "task-state-machine.md"

BASE_FIELDS = {
    "schemaVersion",
    "taskId",
    "sequence",
    "fromState",
    "toState",
    "event",
    "attempt",
    "prerequisiteIds",
}


class TaskTransitionRecordContractTests(unittest.TestCase):
    def test_versioned_schema_and_representative_fixture_exist(self):
        self.assertTrue(SCHEMA_PATH.is_file(), "version 1 record schema is missing")
        self.assertTrue(FIXTURE_PATH.is_file(), "representative record fixture is missing")

    def _load_schema(self):
        if not SCHEMA_PATH.is_file():
            self.skipTest("schema is added after the initial RED test")
        return json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))

    def _load_fixture(self):
        if not FIXTURE_PATH.is_file():
            self.skipTest("fixture is added after the initial RED test")
        return json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))

    def test_schema_is_versioned_and_declares_transition_record_fields(self):
        schema = self._load_schema()

        self.assertEqual(
            schema["$schema"],
            "https://json-schema.org/draft/2020-12/schema",
        )
        self.assertEqual(schema["properties"]["schemaVersion"]["const"], 1)
        self.assertEqual(schema["type"], "object")
        self.assertFalse(schema["additionalProperties"])
        self.assertTrue(BASE_FIELDS.issubset(set(schema["required"])))
        self.assertEqual(
            set(schema["properties"]["event"]["enum"]),
            {
                "created",
                "attempt_started",
                "attempt_succeeded",
                "attempt_failed",
                "attempts_exhausted",
            },
        )
        self.assertEqual(
            set(schema["properties"]["fromState"]["enum"]),
            {"pending", "running", "failed", "succeeded", "blocked", None},
        )
        self.assertEqual(
            set(schema["properties"]["toState"]["enum"]),
            {"pending", "running", "failed", "succeeded", "blocked"},
        )
        self.assertEqual(schema["properties"]["attempt"]["minimum"], 0)
        self.assertEqual(schema["properties"]["attempt"]["maximum"], 3)
        self.assertEqual(schema["properties"]["sequence"]["minimum"], 1)

    def test_schema_conditions_bind_input_and_terminal_execution_evidence(self):
        schema = self._load_schema()
        conditions = {}
        for rule in schema["allOf"]:
            event = rule["if"]["properties"]["event"]["const"]
            conditions[event] = rule["then"]

        self.assertEqual(
            {
                "created",
                "attempt_started",
                "attempt_succeeded",
                "attempt_failed",
                "attempts_exhausted",
            },
            set(conditions),
        )
        self.assertIn("inputCommit", conditions["attempt_started"]["required"])
        started = conditions["attempt_started"]
        self.assertEqual(started["properties"]["toState"]["const"], "running")
        self.assertEqual(
            started["oneOf"][0]["properties"]["fromState"]["const"], "pending"
        )
        self.assertEqual(started["oneOf"][0]["properties"]["attempt"]["const"], 0)
        self.assertEqual(
            started["oneOf"][1]["properties"]["fromState"]["const"], "failed"
        )
        self.assertEqual(
            started["oneOf"][1]["properties"]["attempt"]["minimum"], 1
        )
        self.assertEqual(
            started["oneOf"][1]["properties"]["attempt"]["maximum"], 3
        )
        for event in ("attempt_succeeded", "attempt_failed"):
            self.assertIn("executionEvidence", conditions[event]["required"])
        self.assertEqual(
            conditions["attempt_succeeded"]["properties"]["toState"]["const"],
            "succeeded",
        )
        self.assertEqual(
            conditions["attempt_failed"]["properties"]["toState"]["const"],
            "failed",
        )
        self.assertEqual(
            conditions["attempt_succeeded"]["properties"]["executionEvidence"]
            ["properties"]["result"]["properties"]["outcome"]["const"],
            "success",
        )
        self.assertEqual(
            conditions["attempt_failed"]["properties"]["executionEvidence"]
            ["properties"]["result"]["properties"]["outcome"]["const"],
            "failure",
        )
        self.assertEqual(
            conditions["attempts_exhausted"]["properties"]["attempt"]["const"],
            3,
        )
        self.assertEqual(
            conditions["attempts_exhausted"]["properties"]["fromState"]["const"],
            "failed",
        )
        self.assertEqual(
            conditions["attempts_exhausted"]["properties"]["toState"]["const"],
            "blocked",
        )

    def test_fixture_has_monotonic_per_task_sequences_and_exact_attempt_inputs(self):
        fixture = self._load_fixture()
        self.assertEqual(fixture["formatVersion"], 1)
        records = fixture["records"]
        self.assertTrue(records)

        by_task = {}
        for record in records:
            self.assertTrue(BASE_FIELDS.issubset(record))
            self.assertEqual(record["schemaVersion"], 1)
            self.assertIsInstance(record["taskId"], str)
            self.assertTrue(record["taskId"])
            self.assertIsInstance(record["prerequisiteIds"], list)
            self.assertTrue(
                all(isinstance(item, str) and item for item in record["prerequisiteIds"])
            )
            by_task.setdefault(record["taskId"], []).append(record)

            if record["event"] == "attempt_started":
                self.assertEqual(record["toState"], "running")
                self.assertRegex(
                    record["inputCommit"],
                    re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$"),
                )
                if record["fromState"] == "pending":
                    self.assertEqual(record["attempt"], 0)
                else:
                    self.assertEqual(record["fromState"], "failed")
                    self.assertIn(record["attempt"], (1, 2, 3))

            if record["event"] in ("attempt_succeeded", "attempt_failed"):
                self.assertEqual(record["fromState"], "running")
                evidence = record["executionEvidence"]
                self.assertTrue(evidence["command"])
                self.assertTrue(all(isinstance(part, str) for part in evidence["command"]))
                self.assertTrue(evidence["result"]["detail"])
                expected = (
                    "success"
                    if record["event"] == "attempt_succeeded"
                    else "failure"
                )
                self.assertEqual(evidence["result"]["outcome"], expected)

        for task_records in by_task.values():
            self.assertEqual(
                [record["sequence"] for record in task_records],
                list(range(1, len(task_records) + 1)),
            )
            prerequisite_ids = task_records[0]["prerequisiteIds"]
            self.assertTrue(
                all(record["prerequisiteIds"] == prerequisite_ids for record in task_records)
            )

    def test_fixture_blocks_only_after_initial_and_three_correction_failures(self):
        fixture = self._load_fixture()
        self.assertEqual(fixture["formatVersion"], 1)
        records = fixture["records"]
        blocked = [record for record in records if record["toState"] == "blocked"]

        self.assertEqual(len(blocked), 1)
        blocked_record = blocked[0]
        task_records = [
            record for record in records if record["taskId"] == blocked_record["taskId"]
        ]
        self.assertEqual(
            [record["event"] for record in task_records],
            [
                "created",
                "attempt_started",
                "attempt_failed",
                "attempt_started",
                "attempt_failed",
                "attempt_started",
                "attempt_failed",
                "attempt_started",
                "attempt_failed",
                "attempts_exhausted",
            ],
        )
        failed_attempts = [
            record["attempt"]
            for record in task_records
            if record["event"] == "attempt_failed"
        ]
        self.assertEqual(failed_attempts, [0, 1, 2, 3])
        self.assertEqual(blocked_record["fromState"], "failed")
        self.assertEqual(blocked_record["attempt"], 3)

    def test_fixture_covers_success_and_contract_documents_fail_closed_unknown_versions(self):
        fixture = self._load_fixture()
        self.assertEqual(fixture["formatVersion"], 1)
        succeeded = [
            record for record in fixture["records"] if record["toState"] == "succeeded"
        ]
        self.assertEqual(
            [record["taskId"] for record in succeeded],
            ["task-prerequisite", "task-dependent"],
        )
        self.assertEqual(succeeded[0]["event"], "attempt_succeeded")
        self.assertEqual(
            succeeded[0]["executionEvidence"]["result"]["outcome"],
            "success",
        )

        contract = CONTRACT_PATH.read_text(encoding="utf-8")
        self.assertIn("corrupt, or unsupported-version records fail closed", contract)
        self.assertIn("Project manifest history", contract)
        self.assertIn("not authoritative for task state", contract)

    def test_prerequisite_is_succeeded_before_dependent_task_starts(self):
        records = self._load_fixture()["records"]
        prerequisite_success_index = next(
            index
            for index, record in enumerate(records)
            if record["taskId"] == "task-prerequisite"
            and record["event"] == "attempt_succeeded"
        )
        dependent_start_index = next(
            index
            for index, record in enumerate(records)
            if record["taskId"] == "task-dependent"
            and record["event"] == "attempt_started"
        )
        dependent_start = records[dependent_start_index]

        self.assertLess(prerequisite_success_index, dependent_start_index)
        self.assertEqual(dependent_start["prerequisiteIds"], ["task-prerequisite"])


if __name__ == "__main__":
    unittest.main()
