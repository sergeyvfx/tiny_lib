# Copyright (c) 2021 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

# Utility functions for defnining regression and unit test targets.

# Define unit test target.
#
# The target is specified by the name of a test (without "_test" suffic) and the
# file name it is compiled from. The "_test" for the target suffix will be added
# automatically.
#
# Extra definitions and compiler options are controlled via the DEFINITIONS
# and COMPILE_OPTIONS flags.
#
# It is possible to pass additional linking libraries by specifying "LIBRARIES"
# argument (the target will be linked against all libraries listed after the
# "LIBRRAIES" keyword).
#
# It is possible to specify runtime command line arguments passed to the test
# executable by using "ARGUMENTS" argument and passing all desired command line
# arguments after it.
#
# Example:
#
#   tl_single_test(filter internal/my_test.cc
#                  DEFINITIONS USE_INTERESTING_FEATURE
#                  INCLUDES ny/private/include
#                  LIBRARIES my_library
#                  ARGUMENTS --test_srcdir ${TEST_SRCDIR})
function(tl_single_test TEST_NAME FILENAME)
  if(NOT WITH_TESTS)
    return()
  endif()

  cmake_parse_arguments(
    TEST
    ""
    ""
    "COMPILE_OPTIONS;DEFINITIONS;INCLUDES;LIBRARIES;ARGUMENTS"
    ${ARGN}
  )

  set(target_name "tl_${TEST_NAME}_test")

  add_executable(${target_name} ${FILENAME})

  target_include_directories(${target_name} SYSTEM PRIVATE
    ${TEST_INCLUDES}
  )

  target_compile_options(${target_name} PRIVATE ${TEST_COMPILE_OPTIONS})
  target_compile_definitions(${target_name} PRIVATE ${TEST_DEFINITIONS})

  target_link_libraries(${target_name}
    ${TEST_LIBRARIES}
    tl_test_main
    GTest::gtest
    GTest::gmock
    gflags::gflags
  )

  add_test(NAME ${target_name}
           COMMAND $<TARGET_FILE:${target_name}> ${TEST_ARGUMENTS}
  )

  # Make sure test is created in it's own dedicated directory.
  target_set_output_directory(${target_name} ${TEST_EXECUTABLE_OUTPUT_DIR})

  # Make it easy to run test projects from IDE.
  target_set_debugger_command_arguments(${target_name} ${TEST_ARGUMENTS})
endfunction()

# Define unit test target which tests both default configuration and the
# configuration with exceptions disabled.
#
# The target is specified by the name of a test (without "_test" suffix) and the
# file name it is compiled from. The "_test" for the target suffix will be added
# automatically.
# For the test without the exceptions th suffix is "_noexc_test".
#
# Extra definitions and compiler options are controlled via the DEFINITIONS
# and COMPILE_OPTIONS flags.
#
# It is possible to pass additional linking libraries by specifying "LIBRARIES"
# argument (the target will be linked against all libraries listed after the
# "LIBRARIES" keyword).
#
# It is possible to specify runtime command line arguments passed to the test
# executable by using "ARGUMENTS" argument and passing all desired command line
# arguments after it.
function(tl_test TEST_NAME FILENAME)
  if(NOT WITH_TESTS)
    return()
  endif()

  tl_single_test(${TEST_NAME} ${FILENAME} ${ARGN})

  tl_single_test(${TEST_NAME}_noexc ${FILENAME} ${ARGN})
  disable_target_exceptions(tl_${TEST_NAME}_noexc_test COMPILE_OPTIONS)
endfunction()
