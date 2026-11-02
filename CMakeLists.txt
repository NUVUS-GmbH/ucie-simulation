cmake_minimum_required(VERSION 3.16)
project(ucie_timed_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMC REQUIRED IMPORTED_TARGET systemc)

add_library(ucie_core
  src/common/classify.cpp
  src/common/config.cpp
  src/common/arbitration.cpp
  src/experiments/scenarios.cpp
  src/axi/target_model.cpp
  src/ucie/retry_buffer.cpp
  src/ucie/credit_manager.cpp
)
target_include_directories(ucie_core PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(ucie_core_tests tests/test_core.cpp)
target_link_libraries(ucie_core_tests PRIVATE ucie_core)

add_executable(ucie_sim
  src/main.cpp
  src/sim/world.cpp
  src/sim/system.cpp
  src/virtualization/hypervisor.cpp
  src/axi/axi_master.cpp
  src/axi/axi_interconnect.cpp
  src/axi/axi_target.cpp
  src/axi/txn_watchdog.cpp
  src/aou/aou_endpoint.cpp
  src/ucie/ucie_adapter.cpp
  src/ucie/ucie_link.cpp
  src/ucie/link_state_machine.cpp
  src/ucie/sideband.cpp
)
target_include_directories(ucie_sim PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ucie_sim PRIVATE ucie_core PkgConfig::SYSTEMC)

enable_testing()
add_test(NAME core COMMAND ucie_core_tests)

set(SIM_CFG ${CMAKE_SOURCE_DIR}/configs/automotive.yaml)
function(add_sim_test name experiment)
  add_test(
    NAME ${name}
    COMMAND ucie_sim --experiment ${experiment} --config ${SIM_CFG} --output ${CMAKE_BINARY_DIR}/results/${experiment}
  )
endfunction()

add_sim_test(baseline_unknown baseline_unknown)
add_sim_test(precommit precommit)
add_sim_test(blind_retry blind_retry)
add_sim_test(observational_equivalence observational_equivalence)
add_sim_test(multi_guest multi_guest)
add_sim_test(retry_storm retry_storm)
add_sim_test(guest_a_storm guest_a_storm)
add_sim_test(shared_quiet shared_quiet)
add_sim_test(guest_b_isolated guest_b_isolated)
add_sim_test(guest_b_storm guest_b_storm)
add_sim_test(mit_safety_priority mit_safety_priority)
add_sim_test(mit_per_guest_slots mit_per_guest_slots)
add_sim_test(mit_replay_budget mit_replay_budget)
add_sim_test(mit_reserved_bw mit_reserved_bw)
add_sim_test(mit_virtual_channels mit_virtual_channels)
add_sim_test(priority_sideband priority_sideband)
add_sim_test(fast_throttle fast_throttle)
add_sim_test(emergency_shutdown emergency_shutdown)
add_sim_test(degraded_link degraded_link)
add_sim_test(random_campaign random_campaign)
add_sim_test(fh_commit_then_local_slverr fh_commit_then_local_slverr)
add_sim_test(fh_watchdog_then_late_okay fh_watchdog_then_late_okay)
add_sim_test(fh_timeout_then_remote_commit fh_timeout_then_remote_commit)
add_sim_test(fh_blind_retry_after_slverr fh_blind_retry_after_slverr)
add_sim_test(fh_read_burst_error_completion fh_read_burst_error_completion)
add_sim_test(fh_classifier_diag_compare fh_classifier_diag_compare)
add_sim_test(fh_diag_ack_loss_after_delivery fh_diag_ack_loss_after_delivery)
add_sim_test(fh_observer_history_compare fh_observer_history_compare)
add_sim_test(fh_safe_retry_cancel_remote fh_safe_retry_cancel_remote)
add_sim_test(fh_cancel_latency_race fh_cancel_latency_race)
add_sim_test(ax_clean_packet_imm ax_clean_packet_imm)
add_sim_test(ax_clean_packet_pkt ax_clean_packet_pkt)
add_sim_test(ax_loss_mid_packet_imm ax_loss_mid_packet_imm)
add_sim_test(ax_loss_mid_packet_pkt ax_loss_mid_packet_pkt)
add_sim_test(ax_loss_before_first_imm ax_loss_before_first_imm)
add_sim_test(ax_backpressure_tready ax_backpressure_tready)
add_sim_test(ax_continuous_tid_stable ax_continuous_tid_stable)
add_sim_test(ax_ack_loss_after_delivery ax_ack_loss_after_delivery)
add_sim_test(ax_safe_retry_cancel0 ax_safe_retry_cancel0)
add_sim_test(fh_race_resp_after_timeout fh_race_resp_after_timeout)
add_sim_test(a5_atomic_store_add_ok a5_atomic_store_add_ok)
add_sim_test(a5_atomic_blind_retry a5_atomic_blind_retry)
add_sim_test(a5_poison_reject_no_commit a5_poison_reject_no_commit)
add_sim_test(a5_trace_not_in_classifier a5_trace_not_in_classifier)
add_sim_test(a5_safe_retry_cancel0 a5_safe_retry_cancel0)
