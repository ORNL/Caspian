cmake_minimum_required(VERSION 3.15..3.26)

project(CASPIAN VERSION 0.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

set(sources
   src/network_conversion.cpp
   src/network.cpp
   src/processor.cpp
   src/simulator.cpp
)

set(bindings
   bindings/backend.cpp
   bindings/bindings.cpp
   bindings/concurrentqueue.h
   bindings/fast_infer.cpp
   bindings/network.cpp
   bindings/processor.cpp
)

add_library(framework_caspian SHARED ${sources})
add_library(framework::caspian ALIAS framework_caspian)
target_compile_features(framework_caspian PUBLIC cxx_std_14)
target_include_directories(framework_caspian PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/framework/caspian>
)
target_link_libraries(framework_caspian PUBLIC framework)

pybind11_add_module(caspian ${bindings})
target_link_libraries(caspian PRIVATE framework_caspian)
target_compile_definitions(caspian PRIVATE VERSION_INFO=${PROJECT_VERSION})

if(DEFINED SKBUILD)
    if(APPLE)
    set_target_properties(
      caspian PROPERTIES INSTALL_RPATH "@loader_path/framework/${CMAKE_INSTALL_LIBDIR}")
    else()
    set_target_properties(
      caspian PROPERTIES INSTALL_RPATH "$ORIGIN/framework/${CMAKE_INSTALL_LIBDIR}")
    endif()

    install(TARGETS framework_caspian LIBRARY DESTINATION framework/${CMAKE_INSTALL_LIBDIR})
    install(TARGETS caspian LIBRARY DESTINATION .)
else()
  # TODO CMake package config helpers
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/framework/caspian)
  install(TARGETS framework_caspian LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(TARGETS caspian LIBRARY DESTINATION ${Python_SITEARCH})
endif()
