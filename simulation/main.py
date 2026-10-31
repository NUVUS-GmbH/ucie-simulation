"""Run the UCIe 2.0 study and the UCIe 2.0 versus UCIe 3.0 comparison.

From the repository root:

    python -m simulation.main
    python -m simulation.main --seed 1 --random 200

The UCIe 2.0 report is written to ``--out``. The same runs produce
``findings_ucie3.md`` and ``comparison_ucie2_vs_ucie3.md`` beside it.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from simulation.experiments.compare import run_comparison
from simulation.reporting.statistics import render_report
from simulation.reporting.ucie3 import render_comparison, render_findings


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="AoU 0.8 over UCIe 2.0 and UCIe 3.0 fault injection"
    )
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--random", type=int, default=200, dest="random_count")
    parser.add_argument(
        "--out",
        type=Path,
        default=Path(__file__).resolve().parent / "results" / "findings.md",
    )
    args = parser.parse_args(argv)

    comparison = run_comparison(args.seed, args.random_count)
    report = render_report(
        comparison.v2.milestones,
        comparison.v2.explorer,
        comparison.v2.random_results,
        args.seed,
    )
    findings3 = render_findings(comparison)
    comparison_text = render_comparison(comparison)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    findings_path = args.out.parent / "findings_ucie3.md"
    comparison_path = args.out.parent / "comparison_ucie2_vs_ucie3.md"
    args.out.write_text(report, encoding="utf-8")
    findings_path.write_text(findings3, encoding="utf-8")
    comparison_path.write_text(comparison_text, encoding="utf-8")
    print(comparison_text, end="")
    print(f"\nWrote {args.out}")
    print(f"Wrote {findings_path}")
    print(f"Wrote {comparison_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
