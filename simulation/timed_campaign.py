"""Run the timed SystemC simulator from Python.

The binary is the stable interface. pybind11 is intentionally not required.
"""

from __future__ import annotations

import json
import os
import subprocess
from pathlib import Path


def binary() -> Path:
    env = os.environ.get("UCIE_SIM")
    if env:
        return Path(env)
    root = Path(__file__).resolve().parents[1]
    candidate = root / "build" / "ucie_sim"
    if candidate.exists():
        return candidate
    raise FileNotFoundError("ucie_sim not found; set UCIE_SIM or build with CMake")


def run(config: str, experiment: str, seed: int = 1, output: str | None = None) -> dict:
    out = Path(output) if output else Path("timed_results") / f"{experiment}_{seed}"
    out.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            str(binary()),
            "--config",
            config,
            "--experiment",
            experiment,
            "--seed",
            str(seed),
            "--output",
            str(out),
        ],
        check=True,
    )
    return json.loads((out / "result.json").read_text(encoding="utf-8"))


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(description="Orchestrate timed UCIe experiments")
    parser.add_argument("--config", default="configs/automotive.yaml")
    parser.add_argument("--experiment", required=True)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--output")
    args = parser.parse_args()
    result = run(args.config, args.experiment, args.seed, args.output)
    print(json.dumps({"experiment": result["experiment"], "pass": result["pass"]}, indent=2))


if __name__ == "__main__":
    main()
