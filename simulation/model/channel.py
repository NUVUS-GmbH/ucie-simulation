"""Fault-injection channel between the two UCIe adapters.

The channel is not a PHY. It carries one flit in each direction and applies
the corruption the fault injector has armed. Detection is decided by the
UCIe CRC guarantee in ``ucie.crc_detects``, not by a transistor or bit-lane
model.
"""

from __future__ import annotations

from simulation.model.ucie import Flit


def apply_corruption(flit: Flit, bits: int) -> Flit:
    """Mark a flit corrupt. ``bits`` is the injected error weight."""

    flit.corrupt_bits = bits
    flit.crc_ok = bits <= 0
    return flit
