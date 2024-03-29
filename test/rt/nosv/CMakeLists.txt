# Copyright (c) 2021-2024 Barcelona Supercomputing Center (BSC)
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Nosv)

if(NOT NOSV_FOUND)
  if(ENABLE_ALL_TESTS)
    message(FATAL_ERROR "nOS-V not found, cannot enable nOS-V RT tests")
  else()
    message(STATUS "nOS-V not found, disabling nOS-V RT tests")
  endif()
  return()
endif()

# Needed for nesting nosv_init/nosv_shutdown
if("${NOSV_VERSION}" VERSION_LESS "2.1.2")
  message(FATAL_ERROR "nOS-V version ${NOSV_VERSION} too old, required at least 2.1.2")
endif()

message(STATUS "Enabling nOS-V RT tests")

function(nosv_test)
  set(switches "")
  set(single LEVEL)
  set(multi "")
  cmake_parse_arguments(
    NOSV_TEST "${switches}" "${single}" "${multi}" ${ARGN})
  ovni_test(${ARGN})
  target_link_libraries("${OVNI_TEST_NAME}" PRIVATE PkgConfig::NOSV)
  set_property(TEST "${OVNI_TEST_NAME}" APPEND
    PROPERTY
      ENVIRONMENT "NOSV_CONFIG=${OVNI_TEST_SOURCE_DIR}/rt/nosv/nosv.toml")
  if(DEFINED NOSV_TEST_LEVEL)
    set_property(TEST "${OVNI_TEST_NAME}" APPEND PROPERTY
      ENVIRONMENT "NOSV_CONFIG_OVERRIDE=ovni.level=${NOSV_TEST_LEVEL}")
  endif()
endfunction()

nosv_test(attach.c SORT)
nosv_test(waitfor.c SORT)
nosv_test(several-tasks.c SORT)
nosv_test(init-nested.c SORT)
nosv_test(parallel-tasks.c SORT)
nosv_test(inline.c SORT)
nosv_test(mutex.c SORT LEVEL 3)
nosv_test(barrier.c SORT LEVEL 3)

# Test multiple instrumentation levels
nosv_test(several-tasks.c SORT NAME several-tasks-level-0 LEVEL 0)
nosv_test(several-tasks.c SORT NAME several-tasks-level-1 LEVEL 1)
nosv_test(several-tasks.c SORT NAME several-tasks-level-2 LEVEL 2)
nosv_test(several-tasks.c SORT NAME several-tasks-level-3 LEVEL 3)
nosv_test(several-tasks.c SORT NAME several-tasks-level-4 LEVEL 4)
