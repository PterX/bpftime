cmake_minimum_required(VERSION 3.15)

#
# Set project options
#
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/StandardSettings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/CompilerWarnings.cmake)


add_library(bpftime_vm_compat INTERFACE)

target_include_directories(bpftime_vm_compat INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include ${SPDLOG_INCLUDE})

if(${BPFTIME_LLVM_JIT})
  message(STATUS "Supporting llvm-jit")
  add_subdirectory(llvm-vm)
endif()

if(${BPFTIME_UBPF_JIT})
  message(STATUS "Supporting ubpf-jit")
  add_subdirectory(ubpf-vm)
endif()
