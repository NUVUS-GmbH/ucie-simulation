"""Link-scoped status the initiator may read. No field is an AXI commit bit.

UCIe 3.0 §9.5 error and link registers, §9.5.3.40 Link Health Monitor,
§8.1 management transport, §4.1.5.2 priority sideband, §8.4.2.1 Fast
Throttle, and §8.4.2.2 Emergency Shutdown are all represented here. Each
field is a fact the specification actually defines. None of those sections
define a bit that names an outstanding AXI transaction or its architectural
result. ``management_reports_axi_commit``, ``header_log_has_axi_id``, and
``link_health_has_axi_id`` stay false for that reason. They are not a
policy.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class StdSignals:
    crc_error: bool = False
    retrain: bool = False
    linkerror: bool = False
    linkreset: bool = False
    uncorrectable: bool = False
    remote_linkerror_sideband: bool = False
    runtime_parity: bool = False
    fast_throttle: bool = False
    emergency_shutdown: bool = False
    pspt_opcode_only: bool = False
    runtime_recalibration: bool = False
    endpoint_reset: bool = False
    # These three are hard false. The registers they correspond to do not
    # have an AXI-id or commit field in UCIe 3.0.
    header_log_has_axi_id: bool = False
    link_health_has_axi_id: bool = False
    management_reports_axi_commit: bool = False

    def profile1(self) -> tuple[object, ...]:
        """AXI completion is applied separately. FDI contributes link-down."""

        return (self.linkerror, self.linkreset)

    def profile2(self) -> tuple[object, ...]:
        """Software-visible error state. Still no per-transaction commit bit."""

        return self.profile1() + (
            self.crc_error,
            self.retrain,
            self.uncorrectable,
            self.remote_linkerror_sideband,
            self.runtime_parity,
            self.header_log_has_axi_id,
        )

    def profile3(self) -> tuple[object, ...]:
        """All standardized management information modeled for this link."""

        return self.profile2() + (
            self.fast_throttle,
            self.emergency_shutdown,
            self.pspt_opcode_only,
            self.runtime_recalibration,
            self.endpoint_reset,
            self.link_health_has_axi_id,
            self.management_reports_axi_commit,
        )

    def view(self, profile: str) -> tuple[object, ...]:
        if profile == "fdi_axi":
            return self.profile1()
        if profile == "software_error":
            return self.profile2()
        if profile == "management":
            return self.profile3()
        return self.profile1()
