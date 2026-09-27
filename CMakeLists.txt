cmake_minimum_required(VERSION 3.20) 
project(KCache)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories(${CMAKE_SOURCE_DIR}/src/include 
                    ${CMAKE_SOURCE_DIR}/src/proto)  # src/proto 生成的 proto 和 grpc

find_package(GTest CONFIG REQUIRED) 
find_package(gflags CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED) 
find_package(gRPC CONFIG REQUIRED) 
find_package(etcd-cpp-apiv3 CONFIG REQUIRED) 
find_package(fmt CONFIG REQUIRED) 
find_package(spdlog CONFIG REQUIRED) 
find_package(httplib CONFIG REQUIRED) 
find_package(nlohmann_json CONFIG REQUIRED)

add_subdirectory(src) 
add_subdirectory(test) 
add_subdirectory(example) 