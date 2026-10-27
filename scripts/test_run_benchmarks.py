import csv
from pathlib import Path
import tempfile
import unittest

from run_benchmarks import default_episodes, parse_rows, write_summary


SAMPLE = """========== BARRIER SYNCHRONIZATION PERFORMANCE BENCHMARK ==========
SenseReversingBarrier           | Threads: 4   | Episodes: 200000   | Time: 0.100000 s | Rate: 2000000.00 ep/s
SenseReversing (with backoff)    | Threads: 4   | Episodes: 200000   | Time: 0.200000 s | Rate: 1000000.00 ep/s
StdBarrierWrapper (baseline)    | Threads: 4   | Episodes: 200000   | Time: 1.000000 s | Rate: 200000.00 ep/s
"""


class ResultsTest(unittest.TestCase):
    def test_three_barriers_and_global_episode_count(self):
        rows = parse_rows(SAMPLE, 2, 4, 200000)
        self.assertEqual(len(rows), 3)
        self.assertEqual(rows[0]["Run"], 2)
        self.assertEqual(rows[0]["EpisodesPerSecond"], 2000000)
        self.assertEqual(rows[0]["Episodes"], 200000)

    def test_reject_partial_or_mismatched_output(self):
        for text in ("", SAMPLE.rsplit("StdBarrierWrapper", 1)[0],
                     SAMPLE.replace("Threads: 4", "Threads: 8"),
                     SAMPLE.replace("0.100000", "0.000000"),
                     SAMPLE.replace("StdBarrierWrapper (baseline)", "SenseReversingBarrier")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_rows(text, 1, 4, 200000)

    def test_summary_median_min_max_and_samples(self):
        rows = []
        for run, rate in enumerate((9, 2, 4, 8, 1), 1):
            row = parse_rows(SAMPLE, run, 4, 200000)[0]
            row["EpisodesPerSecond"] = rate
            rows.append(row)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.csv"
            write_summary(rows, path)
            with path.open(newline="", encoding="utf-8") as stream:
                summary = list(csv.DictReader(stream))
        self.assertEqual(len(summary), 1)
        self.assertEqual(summary[0]["MedianEpisodesPerSecond"], "4")
        self.assertEqual(summary[0]["MinEpisodesPerSecond"], "1")
        self.assertEqual(summary[0]["MaxEpisodesPerSecond"], "9")
        self.assertEqual(summary[0]["Samples"], "5")

    def test_report_episode_plan_and_added_core_counts(self):
        self.assertEqual([default_episodes(n) for n in (1, 2, 3, 8, 10, 16, 24)],
                         [20000000, 2000000, 200000, 200000, 200000, 20000, 20000])


if __name__ == "__main__":
    unittest.main()
