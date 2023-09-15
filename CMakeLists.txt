cmake_minimum_required(VERSION 3.15..3.26)

project(CASPIAN LANGUAGES CXX)

include(GNUInstallDirs)

set(PYBIND11_NEWPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

set(headers
   include/backend.hpp
   include/constants.hpp
   include/hardware.hpp
   include/network_conversion.hpp
   include/network.hpp
   include/processor.hpp
   include/simulator.hpp
   include/ucaspian.hpp
)

set(sources
   src/network_conversion.cpp
   src/network.cpp
   src/packets.cpp
   src/processor.cpp
   src/simulator.cpp
   src/ucaspian.cpp
   src/verilator_caspian.cpp
)

set(bindings
   bindings/backend.cpp
   bindings/bindings.cpp
   bindings/concurrentqueue.h
   bindings/fast_infer.cpp
   bindings/network.cpp
   bindings/processor.cpp
)

add_library(caspian_static STATIC ${sources} ${headers})
add_library(framework::caspian ALIAS caspian_static)
target_compile_features(caspian_static PUBLIC cxx_std_14)
target_include_directories(caspian_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(caspian_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_link_libraries(caspian_static PUBLIC framework::framework)

pybind11_add_module(caspian ${bindings})
target_link_libraries(caspian PRIVATE caspian_static)
target_compile_definitions(caspian PRIVATE VERSION_INFO=${PROJECT_VERSION})

if(DEFINED SKBUILD)
    install(TARGETS caspian LIBRARY DESTINATION .)
else()
    install(TARGETS caspian_static)
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
