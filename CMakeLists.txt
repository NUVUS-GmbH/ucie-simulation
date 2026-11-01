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
