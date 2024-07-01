cmake_minimum_required(VERSION 3.00)
project(
        SimpleHttp
        VERSION 0.1
        DESCRIPTION "Simple Http"
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(Boost_INCLUDE_DIR /usr/include/boost)
set(Boost_LIBRARIES /usr/lib64)

link_libraries(
        pthread
)
add_compile_options(-Wall)

FIND_PACKAGE(Boost 1.81 COMPONENTS system thread)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})

add_executable(SimpleHttp main.cpp)

TARGET_LINK_LIBRARIES(SimpleHttp LINK_PUBLIC ${Boost_LIBRARIES})

