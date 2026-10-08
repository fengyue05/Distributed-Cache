cmake_minimum_required(VERSION 3.20) 
project(KCache)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories(${CMAKE_SOURCE_DIR}/src/include 
                    ${CMAKE_SOURCE_DIR}/src/proto)  # src/proto 生成的 proto 和 grpc

find_package(GTest CONFIG REQUIRED) 
set(GFLAGS_USE_TARGET_NAMESPACE TRUE)
find_package(gflags CONFIG REQUIRED)
find_package(Protobuf REQUIRED) 
find_package(gRPC CONFIG QUIET)
if(NOT gRPC_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET grpc++)
    add_library(gRPC::grpc++ ALIAS PkgConfig::GRPC)

    find_program(GRPC_CPP_PLUGIN_PATH grpc_cpp_plugin REQUIRED)
    add_executable(gRPC::grpc_cpp_plugin IMPORTED GLOBAL)
    set_target_properties(gRPC::grpc_cpp_plugin PROPERTIES
        IMPORTED_LOCATION "${GRPC_CPP_PLUGIN_PATH}")
endif()
find_package(etcd-cpp-api CONFIG REQUIRED) 
find_package(fmt CONFIG REQUIRED) 
find_package(spdlog CONFIG REQUIRED) 
find_package(httplib CONFIG QUIET)
if(NOT httplib_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CPP_HTTPLIB REQUIRED IMPORTED_TARGET cpp-httplib)
    add_library(httplib::httplib ALIAS PkgConfig::CPP_HTTPLIB)
endif()
find_package(nlohmann_json CONFIG REQUIRED)

add_subdirectory(src) 
add_subdirectory(test) 
add_subdirectory(example) 
