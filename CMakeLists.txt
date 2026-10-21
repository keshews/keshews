cmake_minimum_required(VERSION 3.20)
project(keshews LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(APPLE AND NOT LLVM_DIR)
  foreach(_prefix /opt/homebrew/opt/llvm /usr/local/opt/llvm)
    if(EXISTS "${_prefix}/lib/cmake/llvm")
      list(APPEND CMAKE_PREFIX_PATH "${_prefix}")
    endif()
  endforeach()
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION} from ${LLVM_DIR}")
if(LLVM_PACKAGE_VERSION VERSION_LESS 15)
  message(FATAL_ERROR "LLVM 15 or newer is required (found ${LLVM_PACKAGE_VERSION})")
endif()

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("

  int main() { std::string s = std::format(\"{} {}\", 1, \"x\"); return s.empty(); }
" HAVE_STD_FORMAT)
if(NOT HAVE_STD_FORMAT)
  message(FATAL_ERROR
    "This project needs std::format (<format>). Use GCC 13+, Clang 17+ "
    "(with a recent libstdc++/libc++), or MSVC 2019 16.10+ / 2022.")
endif()





add_executable(keshews
    src/main.cpp
    src/ast.cpp
    src/tac.cpp
    src/semantic_analyzer.cpp
    src/symbol_table.cpp
)

target_compile_features(keshews PRIVATE cxx_std_20)

target_include_directories(keshews PRIVATE
    include
    ${LLVM_INCLUDE_DIRS}
)


separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(keshews PRIVATE ${LLVM_DEFINITIONS_LIST})

if(MSVC)
  target_compile_options(keshews PRIVATE /W4 /EHsc /utf-8 /permissive-)

  target_compile_definitions(keshews PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(keshews PRIVATE -Wall -Wextra)
endif()





if(LLVM_LINK_LLVM_DYLIB AND TARGET LLVM)
  message(STATUS "Linking against the LLVM shared library")
  target_link_libraries(keshews PRIVATE LLVM)
else()
  llvm_map_components_to_libnames(LLVM_LIBS core passes support)
  target_link_libraries(keshews PRIVATE ${LLVM_LIBS})
endif()
