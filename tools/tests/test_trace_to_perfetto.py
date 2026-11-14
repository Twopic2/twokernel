import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from trace_to_perfetto import convert


class TraceConversionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.capture = Path(self.directory.name) / "trace.jsonl"
        self.output = Path(self.directory.name) / "trace.json"

    def test_preserves_counters_slices_metadata_and_microseconds(self):
        events = [
            {"ph": "M", "pid": 1, "name": "process_name", "args": {"name": "TwoKernel"}},
            {"ph": "C", "pid": 1, "tid": 0, "ts": 100000,
             "name": "Heap requested bytes", "args": {"value": 8192}},
            {"ph": "X", "pid": 1, "tid": 7, "ts": 100000,
             "name": 'Running "worker"\\\n', "dur": 6000},
            {"ph": "C", "pid": 1, "tid": 0, "ts": 200000,
             "name": "Trace dropped events", "args": {"value": 12}},
        ]
        self.capture.write_text("\n".join(map(json.dumps, events)) + "\n")
        self.assertEqual(convert(self.capture, self.output), (4, False, 12))
        self.assertEqual(json.loads(self.output.read_text())["traceEvents"], events)

    def test_interrupted_utf8_record_preserves_complete_events(self):
        event = {"ph": "C", "pid": 1, "tid": 0, "ts": 0,
                 "name": "Heap bytes", "args": {"value": 0}}
        self.capture.write_bytes(json.dumps(event).encode() + b'\n{"name":"\xe2\x82')
        self.assertEqual(convert(self.capture, self.output), (1, True, 0))
        self.assertEqual(json.loads(self.output.read_text())["traceEvents"], [event])

    def test_malformed_middle_record_leaves_previous_output_intact(self):
        self.capture.write_bytes(b'{"ph":\n')
        self.output.write_text("previous trace")
        with self.assertRaisesRegex(ValueError, "line 1: malformed JSON"):
            convert(self.capture, self.output)
        self.assertEqual(self.output.read_text(), "previous trace")
        self.assertEqual(len(list(self.output.parent.iterdir())), 2)

    def test_rejects_invalid_durations_and_empty_capture(self):
        self.capture.write_text(json.dumps({"ph": "X", "pid": 1, "tid": 1,
                                           "ts": 0, "dur": -1, "name": "Running"}) + "\n")
        with self.assertRaisesRegex(ValueError, "dur"):
            convert(self.capture, self.output)
        self.capture.write_bytes(b"")
        with self.assertRaisesRegex(ValueError, "no complete"):
            convert(self.capture, self.output)
        self.assertFalse(self.output.exists())

    def test_cannot_overwrite_capture(self):
        self.capture.write_bytes(b"original")
        with self.assertRaisesRegex(ValueError, "different files"):
            convert(self.capture, self.capture)
        self.assertEqual(self.capture.read_bytes(), b"original")


if __name__ == "__main__":
    unittest.main()
