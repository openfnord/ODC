################################################################################
#    Copyright (C) 2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

cmake_host_system_information(RESULT fqdn QUERY FQDN)

if(NOT CTEST_SOURCE_DIRECTORY)
  set(CTEST_SOURCE_DIRECTORY .)
endif()
if(NOT CTEST_BINARY_DIRECTORY)
  set(CTEST_BINARY_DIRECTORY build)
endif()
set(CTEST_CMAKE_GENERATOR "Ninja")
set(CTEST_USE_LAUNCHERS ON)
if(CMAKE_CXX_FLAGS)
  set(CTEST_CONFIGURATION_TYPE "Debug")
else()
  set(CTEST_CONFIGURATION_TYPE "RelWithDebInfo")
endif()
set(CTEST_BUILD_TARGET install)

if(NOT NCPUS)
  if(ENV{SLURM_CPUS_PER_TASK})
    set(NCPUS $ENV{SLURM_CPUS_PER_TASK})
  else()
    include(ProcessorCount)
    ProcessorCount(NCPUS)
    if(NCPUS EQUAL 0)
      set(NCPUS 1)
    endif()
  endif()
endif()

if("$ENV{CTEST_SITE}" STREQUAL "")
  set(CTEST_SITE "${fqdn}")
else()
  set(CTEST_SITE $ENV{CTEST_SITE})
endif()

if("$ENV{LABEL}" STREQUAL "")
  set(CTEST_BUILD_NAME "build")
else()
  set(CTEST_BUILD_NAME $ENV{LABEL})
endif()

if(CTEST_DASHBOARD_MODEL_NIGHTLY)
  ctest_start(Nightly)
else()
  ctest_start(Continuous)
endif()

if(NOT TEST_ONLY)
list(APPEND options "-DCMAKE_INSTALL_PREFIX=install")
if(ENABLE_SANITIZERS)
  list(APPEND options "-DCMAKE_CXX_FLAGS='-O1 -fsanitize=address,leak,undefined -fno-omit-frame-pointer -fno-var-tracking-assignments'")
endif()
list(REMOVE_DUPLICATES options)
list(JOIN options ";" optionsstr)
ctest_configure(OPTIONS "${optionsstr}")

ctest_submit()

ctest_build(FLAGS "-j${NCPUS}")

ctest_submit()
endif()

ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}"
           PARALLEL_LEVEL 1
           SCHEDULE_RANDOM ON
           RETURN_VALUE _ctest_test_ret_val)

ctest_submit()

if(_ctest_test_ret_val)
  Message(FATAL_ERROR "Some tests failed.")
endif()
