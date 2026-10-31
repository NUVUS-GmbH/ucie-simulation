"""AXI target.

Architectural commit is a separate step from protocol completion. A write can
increment ``ground_commit_count`` and still have ``requester_received_response``
false. That split is the point of the study.

ASSUMPTION: the target commits a write only after both the request and the
write data have been delivered to it. It does not commit on address alone.
AoU and UCIe do not define device commit. This is a device model.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from simulation.model.axi import Transaction
from simulation.model.types import OpKind


@dataclass
class Target:
    counter: int = 0
    actuator: int = 0
    fifo: list[int] = field(default_factory=list)
    dma_starts: int = 0
    accelerator_starts: int = 0
    doorbell_rings: int = 0
    interrupts_acked: int = 0
    memory: dict[int, int] = field(default_factory=dict)

    def commit(self, tx: Transaction) -> str:
        """Apply the operation once. Caller is responsible for the phase."""

        tx.ground_commit_count += 1
        op = tx.op
        if op == OpKind.INCREMENT_COUNTER:
            old = self.counter
            self.counter += 1
            return f"Counter changes from {old} to {self.counter}"
        if op == OpKind.INCREMENT_ACTUATOR:
            old = self.actuator
            self.actuator += 1
            return f"Actuator-command counter changes from {old} to {self.actuator}"
        if op == OpKind.PUSH_FIFO:
            old = len(self.fifo)
            self.fifo.append(tx.data)
            return f"FIFO depth changes from {old} to {len(self.fifo)}"
        if op == OpKind.POP_FIFO:
            old = len(self.fifo)
            value = self.fifo.pop(0) if self.fifo else 0
            tx.data = value
            return f"FIFO pop changes depth from {old} to {len(self.fifo)}"
        if op == OpKind.TRIGGER_DMA:
            self.dma_starts += 1
            return f"DMA start count changes to {self.dma_starts}"
        if op == OpKind.START_ACCELERATOR:
            self.accelerator_starts += 1
            return f"Accelerator start count changes to {self.accelerator_starts}"
        if op == OpKind.RING_DOORBELL:
            self.doorbell_rings += 1
            return f"Doorbell ring count changes to {self.doorbell_rings}"
        if op == OpKind.ACK_INTERRUPT:
            self.interrupts_acked += 1
            return f"Interrupt-ack count changes to {self.interrupts_acked}"
        if op == OpKind.MEMORY_WRITE:
            self.memory[tx.address] = tx.data
            return f"Memory[{tx.address:#x}] written with {tx.data}"
        if op == OpKind.MEMORY_READ:
            tx.data = self.memory.get(tx.address, 0)
            return f"Memory[{tx.address:#x}] read as {tx.data}"
        raise AssertionError(f"unhandled op {op}")
